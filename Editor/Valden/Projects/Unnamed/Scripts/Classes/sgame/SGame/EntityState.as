namespace TheNomad::SGame {
	class EntityState {
		EntityState() {
		}

		void SetInfo( const string& in name, uint tics, uint num, const uvec2& in spriteOffset ) {
			m_Name = name;
			m_nTics = tics;
			m_nStateNum = num;
			m_SpriteOffset = spriteOffset;
			
			m_nTicker = 0;
		}
		
		const string& GetName() const {
			return m_Name;
		}
		
		void Log() const {
			ConsolePrint( "Name: " + m_Name + "\n" );
			ConsolePrint( "Tics: " + m_nTics + "\n" );
			ConsolePrint( "Id: " + m_nStateNum + m_nStateOffset + "\n" );
			ConsolePrint( "Sprite Offset: [" + m_SpriteOffset.x + ", " + m_SpriteOffset.y + "]\n" );
			ConsolePrint( "[Animation]\n" );
			m_Animation.Log();
		}

		bool Load( json@ data ) {
			array<json@> values;
			string base;
			json@ anim;

			if ( !data.get( "Id", m_Name ) ) {
				ConsoleWarning( "invalid state info, missing variable 'Id'\n" );
				return false;
			}
			if ( !data.get( "SpriteOffset", values ) ) {
				ConsoleWarning( "invalid state info, missing variable 'SpriteOffset'\n" );
				return false;
			} else {
				if ( values.Count() != 2 ) {
					GameError( "invalid state info, variable 'SpriteOffset' is not a uvec2" );
				}
				m_SpriteOffset.x = uint( values[0] );
				m_SpriteOffset.y = uint( values[1] );
			}
			if ( !data.get( "Tics", base ) ) {
				ConsoleWarning( "invalid state info, missing variable 'Tics'\n" );
				return false;
			}
			m_nTics = Convert().ToInt( base );
			if ( !data.get( "Entity", base ) ) {
				ConsoleWarning( "invalid state info, missing variable 'Entity'\n" );
				return false;
			} else {
				if ( Util::StrICmp( base, "player" ) == 0 ) {
					m_nStateOffset = 0;
				} else if ( InfoSystem::InfoManager.GetMobTypes().TryGetValue( base, m_nStateOffset ) ) {
				} else if ( InfoSystem::InfoManager.GetItemTypes().TryGetValue( base, m_nStateOffset ) ) {
				} else {
					GameError( "invalid state info, Entity \"" + base + "\" doesn't exist" );
				}
			}
			if ( data.get( "NextState", base ) ) {
				if ( Util::StrICmp( base, "this" ) == 0 ) {
					// this is a flip-flop state
					@m_NextState = @this;
				} else {
					@m_NextState = @StateManager.GetStateById( base );
					if ( @m_NextState is null ) {
						GameError( "invalid state info, variable NextState \"" + base + "\" isn't a valid state" );
					}
				}
			}
			if ( !data.get( "BaseNum", base ) ) {
				ConsoleWarning( "invalid state info, missing variable 'BaseNum'\n" );
				return false;
			} else {
				if ( !StateManager.GetBaseStateCache().TryGetValue( base, m_nStateNum ) ) {
					GameError( "invalid state info, variable BaseNum \"" + base + "\" isn't a valid state" );
				}
			}

			if ( @m_NextState is null ) {
				@m_NextState = @StateManager.GetStateForNum( ( m_nStateNum + m_nStateOffset ) + 1 );
			}
			if ( @m_NextState is null ) {
				ConsoleWarning( "EntityState::Load: next state is null\n" );
			}

			@anim = @data[ "Animation" ];
			if ( @anim is null ) {
				ConsoleWarning( "invalid state info, missing variable 'Animation'\n" );
				return false;
			} else {
				if ( !m_Animation.Load( @anim ) ) {
					return false;
				}
				m_Animation.SetState( @this );
			}

			ConsolePrint( "State \"" + m_Name + "\" registered with ID " + m_nStateNum + " and offset of " + m_nStateOffset +
				", with next state \"" + m_NextState.m_Name + "\" with " + m_nTics + " tics\n" );

			return true;
		}

		uint GetID() const {
			return m_nStateNum + m_nStateOffset;
		}

		void Reset() {
			m_nTicker = TheNomad::GameSystem::GameManager.GetGameTic();
		}
		
		const uvec2& GetSpriteOffset() const {
			return m_SpriteOffset;
		}
		bool Done() const {
			return TheNomad::GameSystem::GameManager.GetGameTic() - m_nTicker >= m_nTics;
		}
		uint64 GetTics() const {
			return m_nTicker;
		}
		EntityState@ Run() {
			if ( TheNomad::GameSystem::GameManager.GetGameTic() - m_nTicker > m_nTics ) {
				Reset();
				return @m_NextState !is null ? @m_NextState : @StateManager.GetStateForNum( ( m_nStateNum + m_nStateOffset ) + 1 );
			}
			m_Animation.Run();
			return @this;
		}
		EntityState@ Cycle() {
			return @m_NextState;
		}
		const Animation@ GetAnimation() const {
			return @m_Animation;
		}
		
		// static data
		private string m_Name;
		private uvec2 m_SpriteOffset = uvec2( 0 );
		private EntityState@ m_NextState = null;
		private Animation m_Animation;
		private uint64 m_nTics = 0;
		private uint m_nStateNum = 0;
		private uint m_nStateOffset = 0;

		// runtime data
		private uint64 m_nTicker = 0;
	};
};