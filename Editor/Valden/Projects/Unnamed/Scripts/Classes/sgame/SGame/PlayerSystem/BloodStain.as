namespace TheNomad::SGame {
    class BloodStain {
        BloodStain() {
        }

        void Draw() {
            m_Origin += m_Velocity;
            if ( m_Origin.z < 0.0f ) {
                m_Velocity = vec3( 0.0f );
                m_Origin.z = 0.0f;
            }

//            TheNomad::Engine::Renderer::AddSpriteToScene( m_hShader );
        }

        vec3 m_Origin = vec3( 0.0f );
        vec3 m_Velocity = vec3( 0.0f );
        BloodStain@ m_Next = null;
        BloodStain@ m_Prev = null;
        int m_hShader = FS_INVALID_HANDLE;
    };
};