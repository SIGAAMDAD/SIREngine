#include "SGame/MapCheckpoint.as"
#include "SGame/MapSpawn.as"
#include "SGame/MapData.as"
#include "SGame/LevelRankData.as"
#include "SGame/LevelInfoData.as"
#include "SGame/LevelStats.as"
#include "SGame/LevelMapData.as"
#include "SGame/InfoSystem/InfoDataManager.as"
#include "Engine/SoundSystem/SoundEffect.as"
#include "SGame/Sprite.as"
#include "SGame/SpriteSheet.as"
#include "SGame/GfxSystem.as"

namespace TheNomad::SGame {
    enum LevelRank {
		RankS,
		RankA,
		RankB,
		RankC,
		RankD,
		RankF,
		RankWereUBotting,
		
		NumRanks
	};

    enum GameState {
		Inactive,
		InLevel,
		LevelFinish,
		EndOfLevel,
		StatsMenu,
		DeathMenu,
	};

    class LevelSystem : TheNomad::GameSystem::GameObject {
		LevelSystem() {
			m_PassedCheckpointSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/misc/passCheckpoint.ogg" );
		}
		
		void OnInit() {
			LevelInfoData@ data;
			json@ rankS, rankA, rankB, rankC, rankD, rankF, rankU;
			uint i;
			string mapname;
			string levelName;
			string music;

			ConsolePrint( "Loading level infos...\n" );

			for ( i = 0; i < sgame_ModList.Count(); i++ ) {
				LoadLevelsFromFile( sgame_ModList[i] );
			}

			@m_Current = null;

			ConsolePrint( formatUInt( NumLevels() ) + " levels parsed.\n" );

			for ( i = 0; i < NumLevels(); i++ ) {
				json@ info = @m_LevelInfos[ i ];
				if ( !info.get( "Name", levelName ) ) {
					ConsoleWarning( "invalid level info, Name variable is missing.\n" );
					continue;
				}

				@data = LevelInfoData( levelName );

				ConsolePrint( "Loaded level '" + data.m_Name + "'...\n" );

				for ( uint j = 0; j < TheNomad::GameSystem::GameDifficulty::NumDifficulties; j++ ) {
					if ( !info.get( "MapNameDifficulty_" + formatUInt( j ), mapname ) ) {
						ConsoleWarning( "invalid level map info for \"" + levelName
							+ "\", missing value for 'MapNameDifficulty_" + j + "', skipping.\n" );
						continue;
					}

					LevelMapData MapData = LevelMapData( mapname );
					MapData.mapHandle = TheNomad::GameSystem::LoadMap( mapname );
					MapData.difficulty = TheNomad::GameSystem::GameDifficulty( j );

					if ( MapData.mapHandle == FS_INVALID_HANDLE ) {
						ConsoleWarning( "failed to load map '" + mapname + "' for level '" + data.m_Name + "'\n" );
						continue;
					}
					data.m_MapHandles.Add( MapData );
				}

				if ( info.get( "SoundTrack_Ambient", music ) ) {
					data.m_hAmbientTheme = TheNomad::Engine::ResourceCache.GetSfx( music );
					if ( data.m_hAmbientTheme == -1 ) {
						ConsoleWarning( "SoundTrack_Ambient for LevelData \"" + data.m_Name + "\" not found.\n" );
					}
				}
				if ( info.get( "SoundTrack_Combat", music ) ) {
					data.m_hCombatTheme = TheNomad::Engine::ResourceCache.GetSfx( music );
					if ( data.m_hCombatTheme == -1 ) {
						ConsoleWarning( "SoundTrack_Combat for LevelData \"" + data.m_Name + "\" not found.\n" );
					}
				}

				info.get( "BeginLevelScript", data.m_StartLevelScript );
				info.get( "EndLevelScript", data.m_EndLevelScript );

				data.m_nIndex = i;

				@rankS = @info[ "RankS" ];
				@rankA = @info[ "RankA" ];
				@rankB = @info[ "RankB" ];
				@rankC = @info[ "RankC" ];
				@rankD = @info[ "RankD" ];
				@rankF = @info[ "RankF" ];
				@rankU = @info[ "RankU" ];

				//
				// all rank infos must be present
				//
				if ( @rankS is null ) {
					ConsoleWarning( "invalid level info file, missing object 'RankS'.\n" );
					continue;
				}
				if ( @rankA is null ) {
					ConsoleWarning( "invalid level info file, missing object 'RankA'.\n" );
					continue;
				}
				if ( @rankB is null ) {
					ConsoleWarning( "invalid level info file, missing object 'RankB'.\n" );
					continue;
				}
				if ( @rankC is null ) {
					ConsoleWarning( "invalid level info file, missing object 'RankC'.\n" );
					continue;
				}
				if ( @rankD is null ) {
					ConsoleWarning( "invalid level info file, missing object 'RankD'.\n" );
					continue;
				}
				if ( @rankF is null ) {
					ConsoleWarning( "invalid level info file, missing object 'RankF'.\n" );
					continue;
				}
				if ( @rankU is null ) {
					ConsoleWarning( "invalid level info file, missing object 'RankU'.\n" );
					continue;
				}

				data.m_RankS.rank = LevelRank::RankS;
				if ( !LoadLevelRankData( "RankS", @data.m_RankS, rankS ) ) {
					continue;
				}
				data.m_RankA.rank = LevelRank::RankA;
				if ( !LoadLevelRankData( "RankA", @data.m_RankA, rankA ) ) {
					continue;
				}
				data.m_RankB.rank = LevelRank::RankB;
				if ( !LoadLevelRankData( "RankB", @data.m_RankB, rankB ) ) {
					continue;
				}
				data.m_RankC.rank = LevelRank::RankC;
				if ( !LoadLevelRankData( "RankC", @data.m_RankC, rankC ) ) {
					continue;
				}
				data.m_RankD.rank = LevelRank::RankD;
				if ( !LoadLevelRankData( "RankD", @data.m_RankD, rankD ) ) {
					continue;
				}
				data.m_RankF.rank = LevelRank::RankF;
				if ( !LoadLevelRankData( "RankF", @data.m_RankF, rankF ) ) {
					continue;
				}
				data.m_RankU.rank = LevelRank::RankWereUBotting;
				if ( !LoadLevelRankData( "RankU", @data.m_RankU, rankU ) ) {
					continue;
				}

				m_LevelInfoDatas.Add( @data );
			}
			m_LevelInfos.Clear();
		}
		void OnShutdown() {
			m_LevelInfoDatas.Clear();
		}
		void OnRenderScene() {
		}
		void OnRunTic() {
			MapCheckpoint@ cp = null;
			uint checkpointIndex = 0;

			if ( GlobalState == GameState::StatsMenu ) {
				m_RankData.Draw( true, m_nLevelTimer );
				return;
			}
			
			//
			// checkpoint updates
			//
			
			// check if we are passing it
			if ( ( @cp = @PlayerPassedCheckpoint( checkpointIndex ) ) !is null ) {
				m_PassedCheckpointSfx.Play();
				cp.Activate( m_nLevelTimer );

				DebugPrint( "Setting checkpoint " + m_CurrentCheckpoint + " to completed.\n" );

				// done with the level?
				if ( @cp is @m_MapData.GetCheckpoints()[ m_MapData.GetCheckpoints().Count() - 1 ] ) {
					CalcTotalLevelTime();
					m_RankData.CalcTotalLevelStats();
					GlobalState = GameState::LevelFinish;
					return;
				}
				
				for ( uint i = 0; i < TheNomad::GameSystem::GameSystems.Count(); i++ ) {
					TheNomad::GameSystem::GameSystems[i].OnCheckpointPassed( i );
				}
				
				TheNomad::Engine::CmdExecuteCommand( "sgame.save_game\n" );
			}
			
			m_RankData.Draw( false, m_nLevelTimer );
		}
		void OnPlayerDeath( int ) {
		}
		void OnCheckpointPassed( uint ) {
		}
		void OnLevelEnd() {
			if ( m_Current.m_EndLevelScript.Length() != 0 ) {
				string script;
				if ( TheNomad::Engine::FileSystem::LoadFile( m_Current.m_EndLevelScript, script ) == 0 ) {
					ConsoleWarning( "Error loading EndOfLevel script\n" );
				} else {
					ConsolePrint( "Executing \"" + script + "\"\n" );
					TheNomad::Engine::CmdExecuteCommand( script );
				}
			}
		}
		void OnSave() const {
			const LevelStats@ stats;
			uint i, a;
			
			DebugPrint( "Archiving level stats...\n" );
			
			{
				TheNomad::GameSystem::SaveSystem::SaveSection section( GetName() );
				section.SaveUInt( "NumLevels", m_LevelInfoDatas.Count() );
				for ( i = 0; i < m_LevelInfoDatas.Count(); i++ ) {
					section.SaveString( "LevelNames" + i, m_LevelInfoDatas[i].m_Name );
				}

				// save checkpoint data
				section.SaveUInt( "CurrentCheckpoint", m_CurrentCheckpoint );
				for ( i = 0; i < m_MapData.GetCheckpoints().Count(); i++ ) {
					section.SaveUInt64( "CheckpointTime_" + i, m_MapData.GetCheckpoints()[i].m_nTime );
				}
			}
			
			// save individual stats for each level
			for ( i = 0; i < m_LevelInfoDatas.Count(); i++ ) {
				// each section is a different .prt file, so the stats will remain per save-slot unless overwritten
				// hopefully this doesn't lead to other mods overwriting to data if two levels have the same name,
				// but that is a rare case and possibly a user/modder error
				TheNomad::GameSystem::SaveSystem::SaveSection save( m_LevelInfoDatas[i].m_Name + "_RankData" );
				
				// we don't use NumDifficulties because a map doesn't have to be registered with all difficulties
				save.SaveUInt( "MapCount", m_LevelInfoDatas[i].m_MapHandles.Count() );
				for ( a = 0; a < m_LevelInfoDatas[i].m_MapHandles.Count(); a++ ) {
					@stats = @m_LevelInfoDatas[i].m_MapHandles[a].highStats;
					
					//
					// save the ranks, then the actual numbers (for the perfectionists)
					//
					
					save.SaveUInt( "GameDifficulty", uint( m_LevelInfoDatas[i].m_MapHandles[a].difficulty ) );
					
					save.SaveUInt( "Rank", uint( stats.total_Rank ) );
					save.SaveUInt( "TimeRank", uint( stats.time_Rank ) );
					save.SaveUInt( "KillsRank", uint( stats.kills_Rank ) );
					save.SaveUInt( "StyleRank", uint( stats.style_Rank ) );
					save.SaveUInt( "DeathsRank", uint( stats.deaths_Rank ) );
					
					save.SaveBool( "CleanRun", stats.isClean );
					save.SaveUInt64( "Time", stats.m_TimeMilliseconds );
					
					save.SaveUInt( "NumKills", stats.numKills );
					save.SaveUInt( "StylePoints", stats.stylePoints );
					save.SaveUInt( "CollateralScore", stats.collateralScore );
					save.SaveUInt( "NumDeaths", stats.numDeaths );
				}
			}
		}
		void OnLoad() {
			LevelStats@ stats;
			LevelInfoData@ data;
			uint i, a;
			array<string> names;

			TheNomad::GameSystem::SaveSystem::LoadSection load( GetName() );
			DebugPrint( "Loading level stats...\n" );
			if ( !load.Found() ) {
				GameError( "LevelSystem::OnLoad: save file corruption, section 'LevelStats' not found!" );
			}

			const uint numLevels = load.LoadUInt( "NumLevels" );
			ConsolePrint( "Got " + numLevels + " levels\n" );
			m_LevelInfoDatas.Resize( numLevels );
			names.Resize( numLevels );
			for ( i = 0; i < m_LevelInfoDatas.Count(); i++ ) {
				load.LoadString( "LevelNames" + i, names[i] );
			}
			m_nLevelTimer = load.LoadUInt( "Time" );
			m_CurrentCheckpoint = load.LoadUInt( "CurrentCheckpoint" );

			for ( i = 0; i < m_LevelInfoDatas.Count(); i++ ) {
				@data = @m_LevelInfoDatas[i];
				ConsolePrint( "Loading level stats for " + names[i] + "\n" );
				TheNomad::GameSystem::SaveSystem::LoadSection section( names[i] + "_RankData" );
				if ( !section.Found() ) {
					GameError( "LevelSystem::OnLoad: save file corruption, section '" + names[i] + "_RankData' not found!" );
				}
				
				// there might be some levels in the save that just aren't present with the mods loaded
				data.m_nIndex = i;
				
				const uint mapCount = section.LoadUInt( "MapCount" );
				data.m_MapHandles.Resize( mapCount );
				for ( a = 0; a < mapCount; a++ ) {
					@stats = @data.m_MapHandles[a].highStats;
					
					data.m_MapHandles[a].difficulty = TheNomad::GameSystem::GameDifficulty( section.LoadUInt( "GameDifficulty" ) );
					
					stats.total_Rank = LevelRank( section.LoadUInt( "Rank" ) );
					stats.time_Rank = LevelRank( section.LoadUInt( "TimeRank" ) );
					stats.kills_Rank = LevelRank( section.LoadUInt( "KillsRank" ) );
					stats.style_Rank = LevelRank( section.LoadUInt( "StyleRank" ) );
					stats.deaths_Rank = LevelRank( section.LoadUInt( "DeathsRank" ) );

					stats.isClean = section.LoadBool( "CleanRun" );
					stats.m_TimeMilliseconds = section.LoadUInt64( "Time" );
					stats.m_TimeSeconds = stats.m_TimeMilliseconds / 1000;
					stats.m_TimeMinutes = stats.m_TimeMilliseconds / 60000;
					
					stats.numKills = section.LoadUInt( "NumKills" );
					stats.stylePoints = section.LoadUInt( "StylePoints" );
					stats.collateralScore = section.LoadUInt( "CollateralScore" );
					stats.numDeaths = section.LoadUInt( "NumDeaths" );
				}
			}
		}
		const string& GetName() const {
			return "LevelSystem";
		}
		//!
		//!
		//!
		void OnLevelStart() {
			int difficulty;

			// get the level index
			m_nIndex = TheNomad::Engine::CvarVariableInteger( "g_levelIndex" );
			difficulty = sgame_Difficulty.GetInt();
			m_CurrentCheckpoint = 0;

			DebugPrint( "Initializing level at " + m_nIndex + ", difficulty set to \"" + SP_DIFF_STRINGS[ difficulty ] + "\".\n" );
			
			ConsolePrint( "Loading level \"" + m_LevelInfoDatas[m_nIndex].m_MapHandles[difficulty].m_Name + "\"...\n" );
			
			m_RankData = LevelStats();
			@m_Current = @m_LevelInfoDatas[m_nIndex];
			@m_MapData = MapData();
			m_MapData.Init( m_LevelInfoDatas[m_nIndex].m_MapHandles[difficulty].m_Name, 1 );
			m_MapData.Load( m_LevelInfoDatas[m_nIndex].m_MapHandles[difficulty].mapHandle );

			switch ( TheNomad::GameSystem::GameDifficulty( difficulty ) ) {
			case TheNomad::GameSystem::GameDifficulty::VeryEasy: // Noob
				m_nDifficultyScale = 0.5f;
				break;
			case TheNomad::GameSystem::GameDifficulty::Easy: // Recruit
				m_nDifficultyScale = 0.75f;
				break;
			case TheNomad::GameSystem::GameDifficulty::Normal: // Mercenary
				m_nDifficultyScale = 1.0f;
				break;
			case TheNomad::GameSystem::GameDifficulty::Hard: // Nomad
				m_nDifficultyScale = 1.90f;
				break;
			case TheNomad::GameSystem::GameDifficulty::VeryHard: // The Black Death
				m_nDifficultyScale = 2.5f;
				break;
			case TheNomad::GameSystem::GameDifficulty::TryYourBest: // MemeMode
				m_nDifficultyScale = 5.0f; // ... ;)
				break;
			};
			
			// check if there's a player spawn at index 0, as that is required for all levels.
			// Even if it's a cinematic where the camera will not be on the player, they must
			// always be active and alive for the level to actually be running
			//
			// in the case we're in a split-screen co-op situation, then all the players after the
			// first will be spawned around them
			if ( m_MapData.GetSpawns().Count() < 1 || m_MapData.GetCheckpoints().Count() < 1 ) {
				ForcePlayerSpawn();
			} else {
				const MapSpawn@ spawn = @m_MapData.GetSpawns()[ 0 ];
				if ( spawn.m_nEntityType != TheNomad::GameSystem::EntityType::Playr ) {
					ForcePlayerSpawn();
				}
			}

			if ( m_Current.m_StartLevelScript.Length() != 0 ) {
				string script;
				if ( TheNomad::Engine::FileSystem::LoadFile( m_Current.m_StartLevelScript, script ) == 0 ) {
					ConsoleWarning( "Error loading StartOfLevel script\n" );
				} else {
					TheNomad::Engine::CmdExecuteCommand( script );
				}
			}
			
			m_nLevelTimer = TheNomad::GameSystem::GameManager.GetGameTic();

			m_PassedCheckpointSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/misc/passCheckpoint.ogg" );
		}
		
