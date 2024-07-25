#ifndef __RENDER_COMMON_HPP__
#define __RENDER_COMMON_HPP__

#pragma once

#include <Engine/Core/SIREngine.hpp>
#include <Engine/Core/ResourceDef.hpp>

#include "RenderContext.hpp"
#include "RenderBuffer.hpp"

typedef enum {
    BUFFER_TYPE_VERTEX,
    BUFFER_TYPE_INDEX,
    BUFFER_TYPE_UNIFORM,
} gpuBufferType_t;

#endif