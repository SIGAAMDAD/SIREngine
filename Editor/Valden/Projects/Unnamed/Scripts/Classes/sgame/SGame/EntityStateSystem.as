#include "SGame/Animation.as"

namespace TheNomad::SGame {
	enum StateNum {
		ST_MOB_IDLE,
		ST_MOB_SEARCH,
		ST_MOB_CHASE,
		ST_MOB_FIGHT,
		ST_MOB_FIGHT_MELEE,
		ST_MOB_FIGHT_MISSILE,
		ST_MOB_FLEE,
		ST_MOB_DEAD,

		ST_GFX_ANIMATION,
		
		ST_PLAYR_IDLE,
		ST_PLAYR_DOUBLEJUMP,
		ST_PLAYR_MELEE,
		ST_PLAYR_COMBAT,
		ST_PLAYR_DEAD,
		ST_PLAYR_QUICKSHOT,

		ST_PLAYR_ARMS_IDLE,
		ST_PLAYR_ARMS_MOVE,
		ST_PLAYR_ARMS_MELEE,
		ST_PLAYR_ARMS_PARRY,
		ST_PLAYR_ARMS_BLADE_IDLE,
		ST_PLAYR_ARMS_BLADE_READY,
		ST_PLAYR_ARMS_STUN,
		
		// legs on ground states
		ST_PLAYR_LEGS_IDLE_GROUND,
		ST_PLAYR_LEGS_SLIDE,
		ST_PLAYR_LEGS_MOVE_GROUND,
		ST_PLAYR_LEGS_STUN_GROUND,
		ST_PLAYR_LEGS_BACKPEDAL,

		// legs in air states
		ST_PLAYR_LEGS_IDLE_AIR,
		ST_PLAYR_LEGS_MOVE_AIR,
		ST_PLAYR_LEGS_FALL_AIR,
		ST_PLAYR_LEGS_STUN_AIR,

		ST_NULL,
		
		NumStates
	};

	class EntityStateSystem : TheNomad::GameSystem::GameObject {
		EntityStateSystem() {
		}
		
		private array<json@>@ LoadJSonFile( const string& in modName, const string& in fileName, const string& in name ) {
			string path;
			array<json@> values;
			json@ data;
			
			path = "modules/" + modName + "/DataScripts/" + fileName + ".json";
			
			@data = json();
			if ( !data.ParseFile( path ) ) {
				ConsoleWarning( "failed to load " + name + " info file '" + path + "', skipping.\n" );
				return null;
			}

			if ( !data.get( name, values ) ) {
				ConsoleWarning( fileName + "info file found, but no " + name + " infos found.\n" );
				return null;
			}
			
			return @values;
		}

		private void LoadStatesFromFile( const string& in modName ) {
			array<json@>@ states;

			@states = @LoadJSonFile( modName, "states", "StateInfo" );
			if ( @states is null ) {
				return;
			}

			ConsolePrint( "Got " + states.Count() + " state infos from \"" + modName + "\"\n" );
			for ( uint i = 0; i < states.Count(); i++ ) {
				m_StateInfos.Add( @states[i] );
			}
		}

