#include "Engine/SoundSystem/SoundFrameData.as"
#include "SGame/KeyBind.as"
#include "SGame/PlayerSystem/PlayerSystem.as"

namespace TheNomad::SGame {
	const float HEAL_MULT_BASE = 0.001f;

	const uint[] sgame_WeaponModeList = {
		uint( InfoSystem::WeaponProperty::OneHandedBlade | InfoSystem::WeaponProperty::TwoHandedBlade ),
		uint( InfoSystem::WeaponProperty::OneHandedBlunt | InfoSystem::WeaponProperty::TwoHandedBlunt ),
		uint( InfoSystem::WeaponProperty::OneHandedPolearm | InfoSystem::WeaponProperty::TwoHandedPolearm ),
		uint( InfoSystem::WeaponProperty::OneHandedSideFirearm | InfoSystem::WeaponProperty::OneHandedPrimFirearm
			| InfoSystem::WeaponProperty::TwoHandedSideFirearm | InfoSystem::WeaponProperty::TwoHandedPrimFirearm ),
	};
	
    class PlayrObject : EntityObject {
		PlayrObject() {
		}
		
		/*
		private bool TryOneHanded( HandMode hand, const WeaponObject@ handWeapon, const WeaponObject@ otherWeapon ) {
			if ( hand != HandMode::Empty ) {
				if ( ( handWeapon.IsTwoHanded() && handWeapon.IsOneHanded() ) && @otherWeapon is @handWeapon ) {
					// being used by both arms, switch to one-handed
					return true;
				}
			}
			return false;
		}
		
		private WeaponObject@ GetEmptyHand() {
			if ( TryOneHanded( m_LeftHandMode, @m_LeftHandWeapon, @m_RightHandWeapon ) ) {
				m_LeftHandMode = HandMode::OneHanded;
				m_RightHandMode = HandMode::Empty;
				@m_RightHandWeapon = @m_RightArm;
				return @m_RightHandWeapon;
			}
			else if ( TryOneHanded( m_RightHandMode, @m_RightHandWeapon, @m_LeftHandWeapon ) ) {
				m_RightHandMode = HandMode::OneHanded;
				m_LeftHandMode = HandMode::Empty;
				@m_LeftHandWeapon = @m_LeftArm;
				return @m_LeftHandWeapon;
			}
		}
		*/

		bool opCmp( const PlayrObject& in other ) const {
			return @this == @other;
		}
		
		void SwitchWeaponWielding( InfoSystem::WeaponProperty& in hand, InfoSystem::WeaponProperty& in otherHand,
			WeaponObject@ weapon, WeaponObject@ other )
		{
			switch ( InfoSystem::WeaponProperty( uint( hand ) & InfoSystem::WeaponProperty::IsOneHanded ) ) {
			case InfoSystem::WeaponProperty::OneHandedPolearm:
			case InfoSystem::WeaponProperty::OneHandedBlunt:
			case InfoSystem::WeaponProperty::OneHandedBlade:
			case InfoSystem::WeaponProperty::OneHandedSideFirearm:
			case InfoSystem::WeaponProperty::OneHandedPrimFirearm:
				if ( weapon.IsTwoHanded() ) {
					// set to two-handed
					hand = InfoSystem::WeaponProperty( uint( hand ) & ~uint( InfoSystem::WeaponProperty::IsOneHanded ) );
					otherHand = hand;
					@other = @weapon;
					return;
				}
				break;
			case InfoSystem::WeaponProperty::None:
			default:
				// nothing in that hand
				break;
			};
			
			switch ( InfoSystem::WeaponProperty( uint( hand ) & InfoSystem::WeaponProperty::IsTwoHanded ) ) {
			case InfoSystem::WeaponProperty::TwoHandedPolearm:
			case InfoSystem::WeaponProperty::TwoHandedBlunt:
			case InfoSystem::WeaponProperty::TwoHandedBlade:
			case InfoSystem::WeaponProperty::TwoHandedSideFirearm:
			case InfoSystem::WeaponProperty::TwoHandedPrimFirearm:
				if ( weapon.IsOneHanded() ) {
					// set to one-handed
					hand = InfoSystem::WeaponProperty( uint( hand ) & ~InfoSystem::WeaponProperty::IsTwoHanded );
					@other = null;
					return;
				}
				break;
			case InfoSystem::WeaponProperty::None:
			default:
				// nothing in that hand
				break;
			};
			
			// if we're here, its most likely that the player is using a two-handed weapon that
			// can't be wielded one-handed or they're duel wielding something
		}
		void SwitchWeaponMode( InfoSystem::WeaponProperty& in hand, const WeaponObject@ weapon ) {
			// weapon mode order (default)
			// blade -> blunt -> polearm -> firearm
			
			// the weapon mode list doesn't contain hand bits
			const uint handBits = ( uint( hand ) & uint( InfoSystem::WeaponProperty::IsTwoHanded ) ) |
				( uint( hand ) & uint( InfoSystem::WeaponProperty::IsOneHanded ) );
			
			// find the next most suitable mode
			for ( uint i = 0; i < sgame_WeaponModeList.Count(); i++ ) {
				if ( ( uint( weapon.GetProperties() ) & sgame_WeaponModeList[i] ) != 0 ) {
					hand = InfoSystem::WeaponProperty( ( uint( weapon.GetProperties() ) & sgame_WeaponModeList[i] ) | handBits );
				}
			}
		}
		InfoSystem::WeaponProperty GetCurrentWeaponMode() {
			switch ( m_nHandsUsed ) {
			case 0:
				return m_LeftHandMode;
			case 1:
			case 2:
				return m_RightHandMode;
			};
			return InfoSystem::WeaponProperty::None;
		}

