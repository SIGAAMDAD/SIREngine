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
// Enforces enabled format usages, multisampling and device bounds before
// invoking the native texture allocator. pTexture is unchanged on failure.
sirGraphicsABIResult_t sirGraphicsCreateABITexture( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsTextureDesc_t *pDesc,
                                                    sirGraphicsHandle_t *pTexture ) noexcept;

sirGraphicsABIResult_t sirGraphicsCreateABIPipeline( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsABIPipeline_t *pDesc,
                                                     sirGraphicsHandle_t *pPipeline ) noexcept;
sirGraphicsABIResult_t sirGraphicsQueryABIFormat( const sirGraphicsABIDevice_t *pDevice, uint32_t nFormat,
                                                  sirGraphicsABIFormat_t *pFormat ) noexcept;

#endif // __SIRENGINE_SIRGRAPHICSMODULE_H__
