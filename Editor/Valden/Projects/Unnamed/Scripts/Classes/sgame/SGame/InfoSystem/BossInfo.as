namespace TheNomad::SGame::InfoSystem {
    class Phase {
    };

    class BossInfo : InfoLoader {
        BossInfo() {
        }

        bool Load( json@ data ) {
            if ( !base.Load( @data ) ) {
                return false;
            }
            
            return true;
        }

        TheNomad::SGame::MobObject@ data = null;
        MobInfo@ base = null;
    };
};