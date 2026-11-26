/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_RENDERTYPES_H__
#define __SIR_RENDERTYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

// ===========================================================================
// Driver-independent RHI vocabulary.
//
// This header does not include a device, command list, OS window, graphics SDK,
// GRDG resource, or driver implementation. A backend validates all optional
// features against its reported capabilities and returns UNSUPPORTED when an
// operation cannot be represented on that backend.
//
// Descriptors are plain source-facing C++ value types. Pointers and SIRSpan
// members borrow storage; implementations must copy the required input before
// returning from calls that retain data for deferred compilation or execution.
//
// This is the canonical RHI source contract during the SIRGraphics -> SIRRender
// migration. It is not a binary-layout replacement for the separate v1 graphics
// module ABI. Converting v1 ABI descriptors requires an explicit adapter.
// ===========================================================================

enum class SIRRenderResult : uint8_t
{
	SIR_RENDER_SUCCESS = 0,
	SIR_RENDER_INVALID_ARGUMENT = 1,
	SIR_RENDER_INVALID_HANDLE = 2,
	SIR_RENDER_UNSUPPORTED = 3,
	SIR_RENDER_OUT_OF_MEMORY = 4,
	SIR_RENDER_CAPACITY_EXCEEDED = 5,
	SIR_RENDER_BUSY = 6,
	SIR_RENDER_TIMEOUT = 7,
	SIR_RENDER_SURFACE_CHANGED = 8,
	SIR_RENDER_DEVICE_LOST = 9,
	SIR_RENDER_DRIVER_ERROR = 10,
	SIR_RENDER_VERSION_MISMATCH = 11
};

enum class SIRRenderBackend : uint8_t
{
	SIR_RENDER_BACKEND_AUTO = 0,
	SIR_RENDER_BACKEND_VULKAN = 1,
	SIR_RENDER_BACKEND_D3D12 = 2,
	SIR_RENDER_BACKEND_D3D11 = 3,
	SIR_RENDER_BACKEND_D3D10 = 4,
	SIR_RENDER_BACKEND_D3D9 = 5,
	SIR_RENDER_BACKEND_D3D8 = 6,
	SIR_RENDER_BACKEND_D3D7 = 7,
	SIR_RENDER_BACKEND_OPENGL_ADVANCED = 8,
	SIR_RENDER_BACKEND_OPENGL_PROGRAMMABLE = 9,
	SIR_RENDER_BACKEND_OPENGL_FIXED_FUNCTION = 10,
	SIR_RENDER_BACKEND_METAL = 11,
	SIR_RENDER_BACKEND_SOFTWARE = 12,
	SIR_RENDER_BACKEND_NULL = 13,
	SIR_RENDER_BACKEND_CUSTOM = 14
};

// Pipeline capability and submission style are independent. A Vulkan backend
// may emulate a fixed pipeline; an OpenGL backend can execute modern shaders.
enum class SIRRenderPipelineModel : uint8_t
{
	SIR_RENDER_MODEL_MODERN = 1,
	SIR_RENDER_MODEL_PROGRAMMABLE = 2,
	SIR_RENDER_MODEL_FIXED_FUNCTION = 3
};

enum class SIRRenderExecutionModel : uint8_t
{
	SIR_RENDER_EXECUTION_EXPLICIT = 1,
	SIR_RENDER_EXECUTION_STATEFUL_SERIAL = 2
};

enum class SIRRenderQueueType : uint32_t
{
	SIR_RENDER_QUEUE_GRAPHICS = 0,
	SIR_RENDER_QUEUE_COMPUTE = 1,
	SIR_RENDER_QUEUE_TRANSFER = 2,
	SIR_RENDER_QUEUE_COUNT = 3
};

// Feature bits report backend support; they are not promises made by a type.
// A backend can support the graphics queue alone and emulate transfer on it.
enum class SIRRenderFeature : uint64_t
{
	SIR_RENDER_FEATURE_GRAPHICS = 1ULL << 0,
	SIR_RENDER_FEATURE_COMPUTE = 1ULL << 1,
	SIR_RENDER_FEATURE_ASYNC_COMPUTE = 1ULL << 2,
	SIR_RENDER_FEATURE_TRANSFER_QUEUE = 1ULL << 3,
	SIR_RENDER_FEATURE_STORAGE_RESOURCE = 1ULL << 4,
	SIR_RENDER_FEATURE_DESCRIPTOR_INDEXING = 1ULL << 5,
	SIR_RENDER_FEATURE_INDIRECT_DRAW = 1ULL << 6,
	SIR_RENDER_FEATURE_MULTIPLE_RENDER_TARGETS = 1ULL << 7,
	SIR_RENDER_FEATURE_MULTISAMPLE = 1ULL << 8,
	SIR_RENDER_FEATURE_TESSELLATION = 1ULL << 9,
	SIR_RENDER_FEATURE_GEOMETRY_SHADER = 1ULL << 10,
	SIR_RENDER_FEATURE_MESH_SHADER = 1ULL << 11,
	SIR_RENDER_FEATURE_RAY_TRACING = 1ULL << 12,
	SIR_RENDER_FEATURE_SPARSE_RESOURCES = 1ULL << 13,
	SIR_RENDER_FEATURE_SAMPLER_FEEDBACK = 1ULL << 14,
	SIR_RENDER_FEATURE_EXPLICIT_BARRIERS = 1ULL << 15,
	SIR_RENDER_FEATURE_GPU_UPLOAD = 1ULL << 16,
	SIR_RENDER_FEATURE_HDR_OUTPUT = 1ULL << 17,
	SIR_RENDER_FEATURE_FIXED_FUNCTION = 1ULL << 18,
	SIR_RENDER_FEATURE_PRESENT = 1ULL << 19,
	SIR_RENDER_FEATURE_BUFFER_TEXTURES = 1ULL << 20,
	SIR_RENDER_FEATURE_INPUT_ATTACHMENTS = 1ULL << 21,
	SIR_RENDER_FEATURE_TIMELINE_SYNC = 1ULL << 22,
	SIR_RENDER_FEATURE_DEVICE_ADDRESS = 1ULL << 23,
	SIR_RENDER_FEATURE_SHADER_BINDING_TABLE = 1ULL << 24,
	SIR_RENDER_FEATURE_SOFTWARE_RASTERIZATION = 1ULL << 25
};

