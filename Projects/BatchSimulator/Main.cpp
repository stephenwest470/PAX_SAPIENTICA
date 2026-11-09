/*##########################################################################################

    PAX SAPIENTICA Library 💀🌿🌏

    [Planning]		2023-2024 As Project
    [Production]	2023-2024 As Project
    [Contact Us]	wanotaitei@gmail.com			https://github.com/AsPJT/PAX_SAPIENTICA
    [License]		Distributed under the CC0 1.0.	https://creativecommons.org/publicdomain/zero/1.0/

##########################################################################################*/

// PAXS_BatchSimulator: headless replicate runs of settlement simulation models, batch runs across
// models, and statistical analysis of the results.
//
//   PAXS_BatchSimulator batch   <Batch.tsv> [--runs N] [--jobs J] [--out DIR] [--seed-base S]
//                                           [--steps N] [--no-analyze]
//   PAXS_BatchSimulator run     <model> [--seed S] [--out DIR] [--steps N]
//   PAXS_BatchSimulator analyze <batch output DIR>
//
// Paths are relative to the asset root set in Config.tsv (run from the repository root, or from the
// build directory whose Config.tsv points at it).
//
// Output layout of a batch:
//   <out>/Batch.tsv, Expected.tsv, Zones.tsv     copies of the batch definition
//   <out>/<id>/Seeds.tsv                         one row per replicate: seed and result folder
//   <out>/<id>/ModelInputs/                      the model's Settings.tsv and District.tsv
//   <out>/<id>/<timestamp>/                      one replicate: the simulator's standard outputs,
//                                                RunInfo.tsv, DistrictTimeSeries.tsv, SettlementSnapshots.bin,
//                                                RunSummary.tsv
//   <out>/Analysis/                              statistics across replicates and models

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <PAX_SAPIENTICA/Interface/IProgressReporter.hpp>
#include "ReplicateStatistics.hpp"
#include <PAX_SAPIENTICA/Simulation/Config/SimulationConst.hpp>
#include <PAX_SAPIENTICA/Simulation/Manager/SettlementSimulator.hpp>
#include <PAX_SAPIENTICA/Simulation/Output/SimulationResultWriter.hpp>
#include <PAX_SAPIENTICA/System/AppConfig.hpp>
#include <PAX_SAPIENTICA/Utility/LoggerIntegration.hpp>
#include <PAX_SAPIENTICA/Utility/MurMur3.hpp>
#include <PAX_SAPIENTICA/Utility/StringUtils.hpp>
#include <PAX_SAPIENTICA/Utility/TimeUtils.hpp>

namespace fs = std::filesystem;

namespace {

    // ---------------------------------------------------------------------------------------------
    // Small utilities
    // ---------------------------------------------------------------------------------------------

    using Table = std::vector<std::vector<std::string>>;

    /// @brief Asset root (from Config.tsv) joined with a relative path
    fs::path assetPath(const std::string& relative) {
        return fs::path(paxs::AppConfig::getInstance().getRootPath() + relative);
    }

