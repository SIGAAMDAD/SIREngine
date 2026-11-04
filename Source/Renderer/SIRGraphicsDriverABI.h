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

#ifndef __SIRENGINE_SIRGRAPHICSDRIVERABI_H__
#define __SIRENGINE_SIRGRAPHICSDRIVERABI_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <Renderer/SIRGraphicsABIOperations.h>

// ABI version 1 is the ongoing foundation contract. Complete the mandatory
// tables and pipeline model descriptions without bumping the draft version.
#define SIR_GRAPHICS_DRIVER_ABI_MAJOR 1U
#define SIR_GRAPHICS_DRIVER_ABI_MINOR 0U
#define SIR_GRAPHICS_DRIVER_ABI_ENTRY "SIRGetGraphicsDriverABI"

// The binary boundary has no STL, C++ vtables, bool, exceptions or SDK types.
// All interface objects are borrowed until their owning destroy call returns.
// Integers are host-endian, with an 8-bit byte and 64-bit pointer requirement.
// Binary modules must use the same architecture and calling convention.
// A binary callback returns the canonical graphics result code verbatim.
// No parallel error numbering is introduced by the module interface.
typedef sirGraphicsResult_t sirGraphicsABIResult_t;
#define SIR_GRAPHICS_ABI_SUCCESS SIR_GRAPHICS_SUCCESS
#define SIR_GRAPHICS_ABI_INVALID_ARGUMENT SIR_GRAPHICS_INVALID_ARGUMENT
#define SIR_GRAPHICS_ABI_VERSION_MISMATCH SIR_GRAPHICS_VERSION_MISMATCH
#define SIR_GRAPHICS_ABI_UNSUPPORTED SIR_GRAPHICS_UNSUPPORTED
#define SIR_GRAPHICS_ABI_DRIVER_ERROR SIR_GRAPHICS_DRIVER_ERROR

typedef enum sirGraphicsABIAdapterKind_e : uint32_t
{
	SIR_GRAPHICS_ABI_ADAPTER_DISCRETE = 1,
	SIR_GRAPHICS_ABI_ADAPTER_INTEGRATED = 2,
	SIR_GRAPHICS_ABI_ADAPTER_VIRTUAL = 3,
	SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE = 4,
	SIR_GRAPHICS_ABI_ADAPTER_OTHER = 5
} sirGraphicsABIAdapterKind_t;

typedef enum sirGraphicsABIFormatFeature_e : uint32_t
{
	SIR_GRAPHICS_ABI_FORMAT_SAMPLED = 1U << 0,
	SIR_GRAPHICS_ABI_FORMAT_STORAGE = 1U << 1,
	SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT = 1U << 2,
	SIR_GRAPHICS_ABI_FORMAT_DEPTH_STENCIL = 1U << 3,
	SIR_GRAPHICS_ABI_FORMAT_TRANSFER_SRC = 1U << 4,
	SIR_GRAPHICS_ABI_FORMAT_TRANSFER_DST = 1U << 5,
	SIR_GRAPHICS_ABI_FORMAT_LINEAR_FILTER = 1U << 6
} sirGraphicsABIFormatFeature_t;

typedef struct sirGraphicsABIHost_s
{
	uint32_t nSize;
	uint32_t nMajor;
	uint32_t nMinor;
	uint32_t nReserved;
	void *pContext;
	void *( *pAllocate )( void *pContext, uint64_t nBytes, uint64_t nAlignment );
	void ( *pFree )( void *pContext, void *pMemory );
	void ( *pLog )( void *pContext, uint32_t nSeverity, const char *pszMessage );
} sirGraphicsABIHost_t;

typedef struct sirGraphicsABIAdapter_s
{
	uint32_t nSize;
	uint32_t nIndex;
	uint32_t nBackend;
	uint32_t nKind;
	uint32_t nVendorId;
	uint32_t nDeviceId;
	uint32_t nApiMajor;
	uint32_t nApiMinor;
	uint64_t nStableId;
	uint64_t nDedicatedBytes;
	uint64_t nSharedBytes;
	uint64_t nFeatureMask;
	uint32_t nQueueMask;
	uint32_t nReserved;
	char szName[128];
} sirGraphicsABIAdapter_t;