enum class SIRRenderHandleType : uint32_t
{
	SIR_RENDER_HANDLE_NONE = 0,
	SIR_RENDER_HANDLE_BUFFER = 1,
	SIR_RENDER_HANDLE_TEXTURE = 2,
	SIR_RENDER_HANDLE_VIEW = 3,
	SIR_RENDER_HANDLE_SAMPLER = 4,
	SIR_RENDER_HANDLE_PIPELINE = 5,
	SIR_RENDER_HANDLE_BINDING_SET = 6,
	SIR_RENDER_HANDLE_SURFACE = 7,
	SIR_RENDER_HANDLE_QUERY_POOL = 8,
	SIR_RENDER_HANDLE_ACCELERATION_STRUCTURE = 9,
	SIR_RENDER_HANDLE_SHADER_BINDING_TABLE = 10,
	SIR_RENDER_HANDLE_FRAMEBUFFER = 11,
	SIR_RENDER_HANDLE_SWAPCHAIN = 12,
	SIR_RENDER_HANDLE_RENDERBUFFER = 13
};

// Memory *class* describes intended use. Memory *properties* describe optional
// host/device behavior. HOST_COHERENT is a property, not an allocation class.
enum class SIRRenderMemoryClass : uint8_t
{
	SIR_RENDER_MEMORY_GPU_LOCAL = 0,
	SIR_RENDER_MEMORY_CPU_UPLOAD = 1,
	SIR_RENDER_MEMORY_CPU_READBACK = 2,
	SIR_RENDER_MEMORY_GPU_UPLOAD = 3,
	SIR_RENDER_MEMORY_TRANSIENT = 4
};

enum class SIRRenderMemoryProperty : uint32_t
{
	SIR_RENDER_MEMORY_PROPERTY_DEVICE_LOCAL = 1U << 0,
	SIR_RENDER_MEMORY_PROPERTY_HOST_VISIBLE = 1U << 1,
	SIR_RENDER_MEMORY_PROPERTY_HOST_COHERENT = 1U << 2,
	SIR_RENDER_MEMORY_PROPERTY_HOST_CACHED = 1U << 3,
	SIR_RENDER_MEMORY_PROPERTY_PERSISTENT_MAPPING = 1U << 4
};

enum class SIRRenderBufferUsage : uint64_t
{
	SIR_RENDER_BUFFER_VERTEX = 1ULL << 0,
	SIR_RENDER_BUFFER_INDEX = 1ULL << 1,
	SIR_RENDER_BUFFER_CONSTANT = 1ULL << 2,
	SIR_RENDER_BUFFER_STORAGE = 1ULL << 3,
	SIR_RENDER_BUFFER_TEXEL = 1ULL << 4,
	SIR_RENDER_BUFFER_INDIRECT = 1ULL << 5,
	SIR_RENDER_BUFFER_TRANSFER_SOURCE = 1ULL << 6,
	SIR_RENDER_BUFFER_TRANSFER_DESTINATION = 1ULL << 7,
	SIR_RENDER_BUFFER_QUERY = 1ULL << 8,
	SIR_RENDER_BUFFER_ACCELERATION_STRUCTURE = 1ULL << 9,
	SIR_RENDER_BUFFER_SHADER_BINDING_TABLE = 1ULL << 10,
	SIR_RENDER_BUFFER_DEVICE_ADDRESS = 1ULL << 11
};

enum class SIRRenderTextureUsage : uint64_t
{
	SIR_RENDER_TEXTURE_SAMPLED = 1ULL << 0,
	SIR_RENDER_TEXTURE_STORAGE = 1ULL << 1,
	SIR_RENDER_TEXTURE_COLOR_ATTACHMENT = 1ULL << 2,
	SIR_RENDER_TEXTURE_DEPTH_STENCIL_ATTACHMENT = 1ULL << 3,
	SIR_RENDER_TEXTURE_TRANSFER_SOURCE = 1ULL << 4,
	SIR_RENDER_TEXTURE_TRANSFER_DESTINATION = 1ULL << 5,
	SIR_RENDER_TEXTURE_INPUT_ATTACHMENT = 1ULL << 6,
	SIR_RENDER_TEXTURE_PRESENT = 1ULL << 7
};

enum class SIRRenderResourceFlag : uint32_t
{
	SIR_RENDER_RESOURCE_EXTERNALLY_OWNED = 1U << 0,
	SIR_RENDER_RESOURCE_SPARSE = 1U << 1,
	SIR_RENDER_RESOURCE_ALIASABLE = 1U << 2
};

