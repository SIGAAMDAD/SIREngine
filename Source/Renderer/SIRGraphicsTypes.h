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

#ifndef __SIRENGINE_SIRGRAPHICSTYPES_H__
#define __SIRENGINE_SIRGRAPHICSTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

class SIRGeneralHeap;

// Driver-neutral contract. No D3D, Vulkan, OpenGL, DXGI, SDL, or GRDG types.
// Explicit capability results govern APIs with no compute/descriptor support.
typedef enum sirGraphicsBackend_e : uint32_t
{
	SIR_GRAPHICS_BACKEND_AUTO = 0,
	SIR_GRAPHICS_BACKEND_VULKAN = 1,
	SIR_GRAPHICS_BACKEND_D3D12 = 2,
	SIR_GRAPHICS_BACKEND_D3D11 = 3,
	SIR_GRAPHICS_BACKEND_D3D10 = 4,
	SIR_GRAPHICS_BACKEND_D3D9 = 5,
	SIR_GRAPHICS_BACKEND_D3D8 = 6,
	SIR_GRAPHICS_BACKEND_D3D7 = 7,
	SIR_GRAPHICS_BACKEND_OPENGL = 8
} sirGraphicsBackend_t;

typedef enum sirGraphicsResult_e : uint32_t
{
	SIR_GRAPHICS_SUCCESS = 0,
	SIR_GRAPHICS_INVALID_ARGUMENT = 1,
	SIR_GRAPHICS_INVALID_HANDLE = 2,
	SIR_GRAPHICS_UNSUPPORTED = 3,
	SIR_GRAPHICS_OUT_OF_MEMORY = 4,
	SIR_GRAPHICS_CAPACITY_EXCEEDED = 5,
	SIR_GRAPHICS_BUSY = 6,
	SIR_GRAPHICS_TIMEOUT = 7,
	SIR_GRAPHICS_SURFACE_CHANGED = 8,
	SIR_GRAPHICS_DEVICE_LOST = 9,
	SIR_GRAPHICS_DRIVER_ERROR = 10
} sirGraphicsResult_t;

typedef enum sirGraphicsFeature_e : uint64_t
{
	SIR_GRAPHICS_FEATURE_GRAPHICS = 1ULL << 0,
	SIR_GRAPHICS_FEATURE_COMPUTE = 1ULL << 1,
	SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE = 1ULL << 2,
	SIR_GRAPHICS_FEATURE_COPY_QUEUE = 1ULL << 3,
	SIR_GRAPHICS_FEATURE_SHADER_STORAGE = 1ULL << 4,
	SIR_GRAPHICS_FEATURE_DESCRIPTOR_INDEXING = 1ULL << 5,
	SIR_GRAPHICS_FEATURE_INDIRECT_DRAW = 1ULL << 6,
	SIR_GRAPHICS_FEATURE_MRT = 1ULL << 7,
	SIR_GRAPHICS_FEATURE_MULTISAMPLE = 1ULL << 8,
	SIR_GRAPHICS_FEATURE_TESSELLATION = 1ULL << 9,
	SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER = 1ULL << 10,
	SIR_GRAPHICS_FEATURE_MESH_SHADER = 1ULL << 11,
	SIR_GRAPHICS_FEATURE_RAY_TRACING = 1ULL << 12,
	SIR_GRAPHICS_FEATURE_SPARSE_RESOURCES = 1ULL << 13,
	SIR_GRAPHICS_FEATURE_SAMPLER_FEEDBACK = 1ULL << 14,
	SIR_GRAPHICS_FEATURE_ENHANCED_BARRIERS = 1ULL << 15,
	SIR_GRAPHICS_FEATURE_GPU_UPLOAD = 1ULL << 16,
	SIR_GRAPHICS_FEATURE_HDR_OUTPUT = 1ULL << 17,
	SIR_GRAPHICS_FEATURE_FIXED_FUNCTION = 1ULL << 18,
	SIR_GRAPHICS_FEATURE_PRESENT = 1ULL << 19
} sirGraphicsFeature_t;

