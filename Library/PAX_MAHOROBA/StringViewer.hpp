﻿/*##########################################################################################

    PAX SAPIENTICA Library 💀🌿🌏

    [Planning]		2023 As Project
    [Production]	2023 As Project
    [Contact Us]	wanotaitei@gmail.com			https://github.com/AsPJT/PAX_SAPIENTICA
    [License]		Distributed under the CC0 1.0.	https://creativecommons.org/publicdomain/zero/1.0/

##########################################################################################*/

#ifndef PAX_MAHOROBA_STRING_VIEWER_HPP
#define PAX_MAHOROBA_STRING_VIEWER_HPP

/*##########################################################################################

##########################################################################################*/

#include <limits>
#include <string>
#include <variant>
#include <vector>

#include <PAX_SAPIENTICA/Calendar/Calendars.hpp>
#include <PAX_SAPIENTICA/Calendar/Date.hpp>
#include <PAX_SAPIENTICA/Calendar/JapaneseEra.hpp>
#include <PAX_SAPIENTICA/Calendar/JulianDayNumber.hpp>
#include <PAX_SAPIENTICA/GraphicVisualizationList.hpp> // 暦にないファイル
#include <PAX_SAPIENTICA/Simulation/Simulator.hpp>
#include <PAX_MAHOROBA/3DModel.hpp>
#include <PAX_MAHOROBA/Calendar.hpp> // 計算時に必要
#include <PAX_SAPIENTICA/Language.hpp>
#include <PAX_MAHOROBA/LocationPoint.hpp>
#include <PAX_MAHOROBA/LocationRange.hpp>
#include <PAX_MAHOROBA/Pulldown.hpp>
#include <PAX_MAHOROBA/XYZTiles.hpp>
#include <PAX_MAHOROBA/XYZTilesList.hpp>
#include <PAX_SAPIENTICA/StringExtensions.hpp>
#include <PAX_SAPIENTICA/TouchManager.hpp>
#include <PAX_SAPIENTICA/Math.hpp> // 数学定数
#include <PAX_SAPIENTICA/MapProjection.hpp> // 地図投影法
#include <PAX_SAPIENTICA/Calendar/JulianDayNumber.hpp>

#include <PAX_GRAPHICA/Key.hpp>


namespace paxs {

    class StringViewerSiv3D {
    public:

        /*##########################################################################################
            フォント
        ##########################################################################################*/

        std::vector<paxg::Font> font_pulldown; // プルダウン用のフォント
        int font_size = 17; // デフォルトのフォントのサイズ
        std::vector<paxg::Font> font; // デフォルトのフォント

        int koyomi_font_size = 17; // 暦のフォントサイズ
        std::vector<paxg::Font> koyomi_font; // 暦のフォント
        paxg::Font license_font;
        paxg::Font pin_font{};

        std::size_t map_view_width_str_index;
        std::size_t map_view_center_x_str_index;
        std::size_t map_view_center_y_str_index;
        std::size_t map_view_center_lat_str_index;
        std::size_t xyz_tile_z_str_index;
#ifdef PAXS_USING_SIV3D
        // UI の影
        s3d::RenderTexture shadow_texture{};
        s3d::RenderTexture internal_texture{};
#endif
        std::vector<std::vector<std::string>> items_pulldown;
        paxs::Pulldown pulldown;
        paxs::MenuBar menu_bar;
        std::unordered_map<std::string, paxg::Texture> texture_dictionary{};

        //const std::string map_license_name = U"Maptiles by\n淺野孝利 2023「古墳時代の『常総の内海』水域復原に関する一試論」\n研究代表者 荒井啓汰『埋葬施設からみた常総地域の地域構造』\n特別研究員奨励費報告書 筑波大学大学院 人文社会科学研究科";
        std::string map_license_name = reinterpret_cast<const char*>(u8"Maptiles by\n農研機構農業環境研究部門, under CC BY 2.1 JP.\n20万分の1シームレス地質図V2.\nOpenStreetMap contributors, under ODbL.");
        //const std::string map_license_name = U"Maptiles by MIERUNE, under CC BY. Data by OpenStreetMap contributors, under ODbL.\nMaptiles by 農研機構農業環境研究部門, under CC BY 2.1 JP";

        paxs::Graphics3DModel g3d_model;


