namespace TheNomad::SGame {
    // NOTE: in titanfall2 campaign opening cinematic, lastimosa does something that looks like
    // inputting commands to the jump kit, so maybe something like that?

    class JumpKit {
        void ResetDash() {
            m_nTimeSinceDash = TheNomad::GameSystem::GameManager.GetGameTic();
            m_nTimeSinceLastUse = m_nTimeSinceDash;
            m_nJumpKitCharges--;
        }
        uint64 GetTimeSinceLastDash() const {
            return TheNomad::GameSystem::GameManager.GetGameTic() - m_nTimeSinceDash;
        }
        void SetDashing( bool bDashing ) {
            m_bDashing = bDashing;
        }
        bool IsDashing() const {
            return m_bDashing;
        }

        void RunTic() {
            if ( TheNomad::GameSystem::GameManager.GetGameTic() - m_nTimeSinceLastUse > 5000 && m_nJumpKitCharges < 6 ) {
                m_nTimeSinceLastUse = TheNomad::GameSystem::GameManager.GetGameTic();
                m_nJumpKitCharges++;
            }
        }
        int NumCharges() const {
            return m_nJumpKitCharges;
        }

        private uint64 m_nTimeSinceDash = 0;
        private uint64 m_nTimeSinceLastUse = 0;

        private bool m_bDashing = false;

        private int m_nJumpKitCharges = 6;
    };
};