		void SetPlayerIndex( uint nIndex ) {
			m_nControllerIndex = nIndex;
		}
		uint GetPlayerIndex() const {
			return m_nControllerIndex;
		}

		void AddItem( ItemObject@ item ) {
		}
		
		void DrawEmoteWheel() {
//			if ( !m_bEmoteWheelActive ) {
//				DebugPrint( "DrawEmoteWheel: called when inactive.\n" );
//				return;
//			}
			
		}

		bool IsSliding() const {
			return m_bSliding;
		}
		void SetSliding( bool bSliding ) {
			m_bSliding = bSliding;
		}
		uint64 GetTimeSinceLastSlide() const {
			return TheNomad::Engine::System::Milliseconds() - m_nTimeSinceSlide;
		}
		void ResetSlide() {
			m_nTimeSinceSlide = TheNomad::Engine::System::Milliseconds();
		}
		
		bool IsCrouching() const {
			return m_bCrouching;
		}
		void SetCrouching( bool bCrouching ) {
			m_bCrouching = bCrouching;
		}

		bool IsDoubleJumping() const {
			return m_State.GetID() == StateNum::ST_PLAYR_DOUBLEJUMP;
		}

		bool IsDashing() const {
			return m_bDashing;
		}
		void SetDashing( bool bDashing ) {
			m_bDashing = bDashing;
		}
		uint64 GetTimeSinceLastDash() const {
			return TheNomad::Engine::System::Milliseconds() - m_nTimeSinceDash;
		}
		void SetTimeSinceLastDash( uint64 time ) {
			m_nTimeSinceDash = time;
		}
		void ResetDash() {
			m_nTimeSinceDash = TheNomad::Engine::System::Milliseconds();
		}

		void SetUsingWeapon( bool bUseWeapon ) {
			m_bUseWeapon = bUseWeapon;
		}
		void SetUsingWeaponAlt( bool bUseAltWeapon ) {
			m_bAltUseWeapon = bUseAltWeapon;
		}
		void SetLeftHandMode( InfoSystem::WeaponProperty weaponProps ) {
			m_LeftHandMode = weaponProps;
		}
		void SetRightHandMode( InfoSystem::WeaponProperty weaponProps ) {
			m_RightHandMode = weaponProps;
		}
		void SetLeftHandWeapon( WeaponObject@ weapon ) {
			@m_LeftHandWeapon = @weapon;
		}
		void SetRightHandWeapon( WeaponObject@ weapon ) {
			@m_RightHandWeapon = @weapon;
		}
		void SetHandsUsed( int nHandsUsed ) {
			m_nHandsUsed = nHandsUsed;
		}
		void SetParryBoxWidth( float nParryBoxWidth ) {
			m_nParryBoxWidth = nParryBoxWidth;
		}
		void SetCurrentWeapon( int nCurrentWeapon ) {
			m_CurrentWeapon = nCurrentWeapon;
		}

		bool UsingWeapon() const {
			return m_bUseWeapon;
		}
		bool UsingWeaponAlt() const {
			return m_bAltUseWeapon;
		}
		InfoSystem::WeaponProperty GetLeftHandMode() const {
			return m_LeftHandMode;
		}
		InfoSystem::WeaponProperty GetRightHandMode() const {
			return m_RightHandMode;
		}
		const WeaponObject@ GetLeftHandWeapon() const {
			return @m_LeftHandWeapon;
		}
		const WeaponObject@ GetRightHandWeapon() const {
			return @m_RightHandWeapon;
		}
		WeaponObject@ GetLeftHandWeapon() {
			return @m_LeftHandWeapon;
		}
		WeaponObject@ GetRightHandWeapon() {
			return @m_RightHandWeapon;
		}
		int GetHandsUsed() const {
			return m_nHandsUsed;
		}
		float GetParryBoxWidth() const {
			return m_nParryBoxWidth;
		}
		int GetCurrentWeaponIndex() const {
			return m_CurrentWeapon;
		}

