#include "SGame/InfoSystem/InfoDataManager.as"
#include "SGame/InfoSystem/WeaponInfo.as"


namespace TheNomad::SGame {
    class WeaponObject : EntityObject {
		WeaponObject() {
		}

		void SetOwner( EntityObject@ ent ) {
			@m_Owner = @ent;
		}
		EntityObject@ GetOwner() {
			return @m_Owner;
		}

		InfoSystem::WeaponProperty GetProperties() const {
			return m_Info.weaponProps;
		}
		
		bool IsOneHanded() const {
			return ( m_Info.weaponProps & InfoSystem::WeaponProperty::IsOneHanded ) == 1;
		}
		bool IsTwoHanded() const {
			return ( m_Info.weaponProps & InfoSystem::WeaponProperty::IsTwoHanded ) == 1;
		}
		bool IsBladed() const {
			return ( m_Info.weaponProps & InfoSystem::WeaponProperty::IsBladed ) == 1;
		}
		bool IsPolearm() const {
			return ( m_Info.weaponProps & InfoSystem::WeaponProperty::IsPolearm ) == 1;
		}
		bool IsFirearm() const {
			return ( m_Info.weaponProps & InfoSystem::WeaponProperty::IsFirearm ) == 1;
		}
		bool IsBlunt() const {
			return ( m_Info.weaponProps & InfoSystem::WeaponProperty::IsBlunt ) == 1;
		}
		
		//
		// AreaOfEffect: does exactly what you think it does
		//
		private float AreaOfEffect( EntityObject@ ent, float damage ) const {
			TheNomad::GameSystem::BBox bounds;
			array<EntityObject@>@ entList;
			const float range = m_Info.range / 2;
			
			// shit works like DOOM 1993 (box instead of circle)
			// this weapon is a projectile or ordnance with its own entity
			bounds.m_Mins = vec3( m_Link.m_Origin.x - range, m_Link.m_Origin.y - range, m_Link.m_Origin.z - range );
			bounds.m_Maxs = vec3( m_Link.m_Origin.x + range, m_Link.m_Origin.y + range, m_Link.m_Origin.z + range );
			bounds.m_nWidth = range;
			bounds.m_nHeight = range;
			
			@entList = EntityManager.GetEntities();
			for ( uint i = 0; i < entList.Count(); i++ ) {
				if ( Util::BoundsIntersectPoint( bounds, entList[i].GetOrigin() ) ) {
					const float dist = Util::Distance( m_Link.m_Origin, entList[i].GetOrigin() );
					if ( dist < 1.0f ) { // immediate impact range means death
						EntityManager.KillEntity( @ent, @entList[i] );
						damage += m_Info.damage;
					} else if ( dist <= m_Info.range ) {
						EntityManager.DamageEntity( @ent, @entList[i], m_Info.damage + ( m_Info.range + ( dist * 0.1f ) ) );
						damage += m_Info.damage;
					}
				}
			}
			// let's be real here, if the player is standing in the middle of an explosion,
			// they should taking damage as well
			return sgame_Difficulty.GetInt() < GameSystem::GameDifficulty::Hard ? damage : 0.0f;
		}
		
		private float UseBlade( EntityObject@ ent, float damage, uint weaponMode ) const {
			return 0.0f;
		}
		private float UseBlunt( EntityObject@ ent, float damage, uint weaponMode ) const {
			const vec3 origin = ent.GetOrigin();
			const float angle = ent.GetAngle();
			TheNomad::GameSystem::BBox bounds;
			
			return damage;
		}
		private float UsePolearm( EntityObject@ ent, float damage, uint weaponMode ) const {
			const vec3 origin = ent.GetOrigin();
			const float angle = ent.GetAngle();
			vec3 end;
			
			end.x = origin.x + ( m_Info.range * cos( angle ) );
			end.y = origin.y + ( m_Info.range * sin( angle ) );
			end.z = m_Info.range * sin( angle );
			
			if ( EntityManager.EntityIntersectsLine( origin, end ) ) {
				m_Info.useSfx.Play();
				// pike, bannerlord mode (you crouch to get a spear brace), high-risk, high-reward
				if ( cast<PlayrObject@>( @ent ).IsCrouching() || cast<PlayrObject@>( @ent ).IsSliding() ) {
					return 1000.0f; // it's an insta-kill for most mobs
				} else {
					return m_Info.damage;
				}
			}
			return 0.0f;
		}
		private float UseFireArm( EntityObject@ ent, float damage, uint weaponMode ) const {
			TheNomad::GameSystem::RayCast ray;
			
			ray.m_nLength = m_Info.range;
			ray.m_Start = ent.GetOrigin();
			ray.m_nAngle = ent.GetAngle();
			
			ray.Cast();
			if ( ray.m_nEntityNumber == ENTITYNUM_INVALID || ray.m_nEntityNumber == ENTITYNUM_WALL ) {
				return 0.0f; // hit nothing or a wall
			}
			
			EntityManager.DamageEntity( @ent, @ray );
			
			// health mult doesn't matter on harder difficulties if the player is attacking with a firearm,
			// that is, unless, the player is very close to the enemy
			if ( sgame_Difficulty.GetInt() < TheNomad::GameSystem::GameDifficulty::Hard ) {
				return damage;
			} else {
				if ( Util::Distance( ray.m_End, ent.GetOrigin() ) <= 2.75f ) {
					// close enough to the blood
					return damage;
				}
				return 0.0f;
			}
		}
		
