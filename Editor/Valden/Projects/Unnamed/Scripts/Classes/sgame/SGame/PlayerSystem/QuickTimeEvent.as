namespace TheNomad::SGame {
    // we've all experienced them (probably)
    enum QuickTimeEventType {
        QTE_ButtonMash,      // spam that button
        QTE_ButtonPress,     // a singular button press, a lot like the ones found in Metal Gear Rising: Revengeance
        QTE_PrecisionButton  // a lot like the last stand in Middle Earth: Shadow of Mordor
    };

    class QuickTimeEvent : PlayerEvent {
        QuickTimeEvent() {
        }

        bool IsActive() const {
            return m_bActive;
        }
        bool Load( json@ json ) {
            return true;
        }
        void Activate( PlayrObject@ ent ) {
            ConsoleWarning( "QuickTimeEvent::Activate: called\n" );
        }
        void OnRunTic( PlayrObject@ ent ) {
            ConsoleWarning( "QuickTimeEvent::OnRunTic: called\n" );
        }

        protected QuickTimeEventType m_nType = QuickTimeEventType::QTE_ButtonMash;
        protected uint m_nDuration = 0;
        protected uint m_nStartTime = 0;
        protected bool m_bActive = false;
    };

    class ButtonMashEvent : QuickTimeEvent {
        ButtonMashEvent() {
        }

        void Init( TheNomad::Engine::KeyNum keyboard, TheNomad::Engine::KeyNum gamepad0, TheNomad::Engine::KeyNum gamepad1,
            uint nDuration, uint nClicks = 1 )
        {
            m_nDuration = nDuration;
            m_nClicks = nClicks;
            m_KeyboardNum = keyboard;
            m_GamepadNum0 = gamepad0;
            m_GamepadNum1 = gamepad1;
        }

        void Activate( PlayrObject@ ent ) {
            m_nStartTime = TheNomad::GameSystem::GameManager.GetGameTic();
            m_nClickCounter = 0;
            m_bActive = true;
        }

        void OnRunTic( PlayrObject@ ent ) {
        }

        private TheNomad::Engine::KeyNum m_KeyboardNum = TheNomad::Engine::KeyNum::X;
        private TheNomad::Engine::KeyNum m_GamepadNum0 = TheNomad::Engine::KeyNum::GamePad_X;
        private TheNomad::Engine::KeyNum m_GamepadNum1 = TheNomad::Engine::KeyNum::GamePad_X;
        private uint m_nClicks = 0;
        private uint m_nClickCounter = 0;
    };
};