typedef enum sirGraphicsQueue_e : uint32_t
{
	SIR_GRAPHICS_QUEUE_DIRECT = 0,
	SIR_GRAPHICS_QUEUE_COMPUTE = 1,
	SIR_GRAPHICS_QUEUE_COPY = 2,
	SIR_GRAPHICS_QUEUE_COUNT = 3
} sirGraphicsQueue_t;

typedef enum sirGraphicsHandleType_e : uint32_t
{
	SIR_GRAPHICS_HANDLE_NONE = 0,
	SIR_GRAPHICS_HANDLE_BUFFER = 1,
	SIR_GRAPHICS_HANDLE_TEXTURE = 2,
	SIR_GRAPHICS_HANDLE_VIEW = 3,
	SIR_GRAPHICS_HANDLE_SAMPLER = 4,
	SIR_GRAPHICS_HANDLE_PIPELINE = 5,
	SIR_GRAPHICS_HANDLE_BINDING_SET = 6,
	SIR_GRAPHICS_HANDLE_SURFACE = 7
} sirGraphicsHandleType_t;

typedef enum sirGraphicsMemoryClass_e : uint32_t
{
	SIR_GRAPHICS_MEMORY_GPU_LOCAL = 0,
	SIR_GRAPHICS_MEMORY_CPU_UPLOAD = 1,
	SIR_GRAPHICS_MEMORY_CPU_READBACK = 2,
	SIR_GRAPHICS_MEMORY_GPU_UPLOAD = 3,
	SIR_GRAPHICS_MEMORY_TRANSIENT = 4
} sirGraphicsMemoryClass_t;

typedef enum sirGraphicsUsage_e : uint32_t
{
	SIR_GRAPHICS_USAGE_VERTEX = 1U << 0,
	SIR_GRAPHICS_USAGE_INDEX = 1U << 1,
	SIR_GRAPHICS_USAGE_CONSTANT = 1U << 2,
	SIR_GRAPHICS_USAGE_SAMPLED = 1U << 3,
	SIR_GRAPHICS_USAGE_STORAGE = 1U << 4,
	SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT = 1U << 5,
	SIR_GRAPHICS_USAGE_DEPTH_STENCIL = 1U << 6,
	SIR_GRAPHICS_USAGE_COPY_SOURCE = 1U << 7,
	SIR_GRAPHICS_USAGE_COPY_DESTINATION = 1U << 8,
	SIR_GRAPHICS_USAGE_INDIRECT = 1U << 9
} sirGraphicsUsage_t;

typedef enum sirGraphicsFormat_e : uint32_t
{
	SIR_GRAPHICS_FORMAT_UNKNOWN = 0,
	SIR_GRAPHICS_FORMAT_RGBA8_UNORM = 1,
	SIR_GRAPHICS_FORMAT_BGRA8_UNORM = 2,
	SIR_GRAPHICS_FORMAT_RGBA8_SRGB = 3,
	SIR_GRAPHICS_FORMAT_RGBA16_FLOAT = 4,
	SIR_GRAPHICS_FORMAT_R8_UNORM = 5,
	SIR_GRAPHICS_FORMAT_RG8_UNORM = 6,
	SIR_GRAPHICS_FORMAT_R16_FLOAT = 7,
	SIR_GRAPHICS_FORMAT_R32_FLOAT = 8,
	SIR_GRAPHICS_FORMAT_R32_UINT = 9,
	SIR_GRAPHICS_FORMAT_RG32_FLOAT = 10,
	SIR_GRAPHICS_FORMAT_RGBA32_FLOAT = 11,
	SIR_GRAPHICS_FORMAT_D16_UNORM = 12,
	SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT = 13,
	SIR_GRAPHICS_FORMAT_D32_FLOAT = 14,
	SIR_GRAPHICS_FORMAT_BC1_UNORM = 15,
	SIR_GRAPHICS_FORMAT_BC3_UNORM = 16,
	SIR_GRAPHICS_FORMAT_BC5_UNORM = 17,
	SIR_GRAPHICS_FORMAT_BC7_UNORM = 18
} sirGraphicsFormat_t;

