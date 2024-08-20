namespace TheNomad::SGame {
    class LevelStats {
		LevelStats() {
		}
		LevelStats() {
		}

		void CalcLevelStats() {
			LevelInfoData@ data = @LevelManager.GetCurrentData();
			LevelRank rank;

			rank = LevelRank::RankWereUBotting;
			if ( numKills > data.m_RankU.minKills ) {
				rank = LevelRank::RankF;
			}
			if ( numKills > data.m_RankF.minKills ) {
				rank = LevelRank::RankD;
			}
			if ( numKills > data.m_RankD.minKills ) {
				rank = LevelRank::RankC;
			}
			if ( numKills > data.m_RankC.minKills ) {
				rank = LevelRank::RankB;
			}
			if ( numKills > data.m_RankB.minKills ) {
				rank = LevelRank::RankA;
			}
			if ( numKills > data.m_RankA.minKills ) {
				rank = LevelRank::RankS;
			}
			kills_Rank = rank;

			rank = LevelRank::RankWereUBotting;
			if ( stylePoints > data.m_RankU.minStyle ) {
				rank = LevelRank::RankF;
			}
			if ( stylePoints > data.m_RankF.minStyle ) {
				rank = LevelRank::RankD;
			}
			if ( stylePoints > data.m_RankD.minStyle ) {
				rank = LevelRank::RankC;
			}
			if ( stylePoints > data.m_RankC.minStyle ) {
				rank = LevelRank::RankB;
			}
			if ( stylePoints > data.m_RankB.minStyle ) {
				rank = LevelRank::RankA;
			}
			if ( stylePoints > data.m_RankA.minStyle ) {
				rank = LevelRank::RankS;
			}
			style_Rank = rank;

			rank = LevelRank::RankWereUBotting;
			if ( numDeaths > data.m_RankS.maxDeaths ) {
				rank = LevelRank::RankA;
			}
			if ( numDeaths > data.m_RankA.maxDeaths ) {
				rank = LevelRank::RankB;
			}
			if ( numDeaths > data.m_RankB.maxDeaths ) {
				rank = LevelRank::RankC;
			}
			if ( numDeaths > data.m_RankC.maxDeaths ) {
				rank = LevelRank::RankD;
			}
			if ( numDeaths > data.m_RankD.maxDeaths ) {
				rank = LevelRank::RankF;
			}
			if ( numDeaths > data.m_RankF.maxDeaths ) {
				rank = LevelRank::RankWereUBotting;
			}
			deaths_Rank = rank;

			rank = LevelRank::RankWereUBotting;
			if ( stylePoints > data.m_RankU.minStyle ) {
				rank = LevelRank::RankF;
			}
			if ( stylePoints > data.m_RankF.minStyle ) {
				rank = LevelRank::RankD;
			}
			if ( stylePoints > data.m_RankD.minStyle ) {
				rank = LevelRank::RankC;
			}
			if ( stylePoints > data.m_RankC.minStyle ) {
				rank = LevelRank::RankB;
			}
			if ( stylePoints > data.m_RankB.minStyle ) {
				rank = LevelRank::RankA;
			}
			if ( stylePoints > data.m_RankA.minStyle ) {
				rank = LevelRank::RankS;
			}
			style_Rank = rank;
		}

		private bool AllRanksAre( LevelRank rank ) const {
			return ( style_Rank == rank && kills_Rank == rank && deaths_Rank == rank && time_Rank == rank );
		}

		void CalcTotalLevelStats() {
			LevelRank highestRank;
			uint numHighestRanks = 0;

			CalcLevelStats();

			// add bonus
			if ( collateralScore == 0 ) {
				totalScore += 1000;
			}
			if ( numDeaths == 0 ) {
				totalScore += 1000;
			}

			// absolute perfection
			if ( AllRanksAre( LevelRank::RankS ) && collateralScore == 0 ) {
				total_Rank = LevelRank::RankS;
				return;
			}

			highestRank = GetHighestRank();
			if ( style_Rank == highestRank ) {
				numHighestRanks++;
			}
			if ( kills_Rank == highestRank ) {
				numHighestRanks++;
			}
			if ( deaths_Rank == highestRank ) {
				numHighestRanks++;
			}
			if ( time_Rank == highestRank ) {
				numHighestRanks++;
			}
			if ( numHighestRanks >= 3 ) {
				total_Rank = highestRank;
			} else {
				total_Rank = LevelRank( uint( highestRank ) - 1 );
			}

			// apply collateral damages
			if ( collateralScore > 0 ) {
				ApplyCollateral();
			}
		}

		private void DrawEndOfLevelStats() const {
			const float fontScale = ImGui::GetFontScale();
			const float scale = TheNomad::GameSystem::GameManager.GetUIScale();
			ImGuiWindowFlags windowFlags = ImGui::MakeWindowFlags( ImGuiWindowFlags::NoTitleBar | ImGuiWindowFlags::NoMove
				| ImGuiWindowFlags::NoResize | ImGuiWindowFlags::NoBringToFrontOnFocus );
			
			ImGui::Begin( "##LevelStatsShow", null, windowFlags );

			// ensure we aren't drawing anything over this
			TheNomad::Engine::CvarSet( "g_paused", "0" );

			ImGui::SetWindowPos( vec2( 256 * scale, 64 * scale ) );
			ImGui::SetWindowSize( vec2( 764 * scale, 500 * scale ) );

			TheNomad::Engine::UserInterface::SetActiveFont( TheNomad::Engine::UserInterface::Font_RobotoMono );
			ImGui::SetWindowFontScale( fontScale * 2.0f );

			ImGui::Text( LevelManager.GetLevelName() );
			
			ImGui::BeginTable( "##LevelStatsNumbersEndOfLevel", 2 );
			{
				ImGui::TableNextColumn();

				ImGui::BeginTable( "##LevelStatsIndividualEndOfLevel", 3 );
				{
					ImGui::SetWindowFontScale( ( fontScale * 2.5f ) );

					ImGui::TableNextColumn();
					ImGui::Text( "TIME" );
					ImGui::TableNextColumn();
					ImGui::Text( formatUInt( m_TimeMinutes ) + ":" + m_TimeSeconds + "." + m_TimeMilliseconds );
					ImGui::PushStyleColor( ImGuiCol::Text, sgame_RankStringColors[ time_Rank ] );
					ImGui::TableNextColumn();
					ImGui::Text( sgame_RankStrings[ time_Rank ] );
					ImGui::PopStyleColor();

					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::Text( "KILLS" );
					ImGui::TableNextColumn();
					ImGui::Text( formatUInt( numKills ) );
					ImGui::PushStyleColor( ImGuiCol::Text, sgame_RankStringColors[ kills_Rank ] );
					ImGui::TableNextColumn();
					ImGui::Text( sgame_RankStrings[ kills_Rank ] );
					ImGui::PopStyleColor();

					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::Text( "STYLE" );
					ImGui::TableNextColumn();
					ImGui::Text( formatUInt( stylePoints ) );
					ImGui::PushStyleColor( ImGuiCol::Text, sgame_RankStringColors[ style_Rank ] );
					ImGui::TableNextColumn();
					ImGui::Text( sgame_RankStrings[ style_Rank ] );
					ImGui::PopStyleColor();
				}
				ImGui::EndTable();

				ImGui::TableNextColumn();

				ImGui::PushStyleColor( ImGuiCol::Text, sgame_RankStringColors[ total_Rank ] );
				ImGui::SetWindowFontScale( ( fontScale * 4.5f ) );
				ImGui::Text( sgame_RankStrings[ total_Rank ] );
				ImGui::SetWindowFontScale( ( fontScale * 2.5f ) );
				ImGui::PopStyleColor();

				ImGui::TableNextRow();

				ImGui::Separator();

				ImGui::TableNextColumn();

				ImGui::Text( "- " );
				ImGui::SameLine();
				if ( collateralScore > 0 ) {
					ImGui::PushStyleColor( ImGuiCol::Text, colorRed );
					ImGui::Text( formatUInt( collateralScore ) );
					ImGui::PopStyleColor();
					ImGui::SameLine();
					ImGui::Text( " COLLATERAL" );
				} else {
					ImGui::PushStyleColor( ImGuiCol::Text, colorGreen );
					ImGui::Text( "NO CLEANUP (+1000)" );
					ImGui::PopStyleColor();
				}

				ImGui::Text( "- " );
				ImGui::SameLine();
				if ( numDeaths > 0 ) {
					ImGui::PushStyleColor( ImGuiCol::Text, colorRed );
					ImGui::Text( formatUInt( numDeaths ) );
					ImGui::PopStyleColor();
					ImGui::SameLine();
					ImGui::Text( " DEATHS" );
				} else {
					ImGui::PushStyleColor( ImGuiCol::Text, colorGreen );
					ImGui::Text( "NO RESTARTS (+1000)" );
					ImGui::PopStyleColor();
				}

				ImGui::TableNextColumn();

				// TODO:
				ImGui::Text( "CHALLENGE:" );

				if ( isClean ) {
					ImGui::Text( "- " );
					ImGui::SameLine();
					ImGui::PushStyleColor( ImGuiCol::Text, colorGreen );
					ImGui::Text( "CLEAN RUN" );
					ImGui::PopStyleColor();
				}
				
				ImGui::Text( "TOTAL " + totalScore );
			}
			ImGui::EndTable();
			
			if ( ImGui::Button( "DONE" ) ) {
				selectedSfx.Play();
				GlobalState = GameState::EndOfLevel;
			}

			ImGui::End();
		}
		
		void Draw( bool endOfLevel, uint64 timer ) {
			ImGuiWindowFlags windowFlags = ImGui::MakeWindowFlags( ImGuiWindowFlags::NoTitleBar | ImGuiWindowFlags::NoMove
				| ImGuiWindowFlags::NoResize );
			const float scale = TheNomad::GameSystem::GameManager.GetUIScale();

			if ( GlobalState != GameState::StatsMenu ) {
				if ( !TheNomad::Engine::IsKeyDown( TheNomad::Engine::KeyNum::Tab )
					&& !TheNomad::Engine::IsKeyDown( TheNomad::Engine::KeyNum::GamePad_Back ) )
				{
					return;
				}
			}

			if ( !endOfLevel ) {
				m_TimeMilliseconds = TheNomad::GameSystem::GameManager.GetGameTic() - timer;
			} else {
				m_TimeMilliseconds = timer;
			}
			m_TimeSeconds = m_TimeMilliseconds / 1000;
			m_TimeMinutes = m_TimeMilliseconds / 60000;
			
			if ( endOfLevel ) {
				DrawEndOfLevelStats();
				return;
			}

			ImGui::Begin( "##LevelStatsShow", null, windowFlags );
			ImGui::SetWindowPos( vec2( 16 * scale, 16 * scale ) );
			ImGui::SetWindowSize( vec2( 256 * scale, 72 * scale ) );
			
			ImGui::BeginTable( "##LevelStatsNumbers", 2 );
			
			//
			// time
			//
			ImGui::TableNextColumn();
			ImGui::Text( "TIME:" );
			ImGui::TableNextColumn();

			ImGui::Text( formatUInt( m_TimeMinutes ) + ":" + formatUInt( m_TimeSeconds ) + "." + formatUInt( m_TimeMilliseconds ) );
			ImGui::SameLine();
			ImGui::TextColored( sgame_RankStringColors[ time_Rank ], sgame_RankStrings[ time_Rank ] );
			
			ImGui::TableNextRow();
			
			//
			// kills
			//
			ImGui::TableNextColumn();
			ImGui::Text( "KILLS:" );
			ImGui::TableNextColumn();
			ImGui::Text( formatUInt( numKills ) );
			ImGui::SameLine();
			ImGui::TextColored( sgame_RankStringColors[ kills_Rank ], sgame_RankStrings[ kills_Rank ] );
			
			ImGui::TableNextRow();
			
			//
			// deaths
			//
			ImGui::TableNextColumn();
			ImGui::Text( "DEATHS:" );
			ImGui::TableNextColumn();
			ImGui::Text( formatUInt( numDeaths ) );
			ImGui::SameLine();
			ImGui::TextColored( sgame_RankStringColors[ deaths_Rank ], sgame_RankStrings[ deaths_Rank ] );
			
			ImGui::TableNextRow();
			
			//
			// style
			//
			ImGui::TableNextColumn();
			ImGui::Text( "STYLE:" );
			ImGui::TableNextColumn();
			ImGui::Text( formatUInt( stylePoints ) );
			ImGui::SameLine();
			ImGui::TextColored( sgame_RankStringColors[ style_Rank ], sgame_RankStrings[ style_Rank ] );
			
			ImGui::EndTable();
			
			ImGui::End();
		}

		//
		// LevelStats::ApplyCollateral: collateral damage (killing innocents, unnecessary destruction)
		// will cost the ranking to go down a lot
		//
		private void ApplyCollateral() {
			LevelInfoData@ data = @LevelManager.GetCurrentData();

			if ( total_Rank == LevelRank::RankS && ( collateralScore >= data.m_RankS.maxCollateral || data.m_RankS.requiresClean ) ) {
				total_Rank = LevelRank::RankA;
			}
			if ( total_Rank == LevelRank::RankA && ( collateralScore >= data.m_RankA.maxCollateral || data.m_RankA.requiresClean ) ) {
				total_Rank = LevelRank::RankB;
			}
			if ( total_Rank == LevelRank::RankB && ( collateralScore >= data.m_RankB.maxCollateral || data.m_RankB.requiresClean ) ) {
				total_Rank = LevelRank::RankC;
			}
			if ( total_Rank == LevelRank::RankC && ( collateralScore >= data.m_RankC.maxCollateral || data.m_RankC.requiresClean ) ) {
				total_Rank = LevelRank::RankD;
			}
			if ( total_Rank == LevelRank::RankD && ( collateralScore >= data.m_RankD.maxCollateral || data.m_RankD.requiresClean ) ) {
				total_Rank = LevelRank::RankF;
			}
			if ( total_Rank == LevelRank::RankF && ( collateralScore >= data.m_RankF.maxCollateral || data.m_RankF.requiresClean ) ) {
				total_Rank = LevelRank::RankWereUBotting;
			}
		}
		private LevelRank GetHighestRank() const {
			LevelRank rank = kills_Rank;

			if ( rank < style_Rank ) {
				rank = style_Rank;
			}
			if ( rank < deaths_Rank ) {
				rank = deaths_Rank;
			}
			if ( rank < time_Rank ) {
				rank = time_Rank;
			}
			return rank;
		}
		private LevelRank GetLowestRank() const {
			LevelRank rank = kills_Rank;

			if ( rank > style_Rank ) {
				rank = style_Rank;
			}
			if ( rank > deaths_Rank ) {
				rank = deaths_Rank;
			}
			if ( rank > time_Rank ) {
				return rank;
			}
			return rank;
		}

		uint64 m_TimeMilliseconds = 0;
		int64 m_TimeSeconds = 0;
		int64 m_TimeMinutes = 0;

		uint stylePoints = 0;
		uint numKills = 0;
		uint numDeaths = 0;
		uint collateralScore = 0;
		bool isClean = true;
		uint totalScore = 0;
		uint numSecrets = 0;

		LevelRank total_Rank = LevelRank::RankS;
		LevelRank time_Rank = LevelRank::RankS;
		LevelRank style_Rank = LevelRank::RankS;
		LevelRank kills_Rank = LevelRank::RankS;
		LevelRank deaths_Rank = LevelRank::RankS;
	};
};