// Retain the explicitly numbered shared-graphics v1 format identities where
// they existed. New formats are appended; never infer an SDK format ordinal.
enum class SIRRenderFormat : uint32_t
{
	SIR_RENDER_FORMAT_UNKNOWN = 0,
	SIR_RENDER_FORMAT_RGBA8_UNORM = 1,
	SIR_RENDER_FORMAT_BGRA8_UNORM = 2,
	SIR_RENDER_FORMAT_RGBA8_SRGB = 3,
	SIR_RENDER_FORMAT_RGBA16_FLOAT = 4,
	SIR_RENDER_FORMAT_R8_UNORM = 5,
	SIR_RENDER_FORMAT_RG8_UNORM = 6,
	SIR_RENDER_FORMAT_R16_FLOAT = 7,
	SIR_RENDER_FORMAT_R32_FLOAT = 8,
	SIR_RENDER_FORMAT_R32_UINT = 9,
	SIR_RENDER_FORMAT_RG32_FLOAT = 10,
	SIR_RENDER_FORMAT_RGBA32_FLOAT = 11,
	SIR_RENDER_FORMAT_D16_UNORM = 12,
	SIR_RENDER_FORMAT_D24_UNORM_S8_UINT = 13,
	SIR_RENDER_FORMAT_D32_FLOAT = 14,
	SIR_RENDER_FORMAT_BC1_UNORM = 15,
	SIR_RENDER_FORMAT_BC3_UNORM = 16,
	SIR_RENDER_FORMAT_BC5_UNORM = 17,
	SIR_RENDER_FORMAT_BC7_UNORM = 18,
	SIR_RENDER_FORMAT_RG16_FLOAT = 19,
	SIR_RENDER_FORMAT_R8_UINT = 20,
	SIR_RENDER_FORMAT_R16_UINT = 21,
	SIR_RENDER_FORMAT_RGBA8_UINT = 22,
	SIR_RENDER_FORMAT_RGBA8_SNORM = 23,
	SIR_RENDER_FORMAT_BGRA8_SRGB = 24,
	SIR_RENDER_FORMAT_R10G10B10A2_UNORM = 25,
	SIR_RENDER_FORMAT_R11G11B10_FLOAT = 26,
	SIR_RENDER_FORMAT_BC1_SRGB = 27,
	SIR_RENDER_FORMAT_BC3_SRGB = 28,
	SIR_RENDER_FORMAT_BC7_SRGB = 29,
	SIR_RENDER_FORMAT_ETC2_RGBA8_UNORM = 30,
	SIR_RENDER_FORMAT_ASTC_4X4_UNORM = 31
};

enum class SIRRenderTextureDimension : uint8_t
{
	SIR_RENDER_TEXTURE_1D = 1,
	SIR_RENDER_TEXTURE_2D = 2,
	SIR_RENDER_TEXTURE_3D = 3,
	SIR_RENDER_TEXTURE_CUBE = 4
};

enum class SIRRenderTextureAspect : uint32_t
{
	SIR_RENDER_ASPECT_COLOR = 1U << 0,
	SIR_RENDER_ASPECT_DEPTH = 1U << 1,
	SIR_RENDER_ASPECT_STENCIL = 1U << 2,
	SIR_RENDER_ASPECT_PLANE_0 = 1U << 3,
	SIR_RENDER_ASPECT_PLANE_1 = 1U << 4,
	SIR_RENDER_ASPECT_PLANE_2 = 1U << 5
};

enum class SIRRenderResourceState : uint8_t
{
	SIR_RENDER_STATE_UNDEFINED = 0,
	SIR_RENDER_STATE_COMMON = 1,
	SIR_RENDER_STATE_VERTEX_INDEX = 2,
	SIR_RENDER_STATE_CONSTANT = 3,
	SIR_RENDER_STATE_SHADER_READ = 4,
	SIR_RENDER_STATE_SHADER_WRITE = 5,
	SIR_RENDER_STATE_COLOR_ATTACHMENT = 6,
	SIR_RENDER_STATE_DEPTH_READ = 7,
	SIR_RENDER_STATE_DEPTH_WRITE = 8,
	SIR_RENDER_STATE_TRANSFER_SOURCE = 9,
	SIR_RENDER_STATE_TRANSFER_DESTINATION = 10,
	SIR_RENDER_STATE_PRESENT = 11,
	SIR_RENDER_STATE_ACCELERATION_STRUCTURE_READ = 12,
	SIR_RENDER_STATE_ACCELERATION_STRUCTURE_WRITE = 13,
	SIR_RENDER_STATE_INDIRECT_ARGUMENT = 14
};

enum class SIRRenderBarrierType : uint8_t
{
	SIR_RENDER_BARRIER_TRANSITION = 0,
	SIR_RENDER_BARRIER_UAV = 1,
	SIR_RENDER_BARRIER_ALIAS = 2
};

// Keep DXIL/SPIR-V/GLSL/DXBC numbering compatible with the old shared ABI.
enum class SIRRenderShaderFormat : uint8_t
{
	SIR_RENDER_SHADER_UNKNOWN = 0,
	SIR_RENDER_SHADER_DXIL = 1,
	SIR_RENDER_SHADER_SPIRV = 2,
	SIR_RENDER_SHADER_GLSL = 3,
	SIR_RENDER_SHADER_DXBC = 4,
	SIR_RENDER_SHADER_FIXED_FUNCTION = 5,
	SIR_RENDER_SHADER_D3D_LEGACY_TOKENS = 6,
	SIR_RENDER_SHADER_MSL = 7,
	SIR_RENDER_SHADER_SOFTWARE_NATIVE = 8
};

