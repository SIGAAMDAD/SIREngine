#ifndef __SIRENGINE_DRAWBUFFER_H__
#define __SIRENGINE_DRAWBUFFER_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "RenderLib.h"
#include <EASTL/stack.h>

namespace SIREngine::RenderLib {
    typedef struct {
        uint16_t szColor[4];
        int16_t szNormal[4];
        CVec3 position;
        CVec3 worldPos;
        CVec2 texCoords;
    } Poly_t;

    typedef struct {
        uint32_t nViewportX;
        uint32_t nViewportY;
        uint32_t nViewportWidth;
        uint32_t nViewportHeight;
        const Poly_t *pPolyList;
    } SceneInfo_t;

    class CDrawBuffer
    {
    public:
        CDrawBuffer( void )
        { m_SceneStack.get_container().reserve( 16 ); }
        ~CDrawBuffer()
        { }

        void AddPolyList( const Poly_t *pPolyList, uint64_t nPolyCount );

        void NewScene( const SceneInfo_t& sceneData );
        void FinishScene( void );
        void Reset( void );

        uint64_t GetSceneCount( void ) const;
    private:
        eastl::stack<SceneInfo_t> m_SceneStack;
        SceneInfo_t *m_pCurrentScene;
        uint64_t m_nRenderPassCount;
    };
};

#endif