namespace TheNomad::SGame {
    class LevelMapData {
        LevelMapData() {
		}
		LevelMapData( const string& in name ) {
			m_Name = name;
		}
		
		string m_RecordHolder; // community-based record of who has the best score for the level
		LevelStats highStats;
		
		string m_Name;
		TheNomad::GameSystem::GameDifficulty difficulty;
		int mapHandle;
    };
};