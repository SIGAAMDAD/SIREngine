namespace TheNomad::Engine::Renderer {
    class LocalEntity {
        LocalEntity() {
        }

		void Spawn( const vec3& in origin, const vec3& in velocity, uint lifeTime, int hShader ) {
			m_Origin = origin;
			m_Velocity = velocity;
			m_nStartTime = TheNomad::GameSystem::GameManager.GetGameTic();
			m_nLifeTime = lifeTime;
			m_hShader = hShader;
		}

        void RunTic() {
            m_Origin += m_Velocity;

			if ( m_bGravity ) {
				m_Velocity.y = 0.01f;
				m_Velocity.z -= 0.01f;
			}

			TheNomad::Engine::Renderer::RenderEntity refEntity;
			refEntity.rotation = m_nRotation;

			if ( refEntity.rotation != 0.0f ) {
				refEntity.rotation += Util::RAD2DEG( Util::PRandom() & 16 );
			}

			refEntity.origin = m_Origin;
			refEntity.scale = 1.0f;
			refEntity.sheetNum = -1;
			refEntity.spriteId = m_hShader;
			refEntity.Draw();
        }

        vec3 m_Origin = vec3( 0.0f );
		vec3 m_Velocity = vec3( 0.0f );
		LocalEntity@ m_Next = null;
		LocalEntity@ m_Prev = null;
		uint m_nStartTime = 0;
		uint m_nLifeTime = 0;
		float m_nRotation = 0.0f;
		int m_hShader = FS_INVALID_HANDLE;
		bool m_bGravity = false;
    };
};