#ifndef __RENDER_CONTEXT_HPP__
#define __RENDER_CONTEXT_HPP__

#pragma once

#include <SDL2/SDL.h>
#include "RenderCommon.hpp"

#define WF_OPENGL_CONTEXT               0x1000
#define WF_VULKAN_CONTEXT               0x2000
#define WF_D3D11_CONTEXT                0x4000
#define WF_SOFTWARE_CONTEXT             0x8000
#define WF_CONTEXT_BITS                 0xf000

#define WF_MODE_FULLSCREEN              0x01000
#define WF_MODE_WINDOWED                0x02000
#define WF_MODE_BORDERLESS_FULLSCREEN   0x04000
#define WF_MODE_BORDERLESS_WINDOWED     0x08000
#define WF_MODE_BITS                    0x0f000

typedef struct {
    const char *pszWindowName;
    const char *pszAppName;
    uint64_t nAppVersion;
    
    int nWindowPosX;
    int nWindowPosY;
    int nWindowWidth;
    int nWindowHeight;

    unsigned eWindowFlags;
} ApplicationInfo_t;

class IRenderContext
{
public:
    IRenderContext( const ApplicationInfo_t& appInfo );
    virtual ~IRenderContext();
protected:
    virtual void GetGPUExtensionList( void ) = 0;

    ApplicationInfo_t m_AppInfo;
    SDL_Window *m_pWindow;

    uint32_t m_nGPUExtensionCount;
    const char **m_pGPUExtensionList;
};

extern IRenderContext *g_pRenderContext;

SIRENGINE_FORCEINLINE IRenderContext *GetRenderContext( void ) {
    return g_pRenderContext;
}

#endif