		void SetLegsFacing( int facing ) {
			m_LegsFacing = facing;
		}
		void SetArmsFacing( int facing ) {
			m_ArmsFacing = facing;
		}
		int GetLegsFacing() const {
			return m_LegsFacing;
		}
		int GetArmsFacing() const {
			return m_ArmsFacing;
		}

		EntityState@ GetLegState() {
			return @m_LegState;
		}
		EntityState@ GetArmState() {
			return @m_ArmState;
		}

		const EntityState@ GetLegState() const {
			return @m_LegState;
		}
		const EntityState@ GetArmState() const {
			return @m_ArmState;
		}

		bool Load( const TheNomad::GameSystem::SaveSystem::LoadSection& in section ) override {
			string skin;
			array<InventoryStack@>@ slots;

			LoadBase( section );

			section.LoadString( "playerSkin", skin );
			TheNomad::Engine::CvarSet( "skin", skin );
			m_nHealth = section.LoadFloat( "health" );
			m_nRage = section.LoadFloat( "rage" );

			m_CurrentWeapon = section.LoadInt( "currentWeapon" );
			@m_LeftHandWeapon = @m_WeaponSlots[ section.LoadUInt( "leftHandSlot" ) ];
			@m_RightHandWeapon = @m_WeaponSlots[ section.LoadUInt( "rightHandSlot" ) ];
			m_LeftHandMode = InfoSystem::WeaponProperty( section.LoadUInt( "leftHandMode" ) );
			m_RightHandMode = InfoSystem::WeaponProperty( section.LoadUInt( "rightHandMode" ) );
			m_nHandsUsed = section.LoadInt( "handsUsed" );

			for ( uint i = 0; i < m_WeaponSlots.Count(); i++ ) {
				m_WeaponSlots[i].Load( section );
			}

			@slots = @m_Inventory.GetSlots();
			for ( uint i = 0; i < slots.Count(); i++ ) {
				const uint count = section.LoadUInt( "itemSlotCount_" + i );

				if ( count > 0 ) {
					const uint type = section.LoadUInt( "itemSlotType_" + i );

					if ( @slots[i] is null ) {
						@slots[i] = InventoryStack( @InfoSystem::InfoManager.GetItemInfo( type ) );
					}
				}
			}

			return true;
		}
		void Save( const TheNomad::GameSystem::SaveSystem::SaveSection& in section ) const {
			uint index;
			const array<InventoryStack@>@ slots;

			SaveBase( section );

			section.SaveString( "playerSkin", TheNomad::Engine::CvarVariableString( "skin" ) );
			section.SaveFloat( "health", m_nHealth );
			section.SaveFloat( "rage", m_nRage );

			section.SaveInt( "currentWeapon", m_CurrentWeapon );

			for ( index = 0; index < m_WeaponSlots.Count(); index++ ) {
				if ( @m_LeftHandWeapon is @m_WeaponSlots[ index ] ) {
					section.SaveUInt( "leftHandSlot", index );
					break;
				}
			}
			for ( index = 0; index < m_WeaponSlots.Count(); index++ ) {
				if ( @m_RightHandWeapon is @m_WeaponSlots[ index ] ) {
					section.SaveUInt( "rightHandSlot", index );
					break;
				}
			}
			section.SaveUInt( "leftHandMode", uint( m_LeftHandMode ) );
			section.SaveUInt( "rightHandMode", uint( m_RightHandMode ) );
			section.SaveInt( "handsUsed", m_nHandsUsed );

			for ( uint i = 0; i < m_WeaponSlots.Count(); i++ ) {
				m_WeaponSlots[i].Save( section );
			}

			@slots = @m_Inventory.GetSlots();
			for ( uint i = 0; i < slots.Count(); i++ ) {
				if ( @slots[i] is null ) {
					section.SaveUInt( "itemSlotCount_" + i, 0 );
					continue;
				}
				section.SaveUInt( "itemSlotCount_" + i, slots[i].Count() );
				section.SaveUInt( "itemSlotType_" + i, slots[i].GetItemType().type );
			}
		}
		
		float GetHealthMult() const {
			return m_nHealMult;
		}
		float GetDamageMult() const {
			return m_nDamageMult;
		}
		float GetRage() const {
			return m_nRage;
		}
		