// Keep graphics/fragment/compute stage bits from the shared graphics ABI;
// ray-tracing bits extend the same mask without overlapping raster stages.
enum class SIRRenderShaderStage : uint32_t
{
	SIR_RENDER_STAGE_NONE = 0,
	SIR_RENDER_STAGE_VERTEX = 1U << 0,
	SIR_RENDER_STAGE_FRAGMENT = 1U << 1,
	SIR_RENDER_STAGE_COMPUTE = 1U << 2,
	SIR_RENDER_STAGE_GEOMETRY = 1U << 3,
	SIR_RENDER_STAGE_TESSELLATION_CONTROL = 1U << 4,
	SIR_RENDER_STAGE_TESSELLATION_EVALUATION = 1U << 5,
	SIR_RENDER_STAGE_MESH = 1U << 6,
	SIR_RENDER_STAGE_TASK = 1U << 7,
	SIR_RENDER_STAGE_RAY_GENERATION = 1U << 8,
	SIR_RENDER_STAGE_ANY_HIT = 1U << 9,
	SIR_RENDER_STAGE_CLOSEST_HIT = 1U << 10,
	SIR_RENDER_STAGE_MISS = 1U << 11,
	SIR_RENDER_STAGE_CALLABLE = 1U << 12,
	SIR_RENDER_STAGE_INTERSECTION = 1U << 13
};

enum class SIRRenderBindingType : uint8_t
{
	SIR_RENDER_BINDING_CONSTANT_BUFFER = 0,
	SIR_RENDER_BINDING_SAMPLED_TEXTURE = 1,
	SIR_RENDER_BINDING_STORAGE_BUFFER = 2,
	SIR_RENDER_BINDING_STORAGE_TEXTURE = 3,
	SIR_RENDER_BINDING_SAMPLER = 4,
	SIR_RENDER_BINDING_TEXEL_BUFFER = 5,
	SIR_RENDER_BINDING_INPUT_ATTACHMENT = 6,
	SIR_RENDER_BINDING_ACCELERATION_STRUCTURE = 7
};

enum class SIRRenderPipelineType : uint8_t
{
	SIR_RENDER_PIPELINE_GRAPHICS = 0,
	SIR_RENDER_PIPELINE_COMPUTE = 1,
	SIR_RENDER_PIPELINE_FIXED_FUNCTION = 2,
	SIR_RENDER_PIPELINE_RAY_TRACING = 3
};

enum class SIRRenderTopology : uint8_t
{
	SIR_RENDER_TOPOLOGY_POINTS = 0,
	SIR_RENDER_TOPOLOGY_LINES = 1,
	SIR_RENDER_TOPOLOGY_LINE_STRIP = 2,
	SIR_RENDER_TOPOLOGY_TRIANGLES = 3,
	SIR_RENDER_TOPOLOGY_TRIANGLE_STRIP = 4,
	SIR_RENDER_TOPOLOGY_PATCHES = 5
};

enum class SIRRenderCullMode : uint8_t
{
	SIR_RENDER_CULL_NONE = 0,
	SIR_RENDER_CULL_FRONT = 1,
	SIR_RENDER_CULL_BACK = 2
};

enum class SIRRenderFilter : uint8_t
{
	SIR_RENDER_FILTER_NEAREST = 0,
	SIR_RENDER_FILTER_LINEAR = 1,
	SIR_RENDER_FILTER_CUBIC = 2
};

enum class SIRRenderMipFilter : uint8_t
{
	SIR_RENDER_MIP_FILTER_NONE = 0,
	SIR_RENDER_MIP_FILTER_NEAREST = 1,
	SIR_RENDER_MIP_FILTER_LINEAR = 2
};

enum class SIRRenderAddressMode : uint8_t
{
	SIR_RENDER_ADDRESS_REPEAT = 0,
	SIR_RENDER_ADDRESS_MIRRORED_REPEAT = 1,
	SIR_RENDER_ADDRESS_CLAMP_TO_EDGE = 2,
	SIR_RENDER_ADDRESS_CLAMP_TO_BORDER = 3,
	SIR_RENDER_ADDRESS_MIRROR_CLAMP_TO_EDGE = 4
};

enum class SIRRenderCompareOp : uint8_t
{
	SIR_RENDER_COMPARE_NEVER = 0,
	SIR_RENDER_COMPARE_LESS = 1,
	SIR_RENDER_COMPARE_EQUAL = 2,
	SIR_RENDER_COMPARE_LESS_EQUAL = 3,
	SIR_RENDER_COMPARE_GREATER = 4,
	SIR_RENDER_COMPARE_NOT_EQUAL = 5,
	SIR_RENDER_COMPARE_GREATER_EQUAL = 6,
	SIR_RENDER_COMPARE_ALWAYS = 7
};

enum class SIRRenderBlendFactor : uint8_t
{
	SIR_RENDER_BLEND_ZERO = 0,
	SIR_RENDER_BLEND_ONE = 1,
	SIR_RENDER_BLEND_SRC_COLOR = 2,
	SIR_RENDER_BLEND_ONE_MINUS_SRC_COLOR = 3,
	SIR_RENDER_BLEND_DST_COLOR = 4,
	SIR_RENDER_BLEND_ONE_MINUS_DST_COLOR = 5,
	SIR_RENDER_BLEND_SRC_ALPHA = 6,
	SIR_RENDER_BLEND_ONE_MINUS_SRC_ALPHA = 7,
	SIR_RENDER_BLEND_DST_ALPHA = 8,
	SIR_RENDER_BLEND_ONE_MINUS_DST_ALPHA = 9
};

enum class SIRRenderBlendOp : uint8_t
{
	SIR_RENDER_BLEND_ADD = 0,
	SIR_RENDER_BLEND_SUBTRACT = 1,
	SIR_RENDER_BLEND_REVERSE_SUBTRACT = 2,
	SIR_RENDER_BLEND_MIN = 3,
	SIR_RENDER_BLEND_MAX = 4
};