typedef enum sirGraphicsTextureDimension_e : uint32_t
{
	SIR_GRAPHICS_TEXTURE_1D = 1,
	SIR_GRAPHICS_TEXTURE_2D = 2,
	SIR_GRAPHICS_TEXTURE_3D = 3,
	SIR_GRAPHICS_TEXTURE_CUBE = 4
} sirGraphicsTextureDimension_t;

typedef enum sirGraphicsState_e : uint32_t
{
	SIR_GRAPHICS_STATE_UNDEFINED = 0,
	SIR_GRAPHICS_STATE_COMMON = 1,
	SIR_GRAPHICS_STATE_VERTEX_INDEX = 2,
	SIR_GRAPHICS_STATE_CONSTANT = 3,
	SIR_GRAPHICS_STATE_SHADER_READ = 4,
	SIR_GRAPHICS_STATE_SHADER_WRITE = 5,
	SIR_GRAPHICS_STATE_COLOR_ATTACHMENT = 6,
	SIR_GRAPHICS_STATE_DEPTH_READ = 7,
	SIR_GRAPHICS_STATE_DEPTH_WRITE = 8,
	SIR_GRAPHICS_STATE_COPY_SOURCE = 9,
	SIR_GRAPHICS_STATE_COPY_DESTINATION = 10,
	SIR_GRAPHICS_STATE_PRESENT = 11
} sirGraphicsState_t;

typedef enum sirGraphicsBarrierType_e : uint32_t
{
	SIR_GRAPHICS_BARRIER_TRANSITION = 0,
	SIR_GRAPHICS_BARRIER_UAV = 1,
	SIR_GRAPHICS_BARRIER_ALIAS = 2
} sirGraphicsBarrierType_t;

typedef enum sirGraphicsShaderFormat_e : uint32_t
{
	SIR_GRAPHICS_SHADER_DXIL = 1,
	SIR_GRAPHICS_SHADER_SPIRV = 2,
	SIR_GRAPHICS_SHADER_GLSL = 3,
	SIR_GRAPHICS_SHADER_DXBC = 4,
	SIR_GRAPHICS_SHADER_FIXED_FUNCTION = 5
} sirGraphicsShaderFormat_t;

typedef enum sirGraphicsPipelineType_e : uint32_t
{
	SIR_GRAPHICS_PIPELINE_GRAPHICS = 0,
	SIR_GRAPHICS_PIPELINE_COMPUTE = 1,
	SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION = 2
} sirGraphicsPipelineType_t;

typedef enum sirGraphicsBindingType_e : uint32_t
{
	SIR_GRAPHICS_BINDING_CONSTANT_BUFFER = 0,
	SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE = 1,
	SIR_GRAPHICS_BINDING_STORAGE_BUFFER = 2,
	SIR_GRAPHICS_BINDING_STORAGE_TEXTURE = 3,
	SIR_GRAPHICS_BINDING_SAMPLER = 4
} sirGraphicsBindingType_t;

typedef enum sirGraphicsShaderStage_e : uint32_t
{
	SIR_GRAPHICS_STAGE_VERTEX = 1U << 0,
	SIR_GRAPHICS_STAGE_FRAGMENT = 1U << 1,
	SIR_GRAPHICS_STAGE_COMPUTE = 1U << 2,
	SIR_GRAPHICS_STAGE_GEOMETRY = 1U << 3,
	SIR_GRAPHICS_STAGE_TESS_CONTROL = 1U << 4,
	SIR_GRAPHICS_STAGE_TESS_EVALUATION = 1U << 5,
	SIR_GRAPHICS_STAGE_MESH = 1U << 6,
	SIR_GRAPHICS_STAGE_TASK = 1U << 7
} sirGraphicsShaderStage_t;