		void Damage( EntityObject@ attacker, float nAmount ) {
			if ( m_bEmoting ) {
				return; // god has blessed thy soul...
			}
			
			m_nHealth -= nAmount;

			if ( m_nHealth < 1 ) {
				if ( m_nFrameDamage > 0 ) {
					return; // as long as you're hitting SOMETHING, you cannot die
				}
				switch ( Util::PRandom() & 3 ) {
				case 0:
					dieSfx0.Play();
					break;
				case 1:
					dieSfx1.Play();
					break;
				case 2:
					dieSfx2.Play();
					break;
				};
				EntityManager.KillEntity( @attacker, @this );
				
				Util::HapticRumble( m_nControllerIndex, 0.80f, 4000 );
			} else {
				switch ( Util::PRandom() & 3 ) {
				case 0:
					painSfx0.Play();
					break;
				case 1:
					painSfx1.Play();
					break;
				case 2:
					painSfx2.Play();
					break;
				};
				
				Util::HapticRumble( m_nControllerIndex, 0.50f, 300 );

				GfxManager.Bleed( m_Link.m_Origin );
			}
		}
		
		WeaponObject@ GetCurrentWeapon() {
			return @m_WeaponSlots[ m_CurrentWeapon ];
		}
		const WeaponObject@ GetCurrentWeapon() const {
			return @m_WeaponSlots[ m_CurrentWeapon ];
		}

		InventoryManager@ GetInventory() {
			return @m_Inventory;
		}
		const InventoryManager@ GetInventory() const {
			return @m_Inventory;
		}
		
		void Think() override {
			TheNomad::Engine::SoundSystem::SetWorldListener( m_Link.m_Origin );

			if ( m_State.GetID() == StateNum::ST_PLAYR_QUICKSHOT ) {
				m_QuickShot.Think();
			}

			if ( m_bUseWeapon ) {
				m_nFrameDamage += GetCurrentWeapon().Use( cast<EntityObject>( @this ), GetCurrentWeaponMode() );
			} else if ( m_bAltUseWeapon ) {
				m_nFrameDamage += GetCurrentWeapon().UseAlt( cast<EntityObject>( @this ), GetCurrentWeaponMode() );
			}

			m_Link.m_Bounds.m_nWidth = sgame_PlayerWidth.GetFloat();
			m_Link.m_Bounds.m_nHeight = sgame_PlayerHeight.GetFloat();
			m_Link.m_Bounds.MakeBounds( m_Link.m_Origin ); // breaks movement
			
			// run a movement frame
			Pmove.RunTic();

			if ( m_nHealth <= 15.0f ) {
				// if there's another haptic going on, we don't want to annihilate their hands
				Util::HapticRumble( ScreenData.GetPlayerIndex( @this ), 0.50f, 1000 );
			}

			if ( m_nHealth < 100.0f ) {
				m_nHealth += m_nHealMult;
				m_nRage -= m_nHealMult; // rage is essentially just converted mana

				if ( m_nHealMult > HEAL_MULT_BASE ) {
					m_nHealMult -= m_nHealMultDecay;
					if ( m_nHealMult < HEAL_MULT_BASE ) {
						m_nHealMult = HEAL_MULT_BASE;
					}
				}
				if ( m_nHealth > 100.0f ) {
					m_nHealth = 100.0f;
				}
			}
			m_nRage = Util::Clamp( m_nRage, 0.0f, m_nRage );

			@m_LegState = @m_LegState.Run();
			@m_ArmState = @m_ArmState.Run();
		}

		void SetArmState( StateNum num ) {
			@m_ArmState = @StateManager.GetStateForNum( num );
		}
		
		private float GetGfxDirection() const {
			if ( m_Facing == 1 ) {
				return -( m_Link.m_Bounds.m_nWidth / 2 );
			}
			return ( m_Link.m_Bounds.m_nWidth / 2 );
		}
		
