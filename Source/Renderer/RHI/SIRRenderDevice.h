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

#ifndef __SIR_RENDER_DEVICE_H__
#define __SIR_RENDER_DEVICE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderCommandList.h>
#include <Renderer/SIRGraphicsTypes.h>

// Driver-neutral contract. No D3D, Vulkan, OpenGL, DXGI, SDL, or GRDG types.
// Explicit capability results govern APIs with no compute/descriptor support.
enum class SIRRenderingBackend : uint8_t {
	SIR_RENDERING_BACKEND_AUTO = 0,
	SIR_RENDERING_BACKEND_VULKAN = 1,
	SIR_RENDERING_BACKEND_D3D12 = 2,
	SIR_RENDERING_BACKEND_D3D11 = 3,
	SIR_RENDERING_BACKEND_D3D10 = 4,
	SIR_RENDERING_BACKEND_D3D9 = 5,
	SIR_RENDERING_BACKEND_D3D8 = 6,
	SIR_RENDERING_BACKEND_D3D7 = 7,
	SIR_RENDERING_BACKEND_OPENGL = 8
};

enum class SIRRenderingDriver : uint8_t {
	SIR_RENDERING_DRIVER_AMD = 0,
	SIR_RENDERING_DRIVER_NVIDIA = 1,
	SIR_RENDERING_DRIVER_INTEL = 2,
	SIR_RENDERING_DRIVER_APPLE = 3
};

enum class SIRRenderingFeature : uint64_t {
	SIR_RENDERING_FEATURE_GRAPHICS = 1ULL << 0,
	SIR_RENDERING_FEATURE_COMPUTE = 1ULL << 1,
	SIR_RENDERING_FEATURE_ASYNC_COMPUTE = 1ULL << 2,
	SIR_RENDERING_FEATURE_COPY_QUEUE = 1ULL << 3,
	SIR_RENDERING_FEATURE_SHADER_STORAGE = 1ULL << 4,
	SIR_RENDERING_FEATURE_DESCRIPTOR_INDEXING = 1ULL << 5,
	SIR_RENDERING_FEATURE_INDIRECT_DRAW = 1ULL << 6,
	SIR_RENDERING_FEATURE_MRT = 1ULL << 7,
	SIR_RENDERING_FEATURE_MULTISAMPLE = 1ULL << 8,
	SIR_RENDERING_FEATURE_TESSELLATION = 1ULL << 9,

	SIR_RENDERING_FEATURE_GEOMETRY_SHADER = 1ULL << 10,
	SIR_RENDERING_FEATURE_MESH_SHADER = 1ULL << 11,
	SIR_RENDERING_FEATURE_RAY_TRACING = 1ULL << 12,
	SIR_RENDERING_FEATURE_SPARSE_RESOURCES = 1ULL << 13,
	SIR_RENDERING_FEATURE_SAMPLER_FEEDBACK = 1ULL << 14,
	SIR_RENDERING_FEATURE_ENHANCED_BARRIERS = 1ULL << 15,
	SIR_RENDERING_FEATURE_GPU_UPLOAD = 1ULL << 16,
	SIR_RENDERING_FEATURE_HDR_OUTPUT = 1ULL << 17,
	SIR_RENDERING_FEATURE_FIXED_FUNCTION = 1ULL << 18,
	SIR_RENDERING_FEATURE_PRESENT = 1ULL << 19
};

typedef struct sirRenderDriverVersion_s
{
	uint32_t nMajor;
	uint32_t nMinor;
	uint32_t nPatch;
} sirRenderDriverVersion_t;

typedef struct sirRenderDriverCapabilities_s
{
	sirRenderDriverVersion_t nApiVersion;
	SIRRenderingBackend eBackend;

	uint64_t nFeatureMask;
	uint32_t nQueueMask;
	uint32_t nPipelineModelMask;
	uint32_t nMaximumTextureDimension;
	uint32_t nMaximumColorAttachments;
	uint32_t nMaximumFramesInFlight;
	uint32_t nMaximumBindingSlots;
	uint32_t nMaximumSampleCount;
	uint32_t nDeviceGeneration;

	bool bUnifiedMemory : 1;
	bool bSoftwareAdapter : 1;
} sirRenderDriverCapabilities_t;

class SIRRenderDevice
{
public:
	constexpr explicit SIRRenderDevice( void ) noexcept = default;
	constexpr explicit SIRRenderDevice( const SIRRenderDevice& ) noexcept = delete;
	constexpr explicit SIRRenderDevice( SIRRenderDevice&& ) noexcept = delete;
	virtual ~SIRRenderDevice() noexcept = default;

	const SIRRenderDevice& operator=( const SIRRenderDevice& ) noexcept = delete;
	const SIRRenderDevice& operator=( SIRRenderDevice&& ) noexcept = delete;

	virtual SIRRenderingBackend getBackend( void ) const noexcept = 0;
	virtual uint32_t getDeviceGeneration( void ) const noexcept = 0;

	/// @brief Must return BUSY rather than waiting behind the caller's back.
	virtual SIRGraphicsResult shutdown( void ) noexcept = 0;
};

#endif // __SIR_RENDER_DEVICE_H__