		private void ForcePlayerSpawn() {
			MapSpawn spawn( uvec3( 0, 0, 0 ), 0, TheNomad::GameSystem::EntityType::Playr );

			ConsoleWarning( "LevelSystem::OnLevelStart: the first spawn in a level must always be the player!\n" );
			ConsolePrint( "Forcing player spawn...\n" );
			m_MapData.GetSpawns().InsertAt( 0, spawn );
			if ( m_MapData.GetCheckpoints().Count() == 0 ) {
				ConsoleWarning( "LevelSystem::OnLevelStart: at least one checkpoint is required per level\n" );
				ConsolePrint( "Forcing mandatory checkpoint...\n" );

				MapCheckpoint cp = MapCheckpoint( uvec3( 0, 0, 0 ) );
				m_MapData.GetCheckpoints().InsertAt( 0, cp );
			}
			m_MapData.GetCheckpoints()[0].m_Spawns.Add( @spawn );
		}
		
		//
		// AddLevelData: for manual level infos
		//
		void AddLevelData( LevelInfoData@ levelData ) {
			m_LevelInfoDatas.Add( @levelData );
		}
		
		const LevelInfoData@ GetLevelDataByIndex( uint nIndex ) const {
			return m_LevelInfoDatas[ nIndex ];
		}
		LevelInfoData@ GetLevelDataByIndex( uint nIndex ) {
			return m_LevelInfoDatas[ nIndex ];
		}
		
