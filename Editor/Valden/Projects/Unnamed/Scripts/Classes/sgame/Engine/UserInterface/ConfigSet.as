namespace TheNomad::Engine::UserInterface {
    funcdef void DrawConfig();

    class ConfigSet {
        ConfigSet( const string& in name, DrawConfig@ fn ) {
            m_Name = name;
            @m_DrawFunc = @fn;
        }

        const string& GetName() const {
            return m_Name;
        }
        DrawConfig@ GetDrawFunc() {
            return @m_DrawFunc;
        }

        void Draw() {
            if ( @m_DrawFunc !is null ) {
                m_DrawFunc();
                return;
            }

            for ( uint i = 0; i < m_Vars.Count(); i++ ) {
                m_Vars[i].Draw();
            }
        }

        void AddVar( ConfigVar@ var ) {
            m_Vars.Add( @var );
        }

        private string m_Name;
        private DrawConfig@ m_DrawFunc = null;
        private array<ConfigVar@> m_Vars;
    };
};