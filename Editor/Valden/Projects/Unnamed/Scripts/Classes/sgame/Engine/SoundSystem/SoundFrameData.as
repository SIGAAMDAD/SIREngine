namespace TheNomad::Engine::SoundSystem {
    class SoundFrameData {
        SoundFrameData() {
            for ( uint i = 0; i < uint( TheNomad::SGame::sgame_MaxSoundChannels.GetInt() ); i++ ) {
                m_Channels.Add( null );
            }
        }
        
        void Flush() {
            for ( uint i = 0; i < m_nUsedChannels; i++ ) {
                m_Channels[i].Play();
            }
            m_nUsedChannels = 0;
        }

        void PushSfxToScene( SoundEffect& in sfx ) {
            if ( m_nUsedChannels == m_Channels.Count() ) {
                return;
            }

            @m_Channels[ m_nUsedChannels ] = @sfx;
            m_nUsedChannels++;
        }

        private array<SoundEffect@> m_Channels;
        private uint m_nUsedChannels = 0;
    };

    SoundFrameData@ SoundManager;
};