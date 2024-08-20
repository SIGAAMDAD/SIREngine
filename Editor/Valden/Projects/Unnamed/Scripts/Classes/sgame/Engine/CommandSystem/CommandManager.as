namespace TheNomad::Engine::CommandSystem {
    funcdef void CommandFunc();

    class GameObjectCommand {
        GameObjectCommand() {
        }
    };
    class Command {
        Command() {
        }
        Command( const string& in name, CommandFunc@ fn, bool isLevelCommand ) {
            m_Name = name;
            @m_Function = @fn;
            m_bLevelCommand = isLevelCommand;
        }

        string m_Name;
        CommandFunc@ m_Function = null;
        bool m_bLevelCommand = false;
    };

    class CommandManager {
        CommandManager() {
        }
        ~CommandManager() {
            for ( uint i = 0; i < m_CommandList.Count(); i++ ) {
                @m_CommandList[i].m_Function = null;
                TheNomad::Engine::CmdRemoveCommand( m_CommandList[i].m_Name );
            }
            m_CommandList.Clear();
        }

        bool CheckCommand( const string& in name ) const {
            for ( uint i = 0; i < m_CommandList.Count(); i++ ) {
                if ( m_CommandList[i].m_Name == name ) {
                    if ( @m_CommandList[i].m_Function is null ) {
                        ConsoleWarning( "Command callback for \"" + name + "\" is null! (" + m_CommandList[i].m_Name + ")\n" );
                        return false;
                    } else if ( m_CommandList[i].m_bLevelCommand && TheNomad::SGame::GlobalState != TheNomad::SGame::GameState::InLevel ) {
                        return false;
                    }
                    m_CommandList[i].m_Function();
                    return true;
                }
            }
            return false;
        }

        void AddCommand( CommandFunc@ fn, const string& in name, bool levelCommand ) {
            m_CommandList.Add( Command( name, @fn, levelCommand ) );
            TheNomad::Engine::CmdAddCommand( name );
        }

        private array<Command> m_CommandList;
    };

    CommandManager CmdManager;
};