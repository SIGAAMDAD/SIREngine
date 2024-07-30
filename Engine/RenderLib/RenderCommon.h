#ifndef __RENDER_COMMON_H__
#define __RENDER_COMMON_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Core/ResourceDef.h>

typedef enum {
    BUFFER_TYPE_VERTEX,
    BUFFER_TYPE_INDEX,
    BUFFER_TYPE_UNIFORM,
    BUFFER_TYPE_TEXTURE,

    BUFFER_TYPE_NONE
} GPUBufferType_t;

#include "Backend/RenderBuffer.h"
#include "Backend/RenderTexture.h"
#include "Backend/RenderShader.h"
#include "Backend/RenderProgram.h"
#include "Backend/RenderShaderBuffer.h"
#include "Backend/RenderShaderPipeline.h"
#include "Backend/RenderVertexArray.h"
#include "Backend/RenderFramebuffer.h"
#include "Backend/RenderContext.h"

#endif