namespace TheNomad::Engine::SoundSystem {
    class SoundEffect {
        SoundEffect() {
        }
        SoundEffect( int hSfx ) {
            m_hSfx = hSfx;
        }
        SoundEffect( const string& in fileName ) {
            Set( fileName );
        }

        void Set( int hSfx ) {
            m_hSfx = hSfx;
        }
        void Set( const string& in fileName ) {
            m_hSfx = RegisterSfx( fileName );
            if ( m_hSfx == FS_INVALID_HANDLE ) {
                ConsoleWarning( "failed to load sfx file '" + fileName + "'\n" );
            }
        }

        void Play() const {
            PlaySfx( m_hSfx );
        }
        void Play( const vec3& in origin ) const {
            PlayWorldSfx( origin, m_hSfx );
        }

        int opConv() const {
            return m_hSfx;
        }

        int opImplConv() const {
            return m_hSfx;
        }

        private int m_hSfx = FS_INVALID_HANDLE;
    };
};