        void init(
            const paxs::Language& language_text,
            const std::string& path8
        ) {
            font_pulldown = setFont(16, path8, 3, "font_path", language_text);
            font = setFont(font_size, path8, 3, "font_path", language_text);

            map_view_width_str_index = language_text.findStart("debug_magnification_power");
            map_view_center_x_str_index = language_text.findStart("debug_mercator_longitude");
            map_view_center_y_str_index = language_text.findStart("debug_mercator_latitude");
            map_view_center_lat_str_index = language_text.findStart("debug_latitude");
            xyz_tile_z_str_index = language_text.findStart("debug_xyz_tiles_z");

            items_pulldown = language_text.getFindStartToVVS("language", 1);
            pulldown = paxs::Pulldown(items_pulldown, 0, 0, font_pulldown, paxg::Vec2i{ 3000, 0 }, PulldownType::Zero, true);
            pulldown.setPos(paxg::Vec2i{ static_cast<int>(paxg::Window::width() - pulldown.getRect().w()), 0 });


            const std::vector<paxg::Font>& font_menu_bar = font_pulldown;

            menu_bar.add(language_text.cget(), language_text.findStart("> menu_bar_file") + 1, 1, font_menu_bar);
            menu_bar.add(language_text.cget(), language_text.findStart("> menu_bar_edit") + 1, 1, font_menu_bar);
            menu_bar.add(language_text.cget(), language_text.findStart("> menu_bar_view") + 1, 1, font_menu_bar);
            menu_bar.add(language_text.cget(), language_text.findStart("> menu_bar_calendar") + 1, 1, font_menu_bar);
            menu_bar.add(language_text.cget(), language_text.findStart("> menu_bar_map") + 1, 1, font_menu_bar);

            const std::string path = (path8);
            { // 暦の時間操作のアイコン
                texture_dictionary.emplace("texture_tlt", paxg::Texture{ path + "Image/Logo/TitleLogoText2.svg"});
                texture_dictionary.emplace("texture_github", paxg::Texture{ path + "Data/MenuIcon/github.svg" });
                texture_dictionary.emplace("texture_d_l", paxg::Texture{ path + "Data/MenuIcon/DayL.svg" });
                texture_dictionary.emplace("texture_d_r", paxg::Texture{ path + "Data/MenuIcon/DayR.svg" });
                texture_dictionary.emplace("texture_m_l", paxg::Texture{ path + "Data/MenuIcon/MonthL.svg" });
                texture_dictionary.emplace("texture_m_r", paxg::Texture{ path + "Data/MenuIcon/MonthR.svg" });
                texture_dictionary.emplace("texture_y_l", paxg::Texture{ path + "Data/MenuIcon/YearL.svg" });
                texture_dictionary.emplace("texture_y_r", paxg::Texture{ path + "Data/MenuIcon/YearR.svg" });
                texture_dictionary.emplace("texture_10y_l", paxg::Texture{ path + "Data/MenuIcon/10YearL.svg" });
                texture_dictionary.emplace("texture_10y_r", paxg::Texture{ path + "Data/MenuIcon/10YearR.svg" });
                texture_dictionary.emplace("texture_c_l", paxg::Texture{ path + "Data/MenuIcon/100YearL.svg" });
                texture_dictionary.emplace("texture_c_r", paxg::Texture{ path + "Data/MenuIcon/100YearR.svg" });
                texture_dictionary.emplace("texture_10c_l", paxg::Texture{ path + "Data/MenuIcon/1kYearL.svg" });
                texture_dictionary.emplace("texture_10c_r", paxg::Texture{ path + "Data/MenuIcon/1kYearR.svg" });
                texture_dictionary.emplace("texture_100c_l", paxg::Texture{ path + "Data/MenuIcon/10kYearL.svg" });
                texture_dictionary.emplace("texture_100c_r", paxg::Texture{ path + "Data/MenuIcon/10kYearR.svg" });
                texture_dictionary.emplace("texture_stop", paxg::Texture{ path + "Data/MenuIcon/stop.svg" });
                texture_dictionary.emplace("texture_playback", paxg::Texture{ path + "Data/MenuIcon/playback.svg" });
                texture_dictionary.emplace("texture_reverse_playback", paxg::Texture{ path + "Data/MenuIcon/reverse-playback.svg" });
            }


            koyomi_font = setFont(koyomi_font_size, path8, 3, "font_path", language_text);

            license_font = paxg::Font{ 14 /*, Typeface::Bold*/
                , (path + "Data/Font/noto-sans-jp/NotoSansJP-Regular.otf"), 3 };

            pin_font = paxg::Font{ 18 /*, Typeface::Bold*/
                , (path + "Data/Font/noto-sans-jp/NotoSansJP-Regular.otf"), 3 };
#ifdef PAXS_USING_SIV3D
            // 影
            shadow_texture = s3d::RenderTexture{ s3d::Scene::Size(), s3d::ColorF{ 1.0, 0.0 } };
            internal_texture = s3d::RenderTexture{ shadow_texture.size() };
#endif
        }