typedef enum sirGraphicsTopology_e : uint32_t
{
	SIR_GRAPHICS_TOPOLOGY_POINTS = 0,
	SIR_GRAPHICS_TOPOLOGY_LINES = 1,
	SIR_GRAPHICS_TOPOLOGY_LINE_STRIP = 2,
	SIR_GRAPHICS_TOPOLOGY_TRIANGLES = 3,
	SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP = 4,
	SIR_GRAPHICS_TOPOLOGY_PATCHES = 5
} sirGraphicsTopology_t;

typedef enum sirGraphicsCullMode_e : uint32_t
{
	SIR_GRAPHICS_CULL_NONE = 0,
	SIR_GRAPHICS_CULL_FRONT = 1,
	SIR_GRAPHICS_CULL_BACK = 2
} sirGraphicsCullMode_t;

typedef enum sirGraphicsSamplerFilter_e : uint32_t
{
	SIR_GRAPHICS_FILTER_NEAREST = 0,
	SIR_GRAPHICS_FILTER_LINEAR = 1,
	SIR_GRAPHICS_FILTER_TRILINEAR = 2,
	SIR_GRAPHICS_FILTER_ANISOTROPIC = 3
} sirGraphicsSamplerFilter_t;

typedef enum sirGraphicsAddressMode_e : uint32_t
{
	SIR_GRAPHICS_ADDRESS_REPEAT = 0,
	SIR_GRAPHICS_ADDRESS_MIRROR = 1,
	SIR_GRAPHICS_ADDRESS_CLAMP = 2,
	SIR_GRAPHICS_ADDRESS_BORDER = 3
} sirGraphicsAddressMode_t;

typedef enum sirGraphicsCompareOp_e : uint32_t
{
	SIR_GRAPHICS_COMPARE_NEVER = 0,
	SIR_GRAPHICS_COMPARE_LESS = 1,
	SIR_GRAPHICS_COMPARE_EQUAL = 2,
	SIR_GRAPHICS_COMPARE_LESS_EQUAL = 3,
	SIR_GRAPHICS_COMPARE_GREATER = 4,
	SIR_GRAPHICS_COMPARE_NOT_EQUAL = 5,
	SIR_GRAPHICS_COMPARE_GREATER_EQUAL = 6,
	SIR_GRAPHICS_COMPARE_ALWAYS = 7
} sirGraphicsCompareOp_t;

typedef enum sirGraphicsBlendFactor_e : uint32_t
{
	SIR_GRAPHICS_BLEND_ZERO = 0,
	SIR_GRAPHICS_BLEND_ONE = 1,
	SIR_GRAPHICS_BLEND_SRC_COLOR = 2,
	SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_COLOR = 3,
	SIR_GRAPHICS_BLEND_DST_COLOR = 4,
	SIR_GRAPHICS_BLEND_ONE_MINUS_DST_COLOR = 5,
	SIR_GRAPHICS_BLEND_SRC_ALPHA = 6,
	SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_ALPHA = 7,
	SIR_GRAPHICS_BLEND_DST_ALPHA = 8,
	SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA = 9
} sirGraphicsBlendFactor_t;

typedef enum sirGraphicsBlendOp_e : uint32_t
{
	SIR_GRAPHICS_BLEND_ADD = 0,
	SIR_GRAPHICS_BLEND_SUBTRACT = 1,
	SIR_GRAPHICS_BLEND_REVERSE_SUBTRACT = 2,
	SIR_GRAPHICS_BLEND_MIN = 3,
	SIR_GRAPHICS_BLEND_MAX = 4
} sirGraphicsBlendOp_t;

