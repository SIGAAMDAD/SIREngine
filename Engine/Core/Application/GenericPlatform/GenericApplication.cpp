#include "GenericApplication.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <Engine/Core/ThreadSystem/Thread.h>

IGenericApplication *g_pApplication;
FileSystem::CFileSystem *g_pFileSystem;

CVar<uint32_t> e_MaxFPS(
    "e.MaxFPS",
    60,
    Cvar_Save,
    "Sets the engine's maximum screen refresh rate (frames per second).",
    CVG_NONE
);

IGenericApplication::IGenericApplication( void )
{
}

IGenericApplication::~IGenericApplication()
{
}

void IGenericApplication::Shutdown( void )
{
    delete g_pRenderContext;
    delete g_pFileSystem;

    CLogManager::ShutdownLogger();
    Mem_Shutdown();
}

void IGenericApplication::Init( void )
{
    //
    // initialize the engine
    //
    CLogManager::LaunchLoggingThread();

    CThread initThread( "FileSystemInit" );

    auto initFilesystem = [&]() -> void {
        g_pFileSystem = new FileSystem::CFileSystem();
    };
    initThread.Start( initFilesystem );

    g_pRenderContext = IRenderContext::CreateRenderContext();

    initThread.Join();
}

void IGenericApplication::Run( void )
{
}

void IGenericApplication::OnOutOfMemory( void )
{
}
