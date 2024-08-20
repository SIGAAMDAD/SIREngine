namespace TheNomad::Engine::FileSystem {
    enum OpenMode {
        Read,
        Write,
        Append,
        ReadWrite
    };

    class FileStream {
        FileStream() {
        }
        FileStream( const string& in npath ) {
            Open( npath, OpenMode::ReadWrite );
        }
        FileStream( const string& in npath, OpenMode mode ) {
            Open( npath, mode );
        }
        ~FileStream() {
            Close();
        }

        bool Open( const string& in npath, OpenMode mode ) {
            switch ( mode ) {
            case OpenMode::Read:
                m_FileHandle = OpenFileRead( npath );
                break;
            case OpenMode::Write:
                m_FileHandle = OpenFileWrite( npath );
                break;
            case OpenMode::Append:
                m_FileHandle = OpenFileAppend( npath );
                break;
            case OpenMode::ReadWrite:
                m_FileHandle = OpenFileRW( npath );
                break;
            default:
                GameError( "FileStream::Open: bad open mode " + mode );
            };

            m_FileMode = mode;
            return m_FileHandle != FS_INVALID_HANDLE;
        }
        void Close() {
            if ( m_FileHandle != FS_INVALID_HANDLE ) {
                CloseFile( m_FileHandle );
            }
            m_FileHandle = FS_INVALID_HANDLE;
        }

        const string& GetPath() const {
            return m_FilePath;
        }
        OpenMode GetMode() const {
            return m_FileMode;
        }

        protected string m_FilePath;
        protected OpenMode m_FileMode;
        protected int m_FileHandle;
    };
};