typedef enum sirGraphicsSurfacePlatform_e : uint32_t
{
	SIR_GRAPHICS_SURFACE_WIN32 = 1,
	SIR_GRAPHICS_SURFACE_X11 = 2,
	SIR_GRAPHICS_SURFACE_WAYLAND = 3
} sirGraphicsSurfacePlatform_t;

typedef enum sirGraphicsPresentMode_e : uint32_t
{
	SIR_GRAPHICS_PRESENT_FIFO = 0,
	SIR_GRAPHICS_PRESENT_IMMEDIATE = 1,
	SIR_GRAPHICS_PRESENT_MAILBOX = 2
} sirGraphicsPresentMode_t;

typedef struct sirGraphicsVersion_s
{
	uint32_t nMajor;
	uint32_t nMinor;
	uint32_t nPatch;
} sirGraphicsVersion_t;

// The slot may be zero. A zero generation or zero device generation is invalid.
// The backend selects resource-table ownership; no GRDG identity is embedded.
typedef struct sirGraphicsHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint32_t nDeviceGeneration;
	sirGraphicsHandleType_t eType;
} sirGraphicsHandle_t;

typedef struct sirGraphicsCompletion_s
{
	uint32_t nDeviceGeneration;
	sirGraphicsQueue_t eQueue;
	uint64_t nValue;
} sirGraphicsCompletion_t;

typedef struct sirGraphicsCompletionSet_s
{
	uint32_t nDeviceGeneration;
	uint32_t nQueueMask;
	uint64_t szQueueValues[SIR_GRAPHICS_QUEUE_COUNT];
} sirGraphicsCompletionSet_t;

typedef struct sirGraphicsCapabilities_s
{
	sirGraphicsBackend_t eBackend;
	sirGraphicsVersion_t nApiVersion;
	uint64_t nFeatureMask;
	uint32_t nQueueMask;
	uint32_t nMaximumTextureDimension;
	uint32_t nMaximumColorAttachments;
	uint32_t nMaximumFramesInFlight;
	uint32_t nMaximumBindingSlots;
	uint32_t nMaximumSampleCount;
	uint32_t nDeviceGeneration;
	bool bUnifiedMemory;
	bool bSoftwareAdapter;
} sirGraphicsCapabilities_t;

typedef struct sirGraphicsAdapterInfo_s
{
	uint32_t nAdapterIndex;
	uint32_t nVendorId;
	uint32_t nDeviceId;
	uint64_t nAdapterLuid;
	uint64_t nDedicatedVideoBytes;
	uint64_t nSharedSystemBytes;
	uint64_t nFeatureMask;
	sirGraphicsVersion_t nApiVersion;
	bool bSoftwareAdapter;
	char szName[128];
} sirGraphicsAdapterInfo_t;

typedef struct sirGraphicsDeviceCreateDesc_s
{
	uint32_t nAdapterIndex;
	uint32_t nFramesInFlight;
	uint64_t nRequiredFeatures;
	uint64_t nResidentBudgetBytes;
	uint64_t nUploadBudgetBytes;
	uint64_t nReadbackBudgetBytes;
	bool bHeadless;
	bool bAllowSoftware;
	// Optional host CPU metadata authority; must outlive native device shutdown.
	// When provided, the heap must own the RENDERING_CPU domain.
	SIRGeneralHeap *pCpuHeap;
	// Opt-in driver diagnostics. Requested but unavailable capabilities fail
	// device creation; validation and DRED are excluded from runtime timings.
	bool bEnableApiValidation;
	bool bEnableGpuValidation;
	bool bEnableDeviceFaultDiagnostics;
} sirGraphicsDeviceCreateDesc_t;

typedef struct sirGraphicsMemoryBudget_s
{
	uint64_t nBudgetBytes;
	uint64_t nCommittedBytes;
	uint64_t nResidentBytes;
	uint64_t nPeakCommittedBytes;
	uint64_t nUploadMappedBytes;
	uint64_t nReadbackMappedBytes;
} sirGraphicsMemoryBudget_t;