    /// @brief Read a TSV file (BOM and CR stripped, empty lines skipped)
    Table readTsv(const fs::path& path) {
        Table rows;
        std::ifstream ifs(path, std::ios::binary);
        std::string line;
        bool first = true;
        while (std::getline(ifs, line)) {
            if (first && line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
            first = false;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::vector<std::string> cells;
            std::stringstream ss(line);
            std::string cell;
            while (std::getline(ss, cell, '\t')) cells.push_back(cell);
            if (!line.empty() && line.back() == '\t') cells.emplace_back();
            rows.push_back(cells);
        }
        return rows;
    }

    /// @brief TSV with a header row, read as one map per row
    std::vector<std::map<std::string, std::string>> readRecords(const fs::path& path) {
        const Table rows = readTsv(path);
        std::vector<std::map<std::string, std::string>> records;
        if (rows.empty()) return records;
        for (std::size_t r = 1; r < rows.size(); ++r) {
            std::map<std::string, std::string> record;
            for (std::size_t c = 0; c < rows[0].size(); ++c) {
                record[rows[0][c]] = (c < rows[r].size()) ? rows[r][c] : std::string();
            }
            records.push_back(record);
        }
        return records;
    }

    /// @brief key<TAB>value file (first two columns; header and #-lines ignored)
    std::map<std::string, std::string> readKeyValues(const fs::path& path) {
        std::map<std::string, std::string> kv;
        const Table rows = readTsv(path);
        for (std::size_t r = 1; r < rows.size(); ++r) {
            if (rows[r].size() < 2 || rows[r][0].empty() || rows[r][0][0] == '#') continue;
            kv[rows[r][0]] = rows[r][1];
        }
        return kv;
    }

    double toDouble(const std::string& s, const double fallback = paxs::stats::nan()) {
        if (s.empty()) return fallback;
        try { return std::stod(s); } catch (...) { return fallback; }
    }

    /// @brief "1-72" or "6,7,8" or "1-5,9" -> set of IDs
    std::set<int> parseIdList(const std::string& text) {
        std::set<int> ids;
        std::stringstream ss(text);
        std::string part;
        while (std::getline(ss, part, ',')) {
            if (part.empty()) continue;
            const std::size_t dash = part.find('-', 1);
            if (dash == std::string::npos) {
                ids.insert(std::stoi(part));
            } else {
                for (int i = std::stoi(part.substr(0, dash)); i <= std::stoi(part.substr(dash + 1)); ++i) ids.insert(i);
            }
        }
        return ids;
    }

    std::string formatDouble(const double v, const int precision = 4) {
        if (std::isnan(v)) return "NA";
        if (std::isinf(v)) return v > 0 ? "Inf" : "-Inf";
        std::ostringstream os;
        os << std::setprecision(precision) << std::fixed << v;
        return os.str();
    }

    std::string formatP(const double p) {
        if (std::isnan(p)) return "NA";
        if (p < 0.0001) return "<0.0001";
        return formatDouble(p, 4);
    }

    /// @brief Astronomical year (1 BC = 0) from a Julian day plus months elapsed
    double astronomicalYear(const int start_julian_day, const std::uint_least64_t step, const int steps_per_year) {
        const double days = static_cast<double>(step) * 365.25 / static_cast<double>(steps_per_year);
        return (static_cast<double>(start_julian_day) + days - 1721423.5) / 365.25 + 1.0;
    }

    /// @brief "950 BC" / "AD 50" from an astronomical year
    std::string calendarLabel(const double astronomical_year) {
        if (std::isnan(astronomical_year)) return "not reached";
        const long y = static_cast<long>(std::floor(astronomical_year));
        return (y <= 0) ? std::to_string(1 - y) + " BC" : "AD " + std::to_string(y);
    }

    /// @brief Calendar year (-950 = 950 BC) to astronomical year
    double calendarToAstronomical(const double calendar_year) {
        return (calendar_year < 0) ? calendar_year + 1.0 : calendar_year;
    }

    std::uint_least64_t splitMix64(std::uint_least64_t x) {
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    }

    /// @brief Nonzero 32-bit seed (0 means "random" elsewhere in the library)
    std::uint_least32_t makeSeed(std::optional<std::uint_least64_t> seed_base, const std::string& model_id, const int run_index) {
        std::uint_least32_t seed = 0;
        if (seed_base.has_value()) {
            std::uint_least64_t x = *seed_base ^ paxs::MurMur3::calcHash(model_id.size(), model_id.c_str());
            x = splitMix64(x + static_cast<std::uint_least64_t>(run_index));
            seed = static_cast<std::uint_least32_t>(x ^ (x >> 32));
        } else {
            static std::random_device device;
            seed = device();
        }
        return (seed == 0) ? 1u : seed;
    }

    /// @brief Model path inside Data/Simulations (the "Sample" placeholder of Data/Settings.tsv)
    std::string modelPath(const std::uint_least32_t setting_key, const std::string& model) {
        std::string path = paxs::AppConfig::getInstance().getSettingPath(setting_key);
        paxs::StringUtils::replace(path, "Sample", model);
        return path;
    }

    std::string currentDateTime() { return paxs::TimeUtils::getCurrentDateTime("%Y-%m-%d %H:%M:%S"); }

    // ---------------------------------------------------------------------------------------------
    // run: one replicate
    // ---------------------------------------------------------------------------------------------

    struct DistrictTotals {
        std::uint_least64_t population = 0;
        std::uint_least64_t farmers = 0;
        double snp_sum = 0.0; // sum over people of migrant-derived ancestry (0..1)
    };

    /// @brief District ID per simulation cell, read from the model's district tiles.
    /// @details The simulator keeps its own copy private, so the runner decodes the same files
    ///          (8-bit run-length tiles, as in Input8BitBinary) instead of modifying the library.
    class DistrictMap {
    public:
        bool load(const std::string& map_list_path, const paxs::Vector2<int>& start_tile, const paxs::Vector2<int>& end_tile) {
            start_ = start_tile;
            for (const auto& r : readRecords(assetPath(map_list_path))) {
                if (r.count("key") == 0 || r.at("key") != "district") continue;
                directory_ = r.at("file_path");
                zoom_ = r.count("z") ? r.at("z") : "10";
                extension_ = r.count("file_name_extension") ? r.at("file_name_extension") : ".bin";
            }
            if (directory_.empty()) return false;
            for (int ty = start_tile.y; ty <= end_tile.y; ++ty) {
                for (int tx = start_tile.x; tx <= end_tile.x; ++tx) {
                    const fs::path file = assetPath(directory_ + "zxy_" + zoom_ + "_" + std::to_string(tx) + "_" + std::to_string(ty) + extension_);
                    if (!fs::exists(file)) continue;
                    tiles_[key(tx, ty)] = decode(file);
                }
            }
            return true;
        }

        /// @brief District at a position in cells from the area origin (0 = sea / no data)
        int at(const paxs::Vector2<int>& position) const {
            if (position.x < 0 || position.y < 0) return 0;
            const auto it = tiles_.find(key(start_.x + position.x / 256, start_.y + position.y / 256));
            if (it == tiles_.end()) return 0;
            return it->second[static_cast<std::size_t>((position.y % 256) * 256 + (position.x % 256))];
        }

    private:
        paxs::Vector2<int> start_{};
        std::string directory_, zoom_, extension_;
        std::map<std::uint_least64_t, std::vector<std::uint_least8_t>> tiles_;

        static std::uint_least64_t key(const int x, const int y) {
            return (static_cast<std::uint_least64_t>(static_cast<std::uint_least32_t>(x)) << 32) | static_cast<std::uint_least32_t>(y);
        }

        static std::vector<std::uint_least8_t> decode(const fs::path& file) {
            std::ifstream ifs(file, std::ios::binary);
            const std::vector<unsigned char> data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            std::vector<std::uint_least8_t> out(256 * 256, 0);
            std::size_t i = 0, j = 0;
            while (i < data.size() && j < out.size()) {
                const unsigned char b = data[i];
                if (b <= 253) { out[j++] = b; ++i; continue; }
                if (i + (b == 255 ? 2 : 3) >= data.size()) break;
                const unsigned char value = data[i + 1];
                const std::size_t length = (b == 255 ? data[i + 2] : (static_cast<std::size_t>(data[i + 2]) << 8) + data[i + 3]) + 1;
                for (std::size_t k = 0; k < length && j < out.size(); ++k) out[j++] = value;
                i += (b == 255) ? 3 : 4;
            }
            return out;
        }
    };

    std::set<std::string> listDirectories(const fs::path& dir) {
        std::set<std::string> names;
        std::error_code ec;
        if (!fs::exists(dir, ec)) return names;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (e.is_directory()) names.insert(e.path().filename().string());
        }
        return names;
    }

    std::map<int, DistrictTotals> collectDistrictTotals(paxs::SettlementSimulator& simulator, const DistrictMap& districts) {
        std::map<int, DistrictTotals> totals;
        for (const auto& grid : simulator.getSettlementGrids()) {
            for (const auto& settlement : grid.second.cgetSettlements()) {
                const std::size_t population = settlement.getPopulation();
                if (population == 0) continue;
                DistrictTotals& t = totals[districts.at(settlement.getPosition())];
                t.population += population;
                t.farmers += settlement.getFarmingPopulation();
                t.snp_sum += settlement.getSNP() * static_cast<double>(population);
            }
        }
        return totals;
    }