		//
		// WeaponObject::Use: returns the amount of damage dealt
		//
		float Use( EntityObject@ ent, const uint weaponMode ) {
			float damage;
			
			damage = m_Info.damage;
			
			// TODO: adaptive weapon animation & cooldowns
			
			if ( ( weaponMode & uint( InfoSystem::WeaponProperty::IsOneHanded ) ) != 0
				&& ( weaponMode & uint( InfoSystem::WeaponProperty::IsFirearm ) == 0 ) )
			{
				// half the damage, we're not swinging with full force.
				// And since the weaponMode is just state and not actually the real
				// weapon mode, we can still use a musket like a baseball bat
				
				// bullets hit just as hard even if we're dual-wielding
				damage /= 2;
			}
			
			if ( ( weaponMode & uint( InfoSystem::WeaponProperty::IsFirearm ) ) != 0 ) {
				return UseFireArm( @ent, damage, weaponMode );
			} else if ( ( weaponMode & uint( InfoSystem::WeaponProperty::IsPolearm ) ) != 0 ) {
				return UsePolearm( @ent, damage, weaponMode );
			} else if ( ( weaponMode & uint( InfoSystem::WeaponProperty::IsBladed ) ) != 0 ) {
				return UseBlade( @ent, damage, weaponMode );
			} else if ( ( weaponMode & uint( InfoSystem::WeaponProperty::IsBlunt ) ) != 0 ) {
				return UseBlunt( @ent, damage, weaponMode );
			} else {
				// not really an error because it could just be an empty slot, but warn anyway
				DebugPrint( "WARNING: bad weapon state " + weaponMode + ".\n" );
			}
			
			return 0.0f;
		}
		float UseAlt( EntityObject@ ent, uint weaponMode ) {
			float damage = 0.0f;

			return damage;
		}

		bool Load( const TheNomad::GameSystem::SaveSystem::LoadSection& in section ) {
			if ( section.LoadBool( "hasOwner" ) ) {
				m_Link.m_nEntityId = section.LoadUInt( "id" );
				m_Link.m_nEntityType = TheNomad::GameSystem::EntityType( section.LoadUInt( "type" ) );
				m_Flags = EntityFlags( section.LoadUInt( "flags" ) );

				@m_Owner = @EntityManager.GetEntityForNum( section.LoadUInt( "owner" ) );
			} else {
				LoadBase( section );
			}

			Spawn( m_Link.m_nEntityId, m_Link.m_Origin );

			return true;
		}
		void Save( const TheNomad::GameSystem::SaveSystem::SaveSection& in section ) {
			if ( @m_Owner is null ) {
				SaveBase( section );
			}
			section.SaveBool( "hasOwner", @m_Owner !is null );
			if ( @m_Owner !is null ) {
				section.SaveUInt( "id",  m_Link.m_nEntityId );
				section.SaveUInt( "type", uint( m_Link.m_nEntityType ) );
				section.SaveUInt( "flags", uint( m_Flags ) );
				section.SaveUInt( "owner", m_Owner.GetEntityNum() );
			}
		}
		void Think() {
			if ( @m_Owner !is null ) {
				return;
			}

			TheNomad::Engine::Renderer::RenderEntity refEntity;

			refEntity.sheetNum = -1;
			refEntity.spriteId = m_Info.iconShader;
			refEntity.origin = m_Link.m_Origin;
			refEntity.scale = 1.5f;
			refEntity.Draw();
			
			m_Link.m_Bounds.m_nWidth = m_Info.width;
			m_Link.m_Bounds.m_nHeight = m_Info.height;
			m_Link.m_Bounds.MakeBounds( m_Link.m_Origin );
		}
		void Spawn( uint id, const vec3& in origin ) override {
			@m_Info = @InfoSystem::InfoManager.GetWeaponInfo( id );
			if ( @m_Info is null ) {
				GameError( "WeaponObject::Spawn: invalid weapon id " + id );
			}

			m_Link.m_Origin = origin;
			m_Link.m_Bounds.m_nWidth = m_Info.width;
			m_Link.m_Bounds.m_nHeight = m_Info.height;
			m_Link.m_Bounds.MakeBounds( origin );
		}

		InfoSystem::ItemInfo@ GetItemInfo() {
			return @m_Info;
		}
		const InfoSystem::ItemInfo@ GetItemInfo() const {
			return @m_Info;
		}

		private InfoSystem::WeaponInfo@ m_Info = null;
		private EntityObject@ m_Owner = null;
	};
};