#include "Engine/FileSystem/FileSystem.as"

namespace TheNomad::Engine {
    void Init() {
        @TheNomad::Engine::FileSystem::FileManager = TheNomad::Engine::FileSystem::FileSystemManager();
    }
};