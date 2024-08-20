namespace TheNomad::SGame {
    class KeyBind {
		KeyBind() {
		}
		
		KeyBind& opAssign( const KeyBind& in other ) {
			down = other.down;
			downtime = other.downtime;
			msec = other.msec;
			active = other.active;
			return this;
		}

		void Down() {
			uint k;
			string c;

			c = TheNomad::Engine::CmdArgv( 1 );
			if ( c.Length() > 0 ) {
				k = Convert().ToUInt( c );
			}
			
			/*
			if ( down[0] == 0 ) {
				down[0] = k;
			} else if ( down[1] == 0 ) {
				down[1] = k;
			} else {
				ConsolePrint( "Three keys down for a button!\n" );
				return;
			}
			*/
			
			if ( active ) {
				return; // still down
			}

			// save the timestamp for partial frame summing
			c = TheNomad::Engine::CmdArgv( 2 );
			downtime = Convert().ToInt( c );
			
			active = true;
		}
		
		void Up() {
			int uptime;
			uint k;
			string c;

			c = TheNomad::Engine::CmdArgv( 1 );
			if ( c.Length() > 0 ) {
				k = Convert().ToUInt( c );
			} else {
				return;
			}

			// save timestamp for partial frame summing
			c = TheNomad::Engine::CmdArgv( 2 );
			uptime = Convert().ToInt( c );
			if ( uptime > 0 ) {
				msec += uptime - downtime;
			} else {
				msec += Game_FrameTime / 2;
			}
			active = false;
		}
		
		uvec2 down = uvec2( 0 );
		int downtime = 0;
		int msec = 0;
		bool active = false;
	};
};