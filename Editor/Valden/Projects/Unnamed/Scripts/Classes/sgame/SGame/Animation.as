namespace TheNomad::SGame {
	class Animation {
		Animation() {
			m_nOldTic = 0;
			m_nTicker = 1;
		}
		
		int GetFrame() const {
			return m_nCurrentFrame;
		}
		uint NumFrames() const {
			return m_nNumFrames;
		}

		void Log() const {
			ConsolePrint( "TicRate: " + m_nTicRate + "\n" );
			ConsolePrint( "Frames: " + m_nNumFrames + "\n" );
			ConsolePrint( "FlipFlop: " + m_bOscillate + "\n" );
		}
		
		void Run() {
			if ( TheNomad::GameSystem::GameManager.GetGameTic() - m_nOldTic > m_nTicRate ) {
				m_nOldTic = TheNomad::GameSystem::GameManager.GetGameTic();

				if ( m_bReverse ) {
					m_nCurrentFrame--;
					if ( m_nCurrentFrame < 0 ) {
						m_nCurrentFrame = m_nNumFrames - 1;
					}
				} else {
					m_nCurrentFrame += m_nTicker;
					if ( m_bOscillate ) {
						if ( m_nCurrentFrame >= m_nNumFrames ) {
							m_nTicker = -1;
						} else if ( m_nCurrentFrame < 0 ) {
							m_nCurrentFrame = 0;
							m_nTicker = 1;
						}
					} else {
						if ( m_nCurrentFrame >= int( m_nNumFrames ) ) {
							m_nCurrentFrame = 0;
						}
					}
				}
			}
		}
		
		bool Load( json@ json ) {
			string base;

			if ( !json.get( "TicRate", base ) ) {
				ConsoleWarning( "invalid animation info, missing variable 'TicRate'\n" );
				return false;
			}
			m_nTicRate = Convert().ToInt( base );
			if ( !json.get( "FlipFlop", base ) ) {
				ConsoleWarning( "invalid animation info, missing variable 'FlipFlop'\n" );
				return false;
			}
			m_bOscillate = Convert().ToBool( base );
			if ( !json.get( "Frames", base ) ) {
				ConsoleWarning( "invalid animation info, missing variable 'Frames'\n" );
				return false;
			}
			m_nNumFrames = Convert().ToUInt( base );
			if ( !json.get( "Reverse", base ) ) {
				ConsoleWarning( "invalid animation info, missing variable 'Reverse'\n" );
				return false;
			}
			m_bReverse = Convert().ToBool( base );

			if ( m_bReverse ) {
				m_nTicker = -1;
			}
			if ( m_nNumFrames == 0 ) {
				m_nTicker = 0;
			}

			return true;
		}

		void SetState( EntityState@ state ) {
			@m_State = @state;
		}
		EntityState@ GetState() {
			return @m_State;
		}

		private EntityState@ m_State = null;
		
		private uint64 m_nOldTic = 0;
		private uint m_nNumFrames = 1;
		private int m_nTicRate = 1;
		private int m_nTicker = 0;
		private int m_nCurrentFrame = 0;
		private bool m_bOscillate = false;
		private bool m_bReverse = false;
	};
};