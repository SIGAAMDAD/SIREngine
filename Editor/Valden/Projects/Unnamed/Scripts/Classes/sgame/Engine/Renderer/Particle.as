namespace TheNomad::Engine::Renderer {
    enum ParticleType {
        None = 0,

    };

    class Particle {
        Particle() {
        }

        Particle@ m_Next = null;

        float m_nStartTime = 0.0f;
        float m_nLifeTime = 0.0f;

        vec3 m_Origin = vec3( 0.0f );
        vec3 m_Velocity = vec3( 0.0f );
        vec3 m_Acceleration = vec3( 0.0f );
        int m_Color = 0;
        float m_nColorVel = 0.0f;
        float m_nAlpha = 0.0f;
        float m_nAlphaVel = 0.0f;
        ParticleType m_nType = ParticleType::None;
        int m_pShader = FS_INVALID_HANDLE;

        float m_nHeight = 0.0f;
        float m_nWidth = 0.0f;

        float m_nEndHeight = 0.0f;
        float m_nEndWidth = 0.0f;

        float m_nStart = 0.0f;
        float m_nEnd = 0.0f;

        float m_nStartFade = 0.0f;
        bool m_bRotate = false;
        int m_nSnum = 0;

        bool m_bLink = false;
    };
};