#include "SGame/EntityObject.as"
#include "SGame/InfoSystem/AttackInfo.as"
#include "SGame/InfoSystem/MobInfo.as"
#include "SGame/InfoSystem/ItemInfo.as"
#include "SGame/InfoSystem/InfoDataManager.as"
#include "SGame/ItemObject.as"
#include "SGame/WeaponObject.as"
#include "SGame/MobObject.as"
#include "SGame/PlayrObject.as"

namespace TheNomad::SGame {
    enum CauseOfDeath {
		Cod_Unknown,
		Cod_Bullet,
		Cod_Imploded,
		Cod_Exploded,
		Cod_Suicide,
		Cod_Telefrag,
		Cod_Punch,
		Cod_Falling
	};
	
	enum AttackEffect {
		Knockback = 0,
		Stunned,
		Bleeding,
		Blinded,
		
		None
	};
	
	enum EntityFlags {
		// DUH.
		Dead      = 0x00000001,
		// can't take damage
		Invul     = 0x00000002,
		// doesn't get drawn
		Invis     = 0x00000004,
		// doesn't get respawned in Nomad or greater difficulties
		PermaDead = 0x00000008,
		// will it bleed?
		Killable  = 0x00000010,

		None      = 0x00000000
	};

    class EntitySystem : TheNomad::GameSystem::GameObject {
		EntitySystem() {
			TheNomad::Engine::CommandSystem::CmdManager.AddCommand(
				TheNomad::Engine::CommandSystem::CommandFunc( @this.SetPlayerPosition_f ), "sgame.set_player_position", true
			);
			TheNomad::Engine::CommandSystem::CmdManager.AddCommand(
				TheNomad::Engine::CommandSystem::CommandFunc( @this.DamagePlayer_f ), "sgame.damage_player", true
			);
			TheNomad::Engine::CommandSystem::CmdManager.AddCommand(
				TheNomad::Engine::CommandSystem::CommandFunc( @this.PrintPlayerState_f ), "sgame.player_state", true
			);
			TheNomad::Engine::CommandSystem::CmdManager.AddCommand(
				TheNomad::Engine::CommandSystem::CommandFunc( @this.GivePlayerWeapon_f ), "sgame.give_player_weapon", true
			);
			TheNomad::Engine::CommandSystem::CmdManager.AddCommand(
				TheNomad::Engine::CommandSystem::CommandFunc( @this.GivePlayerItem_f ), "sgame.give_player_item", true
			);
			TheNomad::Engine::CommandSystem::CmdManager.AddCommand(
				TheNomad::Engine::CommandSystem::CommandFunc( @this.KnockbackPlayer_f ), "sgame.knockback_player", true
			);
		}
		
