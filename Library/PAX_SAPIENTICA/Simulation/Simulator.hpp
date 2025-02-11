﻿/*##########################################################################################

	PAX SAPIENTICA Library 💀🌿🌏

	[Planning]		2023-2024 As Project
	[Production]	2023-2024 As Project
	[Contact Us]	wanotaitei@gmail.com			https://github.com/AsPJT/PAX_SAPIENTICA
	[License]		Distributed under the CC0 1.0.	https://creativecommons.org/publicdomain/zero/1.0/

##########################################################################################*/

#ifndef PAX_SAPIENTICA_SIMULATION_SIMULATOR_HPP
#define PAX_SAPIENTICA_SIMULATION_SIMULATOR_HPP

/*##########################################################################################

##########################################################################################*/

#include <memory>
#include <stdexcept>

#include <PAX_SAPIENTICA/Logger.hpp>
#include <PAX_SAPIENTICA/Simulation/Agent.hpp>
#include <PAX_SAPIENTICA/Simulation/Environment.hpp>
#include <PAX_SAPIENTICA/Simulation/KanakumaLifeSpan.hpp>
#include <PAX_SAPIENTICA/Simulation/SimulationConst.hpp>
#include <PAX_SAPIENTICA/StatusDisplayer.hpp>
#include <PAX_SAPIENTICA/UniqueIdentification.hpp>

namespace paxs {

    /// @brief Class that performs simulation.
    /// @brief シミュレーションを行うクラス
    class Simulator {
    public:
        using Vector2 = paxs::Vector2<GridType>;

        explicit Simulator() = default;
        explicit Simulator(const std::string& setting_file_path, const unsigned seed = 0) noexcept :
            environment(std::make_unique<Environment>(setting_file_path)), gen(seed) {}

        /// @brief Initialize the simulator.
        /// @brief エージェントの初期化
        /// @details エージェントをクリアし、指定された数だけランダムに配置する
        void init() {
            std::cout << "Initializing..." << std::endl;
            clearAgents();
            randomizeAgents(10000);
        }

        /// @brief Run the simulation for the specified number of steps.
        /// @brief シミュレーションを指定されたステップ数だけ実行する
        void run(const int step_count) noexcept {
#ifdef PAX_SAPIENTICA_DEBUG
            printStatus();
#endif
            std::cout << "Running..." << std::endl;
            for(int i = 0; i < step_count; ++i) {
                step();
            }
            std::cout << "Finished." << std::endl;
#ifdef PAX_SAPIENTICA_DEBUG
            printStatus();
#endif
        }

        /// @brief Execute the simulation for the one step.
        /// @brief シミュレーションを1ステップ実行する
        void step() noexcept {
            for(auto& agent : agents) {
                agent.incrementAge();
                agent.move();
            }

            agents.erase(std::remove_if(agents.begin(), agents.end(), [](const Agent& agent) { return agent.isDead(); }), agents.end());

        }

        /// @brief Get the agent list.
        /// @brief エージェントのリストを取得する
        constexpr std::vector<Agent>& getAgents() noexcept { return agents; }
        /// @brief Get the agent list.
        /// @brief エージェントのリストを取得する
        constexpr const std::vector<Agent>& cgetAgents() const noexcept { return agents; }
    private:
        std::vector<Agent> agents; // エージェントのリスト
        std::shared_ptr<Environment> environment; // 環境
        std::mt19937 gen; // 乱数生成器
        std::uniform_int_distribution<> gender_dist{0, 1}; // 性別の乱数分布

        KanakumaLifeSpan kanakuma_life_span{};

        /// @brief Clear the agents.
        /// @brief エージェントをクリアする
        void clearAgents() noexcept { agents.clear(); }

        /// @brief Randomly place the agents.
        /// @brief エージェントをランダムに配置する
        void randomizeAgents(const int agent_count) {
            const Vector2& offset = SimulationConstants::getInstance()->getEndArea() - SimulationConstants::getInstance()->getStartArea();
            std::uniform_int_distribution<> x_dist(0, pixel_size * offset.x);
            std::uniform_int_distribution<> y_dist(0, pixel_size * offset.y);
            std::uniform_int_distribution<> age_dist(0, 20);
            std::cout << "Randomizing agents..." << std::endl;
            for(int i = 0;i < agent_count;++i) {
                StatusDisplayer::displayProgressBar(i, agent_count);
                Vector2 position = Vector2(x_dist(gen), y_dist(gen));
                while(!environment->isLive(position)) {
                    position = Vector2(x_dist(gen), y_dist(gen));
                }

                // idの生成
                std::uint_least32_t id = UniqueIdentification<std::uint_least32_t>::generate();
                const std::uint_least8_t set_gender = static_cast<std::uint_least8_t>(gender_dist(gen));
                agents.emplace_back(Agent( id, "", position,
                    set_gender,
                    age_dist(gen),
                    kanakuma_life_span.setLifeSpan(true, set_gender, gen),
                    environment));
            }
            StatusDisplayer::displayProgressBar(agent_count, agent_count);
            std::cout << std::endl;
        }

        /// @brief Print the current status.
        /// @brief 現在の状態を表示する
        void printStatus() const noexcept {
            std::cout << "Status: " << std::endl;
            std::cout << "  Agent Count: " << agents.size() << std::endl;
        }
    };
}

#endif // !PAX_SAPIENTICA_SIMULATION_SIMULATOR_HPP
