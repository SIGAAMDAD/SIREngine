#include "Engine/UserInterface/ConfigVar.as"

namespace TheNomad::Engine::UserInterface {
    class ConfigListVar : ConfigVar {
        ConfigListVar() {
        }
        ConfigListVar( const string& in name, const string& in id, const array<string>& in values, uint selected, uint varFlags ) {
            m_nCurrent = selected;
            m_Values = values;
            
            super( name, id, Convert().ToString( selected ), varFlags );
        }

        void Draw() {
            ImGui::Text( m_Name );

            ImGui::TableNextColumn();

            if ( ImGui::BeginCombo( m_Name + "##" + m_Id, m_Values[ m_nCurrent ] ) ) {
                for ( uint i = 0; i < m_Values.Count(); i++ ) {
                    if ( ImGui::Selectable( m_Values[ i ] + "##" + m_Id + i, ( m_nCurrent == i ) ) ) {
                        TheNomad::Engine::ResourceCache.GetSfx( "menu0.ogg" ).Play();
                        m_nCurrent = i;
                        m_bModified = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
        void Save() {
            TheNomad::Engine::CvarSet( m_Id, Convert().ToString( m_nCurrent ) );
        }

        private array<string> m_Values;
        private uint m_nCurrent = 0;
    };
};