namespace TheNomad::Engine::Containers {
    class StrList {
        StrList() {
        }

        const string& GetString( uint nIndex ) const {
            return data[ nIndex ];
        }

        private array<string> data;
    };
};