enum class SIRRenderFixedFeature : uint32_t
{
	SIR_RENDER_FIXED_TRANSFORM = 1U << 0,
	SIR_RENDER_FIXED_LIGHTING = 1U << 1,
	SIR_RENDER_FIXED_FOG = 1U << 2,
	SIR_RENDER_FIXED_ALPHA_TEST = 1U << 3,
	SIR_RENDER_FIXED_CPU_TRANSFORM = 1U << 4,
	SIR_RENDER_FIXED_TEXCOORD_GENERATION = 1U << 5
};

enum class SIRRenderFixedTextureOp : uint8_t
{
	SIR_RENDER_FIXED_OP_DISABLE = 0,
	SIR_RENDER_FIXED_OP_SELECT = 1,
	SIR_RENDER_FIXED_OP_MODULATE = 2,
	SIR_RENDER_FIXED_OP_ADD = 3,
	SIR_RENDER_FIXED_OP_SUBTRACT = 4,
	SIR_RENDER_FIXED_OP_INTERPOLATE = 5,
	SIR_RENDER_FIXED_OP_DOT3 = 6
};

enum class SIRRenderFixedTextureArg : uint8_t
{
	SIR_RENDER_FIXED_ARG_TEXTURE = 0,
	SIR_RENDER_FIXED_ARG_CURRENT = 1,
	SIR_RENDER_FIXED_ARG_DIFFUSE = 2,
	SIR_RENDER_FIXED_ARG_CONSTANT = 3
};

enum class SIRRenderFixedFogMode : uint8_t
{
	SIR_RENDER_FIXED_FOG_NONE = 0,
	SIR_RENDER_FIXED_FOG_LINEAR = 1,
	SIR_RENDER_FIXED_FOG_EXP = 2,
	SIR_RENDER_FIXED_FOG_EXP2 = 3
};

enum class SIRRenderSurfacePlatform : uint8_t
{
	SIR_RENDER_SURFACE_NONE = 0,
	SIR_RENDER_SURFACE_WIN32 = 1,
	SIR_RENDER_SURFACE_X11 = 2,
	SIR_RENDER_SURFACE_WAYLAND = 3,
	SIR_RENDER_SURFACE_COCOA = 4
};

// Preserve the shared-graphics FIFO / IMMEDIATE / MAILBOX numeric identities.
enum class SIRRenderPresentMode : uint8_t
{
	SIR_RENDER_PRESENT_FIFO = 0,
	SIR_RENDER_PRESENT_IMMEDIATE = 1,
	SIR_RENDER_PRESENT_MAILBOX = 2,
	SIR_RENDER_PRESENT_FIFO_RELAXED = 3
};

enum class SIRRenderQueryType : uint8_t
{
	SIR_RENDER_QUERY_TIMESTAMP = 0,
	SIR_RENDER_QUERY_OCCLUSION = 1,
	SIR_RENDER_QUERY_PIPELINE_STATISTICS = 2
};

enum class SIRRenderSynchronizationType : uint8_t
{
	SIR_RENDER_SYNC_BINARY = 0,
	SIR_RENDER_SYNC_TIMELINE = 1,
	SIR_RENDER_SYNC_FENCE = 2
};

enum class SIRRenderAccelerationStructureType : uint8_t
{
	SIR_RENDER_ACCELERATION_BOTTOM_LEVEL = 0,
	SIR_RENDER_ACCELERATION_TOP_LEVEL = 1
};

enum class SIRRenderAttachmentLoadOp : uint8_t
{
	SIR_RENDER_LOAD = 0,
	SIR_RENDER_LOAD_CLEAR = 1,
	SIR_RENDER_LOAD_DISCARD = 2
};

enum class SIRRenderAttachmentStoreOp : uint8_t
{
	SIR_RENDER_STORE = 0,
	SIR_RENDER_STORE_DISCARD = 1
};

// ===========================================================================
// Device identity, completion and memory.
// ===========================================================================

struct SIRRenderVersion
{
	uint32_t nMajor{};
	uint32_t nMinor{};
	uint32_t nPatch{};
};

// A slot of zero is valid. A generation of zero invalidates a handle.
// Generation and device generation are checked independently by the backend.
struct SIRRenderHandle
{
	uint32_t nSlot{};
	uint32_t nGeneration{};
	uint32_t nDeviceGeneration{};
	SIRRenderHandleType eType{ SIRRenderHandleType::SIR_RENDER_HANDLE_NONE };

	constexpr bool isValid( void ) const noexcept;
};

constexpr bool SIRRenderHandle::isValid( void ) const noexcept
{
	return nGeneration != 0 && nDeviceGeneration != 0 && eType != SIRRenderHandleType::SIR_RENDER_HANDLE_NONE;
}

struct SIRRenderCompletion
{
	uint32_t nDeviceGeneration{};
	SIRRenderQueueType eQueue{ SIRRenderQueueType::SIR_RENDER_QUEUE_GRAPHICS };
	uint64_t nValue{};
};

// Queue values refer only to queues present in nQueueMask.
// Pipeline-model and execution-model masks use (1U << enum numeric value).
struct SIRRenderCompletionSet
{
	uint32_t nDeviceGeneration{};
	uint32_t nQueueMask{};
	uint64_t szQueueValues[static_cast<uint32_t>( SIRRenderQueueType::SIR_RENDER_QUEUE_COUNT )]{};
};

struct SIRRenderAdapterInfo
{
	uint32_t nAdapterIndex{};
	uint32_t nVendorId{};
	uint32_t nDeviceId{};
	uint64_t nAdapterLuid{};
	uint64_t nDedicatedVideoBytes{};
	uint64_t nSharedSystemBytes{};
	uint64_t nFeatureMask{};
	uint32_t nPipelineModelMask{};
	uint32_t nExecutionModelMask{};
	SIRRenderVersion apiVersion{};
	bool bSoftwareAdapter{};
	char szName[128]{};
};