		private void InitBaseStateCache() {
			m_BaseStateCache.Add( "ST_MOB_IDLE", StateNum::ST_MOB_IDLE );
			m_BaseStateCache.Add( "ST_MOB_SEARCH", StateNum::ST_MOB_SEARCH );
			m_BaseStateCache.Add( "ST_MOB_CHASE", StateNum::ST_MOB_CHASE );
			m_BaseStateCache.Add( "ST_MOB_FIGHT", StateNum::ST_MOB_FIGHT );
			m_BaseStateCache.Add( "ST_MOB_FIGHT_MELEE", StateNum::ST_MOB_FIGHT_MELEE );
			m_BaseStateCache.Add( "ST_MOB_FIGHT_MISSILE", StateNum::ST_MOB_FIGHT_MISSILE );
			m_BaseStateCache.Add( "ST_MOB_FLEE", StateNum::ST_MOB_FLEE );
			m_BaseStateCache.Add( "ST_MOB_DEAD", StateNum::ST_MOB_DEAD );
			m_BaseStateCache.Add( "ST_GFX_ANIMATION", StateNum::ST_GFX_ANIMATION );
			m_BaseStateCache.Add( "ST_PLAYR_IDLE", StateNum::ST_PLAYR_IDLE );
			m_BaseStateCache.Add( "ST_PLAYR_DOUBLEJUMP", StateNum::ST_PLAYR_DOUBLEJUMP );
			m_BaseStateCache.Add( "ST_PLAYR_MELEE", StateNum::ST_PLAYR_MELEE );
			m_BaseStateCache.Add( "ST_PLAYR_COMBAT", StateNum::ST_PLAYR_COMBAT );
			m_BaseStateCache.Add( "ST_PLAYR_DEAD", StateNum::ST_PLAYR_DEAD );
			m_BaseStateCache.Add( "ST_PLAYR_QUICKSHOT", StateNum::ST_PLAYR_QUICKSHOT );
			m_BaseStateCache.Add( "ST_PLAYR_ARMS_IDLE", StateNum::ST_PLAYR_ARMS_IDLE );
			m_BaseStateCache.Add( "ST_PLAYR_ARMS_MOVE", StateNum::ST_PLAYR_ARMS_MOVE );
			m_BaseStateCache.Add( "ST_PLAYR_ARMS_MELEE", StateNum::ST_PLAYR_ARMS_MELEE );
			m_BaseStateCache.Add( "ST_PLAYR_ARMS_PARRY", StateNum::ST_PLAYR_ARMS_PARRY );
			m_BaseStateCache.Add( "ST_PLAYR_ARMS_BLADE_IDLE", StateNum::ST_PLAYR_ARMS_BLADE_IDLE );
			m_BaseStateCache.Add( "ST_PLAYR_ARMS_BLADE_READY", StateNum::ST_PLAYR_ARMS_BLADE_READY );
			m_BaseStateCache.Add( "ST_PLAYR_ARMS_STUN", StateNum::ST_PLAYR_ARMS_STUN );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_IDLE_GROUND", StateNum::ST_PLAYR_LEGS_IDLE_GROUND );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_SLIDE", StateNum::ST_PLAYR_LEGS_SLIDE );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_MOVE_GROUND", StateNum::ST_PLAYR_LEGS_MOVE_GROUND );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_STUN_GROUND", StateNum::ST_PLAYR_LEGS_STUN_GROUND );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_BACKPEDAL", StateNum::ST_PLAYR_LEGS_BACKPEDAL );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_IDLE_AIR", StateNum::ST_PLAYR_LEGS_IDLE_AIR );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_MOVE_AIR", StateNum::ST_PLAYR_LEGS_MOVE_AIR );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_FALL_AIR", StateNum::ST_PLAYR_LEGS_FALL_AIR );
			m_BaseStateCache.Add( "ST_PLAYR_LEGS_STUN_AIR", StateNum::ST_PLAYR_LEGS_STUN_AIR );
		}
		
		const string& GetName() const {
			return "EntityStateSystem";
		}
		void OnInit() {
			InitBaseStateCache();

			TheNomad::Engine::CommandSystem::CmdManager.AddCommand(
				TheNomad::Engine::CommandSystem::CommandFunc( @this.ListStateCache_f ), "sgame.state_cache", false
			);
		}
		void OnShutdown() {
		}
		void OnLevelStart() {
			uint i;
			
			ConsolePrint( "Loading state data...\n" );
			
			for ( i = 0; i < sgame_ModList.Count(); i++ ) {
				LoadStatesFromFile( sgame_ModList[i] );
			}
			
			for ( i = 0; i < m_StateInfos.Count(); i++ ) {
				EntityState@ state = EntityState();

				if ( !state.Load( @m_StateInfos[i] ) ) {
					ConsoleWarning( "failed to load state info at " + i + "\n" );
					continue;
				}

				m_States.Add( @state );
				m_StateCache.Add( state.GetName(), @state );
			}
			m_StateInfos.Clear();

			TheNomad::Engine::CmdExecuteCommand( "sgame.state_cache\n" );
		}
		void OnLevelEnd() {
			m_StateCache.Clear();
			m_States.Clear();
		}
		void OnSave() const {
		}
		void OnLoad() {
		}
		void OnRenderScene() {
		}
		void OnRunTic() {
		}
		void OnPlayerDeath( int ) {
		}
		void OnCheckpointPassed( uint ) {
		}

		private void ListStateCache_f() {
			ConsolePrint( "\n" );
			ConsolePrint( "ENTITY STATE CACHE\n" );
			ConsolePrint( "----------------------------------------\n" );
			for ( uint i = 0; i < m_States.Count(); i++ ) {
				m_States[i].Log();
				ConsolePrint( "\n" );
			}
			ConsolePrint( "----------------------------------------\n" );
		}
		
		EntityState@ GetStateById( const string& in name ) {
			EntityState@ state = null;
			m_StateCache.TryGetValue( name, @state );
			return @state;
		}
		EntityState@ GetStateForNum( uint nIndex ) {
			for ( uint i = 0; i < m_States.Count(); i++ ) {
				if ( m_States[i].GetID() == nIndex ) {
					return @m_States[i];
				}
			}
			return @m_NullState;
		}

		dictionary@ GetBaseStateCache() {
			return @m_BaseStateCache;
		}
		
		private array<json@> m_StateInfos;
		private dictionary m_StateCache;
		private array<EntityState@> m_States;
		private EntityState m_NullState;

		// technically const
		private dictionary m_BaseStateCache;
	};

	EntityStateSystem@ StateManager;
};