    int commandRun(const std::string& model, std::uint_least32_t seed, const std::string& out_dir, int steps_override,
        const std::uint_least64_t max_population) {
        if (seed == 0) seed = makeSeed(std::nullopt, model, 0);
        {
            const fs::path model_dir = assetPath(modelPath(paxs::MurMur3::calcHash("SimulationProvincesPath"), model));
            if (!fs::exists(model_dir / "Settings.tsv") || !fs::exists(model_dir / "District.tsv")) {
                std::cerr << "Model not found: " << model_dir.string() << " needs Settings.tsv and District.tsv" << std::endl;
                return 2;
            }
        }

        // Load this model's constants (singleton) and switch off the shared "Labeled" output, which
        // would make parallel replicates write into the same directory.
        paxs::SimulationConstants& constants = paxs::SimulationConstants::getInstance(model);
        constants.init(model);
        constants.output_directory_name.clear();

        const int total_steps = (steps_override > 0) ? steps_override : constants.total_steps;
        if (total_steps <= 0) {
            std::cerr << "No step count: set total_steps in Settings.tsv or pass --steps." << std::endl;
            return 2;
        }
        const int output_every = std::max(1, constants.output_step_frequency);

        const std::string map_list_path = modelPath(paxs::MurMur3::calcHash("SimulationXYZTiles"), model);
        const std::string provinces_path = modelPath(paxs::MurMur3::calcHash("SimulationProvincesPath"), model);

        std::cout << "Model: " << model << "\nSeed: " << seed << "\nSteps: " << total_steps << std::endl;
        const auto start_time = std::chrono::steady_clock::now();

        DistrictMap districts;
        if (!districts.load(map_list_path, constants.getStartArea(), constants.getEndArea())) {
            std::cerr << "No district map in " << map_list_path << std::endl;
            return 2;
        }

        // The simulator writes to SimulationResults/<date-time>/ (relative to the asset root, which must be
        // the working directory). Find this run's folder by comparing the listing before and after init().
        // Parallel runs must therefore each use their own working directory (the batch command does this).
        const fs::path results_root = assetPath("SimulationResults");
        const std::set<std::string> before = listDirectories(results_root);

        paxs::NullProgressReporter quiet;
        auto simulator_ptr = std::make_unique<paxs::SettlementSimulator>();
        paxs::SettlementSimulator& simulator = *simulator_ptr;
        simulator.setProgressReporter(&quiet);
        simulator.setEnvironment(map_list_path, provinces_path, seed);
        simulator.init();

        std::vector<std::string> created;
        for (const auto& name : listDirectories(results_root)) if (!before.count(name)) created.push_back(name);
        if (created.size() != 1) {
            std::cerr << "Could not identify this run's result folder in " << results_root.string()
                << " (" << created.size() << " new folders). Run parallel jobs through the batch command." << std::endl;
            return 2;
        }
        const std::string run_name = created.front();
        fs::path result_path = results_root / run_name;
        std::cout << "Result directory: " << result_path.string() << std::endl;

        {
            std::ofstream info(result_path / "RunInfo.tsv");
            info << "key\tvalue\n"
                << "model\t" << model << '\n'
                << "seed\t" << seed << '\n'
                << "total_steps\t" << total_steps << '\n'
                << "start_julian_day\t" << constants.start_julian_day << '\n'
                << "steps_per_year\t" << constants.steps_per_year << '\n'
                << "started\t" << currentDateTime() << '\n';
        }
        // Keep a copy of the exact inputs next to the results
        for (const char* name : { "Settings.tsv", "District.tsv" }) {
            std::error_code ec;
            fs::copy_file(assetPath(provinces_path) / name, result_path / name, fs::copy_options::overwrite_existing, ec);
        }

        std::ofstream series(result_path / "DistrictTimeSeries.tsv");
        series << "step\tastronomical_year\tdistrict\tpopulation\tfarmers\tsnp_sum\n";

        // Settlement snapshots for visual replay (Scripts/ReplaySimulation.py). Little-endian binary:
        //   header  "PAXSSNP1", u32 start_julian_day, u32 steps_per_year, i32 area start x, i32 area start y (z10 tiles)
        //   frame   u32 step, u32 settlement count, then 12 bytes per settlement:
        //           u16 x, u16 y (cells from the area origin), u16 population, u16 farmers,
        //           u8 SNP (0-255), u8 mean mtDNA index, u8 majority language, u8 bronze (clamped to 255)
        // Reading state only: the simulation and its random numbers are unaffected.
        std::ofstream snapshots(result_path / "SettlementSnapshots.bin", std::ios::binary);
        auto put = [&snapshots](const auto value, const int bytes) {
            for (int b = 0; b < bytes; ++b) snapshots.put(static_cast<char>((static_cast<std::uint_least64_t>(value) >> (8 * b)) & 0xFF));
        };
        snapshots.write("PAXSSNP1", 8);
        put(constants.start_julian_day, 4);
        put(constants.steps_per_year, 4);
        put(constants.getStartArea().x, 4);
        put(constants.getStartArea().y, 4);
        auto clamp16 = [](const std::size_t v) { return static_cast<std::uint_least16_t>(std::min<std::size_t>(v, 65535)); };

        // Marital status by sex, 5-year age group and subsistence (people and currently married)
        std::ofstream marital(result_path / "MaritalStatus.tsv");
        marital << "step\tsex\tage_group\tfarming\tpeople\tmarried\n";

        auto record = [&](const std::uint_least64_t step) {
            const double year = astronomicalYear(constants.start_julian_day, step, constants.steps_per_year);
            for (const auto& [district, t] : collectDistrictTotals(simulator, districts)) {
                series << step << '\t' << formatDouble(year, 3) << '\t' << district << '\t'
                    << t.population << '\t' << t.farmers << '\t' << formatDouble(t.snp_sum, 3) << '\n';
            }
            series.flush();

            std::uint_least32_t count = 0;
            for (const auto& grid : simulator.getSettlementGrids()) count += static_cast<std::uint_least32_t>(grid.second.cgetSettlements().size());
            put(step, 4);
            put(count, 4);
            for (const auto& grid : simulator.getSettlementGrids()) {
                for (const auto& s : grid.second.cgetSettlements()) {
                    const std::size_t population = s.getPopulation();
                    put(clamp16(static_cast<std::size_t>(std::max(0, s.getPosition().x))), 2);
                    put(clamp16(static_cast<std::size_t>(std::max(0, s.getPosition().y))), 2);
                    put(clamp16(population), 2);
                    put(clamp16(s.getFarmingPopulation()), 2);
                    put(population == 0 ? 0 : static_cast<int>(std::lround(std::clamp(s.getSNP(), 0.0, 1.0) * 255.0)), 1);
                    put(population == 0 ? 0 : std::min<std::size_t>(s.getMostMtDNA(), 255), 1);
                    put(s.getLanguage(), 1);
                    put(std::min<std::uint_least32_t>(s.getBronze(), 255), 1);
                }
            }
            snapshots.flush();

            // [sex][age group][farming] -> {people, married}
            std::array<std::array<std::array<std::array<std::uint_least64_t, 2>, 2>, 21>, 2> tally{};
            for (const auto& grid : simulator.getSettlementGrids()) {
                for (const auto& s : grid.second.cgetSettlements()) {
                    for (const auto& agent : s.cgetAgents()) {
                        auto& cell = tally[agent.isFemale() ? 1 : 0][std::min<std::size_t>(agent.getAgeSizeT() / 5, 20)][agent.cgetFarming() > 0 ? 1 : 0];
                        ++cell[0];
                        if (agent.isMarried()) ++cell[1];
                    }
                }
            }
            for (int sex = 0; sex < 2; ++sex) for (int g = 0; g <= 20; ++g) for (int f = 0; f < 2; ++f) {
                const auto& cell = tally[sex][g][f];
                if (cell[0] == 0) continue;
                marital << step << '\t' << (sex ? "F" : "M") << '\t' << g * 5 << '\t' << f << '\t' << cell[0] << '\t' << cell[1] << '\n';
            }
            marital.flush();
        };

        int done = 0;
        std::string stop_reason = "completed";
        record(0);
        while (done < total_steps) {
            const int chunk = std::min(output_every - (done % output_every), total_steps - done);
            simulator.run(chunk);
            done += chunk;
            if (done % output_every == 0 || done == total_steps) {
                record(static_cast<std::uint_least64_t>(done));
                const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                std::cout << "step " << done << "/" << total_steps << "  "
                    << calendarLabel(astronomicalYear(constants.start_julian_day, done, constants.steps_per_year))
                    << "  population " << simulator.cgetPopulationNum()
                    << "  elapsed " << formatDouble(elapsed / 60.0, 1) << " min" << std::endl;
                if (max_population > 0 && simulator.cgetPopulationNum() >= max_population) {
                    stop_reason = "population_cap";
                    std::cout << "Stopped: population " << simulator.cgetPopulationNum() << " reached the cap of " << max_population << std::endl;
                    break;
                }
            }
        }

        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
        std::ofstream summary(result_path / "RunSummary.tsv");
        summary << "key\tvalue\n"
            << "model\t" << model << '\n'
            << "seed\t" << seed << '\n'
            << "steps_completed\t" << done << '\n'
            << "stop_reason\t" << stop_reason << '\n'
            << "max_population\t" << max_population << '\n'
            << "final_population_all_districts\t" << simulator.cgetPopulationNum() << '\n'
            << "wall_seconds\t" << formatDouble(elapsed, 1) << '\n'
            << "finished\t" << currentDateTime() << '\n';
        summary.close();
        series.close();
        snapshots.close();
        marital.close();
        simulator_ptr.reset(); // closes the simulator's own result files before the folder is moved

        if (!out_dir.empty()) {
            const fs::path target_dir = fs::path(out_dir).is_absolute() ? fs::path(out_dir) : assetPath(out_dir);
            fs::create_directories(target_dir);
            std::error_code ec;
            fs::rename(result_path, target_dir / run_name, ec);
            if (ec) {
                std::cerr << "Could not move results to " << (target_dir / run_name).string() << ": " << ec.message() << std::endl;
                return 1;
            }
            result_path = target_dir / run_name;
        }
        std::cout << "Results: " << result_path.string() << std::endl;
        std::cout << "Finished in " << formatDouble(elapsed / 60.0, 1) << " min" << std::endl;
        return 0;
    }

