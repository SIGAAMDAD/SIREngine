namespace TheNomad::Engine::FileSystem {
    class OutputStream : FileStream {
        OutputStream() {
        }
        OutputStream( const string& in npath ) {
            Open( npath, OpenMode::Write );
        }
    };
};