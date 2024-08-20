namespace TheNomad::Engine::FileSystem {
    class InputStream : FileStream {
        InputStream() {
        }
        InputStream( const string& in npath ) {
            Open( npath, OpenMode::Read );
        }
    };
};