typedef struct sirGraphicsABICapabilities_s
{
	uint32_t nSize;
	uint32_t nDeviceGeneration;
	uint32_t nBackend;
	uint32_t nAdapterKind;
	uint64_t nEnabledFeatures;
	uint32_t nEnabledQueues;
	uint32_t nMaximumTextureDimension;
	uint32_t nMaximumColorAttachments;
	uint32_t nMaximumBindings;
	uint32_t nMaximumSamples;
	uint32_t nMaximumFramesInFlight;
	uint32_t nShaderProfile;
	uint32_t nUnifiedMemory;
	uint32_t nPipelineModelMask;
	uint32_t nReserved;
} sirGraphicsABICapabilities_t;

typedef struct sirGraphicsABIFormat_s
{
	uint32_t nSize;
	uint32_t nFormat;
	uint32_t nOptimalFeatures;
	uint32_t nLinearFeatures;
	uint32_t nSampleCountMask;
	uint32_t nReserved;
} sirGraphicsABIFormat_t;

typedef struct sirGraphicsABIDeviceRequest_s
{
	uint32_t nSize;
	uint32_t nAdapterIndex;
	uint32_t nFramesInFlight;
	uint32_t nAllowSoftware;
	uint32_t nHeadless;
	uint32_t nReserved;
	uint64_t nRequiredFeatures;
	uint64_t nResidentBudgetBytes;
	uint64_t nUploadBudgetBytes;
	uint64_t nReadbackBudgetBytes;
} sirGraphicsABIDeviceRequest_t;

// The device callback table is also size-tagged; all callbacks have at most
// four arguments. The backend must never return success with an invalid output.
typedef struct sirGraphicsABIDevice_s
{
	uint32_t nSize;
	uint32_t nReserved;
	void *pDevice;
	uint32_t ( *pQueryCapabilities )( void *pDevice, sirGraphicsABICapabilities_t *pInfo );
	uint32_t ( *pQueryFormat )( void *pDevice, sirGraphicsABIFormat_t *pFormat );
	uint32_t ( *pQueryPipelineModelCaps )( void *pDevice, uint32_t nModel, sirGraphicsABIPipelineModelCaps_t *pCaps );
	uint32_t ( *pPoll )( void *pDevice, const sirGraphicsCompletion_t *pCompletion, uint32_t *pComplete );
	uint32_t ( *pWait )( void *pDevice, const sirGraphicsCompletion_t *pCompletion, uint64_t nTimeoutNs );

	uint32_t ( *pQueryMemoryBudget )( void *pDevice, sirGraphicsMemoryBudget_t *pBudget );
	uint32_t ( *pCreateBuffer )( void *pDevice, const sirGraphicsBufferDesc_t *pDesc, sirGraphicsHandle_t *pHandle );
	uint32_t ( *pCreateTexture )( void *pDevice, const sirGraphicsTextureDesc_t *pDesc, sirGraphicsHandle_t *pHandle );
	uint32_t ( *pCreateView )( void *pDevice, const sirGraphicsViewDesc_t *pDesc, sirGraphicsHandle_t *pHandle );
	uint32_t ( *pCreateSampler )( void *pDevice, const sirGraphicsSamplerDesc_t *pDesc, sirGraphicsHandle_t *pHandle );
	uint32_t ( *pCreatePipeline )( void *pDevice, const sirGraphicsABIPipeline_t *pDesc, sirGraphicsHandle_t *pHandle );
	uint32_t ( *pCreateBindingSet )( void *pDevice, sirGraphicsHandle_t hPipeline, uint32_t nSet, sirGraphicsHandle_t *pHandle );
	uint32_t ( *pUpdateBindings )( void *pDevice, sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites, uint32_t nCount );
	uint32_t ( *pMapBuffer )( void *pDevice, const sirGraphicsABIBufferRange_t *pRange, sirGraphicsMappedBuffer_t *pMapped );
	uint32_t ( *pUnmapBuffer )( void *pDevice, const sirGraphicsABIBufferRange_t *pRange );
	uint32_t ( *pFlushBuffer )( void *pDevice, const sirGraphicsABIBufferRange_t *pRange );
	uint32_t ( *pInvalidateBuffer )( void *pDevice, const sirGraphicsABIBufferRange_t *pRange );
	uint32_t ( *pAcquireCommands )( void *pDevice, uint32_t nQueue, sirGraphicsABICommandList_t *pCommands );
	uint32_t ( *pSubmit )( void *pDevice, uint32_t nQueue, const sirGraphicsABISubmission_t *pBatch, sirGraphicsCompletion_t *pCompletion );
	uint32_t ( *pRetireResource )( void *pDevice, sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t *pCompletion );
	uint32_t ( *pCreateSurface )( void *pDevice, const sirGraphicsABINativeSurface_t *pNative, const sirGraphicsABIPresentDesc_t *pPresent,
	                              sirGraphicsHandle_t *pSurface );
	uint32_t ( *pResizeSurface )( void *pDevice, sirGraphicsHandle_t hSurface, const sirGraphicsABINativeSurface_t *pNative,
	                              const sirGraphicsABIPresentDesc_t *pPresent );
	uint32_t ( *pAcquireSurfaceImage )( void *pDevice, sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage );
	uint32_t ( *pPresent )( void *pDevice, const sirGraphicsSurfaceImage_t *pImage, const sirGraphicsCompletion_t *pComplete );
	uint32_t ( *pShutdown )( void *pDevice );
} sirGraphicsABIDevice_t;