		const LevelInfoData@ GetLevelInfoByMapName( const string& in mapname ) const {
			for ( uint i = 0; i < m_LevelInfoDatas.Count(); i++ ) {
				for ( uint a = 0; a < m_LevelInfoDatas[i].m_MapHandles.Count(); a++ ) {
					if ( TheNomad::Util::StrICmp( m_LevelInfoDatas[i].m_MapHandles[a].m_Name, mapname ) == 0 ) {
						return @m_LevelInfoDatas[i];
					}
				}
			}
			return null;
		}

		private array<json@>@ LoadJSonFile( const string& in modName ) {
			string path;
			array<json@> values;
			json@ data;
			
			path = "modules/" + modName + "/DataScripts/levels.json";
			
			@data = json();
			if ( !data.ParseFile( path ) ) {
				ConsoleWarning( "failed to load level info file '" + path + "', skipping.\n" );
				return null;
			}

			if ( !data.get( "LevelInfo", values ) ) {
				ConsoleWarning( "level info file found, but no level infos found.\n" );
				return null;
			}
			
			return @values;
		}

		private void LoadLevelsFromFile( const string& in modName ) {
			array<json@>@ levels;

			@levels = @LoadJSonFile( modName );
			if ( @levels is null ) {
				return;
			}

			ConsolePrint( "Got " + levels.Count() + " level infos from \"" + modName + "\"\n" );
			m_nLevels += levels.Count();
			for ( uint i = 0; i < levels.Count(); i++ ) {
				m_LevelInfos.Add( @levels[i] );
			}
		}

