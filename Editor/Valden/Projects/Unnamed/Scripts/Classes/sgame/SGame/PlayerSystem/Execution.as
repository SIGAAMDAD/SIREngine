#include "SGame/PlayerSystem/PlayerEvent.as"

namespace TheNomad::SGame {
    class Execution : PlayerEvent {
        Execution() {
        }

        bool IsActive() const {
            return m_bActive;
        }
        void OnRunTic( PlayrObject@ ent ) {
        }
		void Activate( PlayrObject@ ent ) {
        }
		bool Load( json@ json ) {
            return true;
        }

        private bool m_bActive = false;
    };
};