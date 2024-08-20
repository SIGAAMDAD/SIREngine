namespace TheNomad::GameSystem {
    class GameEvent {
        GameEvent() {
        }

        uint GetType() const {
            return m_nType;
        }
    };

    class KeyEvent {
        KeyEvent() {
        }

        TheNomad::Engine::KeyNum m_KeyNum = TheNomad::Engine::KeyNum( 0 );
        bool m_bDown = false;
    };
};