typedef struct sirGraphicsBufferDesc_s
{
	uint64_t nSizeBytes;
	uint64_t nAlignmentBytes;
	uint32_t nUsage;
	sirGraphicsMemoryClass_t eMemory;
} sirGraphicsBufferDesc_t;

typedef struct sirGraphicsTextureDesc_s
{
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nDepth;
	uint32_t nArrayLayers;
	uint32_t nMipLevels;
	uint32_t nSampleCount;
	uint32_t nUsage;
	sirGraphicsTextureDimension_t eDimension;
	sirGraphicsFormat_t eFormat;
	sirGraphicsMemoryClass_t eMemory;
} sirGraphicsTextureDesc_t;

typedef struct sirGraphicsViewDesc_s
{
	sirGraphicsHandle_t hResource;
	sirGraphicsFormat_t eFormat;
	uint32_t nBaseMip;
	uint32_t nMipLevels;
	uint32_t nBaseLayer;
	uint32_t nLayers;
	uint32_t nUsage;
} sirGraphicsViewDesc_t;

typedef struct sirGraphicsSamplerDesc_s
{
	sirGraphicsSamplerFilter_t eFilter;
	sirGraphicsAddressMode_t eAddressU;
	sirGraphicsAddressMode_t eAddressV;
	sirGraphicsAddressMode_t eAddressW;
	float fMaximumAnisotropy;
	float fMinimumLod;
	float fMaximumLod;
} sirGraphicsSamplerDesc_t;

typedef struct sirGraphicsShaderCode_s
{
	sirGraphicsShaderFormat_t eFormat;
	const void *pBytes;
	uint64_t nByteCount;
	uint64_t nContentHash;
	const char *pszEntryPoint;
} sirGraphicsShaderCode_t;

typedef struct sirGraphicsBindingDesc_s
{
	uint32_t nBinding;
	uint32_t nSpace;
	uint32_t nArrayCount;
	uint32_t nShaderStageMask;
	sirGraphicsBindingType_t eType;
} sirGraphicsBindingDesc_t;

typedef struct sirGraphicsVertexAttribute_s
{
	uint32_t nLocation;
	uint32_t nBufferSlot;
	uint32_t nOffsetBytes;
	uint32_t nStrideBytes;
	sirGraphicsFormat_t eFormat;
} sirGraphicsVertexAttribute_t;

typedef struct sirGraphicsBlendState_s
{
	sirGraphicsBlendFactor_t eSourceColor;
	sirGraphicsBlendFactor_t eDestinationColor;
	sirGraphicsBlendOp_t eColorOperation;
	sirGraphicsBlendFactor_t eSourceAlpha;
	sirGraphicsBlendFactor_t eDestinationAlpha;
	sirGraphicsBlendOp_t eAlphaOperation;
	uint32_t nColorWriteMask;
	bool bEnabled;
} sirGraphicsBlendState_t;

typedef struct sirGraphicsPipelineDesc_s
{
	sirGraphicsPipelineType_t eType;
	sirGraphicsShaderCode_t vertexShader;
	sirGraphicsShaderCode_t pixelShader;
	sirGraphicsShaderCode_t computeShader;
	const sirGraphicsBindingDesc_t *pBindings;
	const sirGraphicsVertexAttribute_t *pAttributes;
	uint32_t nBindingCount;
	uint32_t nAttributeCount;
	uint32_t nColorFormatCount;
	sirGraphicsFormat_t szColorFormats[8];
	sirGraphicsFormat_t eDepthFormat;
	uint32_t nSampleCount;
	sirGraphicsTopology_t eTopology;
	sirGraphicsCullMode_t eCullMode;
	sirGraphicsCompareOp_t eDepthCompare;
	sirGraphicsBlendState_t szBlendStates[8];
	bool bDepthTest;
	bool bDepthWrite;
} sirGraphicsPipelineDesc_t;

