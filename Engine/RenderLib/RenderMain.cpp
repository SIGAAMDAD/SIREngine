#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/RenderLib/RenderLib.h>
#include "Backend/RenderContext.h"

using namespace SIREngine;
using namespace SIREngine::RenderLib;
using namespace SIREngine::RenderLib::Backend;


//CVar<int32_t> render_DrawMode( "renderlib.DrawMode", "0", 0, "", CVG_RENDERER );
//CVar<int32_t> render_WindowMode( "renderlib.WindowMode", "0", 0, "", CVG_RENDERER );
//CVar<int32_t> render_TextureDetail( "renderlib.TextureDetail", "", 0, "", CVG_RENDERER );

namespace SIREngine::RenderLib {

CRenderer *g_pRenderLib;

CVar<uint32_t> render_AntiAliasingType(
	"r.AntiAliasingType",
	0,
	Cvar_Default | Cvar_Save,
	"Sets the anti-aliasing technique used.\n"
	"  0: None\n"
	"  1: 2x MSAA\n"
	"  2: 4x MSAA\n"
	"  3: 8x MSAA\n"
	"  4: 16x MSAA\n"
	"  5: 32x MSAA\n"
	"  6: 2x SSAA\n"
	"  7: 4x SSAA\n"
	"  8: SMAA\n"
	"  9: FXAA",
	CVG_RENDERER
);

CRenderer::CRenderer( void )
{
}

CRenderer::~CRenderer()
{
}

const char *CRenderer::GetName( void ) const
{
	return "RenderLib";
}

bool CRenderer::IsActive( void ) const
{
	return false;
}

uint32_t CRenderer::GetState( void ) const
{
	return 0;
}

void CRenderer::Init( void )
{
	Backend::g_pRenderContext = IRenderContext::CreateRenderContext();
}

void CRenderer::Shutdown( void )
{
	delete Backend::g_pRenderContext;
}

void CRenderer::Frame( int64_t msec )
{
	Backend::g_pRenderContext->BeginFrame();

	Backend::g_pRenderContext->SwapBuffers();
}

void CRenderer::SaveGame( void )
{
}

void CRenderer::LoadGame( void )
{
}

};