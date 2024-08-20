namespace TheNomad::SGame {
    class MapSpawn {
		MapSpawn( const uvec3& in origin, uint nEntityId, TheNomad::GameSystem::EntityType nEntityType ) {
			@m_Checkpoint = null;
			m_Origin = origin;
			m_nEntityId = nEntityId;
			m_nEntityType = nEntityType;
			m_bUsed = false;
		}
		MapSpawn() {
		}
		
		void Activate() {
			if ( m_bUsed ) {
				return; // ensure no recursions
			}
			
			m_bUsed = true;
			EntityManager.Spawn( m_nEntityType, m_nEntityId,
				vec3( float( m_Origin.x ), float( m_Origin.y ),
				float( m_Origin.z ) ), vec2( 0.0f, 0.0f ) );
		}
		
		uvec3 m_Origin = uvec3( 0 );
		MapCheckpoint@ m_Checkpoint = null;
		TheNomad::GameSystem::EntityType m_nEntityType = TheNomad::GameSystem::EntityType::Item;
		uint m_nEntityId = 0;
		bool m_bUsed = false;
	};
};