#include "GenericApplication.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <Engine/Core/ThreadSystem/Thread.h>
#include <Engine/Core/Serialization/JSon/JsonCache.h>

IGenericApplication *g_pApplication;
FileSystem::CFileSystem *g_pFileSystem;

uint32_t g_nMaxFPS = 60;
CVarRef<uint32_t> e_MaxFPS(
    "e.MaxFPS",
    g_nMaxFPS,
    Cvar_Save,
    "Sets the engine's maximum screen refresh rate (frames per second).",
    CVG_NONE
);

uint64_t g_nFrameNumber = 0;
CVarRef<uint64_t> e_FrameNumber(
    "e.FrameNumber",
    g_nFrameNumber,
    Cvar_Default,
    "The current frame index.",
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
    g_pRenderContext->Shutdown();
    g_ConsoleManager.SaveConfig();

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
    g_pFileSystem = new FileSystem::CFileSystem();

    CLogManager::LaunchLoggingThread();

    g_ConsoleManager.RegisterCVar( &e_MaxFPS );
    g_ConsoleManager.RegisterCVar( &e_FrameNumber );

    g_pRenderContext = IRenderContext::CreateRenderContext();

    g_ConsoleManager.LoadConfig();
}

void IGenericApplication::Run( void )
{
}

void IGenericApplication::OnOutOfMemory( void )
{
}