		bool LoadLevelRankData( const string& in rankName, LevelRankData@ data, json@ src ) {
			json@ minTime;

			if ( !src.get( "MinKills", data.minKills ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'MinKills'.\n" );
				return false;
			}
			if ( !src.get( "MinStyle", data.minStyle ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'MinStyle'.\n" );
				return false;
			}
			if ( !src.get( "MaxDeaths", data.maxDeaths ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'MaxDeaths'.\n" );
				return false;
			}
			if ( !src.get( "MaxCollateral", data.maxCollateral ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'MaxCollateral'.\n" );
				return false;
			}
			if ( !src.get( "RequiresClean", data.requiresClean ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'RequiresClean'.\n" );
				return false;
			}
			if ( !src.get( "MinTime.Milliseconds", data.min_TimeMilliseconds ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'MinTime.Milliseconds'.\n" );
				return false;
			}
			if ( !src.get( "MinTime.Seconds", data.min_TimeSeconds ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'MinTime.Seconds'.\n" );
				return false;
			}
			if ( !src.get( "MinTime.Minutes", data.min_TimeMinutes ) ) {
				ConsoleWarning( "invalid level info object '" + rankName + "', no variable 'MinTime.Minutes'.\n" );
				return false;
			}

			return true;
		}

		private void CalcTotalLevelTime() {
			uint64 total = 0;

			for ( uint i = 0; i < m_MapData.GetCheckpoints().Count(); i++ ) {
				total += m_MapData.GetCheckpoints()[i].m_nTime;
			}

			m_nLevelTimer = total;
		}

		private MapCheckpoint@ PlayerPassedCheckpoint( uint& out nCheckpointIndex ) {
			const vec3 origin = EntityManager.GetActivePlayer().GetOrigin();
			array<MapCheckpoint>@ checkpoints = @m_MapData.GetCheckpoints();
			const uvec3 pos = uvec3( uint( origin.x ), uint( origin.y ), uint( origin.z ) );

			for ( uint i = 0; i < checkpoints.Count(); i++ ) {
				if ( checkpoints[i].m_Origin == pos && !checkpoints[i].m_bPassed ) {
					nCheckpointIndex = i;
					return @checkpoints[i];
				}
			}
			return null;
		}

		void Pause() {
			if ( m_bPaused ) {
				return;
			}
			m_bPaused = true;
			m_nPauseTimer = TheNomad::GameSystem::GameManager.GetGameTic();
		}
		void Resume() {
			if ( !m_bPaused ) {
				return;
			}
			m_bPaused = false;

			/*
			* ===========] pause [================================
			* level time | delta | level time = level time + delta
			*/
			const uint64 delta = TheNomad::GameSystem::GameManager.GetGameTic() - m_nPauseTimer;
			m_nLevelTimer += delta;
		}

		float GetDifficultyScale() const {
			return m_nDifficultyScale;
		}
		
		LevelStats& GetStats() {
			return m_RankData;
		}
		
		uint NumLevels() const {
			return m_nLevels;
		}
		uint LoadedIndex() const {
			return m_nIndex;
		}

		const MapData@ GetMapData() const {
			return @m_MapData;
		}
		MapData@ GetMapData() {
			return @m_MapData;
		}

		const LevelInfoData@ GetCurrentData() const {
			return @m_Current;
		}
		LevelInfoData@ GetCurrentData() {
			return @m_Current;
		}
		const string GetLevelName() const {
			return @m_Current !is null ? m_Current.m_Name : "N/A";
		}
		
		private uint64 m_nEndTime = 0;
		private uint64 m_nPauseTimer = 0;
		private uint64 m_nLevelTimer = 0;
		private uint m_CurrentCheckpoint = 0;
		private array<json@> m_LevelInfos;
		private array<LevelInfoData@> m_LevelInfoDatas;
		private uint m_nIndex = 0;
		private uint m_nLevels = 0;
		private bool m_bPaused = false;
		
		private bool m_bNewCheckpoint = true;
		private TheNomad::Engine::SoundSystem::SoundEffect m_PassedCheckpointSfx;

		private float m_nDifficultyScale = 1.0f;
		
		private LevelStats m_RankData = LevelStats();
		private LevelInfoData@ m_Current = null;
		private MapData@ m_MapData = null;
	};

	uint GetMapLevel( float elevation ) {
		uint e;

		e = uint( floor( elevation ) );

		return 0;
	}

	array<string> sgame_ModList;

	GameState GlobalState;

	LevelSystem@ LevelManager;

	TheNomad::Engine::SoundSystem::SoundEffect selectedSfx;
	string[] SP_DIFF_STRINGS( TheNomad::GameSystem::GameDifficulty::NumDifficulties );
	string[] sgame_RankStrings( TheNomad::SGame::LevelRank::NumRanks );
	vec4[] sgame_RankStringColors( TheNomad::SGame::LevelRank::NumRanks );
};