struct SIRRenderDeviceCreateDesc
{
	uint32_t nAdapterIndex{};
	uint32_t nFramesInFlight{ 2 };
	uint64_t nRequiredFeatures{};
	uint64_t nResidentBudgetBytes{};
	uint64_t nUploadBudgetBytes{};
	uint64_t nReadbackBudgetBytes{};
	bool bHeadless{};
	bool bAllowSoftware{};
};

struct SIRRenderMemoryRequirements
{
	uint64_t nSizeBytes{};
	uint64_t nAlignmentBytes{};
	uint32_t nNativeMemoryTypeMask{};
};

struct SIRRenderMemoryBudget
{
	uint64_t nBudgetBytes{};
	uint64_t nCommittedBytes{};
	uint64_t nResidentBytes{};
	uint64_t nPeakCommittedBytes{};
	uint64_t nUploadMappedBytes{};
	uint64_t nReadbackMappedBytes{};
};

// ===========================================================================
// Resources, views, shader inputs and sampling.
// ===========================================================================

struct SIRRenderBufferRange
{
	uint64_t nOffsetBytes{};
	uint64_t nSizeBytes{};
};

// A zero aspect mask means the backend infers aspects from the view format.
struct SIRRenderSubresourceRange
{
	uint32_t nBaseMip{};
	uint32_t nMipCount{ 1 };
	uint32_t nBaseLayer{};
	uint32_t nLayerCount{ 1 };
	uint32_t nAspectMask{};

	constexpr bool isValidFor( uint32_t nMipLevels, uint32_t nLayers ) const noexcept;
};

constexpr bool SIRRenderSubresourceRange::isValidFor( uint32_t nMipLevels, uint32_t nLayers ) const noexcept
{
	if ( nMipLevels == 0 || nLayers == 0 || nMipCount == 0 ) {
		return false;
	}
	if ( nLayerCount == 0 || nBaseMip >= nMipLevels || nBaseLayer >= nLayers ) {
		return false;
	}
	return nMipCount <= nMipLevels - nBaseMip && nLayerCount <= nLayers - nBaseLayer;
}

struct SIRRenderBufferDesc
{
	uint64_t nSizeBytes{};
	uint64_t nAlignmentBytes{};
	uint64_t nUsageMask{};
	uint32_t nMemoryPropertyMask{};
	uint32_t nStrideBytes{};
	uint32_t nResourceFlags{};
	SIRRenderMemoryClass eMemory{ SIRRenderMemoryClass::SIR_RENDER_MEMORY_GPU_LOCAL };
};

struct SIRRenderTextureDesc
{
	uint32_t nWidth{};
	uint32_t nHeight{};
	uint32_t nDepth{ 1 };
	uint32_t nArrayLayers{ 1 };
	uint32_t nMipLevels{ 1 };
	uint32_t nSampleCount{ 1 };
	uint64_t nUsageMask{};
	uint32_t nResourceFlags{};
	uint32_t nMemoryPropertyMask{};
	SIRRenderTextureDimension eDimension{ SIRRenderTextureDimension::SIR_RENDER_TEXTURE_2D };
	SIRRenderFormat eFormat{ SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN };
	SIRRenderMemoryClass eMemory{ SIRRenderMemoryClass::SIR_RENDER_MEMORY_GPU_LOCAL };
};

// Renderbuffers describe attachment-only images on backends exposing them;
// other backends can implement them as non-sampled textures.
struct SIRRenderRenderbufferDesc
{
	uint32_t nWidth{};
	uint32_t nHeight{};
	uint32_t nSampleCount{ 1 };
	uint64_t nUsageMask{};
	SIRRenderFormat eFormat{ SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN };
	SIRRenderMemoryClass eMemory{ SIRRenderMemoryClass::SIR_RENDER_MEMORY_GPU_LOCAL };
};

// One view descriptor covers both buffer byte ranges and texture subresources.
// Only the corresponding range is read according to hResource.eType.
struct SIRRenderViewDesc
{
	SIRRenderHandle hResource{};
	SIRRenderSubresourceRange subresources{};
	SIRRenderBufferRange bufferRange{};
	uint64_t nUsageMask{};
	uint32_t nStrideBytes{};
	SIRRenderTextureDimension eDimension{ SIRRenderTextureDimension::SIR_RENDER_TEXTURE_2D };
	SIRRenderFormat eFormat{ SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN };
};

struct SIRRenderSamplerDesc
{
	SIRRenderFilter eMinFilter{ SIRRenderFilter::SIR_RENDER_FILTER_LINEAR };
	SIRRenderFilter eMagFilter{ SIRRenderFilter::SIR_RENDER_FILTER_LINEAR };
	SIRRenderMipFilter eMipFilter{ SIRRenderMipFilter::SIR_RENDER_MIP_FILTER_LINEAR };
	SIRRenderAddressMode eAddressU{ SIRRenderAddressMode::SIR_RENDER_ADDRESS_REPEAT };
	SIRRenderAddressMode eAddressV{ SIRRenderAddressMode::SIR_RENDER_ADDRESS_REPEAT };
	SIRRenderAddressMode eAddressW{ SIRRenderAddressMode::SIR_RENDER_ADDRESS_REPEAT };
	SIRRenderCompareOp eCompare{ SIRRenderCompareOp::SIR_RENDER_COMPARE_ALWAYS };
	float fMaximumAnisotropy{ 1.0F };
	float fMinimumLod{};
	float fMaximumLod{ 1000.0F };
	float fLodBias{};
	float szBorderColor[4]{};
	bool bCompareEnabled{};
};

