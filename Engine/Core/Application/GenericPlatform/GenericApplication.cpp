#include "GenericApplication.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <unistd.h>

IMemAlloc *g_pMemAlloc;

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

    g_pMemAlloc->Shutdown();
    free( g_pMemAlloc );
}

void IGenericApplication::Init( void )
{
    //
    // initialize the engine
    //

    g_pRenderContext = IRenderContext::CreateRenderContext();
}

void IGenericApplication::Run( void )
{
}

void IGenericApplication::OnOutOfMemory( void )
{
}
