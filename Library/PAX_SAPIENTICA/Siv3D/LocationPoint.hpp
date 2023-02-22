﻿/*##########################################################################################

	PAX SAPIENTICA Library 💀🌿🌏

	[Planning]		2023 As Project
	[Production]	2023 As Project
	[Contact Us]	wanotaitei@gmail.com			https://github.com/AsPJT/PAX_SAPIENTICA
	[License]		Distributed under the CC0 1.0.	https://creativecommons.org/publicdomain/zero/1.0/

##########################################################################################*/

#ifndef PAX_SAPIENTICA_SIV3D_LOCATION_POINT_HPP
#define PAX_SAPIENTICA_SIV3D_LOCATION_POINT_HPP

#include <PAX_SAPIENTICA/Siv3D/Init.hpp>

/*##########################################################################################

##########################################################################################*/

namespace paxs {
	// ユリウス日
	//int jdn = 1600407;
	//int jdn = 1808020;
	int jdn = 1808286;

	enum class LocationPointEnum {
		location_point_place_name, // 地名
		location_point_pit_dwelling, // 集落遺跡
		location_point_zempo_koen_fun, // 前方後円墳
		location_point_zempo_koho_fun, // 前方後方墳
		location_point_hotategai_gata_kofun // 帆立貝型古墳
	};

	// 地名
	struct LocationPoint {
		std::string name;
		std::string en_name;
		double x, y;
		double min_view, max_view;
		int min_year, max_year;
		LocationPointEnum lpe;
		std::string source{};
	};