    // ---------------------------------------------------------------------------------------------
    // analyze
    // ---------------------------------------------------------------------------------------------

    struct Zone {
        int level = 0;
        std::string key;
        std::string name;
        std::set<int> districts;
        double fujio_astronomical_year = paxs::stats::nan();
    };

    struct RunMetrics {
        std::string directory;
        std::uint_least32_t seed = 0;
        double population = 0.0;     // study districts at the final step
        double toraijin_ratio = 0.0; // population-weighted migrant-derived ancestry
        double farmer_share = 0.0;
        double farming_level = 0.0;
        std::vector<double> zone_share;   // final farmer share per zone
        std::vector<double> zone_arrival; // astronomical year the zone first reached the threshold
    };

    struct ModelRuns {
        std::map<std::string, std::string> definition; // Batch.tsv row
        std::vector<RunMetrics> runs;
    };

    std::optional<RunMetrics> readRun(const fs::path& run_dir, const std::set<int>& study, const std::vector<Zone>& zones, const double threshold) {
        if (!fs::exists(run_dir / "RunSummary.tsv") || !fs::exists(run_dir / "DistrictTimeSeries.tsv")) return std::nullopt;
        RunMetrics m;
        m.directory = run_dir.filename().string();
        m.seed = static_cast<std::uint_least32_t>(toDouble(readKeyValues(run_dir / "RunInfo.tsv")["seed"], 0.0));
        m.zone_share.assign(zones.size(), 0.0);
        m.zone_arrival.assign(zones.size(), paxs::stats::nan());

        // step -> district -> totals
        std::map<long, std::pair<double, std::map<int, DistrictTotals>>> by_step;
        for (const auto& r : readRecords(run_dir / "DistrictTimeSeries.tsv")) {
            auto& entry = by_step[std::stol(r.at("step"))];
            entry.first = toDouble(r.at("astronomical_year"));
            DistrictTotals& t = entry.second[std::stoi(r.at("district"))];
            t.population = static_cast<std::uint_least64_t>(toDouble(r.at("population"), 0.0));
            t.farmers = static_cast<std::uint_least64_t>(toDouble(r.at("farmers"), 0.0));
            t.snp_sum = toDouble(r.at("snp_sum"), 0.0);
        }
        if (by_step.empty()) return std::nullopt;

        auto zoneShare = [](const std::map<int, DistrictTotals>& d, const std::set<int>& ids) {
            double pop = 0.0, farm = 0.0;
            for (const int id : ids) {
                const auto it = d.find(id);
                if (it == d.end()) continue;
                pop += static_cast<double>(it->second.population);
                farm += static_cast<double>(it->second.farmers);
            }
            return (pop > 0.0) ? farm / pop : 0.0;
        };

        for (const auto& [step, entry] : by_step) {
            for (std::size_t z = 0; z < zones.size(); ++z) {
                if (std::isnan(m.zone_arrival[z]) && zoneShare(entry.second, zones[z].districts) >= threshold) {
                    m.zone_arrival[z] = entry.first;
                }
            }
        }

        const auto& final_districts = by_step.rbegin()->second.second;
        double pop = 0.0, farm = 0.0, snp = 0.0;
        for (const auto& [id, t] : final_districts) {
            if (!study.count(id)) continue;
            pop += static_cast<double>(t.population);
            farm += static_cast<double>(t.farmers);
            snp += t.snp_sum;
        }
        m.population = pop;
        m.toraijin_ratio = (pop > 0.0) ? snp / pop : 0.0;
        m.farmer_share = (pop > 0.0) ? farm / pop : 0.0;
        for (std::size_t z = 0; z < zones.size(); ++z) {
            m.zone_share[z] = zoneShare(final_districts, zones[z].districts);
            if (m.zone_share[z] >= threshold) m.farming_level = std::max(m.farming_level, static_cast<double>(zones[z].level));
        }
        return m;
    }

    struct Metric {
        std::string key;
        std::string label;
        double scale; // multiply raw values for display
        std::string unit;
        double expected; // NaN = no expectation (raw units)
        std::function<double(const RunMetrics&)> get;
    };

