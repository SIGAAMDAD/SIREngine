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

#include <Renderer/SIRGraphicsModule.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <string.h>

static sirGraphicsABIResult_t sirGraphicsResultFromProvider( uint32_t nResult ) noexcept
{
	if ( nResult > SIR_GRAPHICS_VERSION_MISMATCH ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	return ( sirGraphicsABIResult_t )nResult;
}

static uint32_t sirGraphicsABICountFeatureBits( uint64_t nMask ) noexcept
{
	uint32_t nCount = 0;
	while ( nMask != 0 ) {
		nMask &= nMask - 1;
		++nCount;
	}
	return nCount;
}

static bool sirGraphicsABIPreferAdapter( const sirGraphicsABIAdapter_t& candidate, const sirGraphicsABIAdapter_t& current ) noexcept
{
	const bool bSoftwareCandidate = candidate.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	const bool bSoftwareCurrent = current.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	if ( bSoftwareCandidate != bSoftwareCurrent ) {
		return !bSoftwareCandidate;
	}
	const uint32_t nCandidateFeatures = sirGraphicsABICountFeatureBits( candidate.nFeatureMask );
	const uint32_t nCurrentFeatures = sirGraphicsABICountFeatureBits( current.nFeatureMask );
	if ( nCandidateFeatures != nCurrentFeatures ) {
		return nCandidateFeatures > nCurrentFeatures;
	}
	if ( candidate.nDedicatedBytes != current.nDedicatedBytes ) {
		return candidate.nDedicatedBytes > current.nDedicatedBytes;
	}
	if ( candidate.nSharedBytes != current.nSharedBytes ) {
		return candidate.nSharedBytes > current.nSharedBytes;
	}
	if ( candidate.nBackend != current.nBackend ) {
		return candidate.nBackend < current.nBackend;
	}
	if ( candidate.nStableId != current.nStableId ) {
		return candidate.nStableId < current.nStableId;
	}
	return candidate.nIndex < current.nIndex;
}

sirGraphicsABIResult_t sirGraphicsSelectABIModuleAdapter( const sirGraphicsABIModule_t *const *ppModules, uint32_t nModuleCount,
                                                          const sirGraphicsABIModuleSelection_t *pPolicy,
                                                          sirGraphicsABIAdapterChoice_t *pChoice ) noexcept
{
	if ( ppModules == nullptr || pPolicy == nullptr || pChoice == nullptr || nModuleCount == 0 ||
	     pPolicy->nSize < sizeof( sirGraphicsABIModuleSelection_t ) ||
	     pChoice->nSize < sizeof( sirGraphicsABIAdapterChoice_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nModuleCount > 16 ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pPolicy->nBackend > SIR_GRAPHICS_BACKEND_OPENGL || pPolicy->nAllowSoftware > 1 || pPolicy->nReserved != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPolicy->nBackend == SIR_GRAPHICS_BACKEND_AUTO &&
	     ( pPolicy->nMinimumApiMajor != 0 || pPolicy->nMinimumApiMinor != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPolicy->nMinimumApiMajor == 0 && pPolicy->nMinimumApiMinor != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirGraphicsABIAdapterChoice_t best = {};
	best.nSize = sizeof( best );
	bool bFound = false;
	for ( uint32_t iModule = 0; iModule < nModuleCount; ++iModule ) {
		const sirGraphicsABIModule_t *pModule = ppModules[iModule];
		if ( pModule == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirGraphicsABIResult_t eDriver = sirGraphicsValidateABIDriver( &pModule->driver );
		if ( eDriver != SIR_GRAPHICS_SUCCESS ) {
			return eDriver;
		}
		if ( pPolicy->nBackend != SIR_GRAPHICS_BACKEND_AUTO && pModule->driver.nBackend != pPolicy->nBackend ) {
			continue;
		}
		uint32_t nAdapterCount = 0;
		const sirGraphicsABIResult_t eCount =
		    sirGraphicsResultFromProvider( pModule->driver.pAdapterCount( pModule->driver.pDriver, &nAdapterCount ) );
		if ( eCount == SIR_GRAPHICS_UNSUPPORTED && pPolicy->nBackend == SIR_GRAPHICS_BACKEND_AUTO ) {
			continue;
		}
		if ( eCount != SIR_GRAPHICS_SUCCESS ) {
			return eCount;
		}
		if ( nAdapterCount > 256 ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		for ( uint32_t iAdapter = 0; iAdapter < nAdapterCount; ++iAdapter ) {
			sirGraphicsABIAdapter_t adapter = {};
			adapter.nSize = sizeof( adapter );
			adapter.nIndex = iAdapter;
			const sirGraphicsABIResult_t eAdapter =
			    sirGraphicsResultFromProvider( pModule->driver.pQueryAdapter( pModule->driver.pDriver, &adapter ) );
			if ( eAdapter != SIR_GRAPHICS_SUCCESS ) {
				return eAdapter;
			}
			if ( sirGraphicsValidateABIAdapter( &adapter ) != SIR_GRAPHICS_SUCCESS ||
			     adapter.nIndex != iAdapter || adapter.nBackend != pModule->driver.nBackend ) {
				return SIR_GRAPHICS_DRIVER_ERROR;
			}
			if ( adapter.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE && pPolicy->nAllowSoftware == 0 ) {
				continue;
			}
			if ( ( adapter.nFeatureMask & pPolicy->nRequiredFeatures ) != pPolicy->nRequiredFeatures ) {
				continue;
			}
			if ( pPolicy->nMinimumApiMajor != 0 &&
			     ( adapter.nApiMajor < pPolicy->nMinimumApiMajor ||
		       ( adapter.nApiMajor == pPolicy->nMinimumApiMajor &&
		         adapter.nApiMinor < pPolicy->nMinimumApiMinor ) ) ) {
				continue;
			}
			if ( !bFound || sirGraphicsABIPreferAdapter( adapter, best.adapter ) ) {
				best.nModuleIndex = iModule;
				best.adapter = adapter;
				bFound = true;
			}
		}
	}
	if ( !bFound ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	memcpy( pChoice, &best, sizeof( best ) );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsLoadABIModule( const char *pszPath, const sirGraphicsABIHost_t *pHost,
                                                 sirGraphicsABIModule_t *pModule ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' || pModule == nullptr || pModule->library.pNativeHandle != nullptr ||
	     pModule->driver.pDriver != nullptr || pModule->nActiveDevices != 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eHost = sirGraphicsValidateABIHost( pHost );
	if ( eHost != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eHost;
	}
	sirDynamicLibrary_t library = {};
	if ( !SIRDynamicLibrary::load( pszPath, &library ) ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	const sirGraphicsABIFactory_t pFactory =
	    ( sirGraphicsABIFactory_t )SIRDynamicLibrary::findSymbol( &library, SIR_GRAPHICS_DRIVER_ABI_ENTRY );
	if ( pFactory == nullptr ) {
		SIRDynamicLibrary::unload( &library );
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	const sirGraphicsABIResult_t eStatus = sirGraphicsNegotiateDriverABI( pFactory, pHost, &driver );
	if ( eStatus != SIR_GRAPHICS_ABI_SUCCESS ) {
		SIRDynamicLibrary::unload( &library );
		return eStatus;
	}
	pModule->library = library;
	pModule->driver = driver;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsUnloadABIModule( sirGraphicsABIModule_t *pModule ) noexcept
{
	if ( pModule == nullptr || pModule->library.pNativeHandle == nullptr ||
	     sirGraphicsValidateABIDriver( &pModule->driver ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pModule->nActiveDevices != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	pModule->driver.pReleaseDriver( pModule->driver.pDriver );
	if ( !SIRDynamicLibrary::unload( &pModule->library ) ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	*pModule = {};
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsQueryABIAdapter( const sirGraphicsABIModule_t *pModule, uint32_t nIndex,
                                                   sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pModule == nullptr || pAdapter == nullptr || pAdapter->nSize < sizeof( sirGraphicsABIAdapter_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDriver = sirGraphicsValidateABIDriver( &pModule->driver );
	if ( eDriver != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eDriver;
	}
	uint32_t nCount = 0;
	const sirGraphicsABIResult_t eCount =
	    sirGraphicsResultFromProvider( pModule->driver.pAdapterCount( pModule->driver.pDriver, &nCount ) );
	if ( eCount != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eCount;
	}
	if ( nCount > 256 ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( nIndex >= nCount ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	adapter.nIndex = nIndex;
	const sirGraphicsABIResult_t eProbe =
	    sirGraphicsResultFromProvider( pModule->driver.pQueryAdapter( pModule->driver.pDriver, &adapter ) );
	if ( eProbe != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eProbe;
	}
	if ( sirGraphicsValidateABIAdapter( &adapter ) != SIR_GRAPHICS_ABI_SUCCESS || adapter.nIndex != nIndex ||
	     adapter.nBackend != pModule->driver.nBackend ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	memcpy( pAdapter, &adapter, sizeof( adapter ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsQueryABICapabilities( const sirGraphicsABIDevice_t *pDevice, uint32_t nBackend,
                                                        sirGraphicsABICapabilities_t *pInfo ) noexcept
{
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABICapabilities_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABICapabilities_t info = {};
	info.nSize = sizeof( info );
	const sirGraphicsABIResult_t eQuery = sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &info ) );
	if ( eQuery != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eQuery;
	}
	if ( sirGraphicsValidateABICapabilities( &info ) != SIR_GRAPHICS_ABI_SUCCESS || info.nBackend != nBackend ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	memcpy( pInfo, &info, sizeof( info ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsQueryABIFormat( const sirGraphicsABIDevice_t *pDevice, uint32_t nFormat,
                                                  sirGraphicsABIFormat_t *pFormat ) noexcept
{
	if ( pFormat == nullptr || pFormat->nSize < sizeof( sirGraphicsABIFormat_t ) || nFormat <= SIR_GRAPHICS_FORMAT_UNKNOWN ||
	     nFormat > SIR_GRAPHICS_FORMAT_BC7_UNORM ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = nFormat;
	const sirGraphicsABIResult_t eQuery = sirGraphicsResultFromProvider( pDevice->pQueryFormat( pDevice->pDevice, &format ) );
	if ( eQuery != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eQuery;
	}
	if ( sirGraphicsValidateABIFormat( &format ) != SIR_GRAPHICS_ABI_SUCCESS || format.nFormat != nFormat ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	memcpy( pFormat, &format, sizeof( format ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsQueryABIPipelineModelCaps( const sirGraphicsABIDevice_t *pDevice, uint32_t nModel,
                                                             sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	if ( pCaps == nullptr || pCaps->nSize < sizeof( sirGraphicsABIPipelineModelCaps_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nModel < SIR_GRAPHICS_PIPELINE_MODEL_MODERN || nModel > SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}

	sirGraphicsABICapabilities_t deviceCaps = {};
	deviceCaps.nSize = sizeof( deviceCaps );
	const sirGraphicsABIResult_t eDeviceQuery =
	    sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &deviceCaps ) );
	if ( eDeviceQuery != SIR_GRAPHICS_SUCCESS ) {
		return eDeviceQuery;
	}
	if ( sirGraphicsValidateABICapabilities( &deviceCaps ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( ( deviceCaps.nPipelineModelMask & ( 1U << ( nModel - 1 ) ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nModel = nModel;
	const sirGraphicsABIResult_t eQuery =
	    sirGraphicsResultFromProvider( pDevice->pQueryPipelineModelCaps( pDevice->pDevice, nModel, &caps ) );
	if ( eQuery != SIR_GRAPHICS_SUCCESS ) {
		return eQuery;
	}
	if ( sirGraphicsValidateABIPipelineModelCaps( &caps ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( caps.nModel != nModel || caps.nDeviceGeneration != deviceCaps.nDeviceGeneration ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( ( caps.nRequiredFeatureMask & ~deviceCaps.nEnabledFeatures ) != 0 ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( ( caps.nQueueMask & ~deviceCaps.nEnabledQueues ) != 0 ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( caps.nMaximumSamples > deviceCaps.nMaximumSamples || caps.nMaximumColorAttachments > deviceCaps.nMaximumColorAttachments ||
	     caps.nMaximumTextureDimension > deviceCaps.nMaximumTextureDimension || caps.nMaximumBindings > deviceCaps.nMaximumBindings ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	memcpy( pCaps, &caps, sizeof( caps ) );
	return SIR_GRAPHICS_SUCCESS;
}

// Texture/attachment support must be admitted against the actual enabled
// adapter format table, not inferred from API family or pipeline model.
static sirGraphicsABIResult_t sirGraphicsValidateABIAttachmentFormats( const sirGraphicsABIDevice_t *pDevice,
                                                                      const sirGraphicsABIPipeline_t *pPipeline ) noexcept
{
	if ( pPipeline->nType == SIR_GRAPHICS_PIPELINE_COMPUTE ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( pPipeline->nColorFormatCount == 0 && pPipeline->nDepthFormat == SIR_GRAPHICS_FORMAT_UNKNOWN ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	uint32_t nSampleBit = 0;
	uint32_t nSamples = pPipeline->nSampleCount;
	while ( nSamples > 1 ) {
		nSamples >>= 1;
		++nSampleBit;
	}
	const uint32_t nRequiredSample = 1U << nSampleBit;
	for ( uint32_t iColor = 0; iColor < pPipeline->nColorFormatCount; ++iColor ) {
		sirGraphicsABIFormat_t format = {};
		format.nSize = sizeof( format );
		const sirGraphicsABIResult_t eQuery = sirGraphicsQueryABIFormat( pDevice, pPipeline->szColorFormats[iColor], &format );
		if ( eQuery != SIR_GRAPHICS_SUCCESS ) {
			return eQuery;
		}
		if ( ( format.nOptimalFeatures & SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT ) == 0 ||
		     ( format.nSampleCountMask & nRequiredSample ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	if ( pPipeline->nDepthFormat != SIR_GRAPHICS_FORMAT_UNKNOWN ) {
		sirGraphicsABIFormat_t format = {};
		format.nSize = sizeof( format );
		const sirGraphicsABIResult_t eQuery = sirGraphicsQueryABIFormat( pDevice, pPipeline->nDepthFormat, &format );
		if ( eQuery != SIR_GRAPHICS_SUCCESS ) {
			return eQuery;
		}
		if ( ( format.nOptimalFeatures & SIR_GRAPHICS_ABI_FORMAT_DEPTH_STENCIL ) == 0 ||
		     ( format.nSampleCountMask & nRequiredSample ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

// This callback result is the authority for the current device generation
// and enabled queue mask, even if the caller still owns an older command list.
static sirGraphicsABIResult_t sirGraphicsReadABIQueueCaps( const sirGraphicsABIDevice_t *pDevice,
                                                           sirGraphicsABICapabilities_t *pCaps ) noexcept
{
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eQuery =
	    sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &caps ) );
	if ( eQuery != SIR_GRAPHICS_SUCCESS ) {
		return eQuery;
	}
	if ( sirGraphicsValidateABICapabilities( &caps ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pCaps = caps;
	return SIR_GRAPHICS_SUCCESS;
}

static bool sirGraphicsABIValidQueueCompletion( const sirGraphicsCompletion_t& completion,
                                                 const sirGraphicsABICapabilities_t& caps ) noexcept
{
	return completion.nDeviceGeneration == caps.nDeviceGeneration && completion.nValue != 0 &&
	       completion.eQueue < SIR_GRAPHICS_QUEUE_COUNT &&
	       ( caps.nEnabledQueues & ( 1U << completion.eQueue ) ) != 0;
}

sirGraphicsABIResult_t sirGraphicsAcquireABICommands( const sirGraphicsABIDevice_t *pDevice, uint32_t nQueue,
                                                      sirGraphicsABICommandList_t *pCommands ) noexcept
{
	if ( pCommands == nullptr || pCommands->nSize < sizeof( sirGraphicsABICommandList_t ) ||
	     nQueue >= SIR_GRAPHICS_QUEUE_COUNT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABICapabilities_t caps = {};
	const sirGraphicsABIResult_t eCaps = sirGraphicsReadABIQueueCaps( pDevice, &caps );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( ( caps.nEnabledQueues & ( 1U << nQueue ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsABICommandList_t commands = {};
	commands.nSize = sizeof( commands );
	const sirGraphicsABIResult_t eAcquire =
	    sirGraphicsResultFromProvider( pDevice->pAcquireCommands( pDevice->pDevice, nQueue, &commands ) );
	if ( eAcquire != SIR_GRAPHICS_SUCCESS ) {
		return eAcquire;
	}
	if ( sirGraphicsValidateABICommandList( &commands ) != SIR_GRAPHICS_SUCCESS ||
	     commands.nDeviceGeneration != caps.nDeviceGeneration || commands.nQueue != nQueue ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pCommands = commands;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsSubmitABICommands( const sirGraphicsABIDevice_t *pDevice, uint32_t nQueue,
                                                     const sirGraphicsABISubmission_t *pSubmission,
                                                     sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pSubmission == nullptr || pCompletion == nullptr || nQueue >= SIR_GRAPHICS_QUEUE_COUNT ||
	     pSubmission->nSize < sizeof( sirGraphicsABISubmission_t ) || pSubmission->nReserved != 0 ||
	     pSubmission->nCommandListCount == 0 || pSubmission->nCommandListCount > 64 ||
	     pSubmission->ppCommandLists == nullptr || pSubmission->nWaitCount > 64 ||
	     pSubmission->nAcquisitionCount > 8 ||
	     ( pSubmission->nWaitCount != 0 && pSubmission->pWaits == nullptr ) ||
	     ( pSubmission->nAcquisitionCount != 0 && pSubmission->pAcquisitions == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABICapabilities_t caps = {};
	const sirGraphicsABIResult_t eCaps = sirGraphicsReadABIQueueCaps( pDevice, &caps );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( ( caps.nEnabledQueues & ( 1U << nQueue ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t i = 0; i < pSubmission->nCommandListCount; ++i ) {
		if ( pSubmission->ppCommandLists[i] == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( pSubmission->ppCommandLists[i] == pSubmission->ppCommandLists[j] ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	for ( uint32_t i = 0; i < pSubmission->nWaitCount; ++i ) {
		if ( !sirGraphicsABIValidQueueCompletion( pSubmission->pWaits[i], caps ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	if ( pSubmission->nAcquisitionCount != 0 &&
	     ( nQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
	       ( caps.nEnabledFeatures & SIR_GRAPHICS_FEATURE_PRESENT ) == 0 ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t i = 0; i < pSubmission->nAcquisitionCount; ++i ) {
		const sirGraphicsSurfaceImage_t& image = pSubmission->pAcquisitions[i];
		if ( !sirGraphicsIsHandleValid( image.hSurface, SIR_GRAPHICS_HANDLE_SURFACE, caps.nDeviceGeneration ) ||
		     !sirGraphicsIsHandleValid( image.hImage, SIR_GRAPHICS_HANDLE_TEXTURE, caps.nDeviceGeneration ) ||
		     image.nAcquisitionId == 0 || image.nWindowGeneration == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			const sirGraphicsSurfaceImage_t& earlier = pSubmission->pAcquisitions[j];
			if ( earlier.hSurface.nSlot == image.hSurface.nSlot &&
			     earlier.hSurface.nGeneration == image.hSurface.nGeneration &&
			     earlier.nAcquisitionId == image.nAcquisitionId ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	sirGraphicsCompletion_t completion = {};
	const sirGraphicsABIResult_t eSubmit = sirGraphicsResultFromProvider(
	    pDevice->pSubmit( pDevice->pDevice, nQueue, pSubmission, &completion ) );
	if ( eSubmit != SIR_GRAPHICS_SUCCESS ) {
		return eSubmit;
	}
	if ( !sirGraphicsABIValidQueueCompletion( completion, caps ) || completion.eQueue != nQueue ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pCompletion = completion;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsPollABICompletion( const sirGraphicsABIDevice_t *pDevice,
                                                     const sirGraphicsCompletion_t *pCompletion, uint32_t *pDone ) noexcept
{
	if ( pCompletion == nullptr || pDone == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABICapabilities_t caps = {};
	const sirGraphicsABIResult_t eCaps = sirGraphicsReadABIQueueCaps( pDevice, &caps );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( !sirGraphicsABIValidQueueCompletion( *pCompletion, caps ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t nDone = 0;
	const sirGraphicsABIResult_t ePoll =
	    sirGraphicsResultFromProvider( pDevice->pPoll( pDevice->pDevice, pCompletion, &nDone ) );
	if ( ePoll != SIR_GRAPHICS_SUCCESS ) {
		return ePoll;
	}
	if ( nDone > 1 ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pDone = nDone;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsWaitABICompletion( const sirGraphicsABIDevice_t *pDevice,
                                                     const sirGraphicsCompletion_t *pCompletion, uint64_t nTimeoutNs ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABICapabilities_t caps = {};
	const sirGraphicsABIResult_t eCaps = sirGraphicsReadABIQueueCaps( pDevice, &caps );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( !sirGraphicsABIValidQueueCompletion( *pCompletion, caps ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return sirGraphicsResultFromProvider( pDevice->pWait( pDevice->pDevice, pCompletion, nTimeoutNs ) );
}

sirGraphicsABIResult_t sirGraphicsQueryABIMemoryBudget( const sirGraphicsABIDevice_t *pDevice, sirGraphicsMemoryBudget_t *pBudget ) noexcept
{
	if ( pBudget == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}

	sirGraphicsMemoryBudget_t budget = {};
	const sirGraphicsABIResult_t eQuery =
	    sirGraphicsResultFromProvider( pDevice->pQueryMemoryBudget( pDevice->pDevice, &budget ) );
	if ( eQuery != SIR_GRAPHICS_SUCCESS ) {
		return eQuery;
	}
	if ( budget.nResidentBytes > budget.nCommittedBytes ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( budget.nPeakCommittedBytes != 0 && budget.nPeakCommittedBytes < budget.nCommittedBytes ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pBudget = budget;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsCreateABIBuffer( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsBufferDesc_t *pDesc,
                                                   sirGraphicsHandle_t *pBuffer ) noexcept
{
	if ( pDesc == nullptr || pBuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDescription = sirGraphicsValidateBuffer( *pDesc );
	if ( eDescription != SIR_GRAPHICS_SUCCESS ) {
		return eDescription;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eCaps =
	    sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &caps ) );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( sirGraphicsValidateABICapabilities( &caps ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( ( pDesc->nUsage & ( SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_INDEX ) ) != 0 &&
	     ( caps.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 &&
	     ( caps.nEnabledFeatures & SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_INDIRECT ) != 0 &&
	     ( caps.nEnabledFeatures & SIR_GRAPHICS_FEATURE_INDIRECT_DRAW ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->eMemory == SIR_GRAPHICS_MEMORY_GPU_UPLOAD &&
	     ( caps.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GPU_UPLOAD ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsHandle_t hBuffer = {};
	const sirGraphicsABIResult_t eCreate =
	    sirGraphicsResultFromProvider( pDevice->pCreateBuffer( pDevice->pDevice, pDesc, &hBuffer ) );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	if ( hBuffer.eType != SIR_GRAPHICS_HANDLE_BUFFER || hBuffer.nGeneration == 0 ||
	     hBuffer.nDeviceGeneration != caps.nDeviceGeneration ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pBuffer = hBuffer;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsCreateABIView( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsViewDesc_t *pDesc,
                                                 sirGraphicsHandle_t *pView ) noexcept
{
	if ( pDesc == nullptr || pView == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const bool bTexture = pDesc->hResource.eType == SIR_GRAPHICS_HANDLE_TEXTURE;
	const bool bBuffer = pDesc->hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER;
	if ( !bTexture && !bBuffer ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( bTexture ) {
		if ( pDesc->nMipLevels == 0 || pDesc->nLayers == 0 || pDesc->nBufferOffsetBytes != 0 ||
		     pDesc->nBufferSizeBytes != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pDesc->nBaseMip > UINT32_MAX - pDesc->nMipLevels ||
		     pDesc->nBaseLayer > UINT32_MAX - pDesc->nLayers ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else {
		if ( pDesc->nBaseMip != 0 || pDesc->nMipLevels != 0 || pDesc->nBaseLayer != 0 || pDesc->nLayers != 0 ||
		     pDesc->nBufferSizeBytes == 0 || pDesc->nBufferOffsetBytes > UINT64_MAX - pDesc->nBufferSizeBytes ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	constexpr uint32_t VIEW_USAGE_MASK = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE |
	                                     SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	const uint32_t nUsage = pDesc->nUsage;
	if ( nUsage == 0 || ( nUsage & ~VIEW_USAGE_MASK ) != 0 || ( nUsage & ( nUsage - 1 ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	if ( bBuffer && ( nUsage & ( SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eFormat > SIR_GRAPHICS_FORMAT_BC7_UNORM ||
	     ( bTexture && pDesc->eFormat == SIR_GRAPHICS_FORMAT_UNKNOWN ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const bool bDepthFormat = pDesc->eFormat >= SIR_GRAPHICS_FORMAT_D16_UNORM &&
	                          pDesc->eFormat <= SIR_GRAPHICS_FORMAT_D32_FLOAT;
	if ( ( bDepthFormat && nUsage == SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) ||
	     ( !bDepthFormat && nUsage == SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eCaps =
	    sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &caps ) );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( sirGraphicsValidateABICapabilities( &caps ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( !sirGraphicsIsHandleValid( pDesc->hResource, SIR_GRAPHICS_HANDLE_NONE, caps.nDeviceGeneration ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nUsage == SIR_GRAPHICS_USAGE_STORAGE &&
	     ( caps.nEnabledFeatures & SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( bTexture ) {
		sirGraphicsABIFormat_t format = {};
		format.nSize = sizeof( format );
		const sirGraphicsABIResult_t eFormat = sirGraphicsQueryABIFormat( pDevice, pDesc->eFormat, &format );
		if ( eFormat != SIR_GRAPHICS_SUCCESS ) {
			return eFormat;
		}
		uint32_t nRequired = 0;
		if ( nUsage == SIR_GRAPHICS_USAGE_SAMPLED ) {
			nRequired = SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
		} else if ( nUsage == SIR_GRAPHICS_USAGE_STORAGE ) {
			nRequired = SIR_GRAPHICS_ABI_FORMAT_STORAGE;
		} else if ( nUsage == SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) {
			nRequired = SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT;
		} else {
			nRequired = SIR_GRAPHICS_ABI_FORMAT_DEPTH_STENCIL;
		}
		if ( ( format.nOptimalFeatures & nRequired ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	sirGraphicsHandle_t hView = {};
	const sirGraphicsABIResult_t eCreate =
	    sirGraphicsResultFromProvider( pDevice->pCreateView( pDevice->pDevice, pDesc, &hView ) );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	if ( hView.eType != SIR_GRAPHICS_HANDLE_VIEW || hView.nGeneration == 0 ||
	     hView.nDeviceGeneration != caps.nDeviceGeneration ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pView = hView;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsCreateABISampler( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsSamplerDesc_t *pDesc,
                                                    sirGraphicsHandle_t *pSampler ) noexcept
{
	if ( pDesc == nullptr || pSampler == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDescription = sirGraphicsValidateSampler( *pDesc );
	if ( eDescription != SIR_GRAPHICS_SUCCESS ) {
		return eDescription;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eQuery =
	    sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &caps ) );
	if ( eQuery != SIR_GRAPHICS_SUCCESS ) {
		return eQuery;
	}
	if ( sirGraphicsValidateABICapabilities( &caps ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	sirGraphicsHandle_t hSampler = {};
	const sirGraphicsABIResult_t eCreate =
	    sirGraphicsResultFromProvider( pDevice->pCreateSampler( pDevice->pDevice, pDesc, &hSampler ) );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	if ( hSampler.eType != SIR_GRAPHICS_HANDLE_SAMPLER || hSampler.nGeneration == 0 ||
	     hSampler.nDeviceGeneration != caps.nDeviceGeneration ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pSampler = hSampler;
	return SIR_GRAPHICS_SUCCESS;
}

// The host admits resource creation using real device-enabled format facts;
// the backend remains responsible for allocation, tiling and native ownership.
sirGraphicsABIResult_t sirGraphicsCreateABITexture( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsTextureDesc_t *pDesc,
                                                    sirGraphicsHandle_t *pTexture ) noexcept
{
	if ( pDesc == nullptr || pTexture == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eStructure = sirGraphicsValidateTexture( *pDesc );
	if ( eStructure != SIR_GRAPHICS_SUCCESS ) {
		return eStructure;
	}
	constexpr uint32_t TEXTURE_USAGE_MASK = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE |
	                                       SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL |
	                                       SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	if ( ( pDesc->nUsage & ~TEXTURE_USAGE_MASK ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const bool bDepthFormat = pDesc->eFormat >= SIR_GRAPHICS_FORMAT_D16_UNORM &&
	                          pDesc->eFormat <= SIR_GRAPHICS_FORMAT_D32_FLOAT;
	if ( ( bDepthFormat && ( pDesc->nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ) ||
	     ( !bDepthFormat && ( pDesc->nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}

	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eCaps =
	    sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &caps ) );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( sirGraphicsValidateABICapabilities( &caps ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( pDesc->nWidth > caps.nMaximumTextureDimension || pDesc->nHeight > caps.nMaximumTextureDimension ||
	     pDesc->nDepth > caps.nMaximumTextureDimension || pDesc->nSampleCount > caps.nMaximumSamples ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->nSampleCount > 1 && pDesc->nMipLevels != 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 &&
	     ( caps.nEnabledFeatures & SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	const sirGraphicsABIResult_t eFormat = sirGraphicsQueryABIFormat( pDevice, pDesc->eFormat, &format );
	if ( eFormat != SIR_GRAPHICS_SUCCESS ) {
		return eFormat;
	}
	uint32_t nRequired = 0;
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_SAMPLED ) != 0 ) {
		nRequired |= SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) {
		nRequired |= SIR_GRAPHICS_ABI_FORMAT_STORAGE;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ) {
		nRequired |= SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) {
		nRequired |= SIR_GRAPHICS_ABI_FORMAT_DEPTH_STENCIL;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_COPY_SOURCE ) != 0 ) {
		nRequired |= SIR_GRAPHICS_ABI_FORMAT_TRANSFER_SRC;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_COPY_DESTINATION ) != 0 ) {
		nRequired |= SIR_GRAPHICS_ABI_FORMAT_TRANSFER_DST;
	}
	const bool bLinear = pDesc->eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD ||
	                     pDesc->eMemory == SIR_GRAPHICS_MEMORY_CPU_READBACK;
	const uint32_t nAvailable = bLinear ? format.nLinearFeatures : format.nOptimalFeatures;
	if ( ( nAvailable & nRequired ) != nRequired ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nSampleBit = 0;
	uint32_t nSampleCount = pDesc->nSampleCount;
	while ( nSampleCount > 1 ) {
		nSampleCount >>= 1;
		++nSampleBit;
	}
	if ( ( format.nSampleCountMask & ( 1U << nSampleBit ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsHandle_t hTexture = {};
	const sirGraphicsABIResult_t eCreate =
	    sirGraphicsResultFromProvider( pDevice->pCreateTexture( pDevice->pDevice, pDesc, &hTexture ) );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	if ( hTexture.eType != SIR_GRAPHICS_HANDLE_TEXTURE || hTexture.nGeneration == 0 ||
	     hTexture.nDeviceGeneration != caps.nDeviceGeneration ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pTexture = hTexture;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsSelectABIPipelineVariant( const sirGraphicsABIDevice_t *pDevice,
                                                            const sirGraphicsABIPipeline_t *pCandidates, uint32_t nCount,
                                                            uint32_t *pIndex ) noexcept
{
	if ( pCandidates == nullptr || pIndex == nullptr || nCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nCount > 16 ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_SUCCESS ) {
		return eDevice;
	}

	for ( uint32_t iCandidate = 0; iCandidate < nCount; ++iCandidate ) {
		const sirGraphicsABIPipeline_t *pPipeline = &pCandidates[iCandidate];
		const sirGraphicsABIResult_t eStructure = sirGraphicsValidateABIPipelineStructure( pPipeline );
		if ( eStructure != SIR_GRAPHICS_SUCCESS ) {
			return eStructure;
		}
		sirGraphicsABIPipelineModelCaps_t caps = {};
		caps.nSize = sizeof( caps );
		const sirGraphicsABIResult_t eCaps =
		    sirGraphicsQueryABIPipelineModelCaps( pDevice, pPipeline->nModel, &caps );
		if ( eCaps == SIR_GRAPHICS_UNSUPPORTED ) {
			continue;
		}
		if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
			return eCaps;
		}
		const sirGraphicsABIResult_t eRecipe = sirGraphicsValidateABIPipeline( pPipeline, &caps );
		if ( eRecipe == SIR_GRAPHICS_UNSUPPORTED ) {
			continue;
		}
		if ( eRecipe != SIR_GRAPHICS_SUCCESS ) {
			return eRecipe;
		}
		const sirGraphicsABIResult_t eFormat = sirGraphicsValidateABIAttachmentFormats( pDevice, pPipeline );
		if ( eFormat == SIR_GRAPHICS_UNSUPPORTED ) {
			continue;
		}
		if ( eFormat != SIR_GRAPHICS_SUCCESS ) {
			return eFormat;
		}
		*pIndex = iCandidate;
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}

sirGraphicsABIResult_t sirGraphicsCreateABIPipeline( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsABIPipeline_t *pDesc,
                                                     sirGraphicsHandle_t *pPipeline ) noexcept
{
	if ( pDesc == nullptr || pPipeline == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eStructure = sirGraphicsValidateABIPipelineStructure( pDesc );
	if ( eStructure != SIR_GRAPHICS_SUCCESS ) {
		return eStructure;
	}
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eCaps = sirGraphicsQueryABIPipelineModelCaps( pDevice, pDesc->nModel, &caps );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	const sirGraphicsABIResult_t eValidation = sirGraphicsValidateABIPipeline( pDesc, &caps );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	const sirGraphicsABIResult_t eFormats = sirGraphicsValidateABIAttachmentFormats( pDevice, pDesc );
	if ( eFormats != SIR_GRAPHICS_SUCCESS ) {
		return eFormats;
	}

	sirGraphicsHandle_t hPipeline = {};
	const sirGraphicsABIResult_t eCreate = sirGraphicsResultFromProvider( pDevice->pCreatePipeline( pDevice->pDevice, pDesc, &hPipeline ) );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	if ( hPipeline.eType != SIR_GRAPHICS_HANDLE_PIPELINE || hPipeline.nGeneration == 0 ||
	     hPipeline.nDeviceGeneration != caps.nDeviceGeneration ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pPipeline = hPipeline;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsCreateABIDevice( sirGraphicsABIModule_t *pModule, const sirGraphicsABIDeviceRequest_t *pRequest,
                                                   sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pModule == nullptr || pRequest == nullptr || pRequest->nSize < sizeof( sirGraphicsABIDeviceRequest_t ) ||
	     pRequest->nFramesInFlight == 0 || pRequest->nAllowSoftware > 1 || pRequest->nHeadless > 1 || pRequest->nReserved != 0 ||
	     pDevice == nullptr || pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pModule->nActiveDevices >= 16 ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	const sirGraphicsABIResult_t eAdapter = sirGraphicsQueryABIAdapter( pModule, pRequest->nAdapterIndex, &adapter );
	if ( eAdapter != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eAdapter;
	}
	if ( adapter.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE && pRequest->nAllowSoftware == 0 ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	if ( ( adapter.nFeatureMask & pRequest->nRequiredFeatures ) != pRequest->nRequiredFeatures ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	const sirGraphicsABIResult_t eCreated =
	    sirGraphicsResultFromProvider( pModule->driver.pCreateDevice( pModule->driver.pDriver, pRequest, &device ) );
	if ( eCreated != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eCreated;
	}
	const sirGraphicsABIResult_t eValid = sirGraphicsValidateABIDevice( &device );
	if ( eValid != SIR_GRAPHICS_ABI_SUCCESS ) {
		pModule->driver.pDestroyDevice( pModule->driver.pDriver, &device );
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eCaps = sirGraphicsQueryABICapabilities( &device, pModule->driver.nBackend, &caps );
	if ( eCaps != SIR_GRAPHICS_ABI_SUCCESS ) {
		pModule->driver.pDestroyDevice( pModule->driver.pDriver, &device );
		return eCaps;
	}
	if ( ( caps.nEnabledFeatures & pRequest->nRequiredFeatures ) != pRequest->nRequiredFeatures || caps.nAdapterKind != adapter.nKind ||
	     caps.nMaximumFramesInFlight < pRequest->nFramesInFlight ) {
		pModule->driver.pDestroyDevice( pModule->driver.pDriver, &device );
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	for ( uint32_t i = 0; i < pModule->nActiveDevices; ++i ) {
		if ( pModule->szActiveDevices[i] == device.pDevice ) {
			pModule->driver.pDestroyDevice( pModule->driver.pDriver, &device );
			return SIR_GRAPHICS_ABI_DRIVER_ERROR;
		}
	}
	pModule->szActiveDevices[pModule->nActiveDevices] = device.pDevice;
	++pModule->nActiveDevices;
	memcpy( pDevice, &device, sizeof( device ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsDestroyABIDevice( sirGraphicsABIModule_t *pModule, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pModule == nullptr || pDevice == nullptr || sirGraphicsValidateABIDriver( &pModule->driver ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     sirGraphicsValidateABIDevice( pDevice ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pModule->nActiveDevices; ++i ) {
		if ( pModule->szActiveDevices[i] != pDevice->pDevice ) {
			continue;
		}
		const sirGraphicsABIResult_t eDestroy =
		    sirGraphicsResultFromProvider( pModule->driver.pDestroyDevice( pModule->driver.pDriver, pDevice ) );
		if ( eDestroy != SIR_GRAPHICS_ABI_SUCCESS ) {
			return eDestroy;
		}
		--pModule->nActiveDevices;
		pModule->szActiveDevices[i] = pModule->szActiveDevices[pModule->nActiveDevices];
		pModule->szActiveDevices[pModule->nActiveDevices] = nullptr;
		*pDevice = {};
		return SIR_GRAPHICS_ABI_SUCCESS;
	}
	return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
}