struct SIRRenderShaderCode
{
	SIRRenderShaderStage eStage{ SIRRenderShaderStage::SIR_RENDER_STAGE_NONE };
	SIRRenderShaderFormat eFormat{ SIRRenderShaderFormat::SIR_RENDER_SHADER_UNKNOWN };
	SIRSpan<const uint8_t> code{};
	const char *pszEntryPoint{};
	uint64_t nContentHash{};
	uint64_t nReflectionHash{};
};

struct SIRRenderBindingDesc
{
	uint32_t nBinding{};
	uint32_t nSpace{};
	uint32_t nArrayCount{ 1 };
	uint32_t nShaderStageMask{};
	SIRRenderBindingType eType{ SIRRenderBindingType::SIR_RENDER_BINDING_CONSTANT_BUFFER };
};

struct SIRRenderVertexAttribute
{
	uint32_t nLocation{};
	uint32_t nBufferSlot{};
	uint32_t nOffsetBytes{};
	uint32_t nStrideBytes{};
	SIRRenderFormat eFormat{ SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN };
};

struct SIRRenderBlendState
{
	SIRRenderBlendFactor eSourceColor{ SIRRenderBlendFactor::SIR_RENDER_BLEND_ONE };
	SIRRenderBlendFactor eDestinationColor{ SIRRenderBlendFactor::SIR_RENDER_BLEND_ZERO };
	SIRRenderBlendOp eColorOperation{ SIRRenderBlendOp::SIR_RENDER_BLEND_ADD };
	SIRRenderBlendFactor eSourceAlpha{ SIRRenderBlendFactor::SIR_RENDER_BLEND_ONE };
	SIRRenderBlendFactor eDestinationAlpha{ SIRRenderBlendFactor::SIR_RENDER_BLEND_ZERO };
	SIRRenderBlendOp eAlphaOperation{ SIRRenderBlendOp::SIR_RENDER_BLEND_ADD };
	uint32_t nColorWriteMask{ 0xFU };
	bool bEnabled{};
};

// Resource type is resolved from hResource; handles to samplers or acceleration
// structures use the same descriptor-write grammar as buffers and image views.
struct SIRRenderDescriptorWrite
{
	SIRRenderHandle hResource{};
	uint32_t nBinding{};
	uint32_t nArrayIndex{};
	uint64_t nOffsetBytes{};
	uint64_t nRangeBytes{};
};

// ===========================================================================
// Resource synchronization, commands, rendering and presentation.
// ===========================================================================

struct SIRRenderBarrier
{
	SIRRenderHandle hResource{};
	SIRRenderHandle hAliasedResource{};
	SIRRenderSubresourceRange subresources{};
	SIRRenderBufferRange bufferRange{};
	uint32_t nSourceShaderStageMask{};
	uint32_t nDestinationShaderStageMask{};
	SIRRenderBarrierType eType{ SIRRenderBarrierType::SIR_RENDER_BARRIER_TRANSITION };
	SIRRenderResourceState eBefore{ SIRRenderResourceState::SIR_RENDER_STATE_UNDEFINED };
	SIRRenderResourceState eAfter{ SIRRenderResourceState::SIR_RENDER_STATE_COMMON };
	SIRRenderQueueType eSourceQueue{ SIRRenderQueueType::SIR_RENDER_QUEUE_GRAPHICS };
	SIRRenderQueueType eDestinationQueue{ SIRRenderQueueType::SIR_RENDER_QUEUE_GRAPHICS };
};

struct SIRRenderViewport
{
	float fX{};
	float fY{};
	float fWidth{};
	float fHeight{};
	float fMinimumDepth{};
	float fMaximumDepth{ 1.0F };
};

struct SIRRenderRect
{
	int32_t nX{};
	int32_t nY{};
	uint32_t nWidth{};
	uint32_t nHeight{};
};

struct SIRRenderColorAttachment
{
	SIRRenderHandle hView{};
	SIRRenderHandle hResolveView{};
	float szClearColor[4]{};
	SIRRenderAttachmentLoadOp eLoad{ SIRRenderAttachmentLoadOp::SIR_RENDER_LOAD };
	SIRRenderAttachmentStoreOp eStore{ SIRRenderAttachmentStoreOp::SIR_RENDER_STORE };
};

struct SIRRenderRenderPassDesc
{
	SIRSpan<const SIRRenderColorAttachment> colors{};
	SIRRenderHandle hDepthStencilView{};
	float fClearDepth{ 1.0F };
	uint32_t nClearStencil{};
	SIRRenderAttachmentLoadOp eDepthLoad{ SIRRenderAttachmentLoadOp::SIR_RENDER_LOAD };
	SIRRenderAttachmentStoreOp eDepthStore{ SIRRenderAttachmentStoreOp::SIR_RENDER_STORE };
	SIRRenderAttachmentLoadOp eStencilLoad{ SIRRenderAttachmentLoadOp::SIR_RENDER_LOAD };
	SIRRenderAttachmentStoreOp eStencilStore{ SIRRenderAttachmentStoreOp::SIR_RENDER_STORE };
};

struct SIRRenderFramebufferDesc
{
	uint32_t nWidth{};
	uint32_t nHeight{};
	uint32_t nArrayLayers{ 1 };
	uint32_t nSampleCount{ 1 };
	SIRSpan<const SIRRenderHandle> colorViews{};
	SIRRenderHandle hDepthStencilView{};
};