typedef struct sirGraphicsABIDriver_s
{
	uint32_t nSize;
	uint32_t nMajor;
	uint32_t nMinor;
	uint32_t nBackend;
	void *pDriver;
	uint32_t ( *pAdapterCount )( void *pDriver, uint32_t *pCount );
	uint32_t ( *pQueryAdapter )( void *pDriver, sirGraphicsABIAdapter_t *pAdapter );
	uint32_t ( *pCreateDevice )( void *pDriver, const sirGraphicsABIDeviceRequest_t *pRequest, sirGraphicsABIDevice_t *pDevice );
	uint32_t ( *pDestroyDevice )( void *pDriver, sirGraphicsABIDevice_t *pDevice );
	void ( *pReleaseDriver )( void *pDriver );
} sirGraphicsABIDriver_t;

#ifdef __cplusplus
extern "C" {
#endif

// The backend exports this exact symbol. Caller initializes output nSize;
// all fields beyond the negotiated prefix must remain untouched.
typedef uint32_t ( *sirGraphicsABIFactory_t )( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                               sirGraphicsABIDriver_t *pDriver );

#ifdef __cplusplus
}
#endif

// Host-side validation; never calls untrusted function pointers.
sirGraphicsABIResult_t sirGraphicsNegotiateDriverABI( sirGraphicsABIFactory_t pFactory, const sirGraphicsABIHost_t *pHost,
                                                      sirGraphicsABIDriver_t *pDriver ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIHost( const sirGraphicsABIHost_t *pHost ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIDriver( const sirGraphicsABIDriver_t *pDriver ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIAdapter( const sirGraphicsABIAdapter_t *pAdapter ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABICapabilities( const sirGraphicsABICapabilities_t *pInfo ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIFormat( const sirGraphicsABIFormat_t *pFormat ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIPipelineModelCaps( const sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIPipelineStructure( const sirGraphicsABIPipeline_t *pPipeline ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIPipeline( const sirGraphicsABIPipeline_t *pPipeline,
                                                       const sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABIDevice( const sirGraphicsABIDevice_t *pDevice ) noexcept;
sirGraphicsABIResult_t sirGraphicsValidateABICommandList( const sirGraphicsABICommandList_t *pCommands ) noexcept;
sirGraphicsABIResult_t sirGraphicsSelectABIProfile( const sirGraphicsABICapabilities_t *pInfo, uint32_t *pProfile ) noexcept;

static_assert( sizeof( void * ) == 8, "graphics module ABI requires a 64-bit process" );
static_assert( sizeof( uint32_t ) == 4 && sizeof( uint64_t ) == 8, "graphics module ABI requires fixed-width integers" );

#endif // __SIRENGINE_SIRGRAPHICSDRIVERABI_H__