typedef struct sirGraphicsDescriptorWrite_s
{
	sirGraphicsHandle_t hResource;
	uint32_t nBinding;
	uint32_t nArrayIndex;
	uint64_t nOffsetBytes;
	uint64_t nRangeBytes;
} sirGraphicsDescriptorWrite_t;

typedef struct sirGraphicsBarrier_s
{
	sirGraphicsBarrierType_t eType;
	sirGraphicsHandle_t hResource;
	sirGraphicsHandle_t hAliasedResource;
	sirGraphicsState_t eBefore;
	sirGraphicsState_t eAfter;
	uint32_t nBaseMip;
	uint32_t nMipCount;
	uint32_t nBaseLayer;
	uint32_t nLayerCount;
} sirGraphicsBarrier_t;

typedef struct sirGraphicsViewport_s
{
	float fX;
	float fY;
	float fWidth;
	float fHeight;
	float fMinimumDepth;
	float fMaximumDepth;
} sirGraphicsViewport_t;

typedef struct sirGraphicsRect_s
{
	int32_t nLeft;
	int32_t nTop;
	int32_t nRight;
	int32_t nBottom;
} sirGraphicsRect_t;

typedef struct sirGraphicsColorAttachment_s
{
	sirGraphicsHandle_t hView;
	float szClearColor[4];
	bool bClear;
	bool bStore;
} sirGraphicsColorAttachment_t;

typedef struct sirGraphicsRenderPass_s
{
	const sirGraphicsColorAttachment_t *pColors;
	uint32_t nColorCount;
	sirGraphicsHandle_t hDepthView;
	float fClearDepth;
	uint32_t nClearStencil;
	bool bClearDepth;
	bool bStoreDepth;
} sirGraphicsRenderPass_t;

typedef struct sirGraphicsBufferTextureCopy_s
{
	sirGraphicsHandle_t hBuffer;
	sirGraphicsHandle_t hTexture;
	uint64_t nBufferOffset;
	uint32_t nBytesPerRow;
	uint32_t nRowsPerImage;
	uint32_t nMip;
	uint32_t nLayer;
	uint32_t nX;
	uint32_t nY;
	uint32_t nZ;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nDepth;
	bool bBufferToTexture;
} sirGraphicsBufferTextureCopy_t;

typedef struct sirGraphicsMappedBuffer_s
{
	void *pData;
	uint64_t nOffset;
	uint64_t nBytes;
} sirGraphicsMappedBuffer_t;

typedef struct sirGraphicsNativeSurface_s
{
	sirGraphicsSurfacePlatform_t ePlatform;
	void *pWindow;
	void *pDisplayOrInstance;
	uint64_t nWindowGeneration;
} sirGraphicsNativeSurface_t;

typedef struct sirGraphicsPresentDesc_s
{
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nBackbufferCount;
	sirGraphicsFormat_t eFormat;
	sirGraphicsPresentMode_t eMode;
	bool bAllowTearing;
	bool bHdr;
} sirGraphicsPresentDesc_t;

// A surface acquisition owns a one-use backend synchronization reservation.
// The opaque acquisition ID is not a GPU fence; submit must consume it and
// present must match the same image/window generation.
typedef struct sirGraphicsSurfaceImage_s
{
	sirGraphicsHandle_t hSurface;
	sirGraphicsHandle_t hImage;
	uint64_t nAcquisitionId;
	uint64_t nWindowGeneration;
} sirGraphicsSurfaceImage_t;

static_assert( sizeof( sirGraphicsHandle_t ) == 16, "portable GPU resource handles must occupy 16 bytes" );
static_assert( sizeof( sirGraphicsCompletion_t ) == 16, "portable GPU fence tokens must occupy 16 bytes" );

#endif // __SIRENGINE_SIRGRAPHICSTYPES_H__