    int commandAnalyze(const std::string& batch_dir) {
        const fs::path root = assetPath(batch_dir);
        if (!fs::exists(root / "Batch.tsv")) {
            std::cerr << "Batch.tsv not found in " << root.string() << std::endl;
            return 2;
        }
        auto expected = readKeyValues(root / "Expected.tsv");
        const std::string reference = expected.count("reference_model") ? expected["reference_model"] : "B";
        const double threshold = toDouble(expected["farming_threshold"], 0.5);
        const std::set<int> study = parseIdList(expected.count("study_districts") ? expected["study_districts"] : "1-72");

        std::vector<Zone> zones;
        for (const auto& r : readRecords(root / "Zones.tsv")) {
            Zone z;
            z.level = std::stoi(r.at("level"));
            z.key = r.at("key");
            z.name = r.count("ja-JP") ? r.at("ja-JP") : z.key;
            z.districts = parseIdList(r.at("districts"));
            const double fujio = toDouble(r.count("fujio_year") ? r.at("fujio_year") : "");
            z.fujio_astronomical_year = std::isnan(fujio) ? fujio : calendarToAstronomical(fujio);
            zones.push_back(z);
        }
        std::sort(zones.begin(), zones.end(), [](const Zone& a, const Zone& b) { return a.level < b.level; });

        std::vector<std::string> order;
        std::map<std::string, ModelRuns> models;
        for (const auto& r : readRecords(root / "Batch.tsv")) {
            const std::string id = r.at("id");
            order.push_back(id);
            models[id].definition = r;
            const fs::path model_dir = root / id;
            if (!fs::exists(model_dir)) continue;
            std::vector<fs::path> run_dirs;
            for (const auto& entry : fs::directory_iterator(model_dir)) {
                if (entry.is_directory()) run_dirs.push_back(entry.path());
            }
            std::sort(run_dirs.begin(), run_dirs.end());
            for (const auto& d : run_dirs) {
                if (auto m = readRun(d, study, zones, threshold)) models[id].runs.push_back(*m);
            }
        }

        std::vector<Metric> metrics = {
            { "population", "Population AD 50 (万人)", 1.0 / 10000.0, "万人", toDouble(expected["population"]),
              [](const RunMetrics& m) { return m.population; } },
            { "toraijin_ratio", "Migrant ancestry (%)", 100.0, "%", toDouble(expected["toraijin_ratio"]),
              [](const RunMetrics& m) { return m.toraijin_ratio; } },
            { "farming_level", "Farming spread level (Lv.)", 1.0, "", toDouble(expected["farming_level"]),
              [](const RunMetrics& m) { return m.farming_level; } },
            { "farmer_share", "Farmer share of population (%)", 100.0, "%", paxs::stats::nan(),
              [](const RunMetrics& m) { return m.farmer_share; } },
        };
        for (std::size_t z = 0; z < zones.size(); ++z) {
            metrics.push_back({ "arrival_" + zones[z].key, "Farming reaches " + zones[z].name + " (astronomical year)", 1.0, "year",
                zones[z].fujio_astronomical_year, [z](const RunMetrics& m) { return m.zone_arrival[z]; } });
        }

        auto values = [&](const std::string& id, const Metric& metric) {
            std::vector<double> v;
            for (const auto& run : models[id].runs) {
                const double x = metric.get(run);
                if (!std::isnan(x)) v.push_back(x * metric.scale);
            }
            return v;
        };

        const fs::path out = root / "Analysis";
        fs::create_directories(out);

        // --- Per-run values ---
        {
            std::ofstream f(out / "RunMetrics.tsv");
            f << "id\trun_directory\tseed\tpopulation\ttoraijin_ratio\tfarmer_share\tfarming_level";
            for (const auto& z : zones) f << "\tshare_" << z.key << "\tarrival_" << z.key;
            f << '\n';
            for (const auto& id : order) {
                for (const auto& m : models[id].runs) {
                    f << id << '\t' << m.directory << '\t' << m.seed << '\t' << formatDouble(m.population, 0) << '\t'
                        << formatDouble(m.toraijin_ratio) << '\t' << formatDouble(m.farmer_share) << '\t' << formatDouble(m.farming_level, 0);
                    for (std::size_t z = 0; z < zones.size(); ++z) {
                        f << '\t' << formatDouble(m.zone_share[z]) << '\t' << calendarLabel(m.zone_arrival[z]);
                    }
                    f << '\n';
                }
            }
        }

        // --- Descriptive statistics ---
        {
            std::ofstream f(out / "ModelSummary.tsv");
            f << "id\tdescription\tmetric\tn\tmean\tsd\tci95_low\tci95_high\tmedian\tmin\tmax\n";
            for (const auto& id : order) {
                for (const auto& metric : metrics) {
                    const auto s = paxs::stats::summarize(values(id, metric));
                    f << id << '\t' << models[id].definition["description"] << '\t' << metric.key << '\t' << s.n << '\t'
                        << formatDouble(s.mean) << '\t' << formatDouble(s.sd) << '\t' << formatDouble(s.ci_low) << '\t'
                        << formatDouble(s.ci_high) << '\t' << formatDouble(s.median) << '\t' << formatDouble(s.min) << '\t'
                        << formatDouble(s.max) << '\n';
                }
            }
        }

        // --- Each model vs the reference model (Holm across models, per metric and test) ---
        struct Comparison {
            std::string id;
            paxs::stats::TestResult welch, mann_whitney;
            double welch_holm = paxs::stats::nan(), mann_whitney_holm = paxs::stats::nan();
            double difference = paxs::stats::nan();
        };
        std::map<std::string, std::vector<Comparison>> comparisons;
        for (const auto& metric : metrics) {
            const auto ref = values(reference, metric);
            std::vector<Comparison>& list = comparisons[metric.key];
            for (const auto& id : order) {
                if (id == reference) continue;
                Comparison c;
                c.id = id;
                const auto v = values(id, metric);
                c.welch = paxs::stats::welchTTest(v, ref);
                c.mann_whitney = paxs::stats::mannWhitneyExact(v, ref);
                c.difference = paxs::stats::mean(v) - paxs::stats::mean(ref);
                list.push_back(c);
            }
            std::vector<double> pw, pm;
            for (const auto& c : list) { pw.push_back(c.welch.p_value); pm.push_back(c.mann_whitney.p_value); }
            const auto hw = paxs::stats::holmAdjust(pw);
            const auto hm = paxs::stats::holmAdjust(pm);
            for (std::size_t i = 0; i < list.size(); ++i) { list[i].welch_holm = hw[i]; list[i].mann_whitney_holm = hm[i]; }
        }
        {
            std::ofstream f(out / "ComparisonsVsReference.tsv");
            f << "metric\tid\treference\tmean_difference\twelch_t\twelch_df\twelch_p\twelch_p_holm\thedges_g"
                "\tmann_whitney_U\tmann_whitney_p\tmann_whitney_p_holm\trank_biserial_r\n";
            for (const auto& metric : metrics) {
                for (const auto& c : comparisons[metric.key]) {
                    f << metric.key << '\t' << c.id << '\t' << reference << '\t' << formatDouble(c.difference) << '\t'
                        << formatDouble(c.welch.statistic) << '\t' << formatDouble(c.welch.df, 2) << '\t' << formatP(c.welch.p_value) << '\t'
                        << formatP(c.welch_holm) << '\t' << formatDouble(c.welch.effect_size) << '\t'
                        << formatDouble(c.mann_whitney.statistic, 1) << '\t' << formatP(c.mann_whitney.p_value) << '\t'
                        << formatP(c.mann_whitney_holm) << '\t' << formatDouble(c.mann_whitney.effect_size) << '\n';
                }
            }
        }

        // --- Each model vs the expected value (one-sample t-test, Holm across models) ---
        std::map<std::string, std::map<std::string, std::pair<paxs::stats::TestResult, double>>> vs_expected;
        {
            std::ofstream f(out / "ComparisonsVsExpected.tsv");
            f << "metric\tid\texpected\tmean\tmean_minus_expected\tt\tdf\tp\tp_holm\tcohens_d\n";
            for (const auto& metric : metrics) {
                if (std::isnan(metric.expected)) continue;
                const double target = metric.expected * metric.scale;
                std::vector<paxs::stats::TestResult> results;
                std::vector<double> p;
                for (const auto& id : order) {
                    results.push_back(paxs::stats::oneSampleTTest(values(id, metric), target));
                    p.push_back(results.back().p_value);
                }
                const auto holm = paxs::stats::holmAdjust(p);
                for (std::size_t i = 0; i < order.size(); ++i) {
                    const double m = paxs::stats::mean(values(order[i], metric));
                    vs_expected[metric.key][order[i]] = { results[i], holm[i] };
                    f << metric.key << '\t' << order[i] << '\t' << formatDouble(target) << '\t' << formatDouble(m) << '\t'
                        << formatDouble(m - target) << '\t' << formatDouble(results[i].statistic) << '\t'
                        << formatDouble(results[i].df, 0) << '\t' << formatP(results[i].p_value) << '\t' << formatP(holm[i]) << '\t'
                        << formatDouble(results[i].effect_size) << '\n';
                }
            }
        }

        // --- Dose-response: correlation of each variable with each metric (reference + its variants) ---
        {
            std::ofstream f(out / "Correlations.tsv");
            f << "variable\tmetric\tvalues\tn_runs\tpearson_r\tpearson_p\tspearman_rho\tspearman_p\n";
            std::map<std::string, std::vector<std::string>> by_variable;
            for (const auto& id : order) {
                const std::string variable = models[id].definition["variable"];
                if (variable.empty() || variable == "base") continue;
                by_variable[variable].push_back(id);
            }
            for (const auto& [variable, ids] : by_variable) {
                const double base_value = toDouble(models[ids.front()].definition["base_value"]);
                for (const auto& metric : metrics) {
                    std::vector<double> x, y;
                    std::ostringstream levels;
                    auto add = [&](const std::string& id, const double level) {
                        for (const double v : values(id, metric)) { x.push_back(level); y.push_back(v); }
                    };
                    if (!std::isnan(base_value)) { add(reference, base_value); levels << base_value << "(" << reference << ")"; }
                    for (const auto& id : ids) {
                        add(id, toDouble(models[id].definition["value"]));
                        levels << " " << models[id].definition["value"] << "(" << id << ")";
                    }
                    const auto pr = paxs::stats::pearsonCorrelation(x, y);
                    const auto sr = paxs::stats::spearmanCorrelation(x, y);
                    f << variable << '\t' << metric.key << '\t' << levels.str() << '\t' << x.size() << '\t'
                        << formatDouble(pr.effect_size) << '\t' << formatP(pr.p_value) << '\t'
                        << formatDouble(sr.effect_size) << '\t' << formatP(sr.p_value) << '\n';
                }
            }
        }

        // --- Report (Table 1 layout) ---
        {
            std::ofstream f(out / "Report.md");
            auto cell = [&](const std::string& id, const Metric& metric, const int precision) {
                const auto s = paxs::stats::summarize(values(id, metric));
                if (s.n == 0) return std::string("–");
                std::string text = formatDouble(s.mean, precision);
                if (s.n >= 2) text += " ± " + formatDouble(s.sd, precision);
                if (id != reference) {
                    for (const auto& c : comparisons[metric.key]) {
                        if (c.id == id) {
                            const char* stars = paxs::stats::significanceStars(c.welch_holm);
                            if (std::string(stars) != "n.s." && *stars) text += " " + std::string(stars);
                        }
                    }
                }
                return text;
            };
            f << "# Replicate simulation results\n\n";
            f << "Batch: `" << batch_dir << "`  \nGenerated: " << currentDateTime() << "  \n";
            f << "Reference model: " << reference << ". Farming counts as having reached a zone when farmers are at least "
                << formatDouble(threshold * 100.0, 0) << "% of its population. Population and migrant ancestry are for districts "
                << (expected.count("study_districts") ? expected["study_districts"] : "1-72") << ".\n\n";
            f << "Values are mean ± SD over replicates (n per model in the last column). Stars mark a difference from model "
                << reference << " by Welch's t-test with Holm correction across the " << (order.size() - 1)
                << " comparisons: * p<0.05, ** p<0.01, *** p<0.001.\n\n";
            f << "| @ | Variable | Change | Population (万人) | Migrant ancestry (%) | Lv. | n |\n";
            f << "|---|---|---|---|---|---|---|\n";
            f << "| A | Expected | | " << formatDouble(metrics[0].expected * metrics[0].scale, 1) << " | "
                << formatDouble(metrics[1].expected * metrics[1].scale, 0) << " | " << formatDouble(metrics[2].expected, 0) << " | |\n";
            for (const auto& id : order) {
                f << "| " << id << " | " << models[id].definition["description"] << " | "
                    << models[id].definition["value"] << " | " << cell(id, metrics[0], 1) << " | " << cell(id, metrics[1], 1)
                    << " | " << cell(id, metrics[2], 1) << " | " << models[id].runs.size() << " |\n";
            }

            f << "\n## Difference from the expected values\n\n";
            f << "One-sample t-test of each model's replicates against the expected value (Holm-adjusted p across models).\n\n";
            f << "| @ | Population mean − expected (万人) | p (Holm) | Migrant ancestry mean − expected (pt) | p (Holm) |\n|---|---|---|---|---|\n";
            for (const auto& id : order) {
                auto row = [&](const Metric& metric) {
                    const auto it = vs_expected[metric.key].find(id);
                    if (it == vs_expected[metric.key].end() || models[id].runs.empty()) return std::string("– | –");
                    const double diff = paxs::stats::mean(values(id, metric)) - metric.expected * metric.scale;
                    return formatDouble(diff, 1) + " | " + formatP(it->second.second);
                };
                f << "| " << id << " | " << row(metrics[0]) << " | " << row(metrics[1]) << " |\n";
            }

            f << "\n## Farming arrival (first decade the zone reaches the threshold)\n\n| @ |";
            for (const auto& z : zones) f << " " << z.name << " |";
            f << "\n|---|";
            for (std::size_t z = 0; z < zones.size(); ++z) f << "---|";
            f << "\n| Fujio (2009) |";
            for (const auto& z : zones) f << " " << (std::isnan(z.fujio_astronomical_year) ? "–" : calendarLabel(z.fujio_astronomical_year)) << " |";
            f << '\n';
            for (const auto& id : order) {
                f << "| " << id << " |";
                for (std::size_t z = 0; z < zones.size(); ++z) {
                    const auto v = values(id, metrics[4 + z]);
                    const std::size_t total = models[id].runs.size();
                    if (v.empty()) { f << " not reached (0/" << total << ") |"; continue; }
                    f << " " << calendarLabel(paxs::stats::mean(v)) << " (" << v.size() << "/" << total << ") |";
                }
                f << '\n';
            }
            f << "\nFull statistics: ModelSummary.tsv, ComparisonsVsReference.tsv (Welch t and exact Mann-Whitney), "
                "ComparisonsVsExpected.tsv, Correlations.tsv (dose-response per variable), RunMetrics.tsv (every replicate).\n";
        }

        std::cout << "Analysis written to " << (batch_dir + "/Analysis") << std::endl;
        for (const auto& id : order) std::cout << "  " << id << ": " << models[id].runs.size() << " complete runs" << std::endl;
        return 0;
    }

