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

#ifndef __SIRENGINE_SIRGRAPHICSMODULE_H__
#define __SIRENGINE_SIRGRAPHICSMODULE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRDynamicLibrary.h>
#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGraphicsABIModule_s
{
	sirDynamicLibrary_t library;
	sirGraphicsABIDriver_t driver;
	void *szActiveDevices[16];
	uint32_t nActiveDevices;
	uint32_t nReserved;
} sirGraphicsABIModule_t;

// Host-side adapter admission policy. AUTO ranks across modules and adapters;
// an explicit backend only considers that API. This is not a second DLL ABI.
typedef struct sirGraphicsABIModuleSelection_s
{
	uint32_t nSize;
	uint32_t nBackend;
	uint32_t nMinimumApiMajor;
	uint32_t nMinimumApiMinor;
	uint32_t nAllowSoftware;
	uint32_t nReserved;
	uint64_t nRequiredFeatures;
} sirGraphicsABIModuleSelection_t;

typedef struct sirGraphicsABIAdapterChoice_s
{
	uint32_t nSize;
	uint32_t nModuleIndex;
	sirGraphicsABIAdapter_t adapter;
} sirGraphicsABIAdapterChoice_t;

// The module list is borrowed for this call. Enumeration and output are
// bounded; no module or device is created and no allocation is performed.
sirGraphicsABIResult_t sirGraphicsSelectABIModuleAdapter( const sirGraphicsABIModule_t *const *ppModules, uint32_t nModuleCount,
                                                          const sirGraphicsABIModuleSelection_t *pPolicy,
                                                          sirGraphicsABIAdapterChoice_t *pChoice ) noexcept;

// Startup/shutdown only; the module must outlive every native device and
// recorded command context. No implicit fallback or process-global registry.
sirGraphicsABIResult_t sirGraphicsLoadABIModule( const char *pszPath, const sirGraphicsABIHost_t *pHost,
                                                 sirGraphicsABIModule_t *pModule ) noexcept;
sirGraphicsABIResult_t sirGraphicsUnloadABIModule( sirGraphicsABIModule_t *pModule ) noexcept;
sirGraphicsABIResult_t sirGraphicsQueryABIAdapter( const sirGraphicsABIModule_t *pModule, uint32_t nIndex,
                                                   sirGraphicsABIAdapter_t *pAdapter ) noexcept;
sirGraphicsABIResult_t sirGraphicsCreateABIDevice( sirGraphicsABIModule_t *pModule, const sirGraphicsABIDeviceRequest_t *pRequest,
                                                   sirGraphicsABIDevice_t *pDevice ) noexcept;
sirGraphicsABIResult_t sirGraphicsDestroyABIDevice( sirGraphicsABIModule_t *pModule, sirGraphicsABIDevice_t *pDevice ) noexcept;
sirGraphicsABIResult_t sirGraphicsQueryABICapabilities( const sirGraphicsABIDevice_t *pDevice, uint32_t nBackend,
                                                        sirGraphicsABICapabilities_t *pInfo ) noexcept;
