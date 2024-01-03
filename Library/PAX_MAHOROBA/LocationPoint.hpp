﻿/*##########################################################################################

    PAX SAPIENTICA Library 💀🌿🌏

    [Planning]		2023-2024 As Project
    [Production]	2023-2024 As Project
    [Contact Us]	wanotaitei@gmail.com			https://github.com/AsPJT/PAX_SAPIENTICA
    [License]		Distributed under the CC0 1.0.	https://creativecommons.org/publicdomain/zero/1.0/

##########################################################################################*/

#ifndef PAX_MAHOROBA_LOCATION_POINT_HPP
#define PAX_MAHOROBA_LOCATION_POINT_HPP

#include <cstdint>
#include <algorithm>
#include <string>
#include <unordered_map>

#include <PAX_MAHOROBA/Init.hpp>
#include <PAX_SAPIENTICA/AppConfig.hpp>
#include <PAX_SAPIENTICA/InputFile.hpp>
#ifdef PAXS_USING_SIMULATOR
#include <PAX_SAPIENTICA/Simulation/Agent.hpp>
#include <PAX_SAPIENTICA/Simulation/SettlementAgent.hpp>
#include <PAX_SAPIENTICA/Simulation/SettlementGrid.hpp>
#endif
#include <PAX_SAPIENTICA/StringExtensions.hpp>
#include <PAX_SAPIENTICA/MapProjection.hpp> // 地図投影法
#include <PAX_SAPIENTICA/MurMur3.hpp>

#include <PAX_GRAPHICA/Circle.hpp>
#include <PAX_GRAPHICA/String.hpp>
#include <PAX_GRAPHICA/Texture.hpp>
#include <PAX_GRAPHICA/Rect.hpp>
#include <PAX_GRAPHICA/Window.hpp>

/*##########################################################################################

##########################################################################################*/

namespace paxs {
    // 地名
    struct LocationPoint {
        constexpr explicit LocationPoint() = default;
        explicit LocationPoint(
            const std::unordered_map<std::uint_least32_t, std::string>& place_name_,  // 地名
            const paxs::MercatorDeg& coordinate_,  // 経緯度
            const double overall_length_,  // 全長
            const double min_view_,  // 可視化する地図の最小範囲
            const double max_view_,  // 可視化する地図の最大範囲
            const int min_year_,  // 可視化する時代（古い年～）
            const int max_year_,  // 可視化する時代（～新しい年）
            const std::uint_least32_t lpe_,  // 対象となる地物の種別
            const std::uint_least32_t place_texture_ // 出典
        ) noexcept
            : place_name(place_name_), coordinate(coordinate_), overall_length(overall_length_),
            min_view(min_view_), max_view(max_view_), min_year(min_year_), max_year(max_year_), lpe(lpe_), place_texture(place_texture_) {}

        std::unordered_map<std::uint_least32_t, std::string> place_name{}; // 地名
        paxs::MercatorDeg coordinate{}; // 経緯度
        double overall_length = 10; // 全長
        double min_view = 0.0, max_view = 9999.0; // 可視化する地図の拡大縮小範囲
        int min_year = -99999999, max_year = 99999999; // 可視化する時代（古い年～新しい年）
        std::uint_least32_t lpe = MurMur3::calcHash("place_name"); // 対象となる地物の種別
        std::uint_least32_t place_texture = 0; // 地物の画像
    };

    // 地物の一覧
    struct LocationPointList {
        std::vector<LocationPoint> location_point_list{}; // 地物の一覧

        paxs::MercatorDeg start_coordinate{}; // 経緯度
        paxs::MercatorDeg end_coordinate{}; // 経緯度
        double min_view = 0.0, max_view = 9999.0; // 可視化する地図の拡大縮小範囲
        int min_year = -99999999, max_year = 99999999; // 可視化する時代（古い年～新しい年）
        std::uint_least32_t lpe = MurMur3::calcHash("place_name"); // 対象となる地物の種別
        std::uint_least32_t place_texture = 0; // 地物の画像