		void OnInit() {
		}
		void OnShutdown() {
		}
		const string& GetName() const {
			return "EntityManager";
		}
		void OnLoad() {
			EntityObject@ ent;

			{
				TheNomad::GameSystem::SaveSystem::LoadSection section( GetName() );

				if ( !section.Found() ) {
					ConsoleWarning( "EntitySystem::OnLoad: no save section for entity data found\n" );
					return;
				}

				const uint numEntities = section.LoadUInt( "NumEntities" );
				DebugPrint( "Loading entity data from save file...\n" );

				if ( numEntities == 0 ) {
					return;
				}
				m_EntityList.Resize( numEntities );
			}

			for ( uint i = 0; i < m_EntityList.Count(); i++ ) {
				TheNomad::GameSystem::SaveSystem::LoadSection data( "EntityData_" + i );
				if ( !data.Found() ) {
					GameError( "EntitySystem::OnLoad: save section \"EntityData_" + i + "\" not found" );
				}
				@ent = @m_EntityList[i];
				ent = EntityObject();

				switch ( TheNomad::GameSystem::EntityType( data.LoadUInt( "type" ) ) ) {
				case TheNomad::GameSystem::EntityType::Playr: {
					if ( !cast<PlayrObject@>( @ent ).Load( data ) ) {
						GameError( "EntitySystem::OnLoad: failed to load player data" );
					}
					break; }
				case TheNomad::GameSystem::EntityType::Mob: {
					if ( !cast<MobObject@>( @ent ).Load( data ) ) {
						GameError( "EntitySystem::OnLoad: failed to load mob data" );
					}
					break; }
				case TheNomad::GameSystem::EntityType::Item: {
					if ( !cast<ItemObject@>( @ent ).Load( data ) ) {
						GameError( "EntitySystem::OnLoad: failed to load item data" );
					}
					break; }
				case TheNomad::GameSystem::EntityType::Weapon: {
					if ( !cast<WeaponObject@>( @ent ).Load( data ) ) {
						GameError( "EntitySystem::OnLoad: failed to weapon data" );
					}
					break; }
				default:
					GameError( "EntityObject::OnLoad: invalid entity type" );
					break;
				};
			}

			DebugPrint( "Loaded " + m_EntityList.Count() + " entities.\n" );
		}
		void OnSave() const {
			DebugPrint( "Saving entity data...\n" );

			{
				TheNomad::GameSystem::SaveSystem::SaveSection section( GetName() );
				section.SaveUInt( "NumEntities", m_EntityList.Count() );
			}

			for ( uint i = 0; i < m_EntityList.Count(); i++ ) {
				TheNomad::GameSystem::SaveSystem::SaveSection data( "EntityData_" + i );
				m_EntityList[i].Save( data );
			}
		}
		void OnRenderScene() {
			EntityObject@ ent;

			for ( uint i = 0; i < m_EntityList.Count(); i++ ) {
				@ent = @m_EntityList[i];

				// draw
				ent.Draw();
			}
		}
		void OnRunTic() {
			EntityObject@ ent;
			
			for ( uint i = 0; i < m_EntityList.Count(); i++ ) {
				@ent = @m_EntityList[i];

				if ( ent.CheckFlags( EntityFlags::Dead ) ) {
					if ( ( sgame_Difficulty.GetInt() > TheNomad::GameSystem::GameDifficulty::Hard
						&& ent.GetType() == TheNomad::GameSystem::EntityType::Mob )
						|| ent.GetType() == TheNomad::GameSystem::EntityType::Playr )
					{
						DeadThink( @ent );
					}
					else {
						// remove it
						if ( ent.GetType() == TheNomad::GameSystem::EntityType::Item ) {
							// unlink the item
							RemoveItem( cast<ItemObject@>( @ent ) );
						}
						m_EntityList.RemoveAt( i );
					}
					continue;
				}

				if ( @ent.GetState() is null ) {
//					DebugPrint( "EntitySystem::OnRunTic: null entity state\n" );
					continue;
				} else {
					ent.SetState( @ent.GetState().Run() );
				}

				switch ( ent.GetType() ) {
				case TheNomad::GameSystem::EntityType::Playr: {
					cast<PlayrObject@>( @ent ).Think();
					break; }
				case TheNomad::GameSystem::EntityType::Mob: {
					cast<MobObject@>( @ent ).Think();
					break; }
				case TheNomad::GameSystem::EntityType::Bot:
					break;
				case TheNomad::GameSystem::EntityType::Item:
					cast<ItemObject@>( @ent ).Think();
					break;
				case TheNomad::GameSystem::EntityType::Weapon:
					cast<WeaponObject@>( @ent ).Think();
					break;
				case TheNomad::GameSystem::EntityType::Wall:
					GameError( "WALLS DON'T THINK, THEY ACT" );
					break;
				default:
					GameError( "EntityManager::OnRunTic: invalid entity type " + formatUInt( uint( ent.GetType() ) ) );
				};

				// update engine data
				ent.GetLink().Update();
				
//				if ( m_EntityList[i].GetState().Done() ) {
//					m_EntityList[i].SetState( m_EntityList[i].GetState().Cycle() );
//					continue;
//				}
			}
		}
		void OnLevelStart() {
			vec2 size;

			array<MapSpawn@>@ spawns = @LevelManager.GetMapData().GetCheckpoints()[0].m_Spawns;

			DebugPrint( "Initializing entities...\n" );

			for ( uint i = 0; i < spawns.Count(); i++ ) {
				EntityManager.Spawn( spawns[i].m_nEntityType, spawns[i].m_nEntityId,
					vec3( float( spawns[i].m_Origin.x ), float( spawns[i].m_Origin.y ), float( spawns[i].m_Origin.z ) ),
					vec2( 0.0f, 0.0f ) );
			}
			LevelManager.GetMapData().GetCheckpoints()[0].m_bPassed = true;

			DebugPrint( formatUInt( m_EntityList.Count() ) + " total entities.\n" );
		}
		void OnLevelEnd() {
			// clear all level locals
			m_EntityList.Clear();
			@m_ActivePlayer = null;
		}
		void OnPlayerDeath( int ) {
		}
		void OnCheckpointPassed( uint ) {
		}
		
