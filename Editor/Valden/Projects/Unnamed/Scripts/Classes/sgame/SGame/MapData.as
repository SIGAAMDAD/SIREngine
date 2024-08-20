#include "SGame/MapCheckpoint.as"
#include "SGame/MapSpawn.as"
#include "SGame/MapSecret.as"

namespace TheNomad::SGame {
    class MapData {
		MapData() {
		}

		void Init( const string& in mapName, uint nMapLevels ) {
			m_nWidth = 0;
			m_nHeight = 0;
			m_Name = mapName;
		}
		
		private int GetCheckpointIndex( const MapCheckpoint& in cp ) const {
			return m_Checkpoints.Find( cp );
		}

		void Load( int hMap ) {
			uint nCheckpoints, nSpawns, nTiles;
			uint i;
			uvec3 xyz;
			
			TheNomad::GameSystem::SetActiveMap( hMap, nCheckpoints, nSpawns, nTiles, m_nWidth, m_nHeight );
			TheNomad::GameSystem::GetTileData( @m_TileData );
			TheNomad::Engine::Renderer::LoadWorld( m_Name );
			
			//
			// load the checkpoints
			//
			for ( i = 0; i < nCheckpoints; i++ ) {
				TheNomad::GameSystem::GetCheckpointData( xyz, i );
				m_Checkpoints.Add( MapCheckpoint( xyz ) );
			}
			
			//
			// load in spawns
			//
			for ( i = 0; i < nSpawns; i++ ) {
				MapSpawn spawn;
				uint id, type, checkpoint;
				
				TheNomad::GameSystem::GetSpawnData( xyz, type, id, i, checkpoint );
				spawn = MapSpawn( xyz, id, TheNomad::GameSystem::EntityType( type ) );

				DebugPrint( "Spawn " + i + " linked to checkpoint " + checkpoint + "\n" );
				@spawn.m_Checkpoint = @m_Checkpoints[ checkpoint ];
				spawn.m_Checkpoint.m_Spawns.Add( @spawn );
				
				m_Spawns.Add( spawn );
			}

			DebugPrint( "Map \"" + m_Name + "\" loaded with " + m_Checkpoints.Count() + " checkpoints, " +
				m_Spawns.Count() + " spawns, and " + m_Secrets.Count() + " secrets. "
				+ "Width is " + m_nWidth + " and Height is " + m_nHeight + "\n" );
		}
		
		const array<array<uint>>@ GetTiles() const {
			return @m_TileData;
		}
		array<array<uint>>@ GetTiles() {
			return @m_TileData;
		}
		
		uint NumLevels() const {
			return m_TileData.Count();
		}

		uint GetLevel( const TheNomad::GameSystem::BBox& in bounds ) const {
			const uint level = uint( floor( bounds.m_Mins.z + bounds.m_Maxs.z ) );
			return 0;
		}
		uint GetTile( const vec3& in origin, const TheNomad::GameSystem::BBox& in bounds ) const {
			const uint level = GetLevel( bounds );
			const int x = int( ceil( origin.x ) );
			const int y = int( ceil( origin.y ) );
			if ( level >= m_TileData.Count() || x < 0 || y < 0 || x >= m_nWidth || y >= m_nHeight ) {
				return 0;
			}
			return m_TileData[ level ][ y * m_nWidth + x ];
		}
		const array<MapSpawn>@ GetSpawns() const {
			return @m_Spawns;
		}
		const array<MapCheckpoint>@ GetCheckpoints() const {
			return @m_Checkpoints;
		}
		array<MapSpawn>@ GetSpawns() {
			return @m_Spawns;
		}
		array<MapCheckpoint>@ GetCheckpoints() {
			return @m_Checkpoints;
		}
		int GetWidth() const {
			return m_nWidth;
		}
		int GetHeight() const {
			return m_nHeight;
		}
		
		private string m_Name;
		private array<MapSecret> m_Secrets;
		private array<MapSpawn> m_Spawns;
		private array<MapCheckpoint> m_Checkpoints;
		private array<array<uint>> m_TileData;
		private int m_nWidth = 0;
		private int m_nHeight = 0;
	};
};