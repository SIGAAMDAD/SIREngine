namespace TheNomad::Engine {
    class ResourceManager {
        ResourceManager() {
        }
        
        int GetShader( const string& in shader ) {
            int64 ret;
            if ( !m_ShaderCache.TryGetValue( shader, ret ) ) {
                ret = TheNomad::Engine::Renderer::RegisterShader( shader );
                if ( ret != -1 ) {
                    ConsolePrint( "- Loaded shader \"" + shader + "\".\n" );
                }
                m_ShaderCache.Add( shader, ret );
            }
            return int( ret );
        }
        TheNomad::Engine::SoundSystem::SoundEffect GetSfx( const string& in sfx ) {
            int64 ret;
            if ( !m_SfxCache.TryGetValue( sfx, ret ) ) {
                ret = TheNomad::Engine::SoundSystem::RegisterSfx( sfx );
                if ( ret != -1 ) {
                    ConsolePrint( "- Loaded sfx \"" + sfx + "\".\n" );
                }
                m_SfxCache.Add( sfx, ret );
            }
            return TheNomad::Engine::SoundSystem::SoundEffect( int( ret ) );
        }
        int GetTrack( const string& in npath ) {
            int64 ret;
            if ( !m_MusicCache.TryGetValue( npath, ret ) ) {
                ret = TheNomad::Engine::SoundSystem::RegisterTrack( npath );
                if ( ret != -1 ) {
                    ConsolePrint( "- Loaded music \"" + npath + "\".\n" );
                }
                m_MusicCache.Add( npath, ret );
            }
            return int( ret );
        }
        TheNomad::SGame::SpriteSheet@ GetSpriteSheet( const string& in shader, uint sheetWidth, uint sheetHeight,
            uint spriteWidth, uint spriteHeight )
        {
            TheNomad::SGame::SpriteSheet@ ret;
            if ( !m_SpriteSheetCache.TryGetValue( shader, @ret ) ) {
                @ret = TheNomad::SGame::SpriteSheet( shader, vec2( float( sheetWidth ), float( sheetHeight ) ),
                    vec2( float( spriteWidth ), float( spriteHeight ) ) );
                ConsolePrint( "- Loaded sprite sheet \"" + shader + "\".\n" );
                m_SpriteSheetCache.Add( shader, @ret );
            }
            return @ret;
        }

        void ClearCache() {
            m_ShaderCache.Clear();
            m_SfxCache.Clear();
            m_MusicCache.Clear();
            m_SpriteSheetCache.Clear();
        }

        private dictionary m_ShaderCache;
        private dictionary m_SfxCache;
        private dictionary m_MusicCache;
        private dictionary m_SpriteSheetCache;
    };

    ResourceManager ResourceCache;
};