        constexpr explicit LocationPointList() = default;
        explicit LocationPointList(
            const std::vector<LocationPoint>& location_point_list_,  // 地物
            paxs::MercatorDeg start_coordinate_, // 経緯度
            paxs::MercatorDeg end_coordinate_, // 経緯度
            const double min_view_,  // 可視化する地図の最小範囲
            const double max_view_,  // 可視化する地図の最大範囲
            const int min_year_,  // 可視化する時代（古い年～）
            const int max_year_,  // 可視化する時代（～新しい年）
            const std::uint_least32_t lpe_,  // 対象となる地物の種別
            const std::uint_least32_t place_texture_ // 出典
        ) noexcept
            : location_point_list(location_point_list_),
            start_coordinate(start_coordinate_), end_coordinate(end_coordinate_),
            min_view(min_view_), max_view(max_view_), min_year(min_year_), max_year(max_year_), lpe(lpe_), place_texture(place_texture_) {}

    };

    // GUI に描画する地物の情報を管理・描画するクラス
    class PlaceNameLocation {
    public:
#ifdef PAXS_USING_SIMULATOR
        void update(const std::vector<paxs::Agent<int>>& agents, const paxs::Vector2<int>& start_position) {
            // エージェントの設定を更新
            location_point_list_list.resize(0);
            std::vector<LocationPoint> location_point_list{}; // 地物の一覧
            for (std::size_t i = 0; i < agents.size(); ++i) {
                location_point_list.emplace_back(
                    std::unordered_map < std::uint_least32_t, std::string>(),
                    paxs::MercatorDeg(agents[i].getLocation(start_position, 10)),
                    10, 100, 0, 0, 99999999,
                    (agents[i].getGender()) ?
                    MurMur3::calcHash("agent1") :
                    MurMur3::calcHash("agent2")
                    , 0 /*出典無し*/
                );
            }
            location_point_list_list.emplace_back(location_point_list,
                paxs::EquirectangularDeg(
                    paxs::Vector2<double>(-180/* 経度 */, -90/* 緯度 */)).toMercatorDeg(),
                paxs::EquirectangularDeg(
                    paxs::Vector2<double>(180/* 経度 */, 90/* 緯度 */)).toMercatorDeg(),
                0, 99999999, -99999999, 99999999, 0, 0);
        }
#endif
        // 地物を追加
        void add() {
            std::string str = "Data/PlaceName/List.tsv";
            paxs::InputFile pifs(str, AppConfig::getInstance()->getRootPath());
            if (pifs.fail()) return;
            // 1 行目を読み込む
            if (!(pifs.getLine())) {
                return; // 何もない場合
            }
            // BOM を削除
            pifs.deleteBOM();
            // 1 行目を分割する
            std::unordered_map<std::uint_least32_t, std::size_t> menu = pifs.splitHashMapMurMur3('\t');

            const std::size_t file_path = getMenuIndex(menu, MurMur3::calcHash("file_path"));
            if (file_path == SIZE_MAX) return; // パスがないのはデータにならない
            const std::size_t place_type = getMenuIndex(menu, MurMur3::calcHash("place_type"));

            const std::size_t minimum_size = getMenuIndex(menu, MurMur3::calcHash("min_size"));
            const std::size_t maximum_size = getMenuIndex(menu, MurMur3::calcHash("max_size"));
            const std::size_t first_julian_day = getMenuIndex(menu, MurMur3::calcHash("first_julian_day"));
            const std::size_t last_julian_day = getMenuIndex(menu, MurMur3::calcHash("last_julian_day"));
            const std::size_t place_texture = getMenuIndex(menu, MurMur3::calcHash("texture"));


            // 1 行ずつ読み込み（区切りはタブ）
            while (pifs.getLine()) {
                std::vector<std::string> strvec = pifs.split('\t');

                // パスが空の場合は読み込まない
                if (strvec[file_path].size() == 0) continue;

                // 対象となる地物の種別
                const std::uint_least32_t type = (place_type == SIZE_MAX) ?
                    MurMur3::calcHash("place_name") :
                    ((strvec[place_type].size() == 0) ?
                        MurMur3::calcHash("place_name") :
                        MurMur3::calcHash(strvec[place_type].size(), strvec[place_type].c_str()));

                // 可視化する地図の最小範囲
                const double min_view = (minimum_size >= strvec.size()) ?
                    0 : ((strvec[minimum_size].size() == 0) ?
                        0 : std::stod(strvec[minimum_size]));
                // 可視化する地図の最大範囲
                const double max_view = (maximum_size >= strvec.size()) ?
                    99999999.0 : ((strvec[maximum_size].size() == 0) ?
                        99999999.0 : std::stod(strvec[maximum_size]));
                // 可視化する時代（古い年～）
                const int min_year = (first_julian_day >= strvec.size()) ?
                    -99999999 : ((strvec[first_julian_day].size() == 0) ?
                        -99999999 : std::stoi(strvec[first_julian_day]));
                // 可視化する時代（～新しい年）
                const int max_year = (last_julian_day >= strvec.size()) ?
                    99999999 : ((strvec[last_julian_day].size() == 0) ?
                        99999999 : std::stoi(strvec[last_julian_day]));
                // 画像
                const std::uint_least32_t place_texture_hash = (place_texture >= strvec.size()) ?
                    0 : ((strvec[place_texture].size() == 0) ?
                        0 : MurMur3::calcHash(strvec[place_texture].size(), strvec[place_texture].c_str()));

                // 地物を追加
                inputPlace(strvec[file_path], min_view, max_view, min_year, max_year, type, place_texture_hash);
            }
        }