    // ---------------------------------------------------------------------------------------------
    // batch
    // ---------------------------------------------------------------------------------------------

    struct Job {
        std::string id;
        std::string model;
        int run_number = 0;
        std::uint_least32_t seed = 0;
    };

    std::string quote(const std::string& s) { return "\"" + s + "\""; }

    int countCompleteRuns(const fs::path& model_dir) {
        int count = 0;
        if (!fs::exists(model_dir)) return 0;
        for (const auto& e : fs::directory_iterator(model_dir)) {
            if (e.is_directory() && fs::exists(e.path() / "RunSummary.tsv")) ++count;
        }
        return count;
    }

    /// @brief Result folder of the replicate with this seed (empty if none)
    std::string findRunDirectory(const fs::path& model_dir, const std::uint_least32_t seed) {
        if (!fs::exists(model_dir)) return {};
        for (const auto& e : fs::directory_iterator(model_dir)) {
            if (!e.is_directory() || !fs::exists(e.path() / "RunInfo.tsv")) continue;
            if (readKeyValues(e.path() / "RunInfo.tsv")["seed"] == std::to_string(seed)) return e.path().filename().string();
        }
        return {};
    }

    // Each batch job runs in <model>/_work_<seed>/: a Config.tsv with asset_file ./ and a directory junction
    // Data -> <repository>/Data. The simulator then writes SimulationResults/<date-time>/ inside the job's own
    // folder, so jobs started in the same second cannot collide, and the library stays unmodified.
    const char* kWorkPrefix = "_work_";

