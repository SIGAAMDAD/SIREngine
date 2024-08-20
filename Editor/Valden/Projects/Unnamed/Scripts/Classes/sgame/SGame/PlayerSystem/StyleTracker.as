namespace TheNomad::SGame {
	enum StyleType {
		Invalid,
		DoubleKill,
		TripleKill,
		QuadKill,
		UltraKill, // OH YAS
		Parry,
		BulletSlap,
		StabNGrab // Zandatsu, will come in the future
	};
	
	class StyleData {
		StyleData() {
		}
		StyleData( const string& in name, uint score, const vec4& in color ) {
			m_Title = name;
			m_nStyleScore = score;
			m_Color = color;
		}
		
		string m_Title;
		vec4 m_Color = vec4( 0.0f );
		uint m_nStyleScore = 0;
	};
	
	class StyleTracker {
		StyleTracker() {
		}
		
		private void Init() {
			m_StyleTitles.Add( StyleData( "INVALID", 0, colorWhite ) );
			m_StyleTitles.Add( StyleData( "DOUBLE KILL", 50, colorGold ) );
			m_StyleTitles.Add( StyleData( "TRIPLE KILL", 80, colorGold ) );
			m_StyleTitles.Add( StyleData( "QUAD KILL", 100, colorGold ) );
			m_StyleTitles.Add( StyleData( "ULTRA KILL", 200, colorGold ) );
			m_StyleTitles.Add( StyleData( "PARRY", 20, colorGreen ) );
			m_StyleTitles.Add( StyleData( "BULLETS SLAPPED", 50, colorGreen ) );
		}
		
		//
		// StyleTracker::PopAction: makes room for one more style
		// action in the tracker
		//
		private void PopAction() {
			m_StyleStack.RemoveAt( m_StyleStack.Count() - 1 );
		}
		
		void PushAction( StyleType type ) {
			if ( m_StyleStack.Count() >= 8 ) {
				PopAction();
			}
			
			if ( @m_LastUsedWeapon is @EntityManager.GetActivePlayer().GetCurrentWeapon() ) {
				if ( m_nFreshnessLevel > 0 ) {
					m_nFreshnessLevel--;
				}
			} else {
				if ( m_nFreshnessLevel < 3 ) {
					m_nFreshnessLevel++;
				}
			}
			
			m_StyleStack.Add( type );
			@m_LastUsedWeapon = @EntityManager.GetActivePlayer().GetCurrentWeapon();
			m_nTimeSinceLastPush = TheNomad::GameSystem::GameManager.GetGameTic();
			
			if ( m_nMultiplier >= 1.0f ) {
				m_nTotalStyle += uint( ceil( float( m_StyleTitles[ uint( type ) ].m_nStyleScore ) * m_nMultiplier ) );
			} else {
				m_nTotalStyle += m_StyleTitles[ uint( type ) ].m_nStyleScore;
			}
		}
		void PushKill( CauseOfDeath cause ) {
			if ( TheNomad::GameSystem::GameManager.GetGameTic() - m_nTimeSinceLastKill < 5000 ) {
				// 5 seconds between kills to count as a multikill
				m_nTimeSinceLastKill = TheNomad::GameSystem::GameManager.GetGameTic();
				m_nKillCounter++;
			} else {
				m_nKillCounter = 1;
			}
			
			// if there's a multikill, add that in
			switch ( m_nKillCounter ) {
			case 2:
				PushAction( StyleType::DoubleKill );
				break;
			case 3:
				PushAction( StyleType::TripleKill );
				break;
			case 4:
				PushAction( StyleType::QuadKill );
				break;
			default: {
				if ( m_nKillCounter > 4 ) {
					PushAction( StyleType::UltraKill );
				}
				break; }
			};
			
			
		}
		
		void Draw() {
			const float scale = TheNomad::GameSystem::GameManager.GetUIScale();
			
			// add up to the mult
			if ( EntityManager.GetActivePlayer().IsSliding() ) {
				m_nMultiplier += 0.5f;
			}
			if ( EntityManager.GetActivePlayer().IsDoubleJumping() ) {
				m_nMultiplier += 0.5f;
			}
			if ( m_nMultiplier > 3.0f ) {
				m_nMultiplier = 3.0f;
			}
			
			ImGui::Begin( "##StyleTrackerWindow", null, ImGui::MakeWindowFlags( ImGuiWindowFlags::NoTitleBar |
				ImGuiWindowFlags::NoBackground | ImGuiWindowFlags::NoMove | ImGuiWindowFlags::NoResize ) );
			ImGui::SetWindowSize( vec2( 128 * scale, 256 * scale ) );
			ImGui::SetWindowPos( vec2( 728 * scale, 16 * scale ) );
			
			if ( TheNomad::GameSystem::GameManager.GetGameTic() - m_nTimeSinceLastPush > 8000 ) {
				PopAction();
			}
			
			for ( uint i = 0; i < m_StyleStack.Count(); i++ ) {
			}
			
			ImGui::NewLine();
			ImGui::Text( "MULTIPLIER X" + formatFloat( m_nFreshnessAmount, "%.2f" ) );
			ImGui::PushStyleColor( ImGuiCol::FrameBg, m_FreshnessColor );
			ImGui::PushStyleColor( ImGuiCol::FrameBgActive, m_FreshnessColor );
			ImGui::PushStyleColor( ImGuiCol::FrameBgHovered, m_FreshnessColor );
			switch ( m_nFreshnessLevel ) {
			case 0:
				ImGui::Text( "DULL" );
				break;
			case 1:
				ImGui::Text( "DECENT" );
				break;
			case 2:
				ImGui::Text( "FRESH" );
				break;
			};
			ImGui::PopStyleColor( 3 );
			
			ImGui::End();
			
			m_nMultiplier -= 0.1f;
			
			if ( m_nMultiplier < 1.0f ) {
				m_nMultiplier = 0.0f;
			}
		}
		
		private array<StyleData> m_StyleTitles;
		private array<StyleType> m_StyleStack;
		
		private LevelRank m_nRank = LevelRank::RankD;
		private float m_nMultiplier = 0.0f;
		private uint m_nTimeSinceLastKill = 0;
		private uint m_nTimeSinceLastPush = 0;
		private uint m_nKillCounter = 0;
		private vec4 m_FreshnessColor = vec4( 0.0f );
		private float m_nFreshnessAmount = 1.0f;
		private int m_nFreshnessLevel = 2;
		private WeaponObject@ m_LastUsedWeapon = null;
		private uint m_nTotalStyle = 0;
	};

	StyleTracker StyleInfo;
};