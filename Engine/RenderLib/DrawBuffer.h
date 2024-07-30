#ifndef __SIRENGINE_DRAWBUFFER_H__
#define __SIRENGINE_DRAWBUFFER_H__

#pragma once

#include "RenderLib.h"
#include <EASTL/stack.h>

typedef struct {
    uint32_t m_nViewportX;
    uint32_t m_nViewportY;
    uint32_t m_nViewportWidth;
    uint32_t m_nViewportHeight;
} SceneInfo_t;

typedef struct {
    uint16_t szColor[4];
    int16_t szNormal[4];
    CVec3 position;
    CVec3 worldPos;
    CVec2 texCoords;
} Poly_t;

class CDrawBuffer
{
public:
    CDrawBuffer( void );
    ~CDrawBuffer();

    void AddPoly();

    void NewScene( const SceneInfo_t& sceneData );
    void FinishScene( void );

    uint64_t GetSceneCount( void ) const;
private:
    eastl::stack<SceneInfo_t> m_SceneStack;
    SceneInfo_t *m_pCurrentScene;
};

#endif