		//
		// PlayrObject::CheckParry: called from DamageEntity mob v player
		//
		bool CheckParry( EntityObject@ ent, const InfoSystem::AttackInfo@ info ) {
			if ( Util::BoundsIntersect( ent.GetBounds(), m_ParryBox ) ) {
				if ( ent.IsProjectile() ) {
					// simply invert the direction and double the speed
					const vec3 v = ent.GetVelocity();
					ent.SetVelocity( vec3( v.x * 2, v.y * 2, v.z * 2 ) );
					ent.SetAngle( ent.GetAngle() * 2.0f / M_PI / 360.0f );
					ent.SetDirection( InverseDirs[ ent.GetDirection() ] );
				}
			}
			if ( ent.GetType() == TheNomad::GameSystem::EntityType::Mob && !ent.IsProjectile() ) {
				// just a normal counter
				MobObject@ mob = cast<MobObject@>( @ent );
				
				if ( info.canParry ) {
					// unblockable, deal damage
					return false;
				}
				else {
					// slap it back
					if ( mob.GetState().GetTics() <= ( mob.GetState().GetTics() / 4 ) ) {
						// counter parry, like in MGR, but more brutal, but for the future...
					}
					
					// TODO: make dead mobs with ANY velocity flying corpses
					EntityManager.DamageEntity( cast<EntityObject@>( @this ), @ent );
					parrySfx.Play();
				}
			}
			
			// add in the haptic
			Util::HapticRumble( m_nControllerIndex, 0.80f, 500 );
			
			// add the fireball
			GfxManager.AddExplosionGfx( vec3( m_Link.m_Origin.x + GetGfxDirection(), m_Link.m_Origin.y, m_Link.m_Origin.z ) );

			return true;
		}
		
		void MakeSound() {
			if ( m_bCrouching ) {
				return;
			}
			
			array<EntityObject@>@ entList = @EntityManager.GetEntities();
			for ( uint i = 0; i < entList.Count(); i++ ) {
				if ( entList[i].GetType() != TheNomad::GameSystem::EntityType::Mob ) {
					continue;
				}
				MobObject@ mob = cast<MobObject@>( @entList[i] );
				InfoSystem::MobInfo@ info = cast<InfoSystem::MobInfo@>( @mob.GetInfo() );
				const vec3 detection = vec3( float( info.detectionRangeX ), float( info.detectionRangeY ), 0.0f );
				const float dist = Util::Distance( mob.GetOrigin(), m_Link.m_Origin );

				if ( dist < Util::VectorLength( detection ) ) {
					// is there a wall there?
					TheNomad::GameSystem::RayCast ray;
					const vec3 origin = mob.GetOrigin();
					
					ray.m_nLength = dist;
					ray.m_nAngle = atan2( ( origin.x - m_Link.m_Origin.x ), ( origin.y - m_Link.m_Origin.y ) );
					ray.m_Start = m_Link.m_Origin;
					
					ray.Cast();
					if ( ray.m_nEntityNumber == ENTITYNUM_INVALID || ray.m_nEntityNumber == ENTITYNUM_WALL ) {
						continue; // hit a wall or dead air, no detection
					}
					
					mob.SetAlert( @this, dist );
				}
			}
		}
		
		//
		// PlayrObject::ParryThink: calculates the bounds of the parry box
		//
		private void ParryThink() {
			m_ParryBox.m_nWidth = 2.5f + m_nParryBoxWidth;
			m_ParryBox.m_nHeight = 1.0f;
			m_ParryBox.MakeBounds( vec3( m_Link.m_Origin.x + ( m_Link.m_Bounds.m_nWidth / 2.0f ),
				m_Link.m_Origin.y, m_Link.m_Origin.z ) );

			if ( m_nParryBoxWidth >= 1.5f ) {
				return; // maximum
			}
			
			m_nParryBoxWidth += 0.5f;
		}

		//
		// InitLoadout: initializes runtime player weapon data
		//
		private void InitLoadout() {
			m_EmptyInfo.name = "Fist (Empty)";
			m_EmptyInfo.type = ENTITYNUM_INVALID - 2;
			m_EmptyInfo.damage = 100.0f; // this is the god of war
			m_EmptyInfo.range = 1.5f;
			m_EmptyInfo.magSize = 0;
			m_EmptyInfo.weaponProps = InfoSystem::WeaponProperty( uint( InfoSystem::WeaponProperty::OneHandedBlunt ) );
			m_EmptyInfo.weaponType = InfoSystem::WeaponType::LeftArm;

			InfoSystem::InfoManager.GetWeaponTypes()[ "weapon_fist" ] = ENTITYNUM_INVALID - 2;
			InfoSystem::InfoManager.AddWeaponInfo( @m_EmptyInfo );

			@m_HeavyPrimary = WeaponObject();
			@m_HeavySidearm = WeaponObject();
			@m_LightPrimary = WeaponObject();
			@m_LightSidearm = WeaponObject();
			@m_Melee1 = WeaponObject();
			@m_Melee2 = WeaponObject();
			@m_RightArm = WeaponObject();
			@m_LeftArm = WeaponObject();
			@m_Ordnance = WeaponObject();

			@m_WeaponSlots[0] = @m_HeavyPrimary;
			@m_WeaponSlots[1] = @m_HeavySidearm;
			@m_WeaponSlots[2] = @m_LightPrimary;
			@m_WeaponSlots[3] = @m_LightSidearm;
			@m_WeaponSlots[4] = @m_Melee1;
			@m_WeaponSlots[5] = @m_Melee2;
			@m_WeaponSlots[6] = @m_RightArm;
			@m_WeaponSlots[7] = @m_LeftArm;
			@m_WeaponSlots[8] = @m_Ordnance;

			for ( uint i = 0; i < m_WeaponSlots.Count(); i++ ) {
				m_WeaponSlots[i].SetOwner( cast<EntityObject@>( @this ) );
				m_WeaponSlots[i].Spawn( m_EmptyInfo.type, m_Link.m_Origin );
			}
			
			m_CurrentWeapon = 0;

			@m_LeftHandWeapon = @m_LeftArm;
			@m_RightHandWeapon = @m_RightArm;
			m_LeftHandMode = InfoSystem::WeaponProperty( uint( InfoSystem::WeaponProperty::OneHandedBlunt ) );
			m_RightHandMode = InfoSystem::WeaponProperty( uint( InfoSystem::WeaponProperty::OneHandedBlunt ) );
		}

