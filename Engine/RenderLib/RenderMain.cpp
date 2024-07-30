#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/RenderLib/RenderLib.h>

CVar<int32_t> render_DrawMode( "renderlib.DrawMode", "0", 0, "", CVG_RENDERER );
CVar<int32_t> render_WindowMode( "renderlib.WindowMode", "0", 0, "", CVG_RENDERER );
CVar<int32_t> render_TextureDetail( "renderlib.TextureDetail", "", 0, "", CVG_RENDERER );

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

SIRENGINE_EXPORT_DEMANGLE void CRenderer::Init( void )
{

}

SIRENGINE_EXPORT_DEMANGLE void CRenderer::Shutdown( void )
{
}