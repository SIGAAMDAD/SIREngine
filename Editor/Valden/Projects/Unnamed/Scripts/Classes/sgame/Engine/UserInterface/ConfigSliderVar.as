#include "Engine/UserInterface/ConfigVar.as"

namespace TheNomad::Engine::UserInterface {
    class ConfigSliderVar : ConfigVar {
        ConfigSliderVar() {
        }
        ConfigSliderVar( const string& in name, const string& in id, float value, float min, float max, uint varFlags ) {
            m_Name = name;
            m_nValue = value;
            m_nMinValue = min;
            m_nMaxValue = max;
            m_nType = 0;

            super( name, id, Convert().ToString( value ), varFlags );
        }

        ConfigSliderVar( const string& in name, const string& in id, int value, int min, int max, uint varFlags ) {
            m_Name = name;
            m_nValue = float( value );
            m_nMinValue = float( min );
            m_nMaxValue = float( max );
            m_nType = 1;

            super( name, id, Convert().ToString( value ), varFlags );
        }

        void Draw() {
            ImGui::Text( m_Name );

            ImGui::TableNextColumn();

            switch ( m_nType ) {
            case 0: {
                float value = ImGui::SliderFloat( m_Name + "##" + m_Id, m_nValue, m_nMinValue, m_nMaxValue );
                if ( value != m_nValue ) {
                    m_bModified = true;
                    m_nValue = value;
                } else {
                    m_bModified = false;
                }
                break; }
            case 1: {
                int value = ImGui::SliderInt( m_Name + "##" + m_Id, int( m_nValue ), int( m_nMinValue ), int( m_nMaxValue ) );
                if ( value != int( m_nValue ) ) {
                    m_bModified = true;
                    m_nValue = value;
                } else {
                    m_bModified = false;
                }
                break; }
            };

            ImGui::TableNextRow();
        }
        void Save() {
            TheNomad::Engine::CvarSet( m_Id, Convert().ToString( m_nValue ) );
        }

        private float m_nValue = 0.0f;
        private float m_nMinValue = 0.0f;
        private float m_nMaxValue = 0.0f;
        private int m_nType = 0;
    };
};