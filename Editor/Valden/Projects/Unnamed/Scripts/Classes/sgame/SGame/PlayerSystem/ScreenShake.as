namespace TheNomad::SGame {
	class ScreenShake {
		ScreenShake() {
		}
		
		void Start( uint64 nDuration, float nStrengthX, float nStrengthY ) {
			m_nDuration = nDuration;
			m_nStart = TheNomad::GameSystem::GameManager.GetGameTic();
			m_Strength = vec2( nStrengthX, nStrengthY );
			m_nAngle = atan2( nStrengthX, nStrengthY );
			m_bDone = false;
			
			// save the old camera position to restore it after the screen shake
			m_CameraPos = Game_CameraPos;
		}
		
		void OnRunTic() {
			if ( TheNomad::GameSystem::GameManager.GetGameTic() - m_nStart >= m_nDuration ) {
				if ( !m_bDone ) {
					Game_CameraPos = m_CameraPos;
				}
				m_bDone = true;
				return;
			}
			
//			const float noise = Util::PerlinNoise( m_Strength.x, m_Strength.y );
			float noise = Util::PRandom();
			if ( noise <= 0.0f ) {
				noise = 1.0f;
			} else {
				noise = 1.0f / noise;
			}
			const vec2 offset = vec2( cos( m_nAngle ) + ( m_Strength.x * noise ), sin( m_nAngle ) + ( m_Strength.y * noise ) );
			
			// decrease a little every frame
			m_Strength.x -= 0.1f * m_nDuration;
			m_Strength.y -= 0.1f * m_nDuration;
			
			Game_CameraPos.x += offset.x;
			Game_CameraPos.y += offset.y;
		}
		
		private vec3 m_CameraPos;
		private vec2 m_Strength = vec2( 0.0f );
		private uint64 m_nStart = 0;
		private uint64 m_nDuration = 0;
		private float m_nAngle = 0.0f;
		private bool m_bDone = false;
	};
};