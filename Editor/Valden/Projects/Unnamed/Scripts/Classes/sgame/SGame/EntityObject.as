#include "SGame/EntityState.as"
#include "SGame/EntityStateSystem.as"
#include "SGame/PlayrObject.as"
#include "Engine/Physics/PhysicsObject.as"

namespace TheNomad::SGame {
	const int FACING_RIGHT = 0;
	const int FACING_LEFT = 1;
	const int FACING_UP = 2;
	const int FACING_DOWN = 3;
	const int NUMFACING = 2;

    class EntityObject {
		EntityObject( TheNomad::GameSystem::EntityType type, uint id, const vec3& in origin ) {
			Init( type, id, origin );
		}
		EntityObject() {
		}
		
		void Init( TheNomad::GameSystem::EntityType type, uint id, const vec3& in origin ) {
			// just create a temporary bbox to link it in, we'll rebuild every frame anyway
			TheNomad::GameSystem::BBox bounds( 1.0f, 1.0f, origin );
			m_Link = TheNomad::GameSystem::LinkEntity( origin, bounds, id, uint( type ) );
		}
		
		//
		// getters
		//
		float GetAngle() const {
			return m_PhysicsObject.GetAngle();
		}
		float GetHealth() const {
			return m_nHealth;
		}
		const string& GetName() const {
			return m_Name;
		}
		const EntityState@ GetState() const {
			return @m_State;
		}
		EntityState@ GetState() {
			return @m_State;
		}
		int GetShader() const {
			return m_hShader;
		}
		uint GetFlags() const {
			return m_Flags;
		}
		int GetFacing() const {
			return m_Facing;
		}
		uint GetId() const {
			return m_Link.m_nEntityId;
		}
		uint GetEntityNum() const {
			return m_Link.m_nEntityNumber;
		}
		AttackEffect GetDebuff() const {
			return m_Debuff;
		}
		TheNomad::GameSystem::BBox& GetBounds() {
			return m_Link.m_Bounds;
		}
		const TheNomad::GameSystem::LinkEntity& GetLink() const {
			return m_Link;
		}
		TheNomad::GameSystem::LinkEntity& GetLink() {
			return m_Link;
		}
		TheNomad::GameSystem::DirType GetDirection() const {
			return m_Direction;
		}
		TheNomad::Engine::Physics::PhysicsObject& GetPhysicsObject() {
			return m_PhysicsObject;
		}
		InfoSystem::InfoLoader@ GetInfo() {
			return @m_InfoData;
		}
		vec3& GetOrigin() {
			return m_Link.m_Origin;
		}
		const vec3& GetOrigin() const {
			return m_Link.m_Origin;
		}
		const vec3& GetVelocity() const {
			return m_PhysicsObject.GetVelocity();
		}
		vec3& GetVelocity() {
			return m_PhysicsObject.GetVelocity();
		}
		const vec3& GetAcceleration() const {
			return m_PhysicsObject.GetAcceleration();
		}
		void SetAcceleration( const vec3& in accel ) {
			m_PhysicsObject.SetAcceleration( accel );
		}
		int GetWaterLevel() const {
			return m_PhysicsObject.GetWaterLevel();
		}
		void SetWaterLevel( int level ) {
			m_PhysicsObject.SetWaterLevel( level );
		}
		TheNomad::GameSystem::EntityType GetType() const {
			return TheNomad::GameSystem::EntityType( m_Link.m_nEntityType );
		}
		bool IsProjectile() const {
			return m_bProjectile;
		}
		SpriteSheet@ GetSpriteSheet() {
			return @m_SpriteSheet;
		}
		const SpriteSheet@ GetSpriteSheet() const {
			return @m_SpriteSheet;
		}