	class PlaceNameLocation {
	public:
		PlaceNameLocation() {
			// 古事記の地名
			inputPlace("./../../../../../Data/PlaceName/KojikiPlaceName.tsv");
			// 汎用的な地名
			inputPlace("./../../../../../Data/PlaceName/PlaceName.tsv");
			// 倭名類聚抄の地名
			inputPlace("./../../../../../Data/PlaceName/WamyoRuijushoPlaceName.tsv");
			// おもろさうしの地名
			inputPlace("./../../../../../Data/PlaceName/OmoroSoshiPlaceName.tsv");
		}
		void draw(const double map_view_width, const double map_view_height, const double map_view_center_x, const double map_view_center_y,
			const s3d::Font& font, const s3d::Font& en_font, const s3d::Font& pin_font)const {

			// 地名を描画
			for (int i = 0; i < location_point_list.size(); ++i) {
				auto& lli = location_point_list[i];
				// 範囲外を除去
				if (lli.x < (map_view_center_x - map_view_width / 1.8)
					|| lli.x >(map_view_center_x + map_view_width / 1.8)
					|| lli.y < (map_view_center_y - map_view_height / 1.8)
					|| lli.y >(map_view_center_y + map_view_height / 1.8)) continue;

				// 範囲内の場合
				if (lli.min_view > map_view_width
					|| lli.max_view < map_view_width
					|| lli.min_year > jdn
					|| lli.max_year < jdn) {

					// 地名
					if (lli.lpe == LocationPointEnum::location_point_place_name) {
						texture_pn.resized(14).drawAt(
							s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
						double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) });
						continue;
					}
					// 前方後円墳
					if (lli.lpe == LocationPointEnum::location_point_zempo_koen_fun) {
						texture_kofun1.resized(14).drawAt(
							s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
						double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) });
						continue;
					}
					// 前方後方墳
					if (lli.lpe == LocationPointEnum::location_point_zempo_koho_fun) {
						texture_kofun2.resized(14).drawAt(
							s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
						double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) });
						continue;
					}
					// 帆立貝型古墳
					if (lli.lpe == LocationPointEnum::location_point_hotategai_gata_kofun) {
						texture_kofun3.resized(14).drawAt(
							s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
						double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) });
						continue;
					}
				}
			}

			// 地名を描画
			for (int i = 0; i < location_point_list.size(); ++i) {
				auto& lli = location_point_list[i];

				// 範囲外を除去
				if (lli.x < (map_view_center_x - map_view_width / 1.8)
					|| lli.x >(map_view_center_x + map_view_width / 1.8)
					|| lli.y < (map_view_center_y - map_view_height / 1.8)
					|| lli.y >(map_view_center_y + map_view_height / 1.8)) continue;

				if (lli.min_view > map_view_width) continue;
				if (lli.max_view < map_view_width) continue;
				if (lli.min_year > jdn) continue;
				if (lli.max_year < jdn) continue;

				// 集落遺跡ではない場合
				//if (lli.lpe == LocationPointEnum::location_point_place_name) {
				if (lli.lpe != LocationPointEnum::location_point_pit_dwelling) {

					if (lli.en_name.size() == 0) {
						// 名前を描画
						font(s3d::Unicode::FromUTF8(lli.name)).drawAt(
							s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White),
							(lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height()))// - 30
							, s3d::Palette::Black);
					}
					else {
						// 名前（英語）を描画
						en_font(s3d::Unicode::FromUTF8(lli.en_name)).draw(
							s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White),
							s3d::Arg::bottomCenter = s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height()))// - 30
							}
						, s3d::Palette::Black);
						// 名前を描画
						font(s3d::Unicode::FromUTF8(lli.name)).draw(
							s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White),
							s3d::Arg::topCenter = s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height()))// - 30
							}
						, s3d::Palette::Black);
					}
					// 古事記のアイコン
					if (lli.source == "JP-Kojiki") {
						texture_ko.resized(35).drawAt(s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) + 50
							});
					}
					// 倭名類聚抄のアイコン
					else if (lli.source == "JP-WamyoRuijusho") {
						texture_wam.resized(35).drawAt(s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) + 50
							});
					}
					// 前方後円墳のアイコン
					else if (lli.source == "ZempoKoenFun") {
						texture_kofun1.resized(35).drawAt(s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) + 50
							});
					}
					// 前方後方墳のアイコン
					else if (lli.source == "ZempoKohoFun") {
						texture_kofun2.resized(35).drawAt(s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) + 50
							});
					}
					// 帆立貝型古墳のアイコン
					else if (lli.source == "HotategaiGataKofun") {
						texture_kofun3.resized(35).drawAt(s3d::Vec2{ (lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) + 50
							});
					}
				}
				// それ以外（集落遺跡）
				else {
					pin_font(s3d::Unicode::FromUTF8(lli.name)).drawAt(s3d::TextStyle::Outline(0, 0.6, s3d::Palette::White),
						(lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
						double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())) - 70
						, s3d::Palette::Black);
					texture_pin1.resized(50).draw(
						s3d::Arg::bottomCenter = s3d::Vec2((lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(s3d::Scene::Width()),
							double(s3d::Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(s3d::Scene::Height())))
					);
				}
				//Circle{{(lli.x - (map_view_center_x - map_view_width / 2)) / map_view_width * double(Scene::Width()),
				//double(Scene::Height()) - ((lli.y - (map_view_center_y - map_view_height / 2)) / map_view_height * double(Scene::Height()))},5 }.draw(Palette::Black);

			}
		}
	private:
		std::vector<LocationPoint> location_point_list;
		const s3d::Texture texture_ko{ U"./../../../../../Data/OldDocumentIcon/JP-Kojiki.svg" };
		const s3d::Texture texture_wam{ U"./../../../../../Data/OldDocumentIcon/JP-WamyoRuijusho.svg" };
		const s3d::Texture texture_pin1{ U"./../../../../../Data/Pin/PitDwelling.svg" };
		const s3d::Texture texture_kofun1{ U"./../../../../../Data/MiniIcon/ZempoKoenFun.svg" };
		const s3d::Texture texture_kofun2{ U"./../../../../../Data/MiniIcon/ZempoKohoFun.svg" };
		const s3d::Texture texture_kofun3{ U"./../../../../../Data/MiniIcon/HotategaiGataKofun.svg" };
		const s3d::Texture texture_pn{ U"./../../../../../Data/MiniIcon/PlaceName.svg" };
		void inputPlace(const std::string& str_, const LocationPointEnum lpe_ = LocationPointEnum::location_point_place_name) {
			// 地名を読み込み
			std::ifstream pifs(str_);
			if (pifs.fail()) return;
			std::string pline;
			while (std::getline(pifs, pline)) {
				std::vector<std::string> strvec = paxs::split(pline, '\t');

				location_point_list.emplace_back(
					strvec[0], // 漢字
					strvec[1], // ローマ字
					std::stod(strvec[2]), // 経度
					getLatitudeToMercatorY(std::stod(strvec[3])), // 緯度
					std::stod(strvec[4]), // 最小サイズ
					std::stod(strvec[5]), // 最大サイズ
					std::stod(strvec[6]), // 最小時代
					std::stod(strvec[7]), // 出典
					lpe_,
					strvec[8] // 最大時代
				);
			}
		}
	};
}

#endif // !PAX_SAPIENTICA_SIV3D_LOCATION_POINT_HPP