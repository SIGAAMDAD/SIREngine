#include "level.as"
#include "playr.as"

namespace TheNomad::SGame {
	shared enum StyleType {
		Style_CrushKill = 0,
		Style_MeleeKill,
		Style_NoScope,
		Style_RunAndGun,
		Style_DoubleKill,
		Style_TripleKill,
		Style_QuadKill,
		Style_Parry,
		Style_CounterParry,
		
		NumStyleTypes
	};
	
	shared class StyleEvent {
		StyleEvent( StyleType type ) {
			lifeTime = 0;
			endTime = uint( TheNomad::Engine::CvarVariableInteger( "sgame_StyleEventLifetime" ) );
			eventType = type;
		}
		
		void Draw() {
			lifeTime++;
		}
		
		StyleType eventType;
		uint lifeTime;
		uint endTime;
	};
	
	shared class StyleTracker {
		StyleTracker() {
			m_Rank = LevelRank::RankS;
			@m_Font = TheNomad::GameSystem::Font( "fonts/AlegreySans.ttf" );
			m_nUIScale = TheNomad::GameSystem::GetUIScale();
			
			m_StyleColors[ LevelRank::RankS ] = vec4( 1.0f, 1.0f, 0.0f, 1.0f );
			m_StyleColors[ LevelRank::RankA ] = vec4( 1.0f, 1.0f, 0.0f, 1.0f );
		}
		
		private void PopEvent() {
			int j;
			
			m_nEvents--;
			
			j = 0;
			for ( int i = 1; i < m_nEvents; i++, j++ ) {
				m_Events[j] = m_Events[i];
			}
			
			if ( m_nEvents < 0 ) {
				GameError( "StyleTracker::PopEvent: style event stack underflow" );
			}
		}
		
		private void PushEvent( StyleType type ) {
			if ( m_nEvents == 10 ) {
				PopEvent();
			} else if ( m_nEvents > 10 ) {
				GameError( "StyleTracker::PushEvent: style event stack overflow" );
			}
			
			GetLevelManager().GetStyleData().stylePoints += m_nStylePoints[ type ];
			m_Events[m_nEvents] = StyleEvent( type );
			m_nEvents++;
		}
		
		void AddParryEvent( bool perfect ) {
			const StyleType type = perfect ? Style_CounterParry : Style_Parry;
			
			GetLevelManager().GetStyleData().stylePoints += m_nStylePoints[ type ];
			m_nEvents++;
		}
		
		void Draw() {
			const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
			const float fontScale = ImGui::GetWindowFontScale();
			
			ImGui::Begin( "##StyleEventsWindow", null, windowFlags );
			
			TheNomad::GameSystem::SetFont( m_Font );
			ImGui::SetWindowFontScale( fontScale * 2.5f );
			ImGui::ColoredText( m_StyleColors[ m_Rank ], m_StyleTitles[ m_Rank ] );
			ImGui::SetWindowFontScale( fontScale * 1.5f );
			
			if ( m_Events[0].lifeTime > m_Events[0].endTime ) {
				PopEvent();
			}
			
			for ( uint i = 0; i < m_nEvents; i++ ) {
				m_Events[i].Draw();
			}
			
			ImGui::End();
		}
		
		private uint[] m_nStylePoints( StyleType::NumStyleTypes );
		
		private string[] m_StyleTitles( LevelRank::NumRanks );
		private vec4[] m_StyleColors( LevelRank::NumRanks );
		private TheNomad::GameSystem::Font@ m_Font;
		private StyleEvent[] m_Events( 10 );
		private int m_nEvents;
		private float m_nUIScale;
		float m_nMultiplier;
		LevelRank m_Rank;
	};
};