		private EntityObject@ AllocEntity( TheNomad::GameSystem::EntityType type, uint id, const vec3& in origin, const vec2& in size ) {
			EntityObject@ ent;

			switch ( type ) {
			case TheNomad::GameSystem::EntityType::Playr:
				@ent = PlayrObject();
				ent.Init( type, id, origin );
				cast<PlayrObject@>( @ent ).Spawn( id, origin );
				break;
			case TheNomad::GameSystem::EntityType::Mob:
				@ent = MobObject();
				cast<MobObject@>( @ent ).Spawn( id, origin );
				break;
			case TheNomad::GameSystem::EntityType::Bot:
				break;
			case TheNomad::GameSystem::EntityType::Item:
				@ent = ItemObject();
				cast<ItemObject@>( @ent ).Spawn( id, origin );
				break;
			case TheNomad::GameSystem::EntityType::Weapon:
				@ent = WeaponObject();
				cast<WeaponObject@>( @ent ).Spawn( id, origin );
				break;
			case TheNomad::GameSystem::EntityType::Wall:
				GameError( "WALLS DON'T THINK, THEY ACT" );
				break;
			default:
				GameError( "EntityManager::Spawn: invalid entity type " + id );
			};

			ent.GetBounds().m_nWidth = size.x;
			ent.GetBounds().m_nHeight = size.y;
			ent.GetBounds().MakeBounds( origin );

			m_EntityList.Add( @ent );
			
			return @ent;
		}
		
		EntityObject@ Spawn( TheNomad::GameSystem::EntityType type, int id, const vec3& in origin, const vec2& in size ) {
			return @AllocEntity( type, id, origin, size );
		}
		
		void DeadThink( EntityObject@ ent ) {
			if ( ent.GetType() == TheNomad::GameSystem::EntityType::Mob ) {
				if ( sgame_Difficulty.GetInt() < uint( TheNomad::GameSystem::GameDifficulty::VeryHard )
					|| sgame_NoRespawningMobs.GetInt() == 1 || ( cast<MobObject@>( @ent ).GetMFlags() & InfoSystem::MobFlags::PermaDead ) != 0 )
				{
					return; // no respawning for this one
				} else {
					// TODO: add respawn code here
				}
			} else if ( ent.GetType() == TheNomad::GameSystem::EntityType::Playr ) {
				PlayrObject@ obj = cast<PlayrObject@>( @ent );
				
				// is hellbreaker available?
				if ( sgame_HellbreakerOn.GetInt() == 1 && sgame_HellbreakerActive.GetInt() == 0 ) {
					// ensure there's no recursion
					TheNomad::Engine::CvarSet( "sgame_HellbreakerActive", "1" );
					return; // startup the hellbreak
				}
				
				GlobalState = GameState::DeathMenu;
				ent.SetState( StateNum::ST_PLAYR_DEAD ); // play the death animation
			}
		}
		