    bool makeWorkDirectory(const fs::path& work, const fs::path& data_dir) {
        std::error_code ec;
        fs::create_directories(work, ec);
        std::ofstream(work / "Config.tsv") << "key\tvalue\nasset_file\t./\n";
        if (fs::exists(work / "Data" / "Settings.tsv")) return true;
#ifdef _WIN32
        const std::string command = "\"mklink /J " + quote((work / "Data").string()) + " " + quote(data_dir.string()) + " > nul\"";
        return std::system(command.c_str()) == 0 && fs::exists(work / "Data" / "Settings.tsv");
#else
        fs::create_directory_symlink(data_dir, work / "Data", ec);
        return !ec;
#endif
    }

    /// @brief Remove a job folder. The Data link is removed on its own first (rmdir without /s removes only
    ///        the junction, never the repository data), and nothing is deleted recursively unless it is gone.
    bool removeWorkDirectory(const fs::path& work) {
        std::error_code ec;
        if (!fs::exists(work, ec)) return true;
        const fs::path link = work / "Data";
        if (fs::exists(link, ec) || fs::is_symlink(link, ec)) {
#ifdef _WIN32
            std::system(("\"rmdir " + quote(link.string()) + "\"").c_str());
#else
            fs::remove(link, ec);
#endif
        }
        if (fs::exists(link, ec) || fs::is_symlink(link, ec)) {
            std::cerr << "Left " << work.string() << " in place: could not remove its Data link safely" << std::endl;
            return false;
        }
        fs::remove_all(work, ec);
        return !ec;
    }

    /// @brief Seeds of interrupted replicates; their partial folders are removed
    /// @details An interrupted run is a result folder without RunSummary.tsv or a leftover job folder.
    std::vector<std::uint_least32_t> takeInterruptedSeeds(const fs::path& model_dir) {
        std::vector<std::pair<std::string, std::uint_least32_t>> found;
        if (!fs::exists(model_dir)) return {};
        for (const auto& e : fs::directory_iterator(model_dir)) {
            if (!e.is_directory()) continue;
            const std::string name = e.path().filename().string();
            if (name.rfind(kWorkPrefix, 0) == 0) {
                const double seed = toDouble(name.substr(std::string(kWorkPrefix).size()), 0.0);
                if (seed > 0.0) found.emplace_back(name, static_cast<std::uint_least32_t>(seed));
                continue;
            }
            if (!fs::exists(e.path() / "RunInfo.tsv") || fs::exists(e.path() / "RunSummary.tsv")) continue;
            const double seed = toDouble(readKeyValues(e.path() / "RunInfo.tsv")["seed"], 0.0);
            if (seed > 0.0) found.emplace_back(name, static_cast<std::uint_least32_t>(seed));
        }
        std::sort(found.begin(), found.end());
        std::vector<std::uint_least32_t> seeds;
        for (const auto& [dir, seed] : found) {
            if (dir.rfind(kWorkPrefix, 0) == 0) {
                if (!removeWorkDirectory(model_dir / dir)) continue;
            } else {
                std::error_code ec;
                fs::remove_all(model_dir / dir, ec);
            }
            if (std::find(seeds.begin(), seeds.end(), seed) != seeds.end()) continue;
            std::cout << "  interrupted run " << dir << " (seed " << seed << ") will be redone with the same seed" << std::endl;
            seeds.push_back(seed);
        }
        return seeds;
    }