		private void CacheSfx() {
			dieSfx0 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/death1.ogg" );
			dieSfx1 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/death2.ogg" );
			dieSfx2 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/death3.ogg" );

			painSfx0 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/pain0.ogg" );
			painSfx1 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/pain1.ogg" );
			painSfx2 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/pain2.ogg" );

			slideSfx0 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/slide0.ogg" );
			slideSfx1 = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/slide1.ogg" );

			dashSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/dash.ogg" );

			meleeSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/melee.wav" );

			weaponChangeHandSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/weaponChangeHand.wav" );
			weaponChangeModeSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/weaponChangeMode.wav" );

			crouchDownSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/clothRuffle0.ogg" );
			crouchUpSfx = TheNomad::Engine::ResourceCache.GetSfx( "sfx/players/clothRuffle1.ogg" );
		}

		private void CacheGfx() {
			m_hBloodFxShader = TheNomad::Engine::ResourceCache.GetShader( "gfx/bloodSplatter0" );
			m_hDustTrailShader = TheNomad::Engine::ResourceCache.GetShader( "gfx/env/smokePuff" );
		}

		void Spawn( uint id, const vec3& in origin ) override {
			int i;

			//
			// init all player data
			//
			m_Link.m_Origin = origin;
			m_Link.m_nEntityType = TheNomad::GameSystem::EntityType::Playr;
			m_Link.m_nEntityId = 0;
			m_Link.m_nEntityNumber = m_nControllerIndex;
			m_nHealth = 100.0f;
			m_nRage = 100.0f; // he's always angry
			m_Direction = Util::Angle2Dir( m_PhysicsObject.GetAngle() );
			m_nHealMultDecay = LevelManager.GetDifficultyScale();

			CacheGfx();
			InitLoadout();
			CacheSfx();

			m_HudData.Init( @this );

			m_PhysicsObject.Init( cast<EntityObject>( @this ) );
			m_PhysicsObject.SetAngle( Util::Dir2Angle( TheNomad::GameSystem::DirType::East ) );

			@m_SpriteSheet = TheNomad::Engine::ResourceCache.GetSpriteSheet( "sprites/players/" +
				TheNomad::Engine::CvarVariableString( "skin" ) + "_torso", 512, 512, 32, 32 );
			if ( @m_SpriteSheet is null ) {
				GameError( "PlayrObject::Spawn: failed to load torso sprite sheet" );
			}
			@m_State = @StateManager.GetStateForNum( StateNum::ST_PLAYR_IDLE );
			if ( @m_State is null ) {
				GameError( "PlayrObject::Spawn: failed to load idle torso state" );
			}
			m_Facing = FACING_RIGHT;

			for ( i = 0; i < NUMFACING; i++ ) {
				@m_LegSpriteSheet[ i ] = TheNomad::Engine::ResourceCache.GetSpriteSheet( "sprites/players/"
					+ TheNomad::Engine::CvarVariableString( "skin" ) + "_legs_" + i, 512, 512, 32, 32 );
				if ( @m_LegSpriteSheet[ i ] is null ) {
					GameError( "PlayrObject::Spawn: failed to load leg sprite sheet" );
				}

				@m_ArmSpriteSheet[ i ] = TheNomad::Engine::ResourceCache.GetSpriteSheet( "sprites/players/"
					+ TheNomad::Engine::CvarVariableString( "skin" ) + "_arms_" + i, 512, 512, 32, 32 );
				if ( @m_ArmSpriteSheet[ i ] is null ) {
					GameError( "PlayrObject::Spawn: failed to load arm sprite sheet" );
				}
			}
			@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_IDLE_GROUND );
			if ( @m_LegState is null ) {
				GameError( "PlayrObject::Spawn: failed to load idle leg state" );
			}
			m_LegsFacing = FACING_RIGHT;