		const EntityObject@ GetEntityForNum( uint nIndex ) const {
			return @m_EntityList[ nIndex ];
		}
		EntityObject@ GetEntityForNum( uint nIndex ) {
			return @m_EntityList[ nIndex ];
		}
		const array<EntityObject@>@ GetEntities() const {
			return @m_EntityList;
		}
		array<EntityObject@>@ GetEntities() {
			return @m_EntityList;
		}
		uint NumEntities() const {
			return m_EntityList.Count();
		}

		private bool BoundsIntersectLine( const vec3& in start, const vec3& in end, const TheNomad::GameSystem::BBox& in bounds ) {
			float minX = start.x;
			float maxX = end.x;
			
			if ( start.x > end.x ) {
				minX = end.x;
				maxX = start.x;
			}
			if ( maxX > bounds.m_Maxs.x ) {
				maxX = bounds.m_Maxs.x;
			}
			if ( minX < bounds.m_Mins.x ) {
				minX = bounds.m_Mins.x;
			}
			if ( minX > maxX ) {
				return false;
			}

			float minY = start.y;
			float maxY = end.y;

			const float deltaX = end.x - start.x;

			if ( abs( deltaX ) > 0.0000001f ) {
				float a = ( end.y - start.y ) / deltaX;
				float b = start.y - a * start.x;
				minY = a * minX + b;
				maxY = a * maxX + b;
			}

			if ( minY > maxY ) {
				Util::Swap( maxY, maxX );
			}
			if ( maxY > bounds.m_Maxs.y ) {
				maxY = bounds.m_Maxs.y;
			}
			if ( minY < bounds.m_Mins.y ) {
				minY = bounds.m_Mins.y;
			}
			if ( minY > maxY ) {
				return false;
			}
			return true;
		}

		void ApplyKnockback( EntityObject@ ent, const vec3& in dir ) {
			ent.SetVelocity( dir );
			ent.SetDebuff( AttackEffect::Knockback );
		}

		bool EntityIntersectsLine( const vec3& in origin, const vec3& in end ) {
			for ( uint i = 0; i < m_EntityList.Count(); i++ ) {
				if ( BoundsIntersectLine( origin, end, m_EntityList[i].GetBounds() ) ) {
					return true;
				}
			}
			return false;
		}

		void SpawnProjectile( const vec3& in origin, float angle, InfoSystem::AttackInfo@ info, const vec2& in size ) {
			EntityObject@ obj = @Spawn( TheNomad::GameSystem::EntityType::Weapon, info.projectileEntityType, origin, size );
			obj.SetProjectile( true );
		}

		private void GenObituary( EntityObject@ attacker, EntityObject@ target, InfoSystem::AttackInfo@ info ) {
			if ( target.GetType() != TheNomad::GameSystem::EntityType::Playr ) {
				// unless it's the player, we don't care about the death
				return;
			}
			
			string message = cast<PlayrObject@>( @target ).GetName();
			
			if ( @attacker is null ) {
				if ( target.GetVelocity() > Vec3Origin || target.GetVelocity() > Vec3Origin ) {
					// killed by impact
					if ( target.GetVelocity().z != 0.0f ) {
						message += " thought they could fly";
					}  else {
						message += " was met by the reality of physics";
					}
				} else {
					// player was killed by unknown reasons
					message += " was killed by strange and mysterious forces beyond our control...";
				}
			}
			else {
				// actual death
				
			}
			
			ConsolePrint( message + "\n" );
		}
		