        PlaceNameLocation() = default;
         void init() {
            std::string str = "Data/MiniIcon/List.tsv";
            paxs::InputFile pifs(str, AppConfig::getInstance()->getRootPath());
            if (pifs.fail()) return;
            // 1 行目を読み込む
            if (!(pifs.getLine())) {
                return; // 何もない場合
            }
            // BOM を削除
            pifs.deleteBOM();
            // 1 行目を分割する
            std::unordered_map<std::uint_least32_t, std::size_t> menu = pifs.splitHashMapMurMur3('\t');

            const std::size_t file_path = getMenuIndex(menu, MurMur3::calcHash("file_path"));
            if (file_path == SIZE_MAX) return; // パスがないのはデータにならない
            const std::size_t place_texture = getMenuIndex(menu, MurMur3::calcHash("texture"));
            if (place_texture == SIZE_MAX) return; // テクスチャ名がないのはデータにならない


            // 1 行ずつ読み込み（区切りはタブ）
            while (pifs.getLine()) {
                std::vector<std::string> strvec = pifs.split('\t');

                if (file_path >= strvec.size()) continue;
                if (place_texture >= strvec.size()) continue;

                // パスが空の場合は読み込まない
                if (strvec[file_path].size() == 0) continue;
                // テクスチャ名が空の場合は読み込まない
                if (strvec[place_texture].size() == 0) continue;

                // 画像
                const std::uint_least32_t place_texture_hash =
                    (place_texture >= strvec.size()) ? 0 : // テクスチャがない場合
                    ((strvec[place_texture].size() == 0) ? 0 : // テクスチャ名がない場合
                        MurMur3::calcHash(strvec[place_texture].size(), strvec[place_texture].c_str()));
                if (place_texture_hash == 0) continue; // ハッシュが 0 の場合は追加しない

                // テクスチャを追加
                texture.emplace(MurMur3::calcHash(strvec[place_texture].size(), strvec[place_texture].c_str()), paxg::Texture{ AppConfig::getInstance()->getRootPath() + strvec[file_path] });
            }
        }
        // 描画
        void draw(const double jdn,
            const double map_view_width, const double map_view_height, const double map_view_center_x, const double map_view_center_y,
            paxg::Font& font, paxg::Font& en_font, paxg::Font& /*pin_font*/)const {

            const std::uint_least32_t first_language = MurMur3::calcHash("ja-JP");
            const std::uint_least32_t second_language = MurMur3::calcHash("en-US");

            for (std::size_t h = 0; h < location_point_list_list.size(); ++h) {
                const auto& location_point_list = location_point_list_list[h].location_point_list;

                auto& lll = location_point_list_list[h];
                // 空間の範囲外を除去
                if (lll.end_coordinate.x < (map_view_center_x - map_view_width / 1.6)
                    || lll.start_coordinate.x >(map_view_center_x + map_view_width / 1.6)
                    || lll.end_coordinate.y < (map_view_center_y - map_view_height / 1.6)
                    || lll.start_coordinate.y >(map_view_center_y + map_view_height / 1.6)) continue;
                // 時間の範囲外を除去
                if (lll.min_year > jdn) continue;
                if (lll.max_year < jdn) continue;
                // 拡大率の範囲外を除去
                // if (lll.min_view > map_view_width || lll.max_view < map_view_width) continue;

                // 地名を描画
                for (std::size_t i = 0; i < location_point_list.size(); ++i) {
                    auto& lli = location_point_list[i];
                    // 空間の範囲外を除去
                    if (lli.coordinate.x < (map_view_center_x - map_view_width / 1.6)
                        || lli.coordinate.x >(map_view_center_x + map_view_width / 1.6)
                        || lli.coordinate.y < (map_view_center_y - map_view_height / 1.6)
                        || lli.coordinate.y >(map_view_center_y + map_view_height / 1.6)) continue;
                    // 時間の範囲外を除去
                    if (lli.min_year > jdn) continue;
                    if (lli.max_year < jdn) continue;

                    // 範囲内の場合
                    if (lli.min_view > map_view_width
                        || lli.max_view < map_view_width) {

                        // 描画位置
                        const paxg::Vec2i draw_pos = paxg::Vec2i{
    static_cast<int>((lli.coordinate.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(paxg::Window::width())),
        static_cast<int>(double(paxg::Window::height()) - ((lli.coordinate.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(paxg::Window::height())))
                        };

                        // エージェントを描画
                        if (lli.lpe == MurMur3::calcHash("agent1")) {
                            if (texture.find(MurMur3::calcHash("BlueCircle")) != texture.end()) {
                                texture.at(MurMur3::calcHash("BlueCircle")).resizedDrawAt(15, draw_pos);
                            }
                            continue;
                        }
                        // エージェントを描画
                        else if (lli.lpe == MurMur3::calcHash("agent2")) {
                            if (texture.find(MurMur3::calcHash("RedCircle")) != texture.end()) {
                                texture.at(MurMur3::calcHash("RedCircle")).resizedDrawAt(15, draw_pos);
                            }
                            continue;
                        }
                        const int len = int(lli.overall_length / 2);

                        const std::uint_least32_t place_tex = (lli.place_texture == 0) ? lll.place_texture : lli.place_texture;
                        // 描画
                        if (texture.find(place_tex) != texture.end()) {
                            texture.at(place_tex).resizedDrawAt(len, draw_pos);
                        }
                        continue;
                    }

                    // 描画位置
                    const paxg::Vec2i draw_pos = paxg::Vec2i{
    static_cast<int>((lli.coordinate.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(paxg::Window::width())),
        static_cast<int>(double(paxg::Window::height()) - ((lli.coordinate.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(paxg::Window::height())))
                    };

                    const std::uint_least32_t place_tex = (lli.place_texture == 0) ? lll.place_texture : lli.place_texture;
                    // 描画
                    if (texture.find(place_tex) != texture.end()) {
                        texture.at(place_tex).resizedDrawAt(35, draw_pos);
                    }
                    // 英語名がない場合
                    if (lli.place_name.find(second_language) == lli.place_name.end()) {
                        // 名前を描画
                        if (lli.place_name.find(first_language) != lli.place_name.end()) {
                            font.setOutline(0, 0.6, paxg::Color(255, 255, 255));
                            font.drawAt(lli.place_name.at(first_language)
                                , draw_pos, paxg::Color(0, 0, 0));
                        }
                    }
                    // 英語名がある場合
                    else {
                        // 名前（英語）を描画
                        en_font.setOutline(0, 0.6, paxg::Color(255, 255, 255));
                        en_font.drawBottomCenter(lli.place_name.at(second_language), draw_pos
                            , paxg::Color(0, 0, 0));
                        // 名前を描画
                        if (lli.place_name.find(first_language) != lli.place_name.end()) {
                            font.setOutline(0, 0.6, paxg::Color(255, 255, 255));
                            font.drawTopCenter(lli.place_name.at(first_language)
                                , draw_pos, paxg::Color(0, 0, 0));
                        }
                    }
                }
            }
        }
    private:
        std::vector<LocationPointList> location_point_list_list{}; // 地物の一覧
        // アイコンのテクスチャ
        std::unordered_map<std::uint_least32_t, paxg::Texture> texture{};

        // 項目の ID を返す
        std::size_t getMenuIndex(const std::unordered_map<std::uint_least32_t, std::size_t>& menu, const std::uint_least32_t& str_) const {
            return  (menu.find(str_) != menu.end()) ? menu.at(str_) : SIZE_MAX;
        }

        // 地名を読み込み
        void inputPlace(
            const std::string& str_,
            const double min_view_,  // 可視化する地図の最小範囲
            const double max_view_,  // 可視化する地図の最大範囲
            const int min_year_,  // 可視化する時代（古い年～）
            const int max_year_,  // 可視化する時代（～新しい年）
            const std::uint_least32_t lpe_,  // 対象となる地物の種別
            const std::uint_least32_t place_texture_ // 出典
        ) {

            std::vector<LocationPoint> location_point_list{}; // 地物の一覧

            paxs::InputFile pifs(str_, AppConfig::getInstance()->getRootPath());
            if (pifs.fail()) return;
            // 1 行目を読み込む
            if (!(pifs.getLine())) {
                return; // 何もない場合
            }
            // BOM を削除
            pifs.deleteBOM();
            // 1 行目を分割する
            std::unordered_map<std::uint_least32_t, std::size_t> menu = pifs.splitHashMapMurMur3('\t');

            const std::size_t longitude = getMenuIndex(menu, MurMur3::calcHash("longitude"));
            if (longitude == SIZE_MAX) return; // 経度がないのはデータにならない
            const std::size_t latitude = getMenuIndex(menu, MurMur3::calcHash("latitude"));
            if (latitude == SIZE_MAX) return; // 緯度がないのはデータにならない

            double start_longitude = 180.0; // 始点の経度
            double end_longitude = -180.0; // 終点の経度
            double start_latitude = 90.0; // 始点の緯度
            double end_latitude = -90.0; // 終点の緯度

            // 配列の添え字番号
            const std::size_t overall_length = getMenuIndex(menu, MurMur3::calcHash("overall_length"));
            const std::size_t minimum_size = getMenuIndex(menu, MurMur3::calcHash("min_size"));
            const std::size_t maximum_size = getMenuIndex(menu, MurMur3::calcHash("max_size"));
            const std::size_t first_julian_day = getMenuIndex(menu, MurMur3::calcHash("first_julian_day"));
            const std::size_t last_julian_day = getMenuIndex(menu, MurMur3::calcHash("last_julian_day"));
            const std::size_t place_texture = getMenuIndex(menu, MurMur3::calcHash("texture"));

            // 地名
            const std::size_t language_ja_jp = getMenuIndex(menu, MurMur3::calcHash("ja-JP"));
            const std::size_t language_en_us = getMenuIndex(menu, MurMur3::calcHash("en-US"));

            // 1 行ずつ読み込み（区切りはタブ）
            while (pifs.getLine()) {
                std::vector<std::string> strvec = pifs.split('\t');

                // 経度の文字列が空の場合は読み込まない
                if (strvec[longitude].size() == 0) continue;
                // 経度の文字列が空の場合は読み込まない
                if (strvec[latitude].size() == 0) continue;

                // 地名
                std::unordered_map<std::uint_least32_t, std::string> place_name{};
                if (language_ja_jp < strvec.size() && strvec[language_ja_jp].size() != 0) {
                    place_name.emplace(MurMur3::calcHash("ja-JP"), strvec[language_ja_jp]);
                }
                if (language_en_us < strvec.size() && strvec[language_en_us].size() != 0) {
                    place_name.emplace(MurMur3::calcHash("en-US"), strvec[language_en_us]);
                }
                bool is_overall_length = false;
                if (overall_length < strvec.size()) {
                    if (strvec[overall_length].size() != 0) is_overall_length = true;
                }

                double point_longitude = std::stod(strvec[longitude]); // 経度
                double point_latitude = std::stod(strvec[latitude]); // 緯度
                // 経緯度の範囲を求める
                start_longitude = (std::min)(start_longitude, point_longitude);
                start_latitude = (std::min)(start_latitude, point_latitude);
                end_longitude = (std::max)(end_longitude, point_longitude);
                end_latitude = (std::max)(end_latitude, point_latitude);

                // 格納
                location_point_list.emplace_back(
                    place_name,
                    paxs::EquirectangularDeg(
                        paxs::Vector2<double>(
                            point_longitude, // 経度
                            point_latitude)).toMercatorDeg(), // 緯度
                    (!is_overall_length) ? 10.0 : std::stod(strvec[overall_length]), // 全長
                    (minimum_size >= strvec.size()) ? min_view_ : std::stod(strvec[minimum_size]), // 最小サイズ
                    (maximum_size >= strvec.size()) ? max_view_ : std::stod(strvec[maximum_size]), // 最大サイズ
                    (first_julian_day >= strvec.size()) ? min_year_ : std::stod(strvec[first_julian_day]), // 最小時代
                    (last_julian_day >= strvec.size()) ? max_year_ : std::stod(strvec[last_julian_day]), // 最大時代
                    lpe_,
                    (place_texture >= strvec.size()) ? place_texture_ : MurMur3::calcHash(strvec[place_texture].size(), strvec[place_texture].c_str()) // テクスチャの Key
                );
            }
            // 地物を何も読み込んでいない場合は何もしないで終わる
            if (location_point_list.size() == 0) return;

            // 読み込んだファイルを格納する
            location_point_list_list.emplace_back(location_point_list,
                paxs::EquirectangularDeg(
                    paxs::Vector2<double>(start_longitude/* 経度 */, start_latitude/* 緯度 */)).toMercatorDeg(),
                paxs::EquirectangularDeg(
                    paxs::Vector2<double>(end_longitude/* 経度 */, end_latitude/* 緯度 */)).toMercatorDeg(),
                min_view_, max_view_, min_year_, max_year_
                , lpe_, place_texture_);
        }
    };

#ifdef PAXS_USING_SIMULATOR
    // エージェントの位置を管理
    class AgentLocation {
    public:
        // アイコンのテクスチャ
        paxg::Texture texture_blue_circle{};
        paxg::Texture texture_red_circle{};


        /// @brief Get the mercator coordinate from the XYZTile coordinate.
        /// @brief 座標をメルカトル座標で取得
        paxs::Vector2<double> getLocation(
            const paxs::Vector2<int>& start_position,
            const paxs::Vector2<int>& position,
            const int z) const noexcept {
            return MapUtility::convertToMercatorCoordinate(start_position, position, z);
        }

        // テクスチャ生成
        void init() {
            texture_blue_circle = paxg::Texture{ AppConfig::getInstance()->getRootPath() + std::string("Data/MiniIcon/BlueCircle.svg") };
            texture_red_circle = paxg::Texture{ AppConfig::getInstance()->getRootPath() + std::string("Data/MiniIcon/RedCircle.svg") };
        }

    private:

        paxg::Color getColor(const std::uint_least8_t pop_) const {
            switch (pop_) {
            case 0: return paxg::Color(45, 87, 154);
            case 1: return paxg::Color(36, 92, 156);
            case 2: return paxg::Color(27, 96, 158);
            case 3: return paxg::Color(18, 101, 161);
            case 4: return paxg::Color(9, 105, 163);
            case 5: return paxg::Color(0, 110, 165);
            case 6: return paxg::Color(0, 111, 164);
            case 7: return paxg::Color(0, 112, 163);
            case 8: return paxg::Color(0, 112, 161);
            case 9: return paxg::Color(0, 113, 160);
            case 10: return paxg::Color(0, 114, 159);
            case 11: return paxg::Color(0, 115, 158);
            case 12: return paxg::Color(0, 117, 157);
            case 13: return paxg::Color(0, 118, 156);
            case 14: return paxg::Color(0, 120, 155);
            case 15: return paxg::Color(0, 121, 154);
            case 16: return paxg::Color(0, 125, 148);
            case 17: return paxg::Color(0, 130, 143);
            case 18: return paxg::Color(0, 134, 137);
            case 19: return paxg::Color(0, 139, 132);
            case 20: return paxg::Color(0, 143, 126);
            case 21: return paxg::Color(0, 141, 115);
            case 22: return paxg::Color(0, 138, 104);
            case 23: return paxg::Color(0, 136, 94);
            case 24: return paxg::Color(0, 133, 83);
            case 25: return paxg::Color(0, 131, 72);
            case 26: return paxg::Color(6, 136, 60);
            case 27: return paxg::Color(13, 141, 48);
            case 28: return paxg::Color(19, 145, 37);
            case 29: return paxg::Color(26, 150, 25);
            case 30: return paxg::Color(32, 155, 13);
            case 31: return paxg::Color(55, 159, 16);
            case 32: return paxg::Color(78, 164, 19);
            case 33: return paxg::Color(101, 168, 23);
            case 34: return paxg::Color(124, 173, 26);
            case 35: return paxg::Color(147, 177, 29);
            case 36: return paxg::Color(156, 182, 26);
            case 37: return paxg::Color(166, 187, 24);
            case 38: return paxg::Color(175, 193, 21);
            case 39: return paxg::Color(185, 198, 19);
            case 40: return paxg::Color(194, 203, 16);
            case 41: return paxg::Color(201, 204, 13);
            case 42: return paxg::Color(208, 205, 10);
            case 43: return paxg::Color(216, 205, 6);
            case 44: return paxg::Color(223, 206, 3);
            case 45: return paxg::Color(230, 207, 0);
            case 46: return paxg::Color(228, 202, 1);
            case 47: return paxg::Color(226, 197, 2);
            case 48: return paxg::Color(225, 191, 3);
            case 49: return paxg::Color(223, 186, 4);
            case 50: return paxg::Color(221, 181, 5);
            case 51: return paxg::Color(219, 176, 6);
            case 52: return paxg::Color(218, 171, 7);
            case 53: return paxg::Color(216, 165, 8);
            case 54: return paxg::Color(215, 160, 9);
            case 55: return paxg::Color(213, 155, 10);
            case 56: return paxg::Color(210, 138, 12);
            case 57: return paxg::Color(206, 122, 15);
            case 58: return paxg::Color(203, 105, 17);
            case 59: return paxg::Color(199, 89, 20);
            case 60: return paxg::Color(196, 72, 22);
            case 61: return paxg::Color(193, 64, 28);
            case 62: return paxg::Color(189, 56, 34);
            case 63: return paxg::Color(186, 48, 40);
            case 64: return paxg::Color(182, 40, 46);
            case 65: return paxg::Color(179, 32, 52);
            case 66: return paxg::Color(178, 31, 57);
            case 67: return paxg::Color(176, 30, 62);
            case 68: return paxg::Color(175, 29, 68);
            case 69: return paxg::Color(173, 28, 73);
            case 70: return paxg::Color(172, 27, 78);
            case 71: return paxg::Color(170, 26, 83);
            case 72: return paxg::Color(169, 25, 88);
            case 73: return paxg::Color(167, 24, 92);
            case 74: return paxg::Color(166, 23, 97);
            case 75: return paxg::Color(164, 22, 102);
            case 76: return paxg::Color(131, 18, 82);
            case 77: return paxg::Color(98, 13, 61);
            case 78: return paxg::Color(66, 9, 41);
            case 79: return paxg::Color(33, 4, 20);
            case 80: return paxg::Color(0, 0, 0);
            }
            return paxg::Color(255, 255, 255);
        }

    public:

        void draw(const double jdn,
            std::unordered_map<std::uint_least64_t, paxs::SettlementGrid<int>>& agents,
            const paxs::Vector2<int>& start_position,
            const double map_view_width, const double map_view_height, const double map_view_center_x, const double map_view_center_y,

            std::size_t& pop_num, // 人口数
            std::size_t& sat_num // 集落数

        )const {

            pop_num = 0; // 人口を 0 で初期化
            sat_num = 0; // 集落を 0 で初期化

            //agents.at(0)->getGridPosition().x;
            // 地名を描画
            for (const auto& agent : agents) {

                //if (agent.second == nullptr) continue;

                for (const auto& settlement : agent.second.cgetSettlements()) {
                    ++sat_num; // 集落数を増加させる
                    pop_num += settlement.getPopulation(); // 人口数を増加させる
                    //if (settlement.get() == nullptr) continue;

                    // エージェントの初期設定を定義
                    const auto lli = LocationPoint{
                        std::unordered_map < std::uint_least32_t, std::string>(),
                            paxs::MercatorDeg(getLocation(start_position,
                            paxs::Vector2<int>(
                    settlement.getPosition().x,
                        settlement.getPosition().y
                            )
                            , 10)),
                            10, 100,0,0,99999999,
                        //(agent.getGender()) ?
                        MurMur3::calcHash("agent1")
                        ,0 /* 出典なし */
                    };

                    // 経緯度の範囲外を除去
                    if (lli.coordinate.x < (map_view_center_x - map_view_width / 1.6)
                        || lli.coordinate.x >(map_view_center_x + map_view_width / 1.6)
                        || lli.coordinate.y < (map_view_center_y - map_view_height / 1.6)
                        || lli.coordinate.y >(map_view_center_y + map_view_height / 1.6)) continue;

                    // 範囲内の場合
                    if (lli.min_view > map_view_width
                        || lli.max_view < map_view_width
                        || lli.min_year > jdn
                        || lli.max_year < jdn) {
                        if (lli.min_year > jdn) continue;
                        if (lli.max_year < jdn) continue;

                        // 描画位置
                        const paxg::Vec2i draw_pos = paxg::Vec2i{
    static_cast<int>((lli.coordinate.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(paxg::Window::width())),
        static_cast<int>(double(paxg::Window::height()) - ((lli.coordinate.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(paxg::Window::height())))
                        };

                        // エージェント
                        // if (lli.lpe == MurMur3::calcHash("agent1"))
                        {
                            const std::size_t pop_original = settlement.getPopulation();
                            const std::uint_least8_t pop = (pop_original >= 80) ? 80 : static_cast<std::uint_least8_t>(pop_original);
                            paxg::Circle(draw_pos, 2.0f).draw(getColor(pop));
                        }

                    }

                }
            }
        }

    };
#endif

}

#endif // !PAX_MAHOROBA_LOCATION_POINT_HPP
