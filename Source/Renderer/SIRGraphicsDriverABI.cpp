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

#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <string.h>

sirGraphicsABIResult_t sirGraphicsNegotiateDriverABI( sirGraphicsABIFactory_t pFactory, const sirGraphicsABIHost_t *pHost,
                                                      sirGraphicsABIDriver_t *pDriver ) noexcept
{
	if ( pFactory == nullptr || pDriver == nullptr || pDriver->nSize < sizeof( sirGraphicsABIDriver_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eHostStatus = sirGraphicsValidateABIHost( pHost );
	if ( eHostStatus != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eHostStatus;
	}
	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	const uint32_t nStatus = pFactory( SIR_GRAPHICS_DRIVER_ABI_MAJOR, SIR_GRAPHICS_DRIVER_ABI_MINOR, pHost, &driver );
	if ( nStatus != SIR_GRAPHICS_ABI_SUCCESS ) {
		return nStatus <= SIR_GRAPHICS_VERSION_MISMATCH ? ( sirGraphicsABIResult_t )nStatus : SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	const sirGraphicsABIResult_t eDriverStatus = sirGraphicsValidateABIDriver( &driver );
	if ( eDriverStatus != SIR_GRAPHICS_ABI_SUCCESS ) {
		if ( driver.pReleaseDriver != nullptr && driver.pDriver != nullptr ) {
			driver.pReleaseDriver( driver.pDriver );
		}
		return eDriverStatus;
	}
	memcpy( pDriver, &driver, sizeof( driver ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABIHost( const sirGraphicsABIHost_t *pHost ) noexcept
{
	if ( pHost == nullptr || pHost->nSize < sizeof( sirGraphicsABIHost_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pHost->nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || pHost->nMinor < SIR_GRAPHICS_DRIVER_ABI_MINOR ) {
		return SIR_GRAPHICS_ABI_VERSION_MISMATCH;
	}
	if ( pHost->nReserved != 0 || pHost->pAllocate == nullptr || pHost->pFree == nullptr || pHost->pLog == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABIDriver( const sirGraphicsABIDriver_t *pDriver ) noexcept
{
	if ( pDriver == nullptr || pDriver->nSize < sizeof( sirGraphicsABIDriver_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pDriver->nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || pDriver->nMinor < SIR_GRAPHICS_DRIVER_ABI_MINOR ) {
		return SIR_GRAPHICS_ABI_VERSION_MISMATCH;
	}
	if ( pDriver->nBackend < SIR_GRAPHICS_BACKEND_VULKAN || pDriver->nBackend > SIR_GRAPHICS_BACKEND_OPENGL ||
	     pDriver->pDriver == nullptr || pDriver->pAdapterCount == nullptr || pDriver->pQueryAdapter == nullptr ||
	     pDriver->pCreateDevice == nullptr || pDriver->pDestroyDevice == nullptr || pDriver->pReleaseDriver == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABIAdapter( const sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pAdapter == nullptr || pAdapter->nSize < sizeof( sirGraphicsABIAdapter_t ) || pAdapter->nBackend < SIR_GRAPHICS_BACKEND_VULKAN ||
	     pAdapter->nBackend > SIR_GRAPHICS_BACKEND_OPENGL || pAdapter->nKind < SIR_GRAPHICS_ABI_ADAPTER_DISCRETE ||
	     pAdapter->nKind > SIR_GRAPHICS_ABI_ADAPTER_OTHER || pAdapter->nQueueMask == 0 ||
	     ( pAdapter->nQueueMask & ~( ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U ) ) != 0 || pAdapter->nReserved != 0 ||
	     pAdapter->szName[127] != '\0' ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABICapabilities( const sirGraphicsABICapabilities_t *pInfo ) noexcept
{
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABICapabilities_t ) || pInfo->nDeviceGeneration == 0 ||
	     pInfo->nBackend < SIR_GRAPHICS_BACKEND_VULKAN || pInfo->nBackend > SIR_GRAPHICS_BACKEND_OPENGL ||
	     pInfo->nAdapterKind < SIR_GRAPHICS_ABI_ADAPTER_DISCRETE || pInfo->nAdapterKind > SIR_GRAPHICS_ABI_ADAPTER_OTHER ||
	     pInfo->nEnabledQueues == 0 || ( pInfo->nEnabledQueues & ~( ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U ) ) != 0 ||
	     pInfo->nMaximumFramesInFlight == 0 || pInfo->nMaximumSamples == 0 || pInfo->nUnifiedMemory > 1 || pInfo->nReserved != 0 ||
	     ( pInfo->nEnabledFeatures & ~( ( SIR_GRAPHICS_FEATURE_PRESENT << 1 ) - 1ULL ) ) != 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const uint32_t nValidModels = ( 1U << SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) - 1U;
	if ( pInfo->nPipelineModelMask == 0 || ( pInfo->nPipelineModelMask & ~nValidModels ) != 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const bool bFixedModel = ( pInfo->nPipelineModelMask & ( 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION - 1 ) ) ) != 0;
	const bool bFixedFeature = ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) != 0;
	if ( bFixedModel != bFixedFeature ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_GRAPHICS ) != 0 &&
	     ( pInfo->nEnabledQueues & ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_PRESENT ) != 0 &&
	     ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE ) != 0 &&
	     ( pInfo->nEnabledQueues & ( 1U << SIR_GRAPHICS_QUEUE_COMPUTE ) ) == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_COPY_QUEUE ) != 0 &&
	     ( pInfo->nEnabledQueues & ( 1U << SIR_GRAPHICS_QUEUE_COPY ) ) == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 &&
	     ( pInfo->nEnabledQueues & ( ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COMPUTE ) ) ) == 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABIFormat( const sirGraphicsABIFormat_t *pFormat ) noexcept
{
	if ( pFormat == nullptr || pFormat->nSize < sizeof( sirGraphicsABIFormat_t ) || pFormat->nFormat <= SIR_GRAPHICS_FORMAT_UNKNOWN ||
	     pFormat->nFormat > SIR_GRAPHICS_FORMAT_BC7_UNORM || pFormat->nReserved != 0 ||
	     ( ( pFormat->nOptimalFeatures | pFormat->nLinearFeatures ) & ~0x7FU ) != 0 || ( pFormat->nSampleCountMask & ~0x7FU ) != 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pFormat->nSampleCountMask == 0 && ( pFormat->nOptimalFeatures | pFormat->nLinearFeatures ) != 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABIDevice( const sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDevice == nullptr || pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) || pDevice->nReserved != 0 ||
	     pDevice->pDevice == nullptr || pDevice->pQueryCapabilities == nullptr || pDevice->pQueryFormat == nullptr ||
	     pDevice->pQueryPipelineModelCaps == nullptr || pDevice->pPoll == nullptr || pDevice->pWait == nullptr ||
	     pDevice->pShutdown == nullptr || pDevice->pQueryMemoryBudget == nullptr || pDevice->pCreateBuffer == nullptr ||
	     pDevice->pCreateTexture == nullptr || pDevice->pCreateView == nullptr || pDevice->pCreateSampler == nullptr ||
	     pDevice->pCreatePipeline == nullptr || pDevice->pCreateBindingSet == nullptr || pDevice->pUpdateBindings == nullptr ||
	     pDevice->pMapBuffer == nullptr || pDevice->pUnmapBuffer == nullptr || pDevice->pFlushBuffer == nullptr ||
	     pDevice->pInvalidateBuffer == nullptr || pDevice->pAcquireCommands == nullptr || pDevice->pSubmit == nullptr ||
	     pDevice->pRetireResource == nullptr || pDevice->pCreateSurface == nullptr || pDevice->pResizeSurface == nullptr ||
	     pDevice->pAcquireSurfaceImage == nullptr || pDevice->pPresent == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABICommandList( const sirGraphicsABICommandList_t *pCommands ) noexcept
{
	if ( pCommands == nullptr || pCommands->nSize < sizeof( sirGraphicsABICommandList_t ) || pCommands->nDeviceGeneration == 0 ||
	     pCommands->nQueue >= SIR_GRAPHICS_QUEUE_COUNT || pCommands->nReserved != 0 || pCommands->pCommands == nullptr ||
	     pCommands->pReset == nullptr || pCommands->pBarrier == nullptr || pCommands->pCopyBuffer == nullptr ||
	     pCommands->pCopyBufferTexture == nullptr || pCommands->pBeginRenderPass == nullptr || pCommands->pEndRenderPass == nullptr ||
	     pCommands->pBindPipeline == nullptr || pCommands->pBindBindingSet == nullptr || pCommands->pBindVertexBuffer == nullptr ||
	     pCommands->pBindIndexBuffer == nullptr || pCommands->pSetViewport == nullptr || pCommands->pDraw == nullptr ||
	     pCommands->pDrawIndexed == nullptr || pCommands->pDispatch == nullptr || pCommands->pClose == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

// 0=headless compute; 1=reduced graphics; 2=modern graphics.
// A reported capability is an enabled device fact, never a guessed API tier.
sirGraphicsABIResult_t sirGraphicsSelectABIProfile( const sirGraphicsABICapabilities_t *pInfo, uint32_t *pProfile ) noexcept
{
	if ( pProfile == nullptr ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	*pProfile = 0;
	const sirGraphicsABIResult_t eStatus = sirGraphicsValidateABICapabilities( pInfo );
	if ( eStatus != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eStatus;
	}
	if ( ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ) {
		return ( pInfo->nEnabledFeatures & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 ? SIR_GRAPHICS_ABI_SUCCESS : SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	*pProfile = 1;
	const uint64_t nModern =
	    SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE | SIR_GRAPHICS_FEATURE_MRT | SIR_GRAPHICS_FEATURE_PRESENT;
	if ( ( pInfo->nEnabledFeatures & nModern ) == nModern && pInfo->nMaximumColorAttachments >= 4 && pInfo->nMaximumBindings >= 16 ) {
		*pProfile = 2;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}
