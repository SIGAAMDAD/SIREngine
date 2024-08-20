namespace TheNomad::SGame {
    class MapSecret {
        MapSecret() {
        }
        MapSecret( MapCheckpoint@ checkpointTrigger ) {
            @m_Checkpoint = @checkpointTrigger;
        }
        
        MapCheckpoint@ m_Checkpoint = null;
        bool m_bActivated = false;
    };
};