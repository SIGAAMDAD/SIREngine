namespace TheNomad::SGame {
    class CheatManager {
        CheatManager() {
        }

        //
        // ToggleAllOn_f: /iamapussy
        //
        void ToggleAllOn_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            ConsolePrint( "IAmAPussyMode On\n" );
            ConsolePrint( "Toggling All Cheats ON:\n" );
            ConsolePrint( "BitchMode On\n" );
            ConsolePrint( "BlindMobs On\n" );
            ConsolePrint( "DeafMobs On\n" );
            ConsolePrint( "AmericaFuckYeah On\n" );
            ConsolePrint( "TooAngryToDie On\n" );
            ConsolePrint( "InfiniteHealth On\n" );

            TheNomad::Engine::CvarSet( "sgame_cheat_GodMode", "1" );
            TheNomad::Engine::CvarSet( "sgame_cheat_BlindMobs", "1" );
            TheNomad::Engine::CvarSet( "sgame_cheat_DeafMobs", "1" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteAmmo", "1" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteHealth", "1" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteRage", "1" );
        }

        //
        // ToggleAllOff_f: /imnotapussy
        //
        void ToggleAllOff_f() {
            bool allEnabled = false;

            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheat_GodMode" ) == 1
                && TheNomad::Engine::CvarVariableInteger( "sgame_cheat_BlindMobs" ) == 1
                && TheNomad::Engine::CvarVariableInteger( "sgame_cheat_DeafMobs" ) == 1
                && TheNomad::Engine::CvarVariableInteger( "sgame_cheat_InfiniteAmmo" ) == 1
                && TheNomad::Engine::CvarVariableInteger( "sgame_cheat_InfiniteHealth" ) == 1
                && TheNomad::Engine::CvarVariableInteger( "sgame_cheat_InfiniteRage" ) == 1 )
            {
                allEnabled = true;
            }

            if ( !allEnabled ) {
                ConsolePrint( "IAmNotAPussy On\n" );
            } else {
                ConsolePrint( "TestosteroneMode On\n" );
            }
            ConsolePrint( "Toggling All Cheats OFF:\n" );
            ConsolePrint( "BitchMode Off\n" );
            ConsolePrint( "BlindMobs Off\n" );
            ConsolePrint( "DeafMobs Off\n" );
            ConsolePrint( "AmericaFuckYeah Off\n" );
            ConsolePrint( "TooAngryToDie Off\n" );
            ConsolePrint( "InfiniteHealth Off\n" );

            TheNomad::Engine::CvarSet( "sgame_cheat_GodMode", "0" );
            TheNomad::Engine::CvarSet( "sgame_cheat_BlindMobs", "0" );
            TheNomad::Engine::CvarSet( "sgame_cheat_DeafMobs", "0" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteAmmo", "0" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteHealth", "0" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteRage", "0" );
        }

        //
        // GodMode_f: /iwtbag
        //
        void GodMode_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            int toggle = TheNomad::Engine::CvarVariableInteger( "sgame_cheat_GodMode" );
            ConsolePrint( "BitchMode " + ( toggle == 1 ? "On" : "Off" ) + "\n" );
            TheNomad::Engine::CvarSet( "sgame_cheat_GodMode", toggle == 1 ? "0" : "1" );
        }

        //
        // BlindMobs_f: /blindmobs
        //
        void BlindMobs_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            int toggle = TheNomad::Engine::CvarVariableInteger( "sgame_cheat_BlindMobs" );
            ConsolePrint( "BlindMobs " + ( toggle == 1 ? "On" : "Off" ) + "\n" );
            TheNomad::Engine::CvarSet( "sgame_cheat_BlindMobs", toggle == 1 ? "0" : "1" );
        }

        //
        // DeafMobs: /deafmobs
        //
        void DeafMobs_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            int toggle = TheNomad::Engine::CvarVariableInteger( "sgame_cheat_DeafMobs" );
            ConsolePrint( "DeafMobs " + ( toggle == 1 ? "On" : "Off" ) + "\n" );
            TheNomad::Engine::CvarSet( "sgame_cheat_DeafMobs", toggle == 1 ? "0" : "1" );
        }

        //
        // InfiniteAmmo_f: /tgdnec
        //
        void InfiniteAmmo_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            int toggle = TheNomad::Engine::CvarVariableInteger( "sgame_cheat_InfiniteAmmo" );
            ConsolePrint( "AmericaFuckYeah " + ( toggle == 1 ? "On" : "Off" ) + "\n" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteAmmo", toggle == 1 ? "0" : "1" );
        }

        //
        // InfiniteHealth_f: /gggsham
        //
        void InfiniteHealth_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            int toggle = TheNomad::Engine::CvarVariableInteger( "sgame_cheat_InfiniteHealth" );
            ConsolePrint( "InfiniteHealth " + ( toggle == 1 ? "On" : "Off" ) + "\n" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteHealth", toggle == 1 ? "0" : "1" );
        }

        //
        // InfiniteRage_f: /ltfatd
        //
        void InfiniteRage_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            int toggle = TheNomad::Engine::CvarVariableInteger( "sgame_cheat_InfiniteRage" );
            ConsolePrint( "TooAngryToDie " + ( toggle == 1 ? "On" : "Off" ) + "\n" );
            TheNomad::Engine::CvarSet( "sgame_cheat_InfiniteRage", toggle == 1 ? "0" : "1" );
        }

        //
        // NoClip_f: /noclip
        //
        void NoClip_f() {
            if ( TheNomad::Engine::CvarVariableInteger( "sgame_cheats_enabled" ) == 0 ) {
                ConsolePrint( "Cheats not enabled, launch the game with cheats enabled or +set sgame_cheats_enabled 1\n" );
                return;
            }

            int toggle = TheNomad::Engine::CvarVariableInteger( "sgame_NoClip" );
            ConsolePrint( "noclip " + ( toggle == 1 ? "on" : "off" ) + "\n" );
            TheNomad::Engine::CvarSet( "sgame_NoClip", toggle == 1 ? "0" : "1" );
        }
    };

    CheatManager Cheats;

    void InitCheatCodes() {
        TheNomad::Engine::CommandSystem::CmdManager.AddCommand( @TheNomad::Engine::CommandSystem::CommandFunc( @Cheats.ToggleAllOff_f ),
            "iamnotapussy", true );

        TheNomad::Engine::CommandSystem::CmdManager.AddCommand( @TheNomad::Engine::CommandSystem::CommandFunc( @Cheats.ToggleAllOn_f ),
            "iamapussy", true );

        TheNomad::Engine::CommandSystem::CmdManager.AddCommand( @TheNomad::Engine::CommandSystem::CommandFunc( @Cheats.InfiniteAmmo_f ),
            "tgdnec", true );

        TheNomad::Engine::CommandSystem::CmdManager.AddCommand( @TheNomad::Engine::CommandSystem::CommandFunc( @Cheats.InfiniteRage_f ),
            "ltfatd", true );

        TheNomad::Engine::CommandSystem::CmdManager.AddCommand( @TheNomad::Engine::CommandSystem::CommandFunc( @Cheats.InfiniteHealth_f ),
            "ggsham", true );

        TheNomad::Engine::CommandSystem::CmdManager.AddCommand( @TheNomad::Engine::CommandSystem::CommandFunc( @Cheats.DeafMobs_f ),
            "deafmobs", true );
        
        TheNomad::Engine::CommandSystem::CmdManager.AddCommand( @TheNomad::Engine::CommandSystem::CommandFunc( @Cheats.BlindMobs_f ),
            "blindmobs", true );
    }
};