        void update(
            const std::unique_ptr<MapView>& map_view,
            const SelectLanguage& select_language,
            const paxs::Language& language_text,
            paxs::Simulator<int>& simulator, // コンパイル時の分岐により使わない場合あり
            const paxs::Vector2<int>& start_position, // コンパイル時の分岐により使わない場合あり
            const paxs::Vector2<int>& end_position, // コンパイル時の分岐により使わない場合あり
            const std::string& path8, // コンパイル時の分岐により使わない場合あり
            paxs::TouchManager& tm_,
            paxs::KoyomiSiv3D& koyomi_siv,
            paxs::GraphicVisualizationList& visible
        ) {
            const double map_view_width = map_view->getWidth();
            // const double map_view_center_lat =
                //paxs::MathF64::radToDeg(std::asin(std::tanh(paxs::MathF64::degToRad(map_view->getCenterY()))));
            map_view->getCoordinate().toEquirectangularDegY();

            // 画像の拡大縮小の方式を設定
            const s3d::ScopedRenderStates2D sampler{ s3d::SamplerState::ClampLinear };

            // 暦の位置
            int koyomi_font_x = 0;//220;
            int koyomi_font_y = 50;
            int koyomi_font_en_x = 0;//820;
            int koyomi_font_en_y = 50;

            koyomi_font_x = paxg::Window::width() - 220;
            koyomi_font_en_x = paxg::Window::width() - 220;
            int rect_start_x = paxg::Window::width() - 375;

            int koyomi_height = static_cast<int>(koyomi_siv.date_list.size()) * (koyomi_font_size * 4 / 3); // 暦の縦の幅

            const int arrow_time_icon_size = 24; // 時間操作アイコンの大きさ
            const int time_icon_size = 40; // 時間操作アイコンの大きさ
            int icon_const_start_x = 360;
#ifdef PAXS_USING_DXLIB
#ifndef __ANDROID__
            const int dx_move_x = 400; // 描画ライブラリごとに位置の調整
#else
            const int dx_move_x = 0; // 描画ライブラリごとに位置の調整
#endif
            koyomi_font_x -= dx_move_x;
            koyomi_font_en_x -= dx_move_x;
            rect_start_x -= dx_move_x;
            icon_const_start_x += dx_move_x;
#endif // PAXS_USING_DXLIB
            int icon_start_x = icon_const_start_x;

            int sum_icon_height = arrow_time_icon_size + time_icon_size * 2;

            int icon_start_y = koyomi_height + 10;// 80 + 30;
            const int arrow_icon_move_x = int(arrow_time_icon_size * 1.2);// int(time_icon_size * 1.4);
            const int icon_move_x = int(time_icon_size * 1.1);// int(time_icon_size * 1.4);

            const int arrow_icon_move_y = 30;
            const int icon_move_y = 44;

            int next_rect_start_y = icon_start_y + sum_icon_height + 20;//230;
            int next_rect_end_y = 150;//380;

            if (visible["Calendar"]) {
#ifdef PAXS_USING_DXLIB
                DxLib::DrawRoundRect(rect_start_x, koyomi_font_y - 5,
                    rect_start_x + 360, koyomi_font_y - 5 + next_rect_start_y,
                    10, 10, DxLib::GetColor(255, 255, 255), TRUE);

                // paxg::Rect{ paxg::Vec2i(rect_start_x, koyomi_font_y - 5), paxg::Vec2i(360, next_rect_start_y) }.draw();
                // paxg::Rect{ paxg::Vec2i(rect_start_x, koyomi_font_y + next_rect_start_y + 5), paxg::Vec2i(360, next_rect_end_y) }.draw();
#endif // PAXS_USING_DXLIB

#ifdef PAXS_USING_SIV3D
                // 影を作る図形を shadow_texture に描く
                {
                    const s3d::ScopedRenderTarget2D target{ shadow_texture.clear(s3d::ColorF{ 1.0, 0.0 }) };
                    const s3d::ScopedRenderStates2D blend{ s3d::BlendState::MaxAlpha };
                    const s3d::Transformer2D transform{ s3d::Mat3x2::Translate(3, 3) };

                    s3d::Rect{ 0, 0, paxg::Window::width(), 30 }.draw(); // メニューバー
                    s3d::RoundRect{ rect_start_x, koyomi_font_y - 5, 360, next_rect_start_y, 10 }.draw();
                    s3d::RoundRect{ rect_start_x, koyomi_font_y + next_rect_start_y + 5, 360, next_rect_end_y, 10 }.draw();
                }
                // shadow_texture を 2 回ガウスぼかし
                {
                    s3d::Shader::GaussianBlur(shadow_texture, internal_texture, shadow_texture);
                    s3d::Shader::GaussianBlur(shadow_texture, internal_texture, shadow_texture);
                }

                shadow_texture.draw(s3d::ColorF{ 0.0, 0.5 });
#endif
                // 暦表示の範囲に白背景を追加
                s3d::RoundRect{ rect_start_x, koyomi_font_y - 5, 360, next_rect_start_y, 10 }.draw(s3d::ColorF{ 1, 1, 1 }/*s3d::Palette::White*/);
                s3d::RoundRect{ rect_start_x, koyomi_font_y + next_rect_start_y + 5, 360, next_rect_end_y, 10 }.draw(s3d::ColorF{ 1, 1, 1 }/*s3d::Palette::White*/);
            }
            if (visible["Calendar"] && visible["UI"]) {
                // 暦の表示（日本語）
                if (
                    select_language.cget() == 1
                    || select_language.cget() == 2
                    || select_language.cget() == 3
                    ) {
                    for (std::size_t i = 0; i < koyomi_siv.date_list.size(); ++i) {

                        cal::DateOutputType output_type = cal::DateOutputType::name_and_value;
                        std::visit([&](const auto& x) { output_type = x.getDateOutputType(); }, koyomi_siv.date_list[i].date);

                        int date_year = 0;
                        int date_month = 0;
                        int date_day = 0;
                        bool date_lm = false;
                        switch (output_type) {
                        case paxs::cal::DateOutputType::name_and_ymd:
                            koyomi_font[select_language.cget()].drawTopRight(std::string(koyomi_siv.date_list[i].calendar_name[select_language.cget() + 1 /* 言語位置調整 */]),
                                paxg::Vec2i(static_cast<int>(koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            koyomi_font[select_language.cget()].drawTopRight(reinterpret_cast<const char*>(u8"年"), paxg::Vec2i(static_cast<int>(int(120 * koyomi_font_size / 30.0) + koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            koyomi_font[select_language.cget()].drawTopRight(reinterpret_cast<const char*>(u8"月"), paxg::Vec2i(static_cast<int>(int(220 * koyomi_font_size / 30.0) + koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            koyomi_font[select_language.cget()].drawTopRight(reinterpret_cast<const char*>(u8"日"), paxg::Vec2i(static_cast<int>(int(300 * koyomi_font_size / 30.0) + koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            std::visit([&](const auto& x) {
                                date_year = int(x.cgetYear());
                                date_month = int(x.cgetMonth());
                                date_day = int(x.cgetDay());
                                date_lm = x.isLeapMonth();
                                }, koyomi_siv.date_list[i].date);

                            koyomi_font[select_language.cget()].drawTopRight(std::to_string(date_year), paxg::Vec2i(static_cast<int>(int(85 * koyomi_font_size / 30.0) + koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            koyomi_font[select_language.cget()].drawTopRight(std::to_string(date_month), paxg::Vec2i(static_cast<int>(int(190 * koyomi_font_size / 30.0) + koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            koyomi_font[select_language.cget()].drawTopRight(std::to_string(date_day), paxg::Vec2i(static_cast<int>(int(270 * koyomi_font_size / 30.0) + koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            if (date_lm) {
                                koyomi_font[select_language.cget()].drawTopRight(reinterpret_cast<const char*>(u8"閏"), paxg::Vec2i(static_cast<int>((
                                    (date_month < 10) ? int(167 * koyomi_font_size / 30.0) + koyomi_font_x : int(152 * koyomi_font_size / 30.0) + koyomi_font_x
                                    )), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            }
                            break;
                        case paxs::cal::DateOutputType::name_and_value:
                            koyomi_font[select_language.cget()].drawTopRight(std::string(koyomi_siv.date_list[i].calendar_name[select_language.cget() + 1 /* 言語位置調整 */]), paxg::Vec2i(static_cast<int>(koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            std::visit([&](const auto& x) {
                                date_day = int(x.cgetDay());
                                }, koyomi_siv.date_list[i].date);
                            koyomi_font[select_language.cget()].drawTopRight(std::to_string(date_day), paxg::Vec2i(static_cast<int>(int(300 * koyomi_font_size / 30.0) + koyomi_font_x), static_cast<int>(koyomi_font_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));

                            break;
                        default:
                            break;
                        }
                    }
                }
                else {
                    // 暦の表示（英語）
                    for (std::size_t i = 0; i < koyomi_siv.date_list.size(); ++i) {

                        cal::DateOutputType output_type = cal::DateOutputType::name_and_value;
                        std::visit([&](const auto& x) { output_type = x.getDateOutputType(); }, koyomi_siv.date_list[i].date);
                        
                        int date_year = 0;
                        int date_month = 0;
                        int date_day = 0;
                        bool date_lm = false;
                        const int en_cal_name_pos_x = 85;
                        auto& kfont = (select_language.cget() + 1 >= koyomi_font.size()) ? license_font : koyomi_font[select_language.cget()];
                        switch (output_type) {
                        case paxs::cal::DateOutputType::name_and_ymd:

                            if (koyomi_siv.date_list[i].calendar_name.size() > select_language.cget() + 1) {
                                kfont.drawTopRight(std::string(koyomi_siv.date_list[i].calendar_name[select_language.cget() + 1 /* 言語位置調整 */]), paxg::Vec2i(static_cast<int>(koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            }
                            kfont.drawTopRight(",", paxg::Vec2i(static_cast<int>(int(95 * koyomi_font_size / 30.0) + koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            kfont.drawTopRight(",", paxg::Vec2i(static_cast<int>(int(235 * koyomi_font_size / 30.0) + koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            kfont.drawTopRight("th", paxg::Vec2i(static_cast<int>(int(315 * koyomi_font_size / 30.0) + koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            std::visit([&](const auto& x) {
                                date_year = int(x.cgetYear());
                                date_month = int(x.cgetMonth());
                                date_day = int(x.cgetDay());
                                date_lm = x.isLeapMonth();
                                }, koyomi_siv.date_list[i].date);

                            kfont.drawTopRight(std::to_string(date_year), paxg::Vec2i(static_cast<int>(int(en_cal_name_pos_x * koyomi_font_size / 30.0) + koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            kfont.drawTopRight(std::string(koyomi_siv.month_name[date_month]), paxg::Vec2i(static_cast<int>(int(220 * koyomi_font_size / 30.0) + koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            kfont.drawTopRight(std::to_string(date_day), paxg::Vec2i(static_cast<int>(int(280 * koyomi_font_size / 30.0) + koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            if (date_lm) {
                                kfont.drawTopRight("int.", paxg::Vec2i(static_cast<int>((
                                    int(152 * koyomi_font_size / 30.0) + koyomi_font_en_x
                                    )), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            }
                            break;
                        case paxs::cal::DateOutputType::name_and_value:

                            if (koyomi_siv.date_list[i].calendar_name.size() > select_language.cget() + 1) {
                                kfont.drawTopRight(std::string(koyomi_siv.date_list[i].calendar_name[select_language.cget() + 1 /* 言語位置調整 */]),
                                    paxg::Vec2i(static_cast<int>(koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));
                            }
                            std::visit([&](const auto& x) {
                                date_day = int(x.cgetDay());
                                }, koyomi_siv.date_list[i].date);
                            kfont.drawTopRight(std::to_string(date_day), paxg::Vec2i(static_cast<int>(int(315 * koyomi_font_size / 30.0) + koyomi_font_en_x), static_cast<int>(koyomi_font_en_y + i * (koyomi_font_size * 4 / 3))), paxg::Color(0, 0, 0));

                            break;
                        default:
                            break;
                        }
                    }
                }
                texture_dictionary.at("texture_reverse_playback").resizedDraw(arrow_time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(arrow_time_icon_size, arrow_time_icon_size) }.leftClicked())) {
                    koyomi_siv.move_forward_in_time = false;
                    koyomi_siv.go_back_in_time = true; // 逆再生
                }
                icon_start_x -= arrow_icon_move_x;
                texture_dictionary.at("texture_stop").resizedDraw(arrow_time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(arrow_time_icon_size, arrow_time_icon_size) }.leftClicked())) {
                    koyomi_siv.move_forward_in_time = false; // 一時停止
                    koyomi_siv.go_back_in_time = false;
                }
                icon_start_x -= arrow_icon_move_x;
                texture_dictionary.at("texture_playback").resizedDraw(arrow_time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(arrow_time_icon_size, arrow_time_icon_size) }.leftClicked())) {
                    koyomi_siv.move_forward_in_time = true; // 再生
                    koyomi_siv.go_back_in_time = false;
                }
                icon_start_y += arrow_icon_move_y;
                icon_start_x = icon_const_start_x;

                texture_dictionary.at("texture_d_l").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() -= 1;
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_m_l").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() -= (365.2422 / 12.0);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_y_l").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() -= 365.2422;
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_10y_l").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() -= (365.2422 * 10);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_c_l").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() -= (365.2422 * 100);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_10c_l").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() -= (365.2422 * 1000);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_100c_l").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() -= (365.2422 * 10000);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_y += icon_move_y;
                icon_start_x = icon_const_start_x;

                texture_dictionary.at("texture_d_r").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() += 1;
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_m_r").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() += (365.2422 / 12.0);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_y_r").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() += 365.2422;
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_10y_r").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() += (365.2422 * 10);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_c_r").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() += (365.2422 * 100);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_10c_r").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() += (365.2422 * 1000);
                    koyomi_siv.calcDate(language_text);
                }
                icon_start_x -= icon_move_x;
                texture_dictionary.at("texture_100c_r").resizedDraw(time_icon_size, paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y));
                if (tm_.get(paxg::Rect{ paxg::Vec2i(paxg::Window::width() - icon_start_x, koyomi_font_y + icon_start_y), paxg::Vec2i(time_icon_size, time_icon_size) }.leftClicked())) {
                    koyomi_siv.jdn.getDay() += (365.2422 * 10000);
                    koyomi_siv.calcDate(language_text);
                }

            }
            // 時代区分を選択するラジオボタン
            // static size_t index1 = 2;
            // if (s3d::SimpleGUI::RadioButtons(index1, options, s3d::Vec2{ paxg::Window::width() - 400, 400 })) {
            // jdn = period_jdn[index1];
            // }
            {
                int debug_start_y = koyomi_font_y + next_rect_start_y + 10;
                int debug_move_y = 25;
                // その他のデバッグ用の変数情報の表示
                if (visible["UI"]) {
                    //font[select_language.cget()].

                    //font[select_language.cget()](std::string(language_text.cget()[map_view_center_x_str_index][select_language.cget() + 1 /* 言語位置調整 */]
                    //)).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Arg::topRight = s3d::Vec2(paxg::Window::width() - 160, debug_start_y), s3d::Palette::Black);
                    //font[select_language.cget()](s3d::ToString(map_view->getCenterX())
                    //    ).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Vec2(paxg::Window::width() - 110, debug_start_y), s3d::Palette::Black);
                    //debug_start_y += debug_move_y;
                    //// マップ中心座標 Y
                    //font[select_language.cget()](s3d::Unicode::FromUTF8(language_text.cget()[map_view_center_y_str_index][select_language.cget() + 1 /* 言語位置調整 */]
                    //)).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Arg::topRight = s3d::Vec2(paxg::Window::width() - 160, debug_start_y), s3d::Palette::Black);
                    //font[select_language.cget()](s3d::ToString(map_view->getCenterY())
                    //    ).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Vec2(paxg::Window::width() - 110, debug_start_y), s3d::Palette::Black);
                    //debug_start_y += debug_move_y;
                    //font[select_language.cget()](s3d::Unicode::FromUTF8(language_text.cget()[map_view_center_lat_str_index][select_language.cget() + 1 /* 言語位置調整 */]
                    //)).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Arg::topRight = s3d::Vec2(paxg::Window::width() - 160, debug_start_y), s3d::Palette::Black);
                    //font[select_language.cget()](s3d::ToString(map_view_center_lat)
                    //    ).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Vec2(paxg::Window::width() - 110, debug_start_y), s3d::Palette::Black);
                    //debug_start_y += debug_move_y;
                    //font[select_language.cget()](s3d::Unicode::FromUTF8(language_text.cget()[map_view_width_str_index][select_language.cget() + 1 /* 言語位置調整 */]
                    //)).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Arg::topRight = s3d::Vec2(paxg::Window::width() - 160, debug_start_y), s3d::Palette::Black);

                    // マップの幅
                     font[select_language.cget()].setOutline(0, 0.6, paxg::Color(255, 255, 255));
                     font[select_language.cget()].draw(std::to_string(map_view_width),
                         paxg::Vec2i(paxg::Window::width() - 110, debug_start_y), paxg::Color(0, 0, 0));

                    //debug_start_y += debug_move_y;
                    //font[select_language.cget()](s3d::Unicode::FromUTF8(language_text.cget()[xyz_tile_z_str_index][select_language.cget() + 1 /* 言語位置調整 */]
                    //)).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Arg::topRight = s3d::Vec2(paxg::Window::width() - 160, debug_start_y), s3d::Palette::Black);

                    //font[language](s3d::ToString(xyz_tile2->getZ())
                    //).draw(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White), s3d::Vec2(paxg::Window::width() - 110, debug_start_y), s3d::Palette::Black);
                    //debug_start_y += debug_move_y;
                    // font[select_language.cget()].setOutline(0, 0.6, paxg::Color(255, 255, 255));
                    // font[select_language.cget()].draw(std::to_string(xyz_tile2->getZNum()),
                    //    paxg::Vec2i(paxg::Window::width() - 110, debug_start_y), paxg::Color(0, 0, 0));
                }
            }
            if (visible["License"]) {
                //font(std::string{ U"A" } + s3d::ToString(xyz_tile_cell.x) + std::string{ U":" } + s3d::ToString(xyz_tile_cell.y)).draw(s3d::Arg::topRight = s3d::Vec2(paxg::Window::width() - 10, 400), s3d::Palette::Black);
                //font(std::string{ U"B" } + s3d::ToString(xyz_tile_pos.x) + std::string{ U":" } + s3d::ToString(xyz_tile_pos.y)).draw(s3d::Arg::topRight = s3d::Vec2(paxg::Window::width() - 10, 450), s3d::Palette::Black);
                license_font.setOutline(0, 0.6, paxg::Color(255, 255, 255));
                license_font.drawBottomLeft(std::string(map_license_name),
                    paxg::Vec2i(10, paxg::Window::height() - 10),
                    paxg::Color(255, 255, 255));
                license_font.drawBottomLeft(std::string(map_license_name),
                    paxg::Vec2i(10, paxg::Window::height() - 10),
                    paxg::Color(0, 0, 0));
                //pin_font(std::string{ U"国土地理院（https://maps.gsi.go.jp/development/ichiran.html）" }).draw(s3d::Arg::topLeft = s3d::Vec2(10, 10), s3d::Palette::Black);
                //texture_tlt.resized(180).draw(s3d::Arg::bottomRight = s3d::Vec2(paxg::Window::width() - 10, paxg::Window::height() - 10));
            }

            if (!visible["3D"]) {
                g3d_model.updateRotation(); // 3D モデルを回転させる
            }

            // メニューバー
            paxg::Rect{ 0, 0, static_cast<float>(paxg::Window::width()), 30 }.draw(paxg::Color{ 243, 243, 243 });
            texture_dictionary.at("texture_github").resizedDraw(24, paxg::Vec2i{ paxg::Window::width() - 280, 3 });
            pulldown.draw(); // 言語選択
            menu_bar.draw(); // 左上メニューバー

            if (tm_.get(s3d::Rect(paxg::Window::width() - 280, 3, 28).leftClicked())) {
                // Web ページをブラウザで開く
                s3d::System::LaunchBrowser(U"https://github.com/AsPJT/PAX_SAPIENTICA");
            }

            if (visible["UI"]) {
                if (select_language.cget() + 1 < font.size()) {
                    font[select_language.cget()].setOutline(0, 0.6, paxg::Color(255, 255, 255));
                    font[select_language.cget()].drawTopRight(std::string(language_text.cget()[koyomi_siv.sueki_nakamura_index][select_language.cget() + 1 /* 言語位置調整 */]),
                        paxg::Vec2i(paxg::Window::width() - 160, 590), paxg::Color(0, 0, 0));
                    font[select_language.cget()].drawTopRight(std::string(language_text.cget()[koyomi_siv.sueki_tanabe_index][select_language.cget() + 1 /* 言語位置調整 */]),
                        paxg::Vec2i(paxg::Window::width() - 160, 620), paxg::Color(0, 0, 0));
                }
                {
                    std::string sueki_nakamura = "";
                    std::string sueki_tanabe = "";

                    static std::array<int, 18> sueki_year = { {
                    380,390,410,430,440,450,460,470,490,500,520,530,550,560,590,620,645,670
                    } };
                    static std::array<std::string, 18> sueki_name = { {
        "","TG232","TK73","TK216","ON46","ON46/TK208","TK208","TK23","TK23/TK47","TK47",
        "MT15","TK10","TK10/MT85","MT85","TK43","TK209",reinterpret_cast<const char*>(u8"TK217古"),reinterpret_cast<const char*>(u8"TK217新")
                                } };
                    static std::array<std::string, 18> sueki_nakamura_name = { {
        "",reinterpret_cast<const char*>(u8"I-1前"),reinterpret_cast<const char*>(u8"I-1後"),"I-2","I-3","I-3","I-3","I-4","I-4/I-5","I-5",
        "II-1","II-2","II-2/II-3","II-3","II-4","II-5","II-6","III-1"
                                } };
                    {
                        int date_year = 0;
                        std::visit([&](const auto& x) { date_year = int(x.cgetYear()); }, koyomi_siv.date_list[1].date);
                        for (std::size_t i = 0; i < sueki_year.size(); ++i) {
                            if (date_year < sueki_year[i]) {
                                sueki_tanabe = sueki_name[i];
                                sueki_nakamura = sueki_nakamura_name[i];
                                break;
                            }
                        }
                    }
                    if (select_language.cget() + 1 < font.size()) {
                        font[select_language.cget()].setOutline(0, 0.6, paxg::Color(255, 255, 255));
                        font[select_language.cget()].drawTopRight(
                            std::string(sueki_tanabe),
                            paxg::Vec2i(paxg::Window::width() - 60, 620), paxg::Color(0, 0, 0));
                        font[select_language.cget()].drawTopRight(
                            std::string(sueki_nakamura),
                            paxg::Vec2i(paxg::Window::width() - 60, 590), paxg::Color(0, 0, 0));
                    }
                }
            }
            // シミュレーションのボタン
            if (visible["Simulation"] && visible["UI"]) {
#ifdef PAXS_USING_SIMULATOR
                if (s3d::SimpleGUI::Button(U"Init", s3d::Vec2{ 10, 60 })) {
                    simulator = paxs::Simulator<int>(
                        path8 + "Data/Simulation/MapList.tsv",
                        //paxs::Vector2<int>{861, 350}, paxs::Vector2<int>{950, 450}, 10);
                        start_position, end_position, 10);
                    simulator.init();
                    koyomi_siv.steps.setDay(0); // ステップ数を 0 にする
                }
                if (s3d::SimpleGUI::Button(U"Start", s3d::Vec2{ 110, 60 })) koyomi_siv.is_agent_update = true;
                if (s3d::SimpleGUI::Button(U"Stop", s3d::Vec2{ 210, 60 })) koyomi_siv.is_agent_update = false;
#endif
            }

        }
    private:
        std::vector<paxg::Font> setFont(
            const int font_size_,
            const std::string& path,
            const int buffer_thickness,
            const std::string& key,
            const paxs::Language& language_text) const {
            std::vector<paxg::Font> return_font{};
            const std::vector<std::string>& vs = language_text.cgetFindStart(key);
            for (std::size_t i = 1; i < vs.size(); ++i) {
                return_font.emplace_back(paxg::Font{ font_size_, (path + vs[i]), buffer_thickness});
            }
            return return_font;
        }

    };

}

#endif // !PAX_MAHOROBA_STRING_VIEWER_HPP