struct SIRRenderBufferTextureCopy
{
	SIRRenderHandle hBuffer{};
	SIRRenderHandle hTexture{};
	uint64_t nBufferOffsetBytes{};
	uint32_t nBytesPerRow{};
	uint32_t nRowsPerImage{};
	uint32_t nMip{};
	uint32_t nLayer{};
	uint32_t nX{};
	uint32_t nY{};
	uint32_t nZ{};
	uint32_t nWidth{};
	uint32_t nHeight{};
	uint32_t nDepth{ 1 };
	bool bBufferToTexture{};
};

// Borrowed mapping. Its lifetime is determined by the device's map/unmap API.
struct SIRRenderBufferMapping
{
	SIRSpan<uint8_t> bytes{};
	uint64_t nOffsetBytes{};
};

// The two pointers are opaque OS handles. Interpret them only in platform
// adapters; neither the RHI nor its clients include Win32/X11/Wayland headers.
struct SIRRenderNativeSurface
{
	SIRRenderSurfacePlatform ePlatform{ SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_NONE };
	void *pWindow{};
	void *pDisplayOrInstance{};
	uint64_t nWindowGeneration{};
};

// The acquisition token is a single-use presentation reservation, not a GPU
// fence. Submission must consume it before presentation of the same image.
struct SIRRenderSurfaceImage
{
	SIRRenderHandle hSurface{};
	SIRRenderHandle hImage{};
	uint64_t nAcquisitionId{};
	uint64_t nWindowGeneration{};
};

struct SIRRenderSwapchainDesc
{
	SIRRenderHandle hSurface{};
	uint32_t nWidth{};
	uint32_t nHeight{};
	uint32_t nImageCount{ 2 };
	SIRRenderFormat eFormat{ SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_UNORM };
	SIRRenderPresentMode ePresentMode{ SIRRenderPresentMode::SIR_RENDER_PRESENT_FIFO };
	bool bAllowTearing{};
	bool bHdr{};
};

// Present-time data does not duplicate swapchain creation dimensions/format.
struct SIRRenderPresentDesc
{
	SIRRenderSurfaceImage image{};
	SIRRenderCompletion wait{};
	uint32_t nSyncInterval{ 1 };
};

// ===========================================================================
// Ray tracing and era-specific pipeline recipes.
// ===========================================================================

struct SIRRenderShaderBindingRegion
{
	uint64_t nOffsetBytes{};
	uint64_t nSizeBytes{};
	uint64_t nStrideBytes{};
};

struct SIRRenderShaderBindingTableDesc
{
	SIRRenderShaderBindingRegion rayGeneration{};
	SIRRenderShaderBindingRegion miss{};
	SIRRenderShaderBindingRegion hit{};
	SIRRenderShaderBindingRegion callable{};
};

struct SIRRenderAccelerationStructureDesc
{
	SIRRenderAccelerationStructureType eType{ SIRRenderAccelerationStructureType::SIR_RENDER_ACCELERATION_BOTTOM_LEVEL };
	SIRRenderHandle hBackingBuffer{};
	uint64_t nBackingOffsetBytes{};
	uint64_t nBackingSizeBytes{};
	uint32_t nBuildFlags{};
};

// Reuse the existing SDK-neutral graphics ABI recipes during the migration.
// They are opaque here to avoid a dependency on the binary ABI command table.
// They must be copied or otherwise retained safely by an asynchronous backend.
struct sirGraphicsABIModernRecipe_s;
struct sirGraphicsABIProgrammableRecipe_s;
struct sirGraphicsABIFixedRecipe_s;
struct sirGraphicsABIPipelineModelCaps_s;

// A pipeline's type and model are separate: fixed-function emulation on a
// programmable API is valid, and a stateful API can accept modern shaders.
// Shader lists support arbitrary stages, including ray and mesh pipelines.
// Only the recipe pointer matching eModel may be non-null.
struct SIRRenderPipelineDesc
{
	SIRRenderPipelineType eType{ SIRRenderPipelineType::SIR_RENDER_PIPELINE_GRAPHICS };
	SIRRenderPipelineModel eModel{ SIRRenderPipelineModel::SIR_RENDER_MODEL_MODERN };
	SIRSpan<const SIRRenderShaderCode> shaders{};
	SIRSpan<const SIRRenderBindingDesc> bindings{};
	SIRSpan<const SIRRenderVertexAttribute> vertexAttributes{};
	SIRSpan<const SIRRenderFormat> colorFormats{};
	SIRSpan<const SIRRenderBlendState> blendStates{};
	SIRRenderFormat eDepthFormat{ SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN };
	SIRRenderTopology eTopology{ SIRRenderTopology::SIR_RENDER_TOPOLOGY_TRIANGLES };
	SIRRenderCullMode eCullMode{ SIRRenderCullMode::SIR_RENDER_CULL_BACK };
	SIRRenderCompareOp eDepthCompare{ SIRRenderCompareOp::SIR_RENDER_COMPARE_LESS_EQUAL };
	uint32_t nSampleCount{ 1 };
	uint32_t nPatchControlPoints{};
	const sirGraphicsABIModernRecipe_s *pModern{};
	const sirGraphicsABIProgrammableRecipe_s *pProgrammable{};
	const sirGraphicsABIFixedRecipe_s *pFixed{};
	bool bDepthTest{};
	bool bDepthWrite{};
};

static_assert( sizeof( SIRRenderHandle ) == 16, "portable RHI handles must occupy 16 bytes" );
static_assert( sizeof( SIRRenderCompletion ) == 16, "portable RHI completion tokens must occupy 16 bytes" );
static_assert( std::is_standard_layout<SIRRenderHandle>::value, "RHI handles must have standard layout" );
static_assert( std::is_trivially_copyable<SIRRenderHandle>::value, "RHI handles must be trivially copyable" );

#endif // __SIR_RENDERTYPES_H__
