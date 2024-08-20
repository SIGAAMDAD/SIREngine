#include "Engine/FileSystem/FileStream.as"
#include "Engine/FileSystem/OutputStream.as"
#include "Engine/FileSystem/InputStream.as"

namespace TheNomad::Engine::FileSystem {
    //
    // FileSystemManager:
    // a utility class for the engine's filesystem
    // note that all files opened through this class
    // are allocated as stack objects then returned
    // as references, so treat them as such.
    //
    // also note that all files opened through this will
    // be automatically closed upon program termination
    //
    class FileSystemManager {
        FileSystemManager() {
            m_HomePath = TheNomad::Engine::FileSystem::GetHomePath();
            m_BasePath = TheNomad::Engine::FileSystem::GetBasePath();
            m_BaseGameDir = TheNomad::Engine::FileSystem::GetBaseGameDir();
            m_GamePath = TheNomad::Engine::FileSystem::GetGamePath();

            ConsolePrint( "SGame FileSystem Initialized:\n" );
            ConsolePrint( "[HomePath] " + m_HomePath + "\n" );
            ConsolePrint( "[BasePath] " + m_BasePath + "\n" );
            ConsolePrint( "[BaseGameDir] " + m_BaseGameDir + "\n" );
            ConsolePrint( "[GamePath] " + m_GamePath + "\n" );
        }
        ~FileSystemManager() {
            for ( uint i = 0; i < m_OpenFiles.Count(); i++ ) {
                m_OpenFiles[i].Close();
                @m_OpenFiles[i] = null;
            }
            m_OpenFiles.Clear();
        }

        FileStream@ FileIsOpen( const string& in npath ) {
            for ( uint i = 0; i < m_OpenFiles.Count(); i++ ) {
                if ( m_OpenFiles[i].GetPath() == npath ) {
                    return @m_OpenFiles[i];
                }
            }
            return null;
        }
        
        string FileModeToString( OpenMode mode ) {
            switch ( mode ) {
            case OpenMode::Read: return "R";
            case OpenMode::Write: return "W";
            case OpenMode::Append: return "A";
            case OpenMode::ReadWrite: return "RW";
            default:
                GameError( "FileSystemManager::FileModeToString: bad mode " + uint( mode ) );
            };
            return "";
        }

        array<int8>@ LoadFileBuffer( const string& in npath ) {
            array<int8> buffer;

            if ( LoadFile( npath, buffer ) == 0 ) {
                return null;
            }
            return @buffer;
        }
        bool FileExists( const string& in npath ) const {
            return FileExists( npath );
        }

        const string& GetHomePath() const {
            return m_HomePath;
        }
        const string& GetBasePath() const {
            return m_BasePath;
        }
        const string& GetBaseGameDir() const {
            return m_BaseGameDir;
        }
        const string& GetGamePath() const {
            return m_GamePath;
        }

        OutputStream@ OpenWrite( const string& in npath ) {
            OutputStream@ stream = cast<OutputStream>( FileIsOpen( npath ) );
            if ( @stream is null ) {
                OutputStream outtie( npath );
                m_OpenFiles.Add( cast<FileStream>( @outtie ) );
                @stream = @outtie;
            }
            return @stream;
        }
        InputStream@ OpenRead( const string& in npath ) {
            InputStream@ stream = cast<InputStream>( FileIsOpen( npath ) );
            if ( @stream is null ) {
                InputStream innie( npath );
                m_OpenFiles.Add( cast<FileStream>( @innie ) );
                @stream = @innie;
            }
            return @stream;
        }
        FileStream@ OpenFile( const string& in npath, OpenMode mode ) {
            FileStream@ stream = FileIsOpen( npath );
            if ( @stream is null ) {
                FileStream na( npath, mode );
                m_OpenFiles.Add( @na );
                @stream = @na;
            }
            return @stream;
        }
        void CloseFile( FileStream@ file ) {
            file.Close();
            
            int index = m_OpenFiles.Find( @file );
            if ( index == -1 ) {
                GameError( "FileSystemManager::CloseFile: FileStream " + file.GetPath()
                    + " not opened through FileSystemManager, so don't use it to close the stream" );
            }
            m_OpenFiles.RemoveAt( index );
        }

        //==============================================================================
		// Commands
		//

        void OpenFiles_f() {
            ConsolePrint( "\n" );
            ConsolePrint( "Opened SGame Files:\n" );
            for ( uint i = 0; i < m_OpenFiles.Count(); i++ ) {
                ConsolePrint( m_OpenFiles[i].GetPath() + " " + FileModeToString( m_OpenFiles[i].GetMode() ) );
            }
            ConsolePrint( "\n" );
        }

        private string m_HomePath;
        private string m_BasePath;
        private string m_BaseGameDir;
        private string m_GamePath;

        private array<FileStream@> m_OpenFiles;
    };

    FileSystemManager@ FileManager;
};