    int commandBatch(const std::string& exe, const std::string& batch_file, int runs, int jobs, std::string out_dir,
        std::optional<std::uint_least64_t> seed_base, int steps_override, bool analyze, std::uint_least64_t max_population) {
        const fs::path batch_path = assetPath(batch_file);
        const auto rows = readRecords(batch_path);
        if (rows.empty()) {
            std::cerr << "No models in " << batch_path.string() << std::endl;
            return 2;
        }
        if (out_dir.empty()) {
            out_dir = "SimulationResults/Batch_" + batch_path.parent_path().filename().string();
        }
        const fs::path out = assetPath(out_dir);
        fs::create_directories(out);

        // Keep the batch definition with the results
        for (const char* name : { "Batch.tsv", "Expected.tsv", "Zones.tsv" }) {
            std::error_code ec;
            if (fs::exists(batch_path.parent_path() / name)) {
                fs::copy_file(batch_path.parent_path() / name, out / name, fs::copy_options::overwrite_existing, ec);
            }
        }

        std::vector<Job> queue;
        for (const auto& r : rows) {
            const std::string id = r.at("id");
            const std::string model = r.at("model");
            const fs::path model_dir = out / id;
            fs::create_directories(model_dir / "ModelInputs");
            const fs::path inputs = assetPath(modelPath(paxs::MurMur3::calcHash("SimulationProvincesPath"), model));
            for (const char* name : { "Settings.tsv", "District.tsv" }) {
                std::error_code ec;
                fs::copy_file(inputs / name, model_dir / "ModelInputs" / name, fs::copy_options::overwrite_existing, ec);
            }
            if (!fs::exists(model_dir / "Seeds.tsv")) {
                std::ofstream(model_dir / "Seeds.tsv") << "run\tseed\tresult_directory\texit_code\twall_minutes\tfinished\n";
            }
            const int complete = countCompleteRuns(model_dir);
            const std::vector<std::uint_least32_t> interrupted = takeInterruptedSeeds(model_dir);
            for (int k = complete; k < runs; ++k) {
                const std::size_t redo = static_cast<std::size_t>(k - complete);
                const std::uint_least32_t seed = (redo < interrupted.size()) ? interrupted[redo] : makeSeed(seed_base, id, k);
                queue.push_back({ id, model, k + 1, seed });
            }
            std::cout << id << " (" << model << "): " << complete << " complete, " << std::max(0, runs - complete) << " to run" << std::endl;
        }
        std::cout << queue.size() << " replicate(s) to run with " << jobs << " parallel job(s). Output: " << out_dir << std::endl;

        // Interleave models so partial batches still cover every model
        std::stable_sort(queue.begin(), queue.end(), [](const Job& a, const Job& b) { return a.run_number < b.run_number; });

        std::mutex mutex;
        std::atomic<std::size_t> next{ 0 };
        std::atomic<int> failures{ 0 };
        auto worker = [&]() {
            while (true) {
                const std::size_t index = next++;
                if (index >= queue.size()) return;
                const Job& job = queue[index];
                const fs::path model_out = fs::absolute(out / job.id);
                const fs::path log = model_out / ("run_" + std::to_string(job.run_number) + "_seed_" + std::to_string(job.seed) + ".log");
                const fs::path work = model_out / (std::string(kWorkPrefix) + std::to_string(job.seed));
                if (!makeWorkDirectory(work, fs::absolute(assetPath("Data")))) {
                    std::lock_guard<std::mutex> lock(mutex);
                    std::cerr << "Could not prepare " << work.string() << std::endl;
                    ++failures;
                    continue;
                }
                std::string command = "cd /d " + quote(work.string()) + " && " + quote(exe) + " run " + quote(job.model)
                    + " --seed " + std::to_string(job.seed) + " --out " + quote(model_out.string());
                if (steps_override > 0) command += " --steps " + std::to_string(steps_override);
                if (max_population > 0) command += " --max-population " + std::to_string(max_population);
                command += " > " + quote(log.string()) + " 2>&1";
#ifdef _WIN32
                command = "\"" + command + "\""; // cmd.exe strips one level of outer quotes
#endif
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    std::cout << "[" << currentDateTime() << "] start  " << job.id << " run " << job.run_number << " seed " << job.seed << std::endl;
                }
                const auto start = std::chrono::steady_clock::now();
                const int code = std::system(command.c_str());
                const double minutes = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / 60.0;

                std::lock_guard<std::mutex> lock(mutex);
                const std::string run_dir = findRunDirectory(out / job.id, job.seed);
                if (!run_dir.empty()) {
                    // keep the simulator's warning log with the run, then remove the job folder
                    std::error_code ec;
                    if (fs::exists(work / "Save" / "log.txt")) {
                        fs::copy_file(work / "Save" / "log.txt", out / job.id / run_dir / "SimulatorLog.txt", fs::copy_options::overwrite_existing, ec);
                    }
                    removeWorkDirectory(work);
                }
                std::ofstream(out / job.id / "Seeds.tsv", std::ios::app) << job.run_number << '\t' << job.seed << '\t' << run_dir << '\t'
                    << code << '\t' << formatDouble(minutes, 1) << '\t' << currentDateTime() << '\n';
                if (code != 0 || run_dir.empty() || !fs::exists(out / job.id / run_dir / "RunSummary.tsv")) ++failures;
                std::cout << "[" << currentDateTime() << "] " << (code == 0 ? "done   " : "FAILED ") << job.id << " run " << job.run_number
                    << " (" << formatDouble(minutes, 1) << " min) " << run_dir << std::endl;
            }
        };
        std::vector<std::thread> threads;
        for (int i = 0; i < std::max(1, jobs); ++i) threads.emplace_back(worker);
        for (auto& t : threads) t.join();

        if (failures > 0) std::cerr << failures << " replicate(s) failed; see the .log files. Re-run the same command to retry them." << std::endl;
        if (analyze) commandAnalyze(out_dir);
        return (failures > 0) ? 1 : 0;
    }

    void printUsage() {
        std::cout <<
            "PAXS_BatchSimulator - replicate and batch runs of settlement simulation models\n\n"
            "  batch   <Batch.tsv> [--runs N] [--jobs J] [--out DIR] [--seed-base S] [--steps N] [--max-population P] [--no-analyze]\n"
            "          Run N replicates (default 5) of every model in Batch.tsv, J at a time (default 1).\n"
            "          Re-running the same command resumes: only missing replicates are run,\n"
            "          and interrupted ones are redone with their original seed.\n"
            "  run     <model> [--seed S] [--out DIR] [--steps N] [--max-population P]\n"
            "          Run one replicate of a model (e.g. Yayoi2024/B). A random seed is used if none is given.\n"
            "  analyze <batch output DIR>\n"
            "          Statistics across replicates and models (writes DIR/Analysis).\n\n"
            "Paths are relative to the asset root in Config.tsv.\n";
    }

}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) ? 0 : 2;
    }
    const std::string command = argv[1];
    const std::string target = argv[2];

    std::map<std::string, std::string> options;
    std::set<std::string> flags;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-analyze") { flags.insert(arg); continue; }
        if (arg.rfind("--", 0) == 0 && i + 1 < argc) { options[arg] = argv[++i]; continue; }
        std::cerr << "Unknown argument: " << arg << std::endl;
        return 2;
    }
    auto intOption = [&](const std::string& key, const int fallback) {
        return options.count(key) ? std::stoi(options[key]) : fallback;
    };
    const std::uint_least64_t maxPopulation = options.count("--max-population") ? std::stoull(options["--max-population"]) : 0;

    try {
        if (command == "run") {
            const std::uint_least32_t seed = options.count("--seed") ? static_cast<std::uint_least32_t>(std::stoul(options["--seed"])) : 0u;
            return commandRun(target, seed, options.count("--out") ? options["--out"] : "", intOption("--steps", 0), maxPopulation);
        }
        if (command == "batch") {
            std::error_code ec;
            fs::path exe = fs::absolute(argv[0], ec);
            if (!fs::exists(exe) && fs::exists(exe.string() + ".exe")) exe = exe.string() + ".exe";
            std::optional<std::uint_least64_t> seed_base;
            if (options.count("--seed-base")) seed_base = std::stoull(options["--seed-base"]);
            return commandBatch(exe.string(), target, intOption("--runs", 5), intOption("--jobs", 1),
                options.count("--out") ? options["--out"] : "", seed_base, intOption("--steps", 0), flags.count("--no-analyze") == 0,
                maxPopulation);
        }
        if (command == "analyze") {
            return commandAnalyze(target);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    printUsage();
    return 2;
}