		void KillEntity( EntityObject@ attacker, EntityObject@ target ) {
			if ( attacker.GetType() == TheNomad::GameSystem::EntityType::Mob ) {
				// write an obituary for the player
				GenObituary( @attacker, @target, @cast<MobObject@>( @attacker ).GetCurrentAttack() );
			}
			// TODO: make the enemies nearby declare the infighting individual a traitor
			if ( target.GetType() == TheNomad::GameSystem::EntityType::Mob ) {
				MobObject@ mob = cast<MobObject@>( @target );
				
				// respawn mobs on VeryHard
				if ( uint( sgame_Difficulty.GetInt() ) >= uint( TheNomad::GameSystem::GameDifficulty::VeryHard )
					&& sgame_NoRespawningMobs.GetInt() != 1 && ( uint( mob.GetMFlags() ) & InfoSystem::MobFlags::PermaDead ) == 0 )
				{
					// ST_MOB_DEAD only used with respawning mobs
					target.SetState( StateNum::ST_MOB_DEAD );
				}
				else {
					@target.prev.next = @target.next;
					@target.next.prev = @target.prev;
				}
			}
			else if ( target.GetType() == TheNomad::GameSystem::EntityType::Playr ) {
				target.SetState( StateNum::ST_PLAYR_DEAD );
			}
		}
		void ApplyEntityEffect( EntityObject@ attacker, EntityObject@ target, AttackEffect effect ) {
			target.SetDebuff( effect );
		}
		
		//
		// DamageEntity: entity v entity
		// NOTE: damage is only ever used when calling from a WeaponObject
		//
		void DamageEntity( EntityObject@ attacker, EntityObject@ target, float damage = 1.0f ) {
			switch ( attacker.GetType() ) {
			case TheNomad::GameSystem::EntityType::Mob: {
				target.Damage( @attacker, cast<MobObject@>( @attacker ).GetCurrentAttack().damage );
				break; }
			case TheNomad::GameSystem::EntityType::Playr: {
				target.Damage( @attacker, damage );
				if ( target.GetType() == TheNomad::GameSystem::EntityType::Bot ) {
					// TODO: calculate collateral damage here
				}
				// any other entity is just an inanimate object
				// TODO: add explosive objects
				
				break; }
			default:
				GameError( "EntitySystem::Damage: invalid entity type " + uint( attacker.GetType() ) );
			};
		}
		void DamageEntity( EntityObject@ attacker, TheNomad::GameSystem::RayCast@ rayCast, float damage = 1.0f ) {
			if ( @rayCast is null ) {
				return;
			} else if ( rayCast.m_nEntityNumber == ENTITYNUM_INVALID || rayCast.m_nEntityNumber == ENTITYNUM_WALL ) {
				return; // got nothing
			}

			DamageEntity( @attacker, @m_EntityList[ rayCast.m_nEntityNumber ], damage );
		}

		void SetActivePlayer( PlayrObject@ player ) {
			@m_ActivePlayer = @player;
		}
		PlayrObject@ GetActivePlayer() {
			return @m_ActivePlayer;
		}
		const PlayrObject@ GetActivePlayer() const {
			return @m_ActivePlayer;
		}

		void RemoveItem( ItemObject@ item ) {
			@item.prev.next = @item.next;
			@item.next.prev = @item.prev;
		}
		ItemObject@ FindItemInBounds( const TheNomad::GameSystem::BBox& in bounds ) {
			ItemObject@ item;
			for ( @item = cast<ItemObject@>( @m_ActiveItems.next ); @item !is @m_ActiveItems; @item = cast<ItemObject>( @item.next ) ) {
				if ( Util::BoundsIntersect( bounds, item.GetBounds() ) ) {
					return @item;
				}
			}
			return null;
		}
		ItemObject@ AddItem( uint type, const vec3& in origin ) {
			ItemObject@ item;

			@item = cast<ItemObject@>( @Spawn( TheNomad::GameSystem::EntityType::Item, type, origin, vec2( 1.0f ) ) );
			@m_ActiveItems.prev.next = @item;
			@item.prev = @m_ActiveItems.prev;
			@item.next = @m_ActiveItems;

			return item;
		}
		
		private array<EntityObject@> m_EntityList;
		private ItemObject m_ActiveItems;
		private PlayrObject@ m_ActivePlayer = null;

		//==============================================================================
		// Commands
		//
		
		//
		// effects
		//
		
		void Effect_EntityBleed_f() {
			// sgame.effect_entity_bleed <attacker_num>
		}

