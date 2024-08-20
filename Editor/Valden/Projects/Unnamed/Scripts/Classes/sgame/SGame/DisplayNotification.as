namespace TheNomad::SGame {
	class DisplayNotification {
		DisplayNotification() {
		}
		
		void Shutdown() {
			m_hShader = FS_INVALID_HANDLE;
			@m_SpriteSheet = null;
			m_Size = ivec2( 0 );
			m_Origin = ivec2( 0 );
			m_nLifeTime = 0;
			m_nEndTime = 0;
			m_nSpriteId = 0;
			m_bAlive = false;
		}
		void Init( uint32 duration, int32 hShader, const ivec2& in origin, const ivec2& in size ) {
			m_bAlive = true;
			m_nLifeTime = 0;
			m_nEndTime = duration;
			m_hShader = hShader;
			m_Origin = origin;
			m_Size = size;
		}
		void Init( uint32 duration, uint32 spriteIndex, SpriteSheet@ sheet, const ivec2& in origin, const ivec2& in size ) {
			m_nLifeTime = 0;
			m_nEndTime = duration;
			m_nSpriteId = spriteIndex;
			m_Origin = origin;
			m_Size = size;
			@m_SpriteSheet = @sheet;
			m_bAlive = true;
		}
		
		bool Draw() {
			m_nLifeTime += TheNomad::GameSystem::GameManager.GetDeltaTics();
			if ( m_nLifeTime >= m_nEndTime ) {
				Shutdown();
				return false;
			}
			if ( m_hShader == FS_INVALID_HANDLE && @m_SpriteSheet is null ) {
				DebugPrint( "Draw() called on invalid HUD notification\n" );
				Shutdown();
				return false;
			}
			
			// we expect that a scene has already been cleared out for the HUD,
			// as really on the PlayerDisplayUI should actually be calling into
			// this class
			if ( @m_SpriteSheet !is null ) {
				// we're rendering from a spritesheet
				TheNomad::Engine::Renderer::DrawImage( m_Origin.x, m_Origin.y, m_Size.x, m_Size.y,
					m_SpriteSheet[ m_nSpriteId ][0][0], m_SpriteSheet[ m_nSpriteId ][0][1],
					m_SpriteSheet[ m_nSpriteId ][2][0], m_SpriteSheet[ m_nSpriteId ][2][1],
					m_SpriteSheet[ m_nSpriteId ].GetShader() );
			} else {
				// just use the shader provided
				TheNomad::Engine::Renderer::DrawImage( m_Origin.x, m_Origin.y, m_Size.x, m_Size.y, 0, 0, 1, 1, m_hShader );
			}
			
			return true;
		}
		
		bool Alive() const {
			return m_bAlive;
		}
		
		private ivec2 m_Size = ivec2( 0 );
		private ivec2 m_Origin = ivec2( 0 );
		private SpriteSheet@ m_SpriteSheet = null;
		private uint32 m_nSpriteId = 0;
		private int32 m_hShader = 0;
		private uint32 m_nLifeTime = 0;
		private uint32 m_nEndTime = 0;
		private bool m_bAlive = false;
	};
};