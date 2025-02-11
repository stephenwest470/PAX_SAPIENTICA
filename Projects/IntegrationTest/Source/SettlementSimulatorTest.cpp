﻿/*##########################################################################################

	PAX SAPIENTICA Library 💀🌿🌏

	[Planning]		2023-2024 As Project
	[Production]	2023-2024 As Project
	[Contact Us]	wanotaitei@gmail.com			https://github.com/AsPJT/PAX_SAPIENTICA
	[License]		Distributed under the CC0 1.0.	https://creativecommons.org/publicdomain/zero/1.0/

##########################################################################################*/

#include <memory>

#include <PAX_SAPIENTICA/Simulation/SettlementSimulator.hpp>

int main() {
    const std::string map_list_path = "Data/Simulations/MapList.tsv";
    const std::string& japan_provinces_path = "Data/Simulations/Sample";
    std::random_device seed_gen;
    std::unique_ptr<paxs::SettlementSimulator> simulator = std::make_unique<paxs::SettlementSimulator>(map_list_path, japan_provinces_path, seed_gen());
    simulator->init();
    simulator->run(50);
}
