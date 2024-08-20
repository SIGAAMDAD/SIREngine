namespace TheNomad::GameSystem::SaveSystem {
    interface ObjectDataSync {
        void Write( const SaveSection& in sync );
        void Read( const LoadSection& in sync );
    };
};