		//
		// setters
		//
		void SetDebuff( AttackEffect effect ) {
			m_Debuff = effect;
		}
		void SetHealth( float nHealth ) {
			m_nHealth = nHealth;
		}
		void SetState( StateNum stateNum ) {
			@m_State = @StateManager.GetStateForNum( stateNum );
			m_State.Reset();
		}
		void SetState( EntityState@ state ) {
			@m_State = @state;
			if ( @m_State is null ) {
				ConsoleWarning( "null state\n" );
				return;
			}
			m_State.Reset();
		}
		void SetFlags( uint flags ) {
			m_Flags = EntityFlags( flags );
		}
		void SetProjectile( bool bProjectile ) {
			m_bProjectile = bProjectile;
		}
		void SetVelocity( const vec3& in vel ) {
			m_PhysicsObject.SetVelocity( vel );
		}
		void SetOrigin( const vec3& in origin ) {
			m_Link.m_Origin = origin;
		}
		void SetDirection( TheNomad::GameSystem::DirType dir ) {
			m_Direction = dir;
		}
		void SetFacing( int facing ) {
			m_Facing = facing;
		}
		void SetAngle( float nAngle ) {
			m_PhysicsObject.SetAngle( nAngle );
		}

		//
		// functions that should be implemented by the derived class
		//

		protected void LoadBase( const TheNomad::GameSystem::SaveSystem::LoadSection& in section ) {
			m_Link.m_Origin = section.LoadVec3( "origin" );
			m_Link.m_nEntityId = section.LoadUInt( "id" );
			m_Link.m_nEntityType = TheNomad::GameSystem::EntityType( section.LoadUInt( "type" ) );
			m_Flags = EntityFlags( section.LoadUInt( "flags" ) );
			m_bProjectile = section.LoadBool( "isProjectile" );
		}

		protected void SaveBase( const TheNomad::GameSystem::SaveSystem::SaveSection& in section ) const {
			section.SaveVec3( "origin", m_Link.m_Origin );
			section.SaveUInt( "id",  m_Link.m_nEntityId );
			section.SaveUInt( "type", uint( m_Link.m_nEntityType ) );
			section.SaveUInt( "flags", uint( m_Flags ) );
			section.SaveBool( "isProjectile", m_bProjectile );
		}
		
		//
		// EntityObject::Load: should only return false if we're missing something
		// in the save data
		//
		bool Load( const TheNomad::GameSystem::SaveSystem::LoadSection& in section ) {
			ConsoleWarning( "EntityObject::Load: called\n" );
			return true;
		}
		void Save( const TheNomad::GameSystem::SaveSystem::SaveSection& in section ) const {
			ConsoleWarning( "EntityObject::Save: callled\n" );
		}
		void Damage( EntityObject@ attacker, float nAmount ) {
			ConsoleWarning( "EntityObject::Damage: called\n" );
		}
		void Think() {
			ConsoleWarning( "EntityObject::Think: called\n" );
		}
		void Draw() {
		}
		void Spawn( uint, const vec3& in ) {
			ConsoleWarning( "EntityObject::Spawn: called\n" );
		}

		//
		// utilities
		//
		bool CheckFlags( uint flags ) const {
			return ( m_Flags & flags ) != 0;
		}

		// mostly meant for debugging
		protected string m_Name;

		// used for physics in the modules
		protected TheNomad::Engine::Physics::PhysicsObject m_PhysicsObject;

		// engine data, for physics
		protected TheNomad::GameSystem::LinkEntity m_Link;

		// cached info
		protected InfoSystem::InfoLoader@ m_InfoData = null;

		// the entity's current state
		protected EntityState@ m_State = null;
				
		// current effect the entity's suffereing from
		protected AttackEffect m_Debuff = AttackEffect::None;
		
		// DUH.
		protected float m_nHealth = 0.0f;
		
		// flags, some are specific
		protected EntityFlags m_Flags = EntityFlags::None;
		
		protected TheNomad::GameSystem::DirType m_Direction = TheNomad::GameSystem::DirType::North;
		
		// is it a projectile?
		protected bool m_bProjectile = false;
		
		// for direction based sprite drawing
		protected int m_Facing = 0;
		
		//
		// renderer data
		//
		protected int m_hShader = FS_INVALID_HANDLE;
		protected SpriteSheet@ m_SpriteSheet = null;
		
		// linked list stuff
		EntityObject@ next = null;
		EntityObject@ prev = null;
	};
};