		void Effect_EntityKnockback_f() {
			// sgame.effect_entity_knockback <attacker_num> <target_num>

			EntityObject@ target = null;
			EntityObject@ attacker = null;
			const uint attackerNum = Convert().ToUInt( Engine::CmdArgv( 1 ) );
			const uint targetNum = Convert().ToUInt( Engine::CmdArgv( 2 ) );

			@attacker = GetEntityForNum( attackerNum );
			if ( @attacker is null ) {
				GameError( "Effect_EntityKnockback_f triggered on null attacker" );
			}

			@target = GetEntityForNum( targetNum );
			if ( @target is null ) {
				ConsoleWarning( "Effect_EntityKnockback_f triggered but no target\n" );
				return;
			}

			ApplyEntityEffect( @attacker, @target, AttackEffect::Knockback );
		}

		void Effect_EntityImmolate_f() {
			// sgame.effect_entity_knockback <attacker_num>
		}

		void Effect_EntityStun_f() {
			// sgame.effect_entity_stun <attacker_num>

			EntityObject@ target, attacker;
			const uint attackerNum = Convert().ToUInt( Engine::CmdArgv( 1 ) );

			@attacker = GetEntityForNum( attackerNum );
			if ( attacker is null ) {
				GameError( "Effect_EntityStun_f triggered on null attacker" );
			}

			@target = cast<MobObject@>( @attacker ).GetTarget();
			if ( target is null ) {
				ConsoleWarning( "Effect_EntityStun_f triggered but no target\n" );
				return;
			}

			ApplyEntityEffect( attacker, target, AttackEffect::Stunned );
		}

		//
		// status logs
		//

		void PrintPlayerState_f() {
			ConsolePrint( "\n" );
			ConsolePrint( "[PLAYER STATE]\n" );
			ConsolePrint( "Origin: [ " + m_ActivePlayer.GetOrigin().x + ", " + m_ActivePlayer.GetOrigin().y + " ]\n" );
			ConsolePrint( "LegState: " + m_ActivePlayer.GetLegState().GetName() + "\n" );
			if ( @m_ActivePlayer.GetLegState().GetAnimation() is null ) {
				ConsolePrint( "LegAnimation: didn't load\n" );
			}
		}
		private void ListActiveItems() {
//			ConsolePrint( "Active Game Items:\n" );
//			for ( uint i = 0; i < m_ItemList.Count(); i++ ) {
//				msg = "(";
//				msg += i;
//				msg += ") ";
//				msg += cast<const ItemInfo@>( @m_ItemList[i].GetInfo() ).name;
//				msg += " ";
//				msg += "[ ";
//				msg += m_ItemList[i].GetOrigin().x;
//				msg += ", ";
//				msg += m_ItemList[i].GetOrigin().y;
//				msg += ", ";
//				msg += m_ItemList[i].GetOrigin().z;
//				msg += " ]\n";
//				ConsolePrint( msg );
//			}
		}

		//
		// developer commands
		//

		void KnockbackPlayer_f() {
			const float amountX = Convert().ToFloat( TheNomad::Engine::CmdArgv( 1 ) );
			const float amountY = Convert().ToFloat( TheNomad::Engine::CmdArgv( 2 ) );

			ApplyKnockback( cast<EntityObject>( @m_ActivePlayer ), vec3( amountX, amountY, 0.0f ) );
		}

		void GivePlayerItem_f() {
			
		}
		void GivePlayerWeapon_f() {
			
		}
		void SetPlayerPosition_f() {
			const float x = Convert().ToFloat( TheNomad::Engine::CmdArgv( 1 ) );
			const float y = Convert().ToFloat( TheNomad::Engine::CmdArgv( 2 ) );

			m_ActivePlayer.GetLink().m_Origin = vec3( x, y, 0.0f );
		}
		void DamagePlayer_f() {
			const float damage = Convert().ToFloat( TheNomad::Engine::CmdArgv( 1 ) );

			m_ActivePlayer.Damage( cast<EntityObject@>( @m_ActivePlayer ), damage );
		}
	};

	EntitySystem@ EntityManager = null;
};