			@m_ArmState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_ARMS_IDLE );
			if ( @m_ArmState is null ) {
				GameError( "PlayrObject::Spawn: failed to load idle arm state" );
			}
			m_ArmsFacing = FACING_RIGHT;

//			@GoreManager = cast<GoreSystem>( @TheNomad::GameSystem::AddSystem( GoreSystem() ) );
		}

		uint GetSpriteId( SpriteSheet@ sheet, EntityState@ state ) const {
			const uint offset = state.GetSpriteOffset().y * sheet.GetSpriteCountX() + state.GetSpriteOffset().x;
			return offset + state.GetAnimation().GetFrame();
		}

		private void DrawLegs() {
			TheNomad::Engine::Renderer::RenderEntity refEntity;

			//
			// draw the legs
			//
			/*
			if ( m_PhysicsObject.GetVelocity() == Vec3Origin ) {
				// not moving at all, just draw idle legs
				if ( !Pmove.groundPlane ) {
					// static air legs
					@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_IDLE_AIR );
				} else {
					// idle ground legs
					@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_IDLE_GROUND );
				}
			}
			else if ( !Pmove.groundPlane ) {
				// we're in the air, modify the legs to show a bit of momentum control
				if ( m_PhysicsObject.GetVelocity().z < 0.0f ) {
					// falling down
					@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_FALL_AIR );
				}
				else if ( m_Debuff == AttackEffect::Stunned ) {
					// player is literally flying through the air
					@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_STUN_AIR );
				}
				else {
					// ascending
					@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_IDLE_AIR );
				}
			}
			else {
				// moving on the ground
				if ( m_Debuff == AttackEffect::Stunned ) {
					@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_STUN_GROUND );
				} else {
					@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_MOVE_GROUND );
				}
			}
			*/

			if ( m_PhysicsObject.GetVelocity() == Vec3Origin ) {
				@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_IDLE_GROUND );
			} else if ( m_bSliding ) {
				@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_SLIDE );
			} else {
				@m_LegState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_LEGS_MOVE_GROUND );
			}

			refEntity.origin = m_Link.m_Origin;
			refEntity.sheetNum = m_LegSpriteSheet[ m_LegsFacing ].GetShader();
			refEntity.spriteId = GetSpriteId( @m_LegSpriteSheet[ m_LegsFacing ], @m_LegState );
			refEntity.scale = 2.0f;
			refEntity.Draw();
		}

		private void DrawArms() {
			TheNomad::Engine::Renderer::RenderEntity refEntity;

			if ( m_ArmState.Done() && @m_ArmState !is @StateManager.GetStateForNum( StateNum::ST_PLAYR_ARMS_MOVE )
				&& m_PhysicsObject.GetVelocity() != Vec3Origin )
			{
				@m_ArmState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_ARMS_MOVE );
			}