sirGraphicsABIResult_t sirGraphicsQueryABIPipelineModelCaps( const sirGraphicsABIDevice_t *pDevice, uint32_t nModel,
                                                             sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept;
// Candidates are ordered by the asset/render-path owner, not by API name.
// Return the first compatible recipe; leave pIndex untouched on failure.
// Unsupported models/recipes are skipped. Malformed recipes and driver faults
// stop selection instead of silently falling through to another model.
sirGraphicsABIResult_t sirGraphicsSelectABIPipelineVariant( const sirGraphicsABIDevice_t *pDevice,
                                                            const sirGraphicsABIPipeline_t *pCandidates, uint32_t nCount,
                                                            uint32_t *pIndex ) noexcept;
// Queue admission and completion token safety at the v1 module boundary.
// All callback output is published only after native result verification.
sirGraphicsABIResult_t sirGraphicsAcquireABICommands( const sirGraphicsABIDevice_t *pDevice, uint32_t nQueue,
                                                      sirGraphicsABICommandList_t *pCommands ) noexcept;
sirGraphicsABIResult_t sirGraphicsSubmitABICommands( const sirGraphicsABIDevice_t *pDevice, uint32_t nQueue,
                                                     const sirGraphicsABISubmission_t *pSubmission,
                                                     sirGraphicsCompletion_t *pCompletion ) noexcept;
sirGraphicsABIResult_t sirGraphicsPollABICompletion( const sirGraphicsABIDevice_t *pDevice,
                                                     const sirGraphicsCompletion_t *pCompletion, uint32_t *pDone ) noexcept;
sirGraphicsABIResult_t sirGraphicsWaitABICompletion( const sirGraphicsABIDevice_t *pDevice,
                                                     const sirGraphicsCompletion_t *pCompletion, uint64_t nTimeoutNs ) noexcept;

// Host-side WSI contract: the native driver owns the surface, swapchain,
// acquired image and one-use presentation synchronization.
sirGraphicsABIResult_t sirGraphicsCreateABISurface( const sirGraphicsABIDevice_t *pDevice,
                                                    const sirGraphicsABINativeSurface_t *pNative,
                                                    const sirGraphicsABIPresentDesc_t *pPresent,
                                                    sirGraphicsHandle_t *pSurface ) noexcept;
sirGraphicsABIResult_t sirGraphicsResizeABISurface( const sirGraphicsABIDevice_t *pDevice, sirGraphicsHandle_t hSurface,
                                                    const sirGraphicsABINativeSurface_t *pNative,
                                                    const sirGraphicsABIPresentDesc_t *pPresent ) noexcept;
sirGraphicsABIResult_t sirGraphicsAcquireABISurfaceImage( const sirGraphicsABIDevice_t *pDevice, sirGraphicsHandle_t hSurface,
                                                          sirGraphicsSurfaceImage_t *pImage ) noexcept;
sirGraphicsABIResult_t sirGraphicsPresentABISurfaceImage( const sirGraphicsABIDevice_t *pDevice,
                                                          const sirGraphicsSurfaceImage_t *pImage,
                                                          const sirGraphicsCompletion_t *pCompletion ) noexcept;

// Retirement is a native ownership operation gated by validated queue fences.
// An empty completion set denotes a resource with no outstanding GPU users.
sirGraphicsABIResult_t sirGraphicsRetireABIResource( const sirGraphicsABIDevice_t *pDevice, sirGraphicsHandle_t hResource,
                                                    const sirGraphicsCompletionSet_t *pComplete ) noexcept;

// Returns a validated snapshot of the native device's current memory budget.
// A zero native budget is an unknown limit, not a guaranteed free allocation.
// The output is unchanged on driver error or incoherent counters.
sirGraphicsABIResult_t sirGraphicsQueryABIMemoryBudget( const sirGraphicsABIDevice_t *pDevice, sirGraphicsMemoryBudget_t *pBudget ) noexcept;

// Buffer allocation checks typed usage, enabled features and device generation
// before publishing the native handle. Actual allocation policy stays native.
sirGraphicsABIResult_t sirGraphicsCreateABIBuffer( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsBufferDesc_t *pDesc,
                                                   sirGraphicsHandle_t *pBuffer ) noexcept;

// Typed views and samplers use the same host validation and generation
// contract as buffers/textures. Resource-specific compatibility stays native.
sirGraphicsABIResult_t sirGraphicsCreateABIView( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsViewDesc_t *pDesc,
                                                 sirGraphicsHandle_t *pView ) noexcept;
sirGraphicsABIResult_t sirGraphicsCreateABISampler( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsSamplerDesc_t *pDesc,
                                                    sirGraphicsHandle_t *pSampler ) noexcept;

// Enforces enabled format usages, multisampling and device bounds before
// invoking the native texture allocator. pTexture is unchanged on failure.
sirGraphicsABIResult_t sirGraphicsCreateABITexture( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsTextureDesc_t *pDesc,
                                                    sirGraphicsHandle_t *pTexture ) noexcept;

sirGraphicsABIResult_t sirGraphicsCreateABIPipeline( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsABIPipeline_t *pDesc,
                                                     sirGraphicsHandle_t *pPipeline ) noexcept;
sirGraphicsABIResult_t sirGraphicsQueryABIFormat( const sirGraphicsABIDevice_t *pDevice, uint32_t nFormat,
                                                  sirGraphicsABIFormat_t *pFormat ) noexcept;

#endif // __SIRENGINE_SIRGRAPHICSMODULE_H__
