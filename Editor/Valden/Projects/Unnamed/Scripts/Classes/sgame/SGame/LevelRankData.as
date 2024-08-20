#include "SGame/LevelSystem.as"

namespace TheNomad::SGame {
	//
	// LevelRankData: holds data that will be read-only after parsed in from the json file
	//
    class LevelRankData {
		LevelRankData() {
		}
		
		LevelRank rank = LevelRank::RankS;
		uint minStyle = 0;
		uint minKills = 0;
		uint min_TimeMilliseconds = 0;
		uint min_TimeSeconds = 0;
		uint min_TimeMinutes = 0;
		uint maxDeaths = 0;
		uint maxCollateral = 0;
		bool requiresClean = true; // no warcrimes, no innocent deaths, etc. required for perfect score
	};
};