//			if ( m_ArmState.Done() && @m_ArmState !is @StateManager.GetStateForNum( StateNum::ST_PLAYR_ARMS_IDLE ) ) {
//				@m_ArmState = @StateManager.GetStateForNum( StateNum::ST_PLAYR_ARMS_IDLE );
//			}

			refEntity.origin = m_Link.m_Origin;
			refEntity.sheetNum = m_ArmSpriteSheet[ m_ArmsFacing ].GetShader();
			refEntity.spriteId = GetSpriteId( @m_ArmSpriteSheet[ m_ArmsFacing ], @m_ArmState );
			refEntity.scale = 2.0f;
			refEntity.Draw();
		}
		
		// custom draw because of adaptive weapons and leg sprites
		void Draw() override {
			TheNomad::Engine::Renderer::RenderEntity refEntity;

			@m_State = @StateManager.GetStateForNum( StateNum::ST_PLAYR_IDLE );

			refEntity.origin = m_Link.m_Origin;
			refEntity.sheetNum = m_SpriteSheet.GetShader();
			refEntity.spriteId = GetSpriteId( @m_SpriteSheet, @m_State ) + m_Facing;
			refEntity.scale = 2.0f;
			refEntity.Draw();

			DrawLegs();
			DrawArms();

			m_HudData.Draw();
		}

		KeyBind key_MoveNorth;
		KeyBind key_MoveSouth;
		KeyBind key_MoveEast;
		KeyBind key_MoveWest;
		KeyBind key_Jump;
		KeyBind key_Melee;

		private TheNomad::GameSystem::BBox m_ParryBox;
		private float m_nParryBoxWidth = 0.0f;

		InfoSystem::WeaponInfo m_EmptyInfo;

		// toggled with "sgame_SaveLastUsedWeaponModes"
		private InfoSystem::WeaponProperty[] m_WeaponModes( 9 );

		// 9 weapons in total
		WeaponObject@[] m_WeaponSlots( 9 );
		WeaponObject@ m_HeavyPrimary = null;
		WeaponObject@ m_HeavySidearm = null;
		WeaponObject@ m_LightPrimary = null;
		WeaponObject@ m_LightSidearm = null;
		WeaponObject@ m_Melee1 = null;
		WeaponObject@ m_Melee2 = null;
		WeaponObject@ m_RightArm = null;
		WeaponObject@ m_LeftArm = null;
		WeaponObject@ m_Ordnance = null;

		int m_CurrentWeapon = 0;

		// used for various things but mostly just haptic feedback
		private uint m_nControllerIndex = 0;
		
		private QuickShot m_QuickShot;

		int m_hDustTrailShader = FS_INVALID_HANDLE;
		int m_hDashTrailShader = FS_INVALID_HANDLE;
		int m_hBloodFxShader = FS_INVALID_HANDLE;
		
		//
		// sound effects
		//
		private TheNomad::Engine::SoundSystem::SoundEffect parrySfx;
		private TheNomad::Engine::SoundSystem::SoundEffect counterParrySfx;
		TheNomad::Engine::SoundSystem::SoundEffect beginQuickshotSfx;
		TheNomad::Engine::SoundSystem::SoundEffect endQuickshotSfx;
		TheNomad::Engine::SoundSystem::SoundEffect crouchDownSfx;
		TheNomad::Engine::SoundSystem::SoundEffect crouchUpSfx;
		TheNomad::Engine::SoundSystem::SoundEffect dashSfx;
		TheNomad::Engine::SoundSystem::SoundEffect slideSfx0;
		TheNomad::Engine::SoundSystem::SoundEffect slideSfx1;
		TheNomad::Engine::SoundSystem::SoundEffect weaponChangeModeSfx;
		TheNomad::Engine::SoundSystem::SoundEffect weaponChangeHandSfx;
		TheNomad::Engine::SoundSystem::SoundEffect meleeSfx;
		private TheNomad::Engine::SoundSystem::SoundEffect painSfx0;
		private TheNomad::Engine::SoundSystem::SoundEffect painSfx1;
		private TheNomad::Engine::SoundSystem::SoundEffect painSfx2;
		private TheNomad::Engine::SoundSystem::SoundEffect dieSfx0;
		private TheNomad::Engine::SoundSystem::SoundEffect dieSfx1;
		private TheNomad::Engine::SoundSystem::SoundEffect dieSfx2;

		// the amount of damage dealt in the frame
		private float m_nFrameDamage = 0.0f;
		
		// what does the player have in their hands?
		private int m_nHandsUsed = 0; // 0 for left, 1 for right, 2 if two-handed
		private WeaponObject@ m_LeftHandWeapon = null;
		private WeaponObject@ m_RightHandWeapon = null;
		private InfoSystem::WeaponProperty m_LeftHandMode = InfoSystem::WeaponProperty::None;
		private InfoSystem::WeaponProperty m_RightHandMode = InfoSystem::WeaponProperty::None;
		
		private bool m_bUseWeapon = false;
		private bool m_bAltUseWeapon = false;

		// the lore goes: the harder and faster you hit The Nomad, the harder and faster he hits back
		private float m_nDamageMult = 0.0f;
		private float m_nRage = 0.0f;

		private float m_nHealMult = HEAL_MULT_BASE;
		private float m_nHealMultDecay = HEAL_MULT_BASE;
		
		private SpriteSheet@[] m_ArmSpriteSheet( NUMFACING );
		private EntityState@ m_ArmState = null;
		private int m_ArmsFacing = 0;

		private SpriteSheet@[] m_LegSpriteSheet( NUMFACING );
		private EntityState@ m_LegState = null;
		private int m_LegsFacing = 0;

		private uint64 m_nTimeSinceDash = 0;
		private uint m_nDashCounter = 0;
		private bool m_bDashing = false;

		private uint64 m_nTimeSinceSlide = 0;
		private bool m_bSliding = false;

		bool m_bMeleeActive = false;
		private bool m_bCrouching = false;

		private bool m_bEmoting = false;
		
		private PlayerDisplayUI m_HudData;
		PMoveData Pmove( @this );

		private InventoryManager m_Inventory;
	};
};