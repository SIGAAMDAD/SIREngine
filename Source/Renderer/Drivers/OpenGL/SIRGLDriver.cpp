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

#include <Renderer/Drivers/OpenGL/SIRGLCapabilities.h>
#include <Renderer/Drivers/OpenGL/SIRGLBuffers.h>
#include <Renderer/Drivers/OpenGL/SIRGLContext.h>
#include <Renderer/Drivers/OpenGL/SIRGLFormats.h>
#include <Renderer/Drivers/OpenGL/SIRGLFramebuffers.h>
#include <Renderer/Drivers/OpenGL/SIRGLFramebufferCache.h>
#include <Renderer/Drivers/OpenGL/SIRGLHandlePool.h>
#include <Renderer/Drivers/OpenGL/SIRGLSamplers.h>
#include <Renderer/Drivers/OpenGL/SIRGLTextures.h>
#include <atomic>
#include <new>
#include <string.h>

#if defined( _WIN32 )
#define SIR_GL_EXPORT __declspec( dllexport )
#else
#define SIR_GL_EXPORT __attribute__( ( visibility( "default" ) ) )
#endif

typedef struct sirGLDriverState_s
{
	sirGraphicsABIHost_t host;
	std::atomic<uint32_t> nLiveDevices;
} sirGLDriverState_t;

typedef struct sirGLDevice_s sirGLDevice_t;

typedef struct sirGLCommandState_s
{
	sirGLDevice_t *pDevice;
	uint32_t nPassFramebuffer;
	bool bRecording;
	bool bInRenderPass;
	bool bClosed;
} sirGLCommandState_t;

constexpr uint32_t SIR_GL_PRESENT_SURFACE_COUNT = 8;
typedef struct sirGLPresentSlot_s
{
	sirGLPresentWindow_t window;
	sirGraphicsHandle_t hSurface;
	sirGraphicsHandle_t hImage;
	uint32_t nFramebuffer;
	uint32_t nFormat;
	uint64_t nAcquisitionSerial;
	uint64_t nAcquisitionId;
	bool bActive;
	bool bAcquired;
} sirGLPresentSlot_t;

typedef struct sirGLDevice_s
{
	sirGLDriverState_t *pDriver;
	sirGLNativeContext_t *pContext;
	sirGLProbe_t probe;
	sirGLHandlePool_t handles;
	sirGLBufferDispatch_t buffers;
	sirGLSamplerDispatch_t samplers;
	sirGLTextureDispatch_t textures;
	sirGLFormatDispatch_t formats;
	sirGLFramebufferDispatch_t framebuffers;
	sirGLFramebufferCache_t framebufferCache;
	uint64_t nCommittedResourceBytes;
	uint64_t nPeakResourceBytes;
	uint32_t nGeneration;
	uint64_t nResidentBudgetBytes;
	uint64_t nCompletedDirectValue;
	sirGLCommandState_t commands;
	sirGLPresentSlot_t present[SIR_GL_PRESENT_SURFACE_COUNT];
	void ( *pFinish )( void );
	void ( *pBlitNamedFramebuffer )( uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t,
	                                 int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t );
	void ( *pClearNamedFramebuffer )( uint32_t, uint32_t, int32_t, const float * );
	uint32_t ( *pGetNativeError )( void );
	bool bWindowed;
	bool bDisplayBound;
	bool bAllowSoftware;
} sirGLDevice_t;

static std::atomic<uint32_t> s_nGLDeviceGeneration( 1 );

static uint32_t sirGLAdapterCount( void *pDriver, uint32_t *pCount ) noexcept
{
	if ( pDriver == nullptr || pCount == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
	sirGLDriverState_t *pState = ( sirGLDriverState_t * )pDriver;
	sirGLNativeContext_t *pContext = nullptr;
	const uint32_t nStatus = sirGLCreateNativeContext( &pState->host, &pContext );
	if ( nStatus == SIR_GRAPHICS_OUT_OF_MEMORY ) {
		return nStatus;
	}
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_SUCCESS; // No admissible local GL46 context is an empty adapter list.
	}
	sirGLProbe_t probe = {};
	if ( sirGLReadNativeProbe( pContext, &probe ) == SIR_GRAPHICS_SUCCESS && sirGLValidateCore46( &probe ) == SIR_GRAPHICS_SUCCESS ) {
		*pCount = 1;
	}
	sirGLDestroyNativeContext( pContext );
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLQueryAdapter( void *pDriver, sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pDriver == nullptr || pAdapter == nullptr || pAdapter->nSize < sizeof( *pAdapter ) || pAdapter->nIndex != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDriverState_t *pState = ( sirGLDriverState_t * )pDriver;
	sirGLNativeContext_t *pContext = nullptr;
	const uint32_t nResult = sirGLCreateNativeContext( &pState->host, &pContext );
	if ( nResult != SIR_GRAPHICS_SUCCESS ) {
		return nResult;
	}
	sirGLProbe_t probe = {};
	uint32_t nStatus = sirGLReadNativeProbe( pContext, &probe );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLBuildAdapter( &probe, pAdapter );
	}
	sirGLDestroyNativeContext( pContext );
	return nStatus;
}

static uint32_t sirGLQueryCapabilities( void *pDevice, sirGraphicsABICapabilities_t *pCapabilities ) noexcept
{
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState == nullptr || pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const uint32_t nStatus = sirGLBuildCapabilities( &pState->probe, pState->nGeneration, pCapabilities );
	if ( nStatus == SIR_GRAPHICS_SUCCESS && pState->bWindowed ) {
		pCapabilities->nEnabledFeatures |= SIR_GRAPHICS_FEATURE_PRESENT;
	}
	return nStatus;
}

static uint32_t sirGLQueryPipelineModelCaps( void *pDevice, uint32_t nModel, sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	if ( pDevice == nullptr || pCaps == nullptr || pCaps->nSize < sizeof( *pCaps ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGLDevice_t *pState = ( const sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	// The context foundation has no executable graphics or compute pipelines.
	// No pipeline model may be admitted until native GL programs and draws exist.
	( void )nModel;
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLQueryFormat( void *pDevice, sirGraphicsABIFormat_t *pFormat ) noexcept
{
	if ( pDevice == nullptr || pFormat == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	return sirGLQueryNativeFormat( &pState->formats, pFormat );
}

static uint32_t sirGLQueryMemoryBudget( void *pDevice, sirGraphicsMemoryBudget_t *pBudget ) noexcept
{
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState == nullptr || pBudget == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	*pBudget = {};
	pBudget->nBudgetBytes = pState->nResidentBudgetBytes;
	// These are GL storage requests, not measured driver VRAM residency.
	pBudget->nCommittedBytes = pState->nCommittedResourceBytes;
	pBudget->nPeakCommittedBytes = pState->nPeakResourceBytes;
	// Opaque OpenGL driver residency is not measurable from our unallocated resource pool.
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLPoll( void *, const sirGraphicsCompletion_t *, uint32_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLWait( void *, const sirGraphicsCompletion_t *, uint64_t ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLCreateBuffer( void *pDevice, const sirGraphicsBufferDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pDevice == nullptr || pDesc == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pState->handles.nFreeHead == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pDesc->nSizeBytes > UINT64_MAX - pState->nCommittedResourceBytes ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pState->nResidentBudgetBytes != 0 ) {
		if ( pDesc->nSizeBytes > pState->nResidentBudgetBytes ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		if ( pState->nCommittedResourceBytes > pState->nResidentBudgetBytes - pDesc->nSizeBytes ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
	}
	uint32_t nName = 0;
	uint32_t nStatus = sirGLCreateNativeBuffer( &pState->buffers, pDesc, &nName );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGLNativeResource_t native = {};
	native.nName = nName;
	native.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	native.nRequestedBytes = pDesc->nSizeBytes;
	nStatus = sirGLHandlePoolInsert( &pState->handles, &native, pHandle );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLDeleteNativeBuffer( &pState->buffers, nName );
		return nStatus;
	}
	pState->nCommittedResourceBytes += pDesc->nSizeBytes;
	if ( pState->nCommittedResourceBytes > pState->nPeakResourceBytes ) {
		pState->nPeakResourceBytes = pState->nCommittedResourceBytes;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLCreateTexture( void *pDevice, const sirGraphicsTextureDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pDevice == nullptr || pDesc == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pState->handles.nFreeHead == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirGLNativeTexture_t texture = {};
	uint32_t nStatus = sirGLDescribeTexture2D( pDesc, pState->probe.nMaximumTextureSize, &texture );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = pDesc->eFormat;
	nStatus = sirGLQueryNativeFormat( &pState->formats, &format );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	if ( texture.nRequestedBytes > UINT64_MAX - pState->nCommittedResourceBytes ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pState->nResidentBudgetBytes != 0 &&
	     ( texture.nRequestedBytes > pState->nResidentBudgetBytes ||
	       pState->nCommittedResourceBytes > pState->nResidentBudgetBytes - texture.nRequestedBytes ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	nStatus = sirGLCreateNativeTexture2D( &pState->textures, pDesc, &texture );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGLNativeResource_t native = {};
	native.nName = texture.nName;
	native.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	native.nRequestedBytes = texture.nRequestedBytes;
	native.nInternalFormat = texture.nInternalFormat;
	native.eFormat = pDesc->eFormat;
	native.nMipLevels = pDesc->nMipLevels;
	native.nUsage = pDesc->nUsage;
	nStatus = sirGLHandlePoolInsert( &pState->handles, &native, pHandle );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLDeleteNativeTexture( &pState->textures, texture.nName );
		return nStatus;
	}
	pState->nCommittedResourceBytes += texture.nRequestedBytes;
	if ( pState->nCommittedResourceBytes > pState->nPeakResourceBytes ) {
		pState->nPeakResourceBytes = pState->nCommittedResourceBytes;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLCreateView( void *pDevice, const sirGraphicsViewDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pDevice == nullptr || pDesc == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pState->handles.nFreeHead == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirGLNativeResource_t parent = {};
	uint32_t nStatus = sirGLHandlePoolFind( &pState->handles, pDesc->hResource, &parent );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	if ( parent.eType != SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->nMipLevels == 0 || pDesc->nLayers == 0 || pDesc->nBufferOffsetBytes != 0 || pDesc->nBufferSizeBytes != 0 ||
	     pDesc->nBaseMip >= parent.nMipLevels || pDesc->nMipLevels > parent.nMipLevels - pDesc->nBaseMip ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Only immutable 2D sampled views with the parent's exact storage format are implemented.
	if ( pDesc->eFormat != parent.eFormat || pDesc->nUsage != SIR_GRAPHICS_USAGE_SAMPLED ||
	     ( parent.nUsage & SIR_GRAPHICS_USAGE_SAMPLED ) == 0 || pDesc->nBaseLayer != 0 || pDesc->nLayers != 1 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGLTextureViewCreateInfo_t info = { parent.nName, parent.nInternalFormat, pDesc->nBaseMip, pDesc->nMipLevels };
	uint32_t nName = 0;
	nStatus = sirGLCreateNativeTextureView( &pState->textures, &info, &nName );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGLNativeResource_t native = {};
	native.nName = nName;
	native.eType = SIR_GRAPHICS_HANDLE_VIEW;
	native.hParent = pDesc->hResource;
	nStatus = sirGLHandlePoolInsert( &pState->handles, &native, pHandle );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLDeleteNativeTexture( &pState->textures, nName );
	}
	return nStatus;
}

static uint32_t sirGLCreateSampler( void *pDevice, const sirGraphicsSamplerDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pDevice == nullptr || pDesc == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pState->handles.nFreeHead == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	uint32_t nName = 0;
	uint32_t nStatus = sirGLCreateNativeSampler( &pState->samplers, pDesc, &nName );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGLNativeResource_t native = {};
	native.nName = nName;
	native.eType = SIR_GRAPHICS_HANDLE_SAMPLER;
	nStatus = sirGLHandlePoolInsert( &pState->handles, &native, pHandle );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLDeleteNativeSampler( &pState->samplers, nName );
	}
	return nStatus;
}

static uint32_t sirGLCreatePipeline( void *, const sirGraphicsABIPipeline_t *, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLCreateBindingSet( void *, sirGraphicsHandle_t, uint32_t, sirGraphicsHandle_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLUpdateBindings( void *, sirGraphicsHandle_t, const sirGraphicsDescriptorWrite_t *, uint32_t ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLMapBuffer( void *, const sirGraphicsABIBufferRange_t *, sirGraphicsMappedBuffer_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLUnmapBuffer( void *, const sirGraphicsABIBufferRange_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLFlushBuffer( void *, const sirGraphicsABIBufferRange_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLInvalidateBuffer( void *, const sirGraphicsABIBufferRange_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLAcquireCommands( void *, uint32_t, sirGraphicsABICommandList_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLSubmit( void *, uint32_t, const sirGraphicsABISubmission_t *, sirGraphicsCompletion_t * ) noexcept
{
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLRetireResource( void *pDevice, sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t *pCompletion ) noexcept
{
	if ( pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	// GPU submissions and fence-backed retirement are not yet implemented.
	if ( pCompletion != nullptr ) {
		if ( pCompletion->nDeviceGeneration != pState->nGeneration ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( pCompletion->nQueueMask != 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		for ( uint32_t i = 0; i < SIR_GRAPHICS_QUEUE_COUNT; ++i ) {
			if ( pCompletion->szQueueValues[i] != 0 ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	sirGLNativeResource_t resource = {};
	uint32_t nStatus = sirGLHandlePoolFind( &pState->handles, hResource, &resource );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	if ( resource.eType == SIR_GRAPHICS_HANDLE_TEXTURE && pState->handles.pSlots[hResource.nSlot].nChildCount != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( resource.eType == SIR_GRAPHICS_HANDLE_TEXTURE || resource.eType == SIR_GRAPHICS_HANDLE_VIEW ) {
		nStatus = sirGLFramebufferCacheInvalidate( &pState->framebufferCache, hResource );
		if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
			return nStatus;
		}
	}
	if ( resource.eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
		nStatus = sirGLDeleteNativeBuffer( &pState->buffers, resource.nName );
	} else if ( resource.eType == SIR_GRAPHICS_HANDLE_SAMPLER ) {
		nStatus = sirGLDeleteNativeSampler( &pState->samplers, resource.nName );
	} else if ( resource.eType == SIR_GRAPHICS_HANDLE_TEXTURE || resource.eType == SIR_GRAPHICS_HANDLE_VIEW ) {
		nStatus = sirGLDeleteNativeTexture( &pState->textures, resource.nName );
	} else {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	nStatus = sirGLHandlePoolTake( &pState->handles, hResource, &resource );
	if ( nStatus == SIR_GRAPHICS_SUCCESS &&
	     ( resource.eType == SIR_GRAPHICS_HANDLE_BUFFER || resource.eType == SIR_GRAPHICS_HANDLE_TEXTURE ) ) {
		pState->nCommittedResourceBytes -= resource.nRequestedBytes;
	}
	return nStatus;
}


static uint32_t sirGLLoadPresentationDispatch( sirGLDevice_t *pState, sirGLNativeContext_t *pContext ) noexcept
{
	pState->pFinish = ( void ( * )( void ) )sirGLResolveNativeProcedure( pContext, "glFinish" );
	pState->pBlitNamedFramebuffer =
	    ( void ( * )( uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
	                   uint32_t, uint32_t ) )sirGLResolveNativeProcedure( pContext, "glBlitNamedFramebuffer" );
	pState->pClearNamedFramebuffer =
	    ( void ( * )( uint32_t, uint32_t, int32_t, const float * ) )sirGLResolveNativeProcedure(
	        pContext, "glClearNamedFramebufferfv" );
	pState->pGetNativeError = ( uint32_t ( * )( void ) )sirGLResolveNativeProcedure( pContext, "glGetError" );
	return pState->pFinish != nullptr && pState->pBlitNamedFramebuffer != nullptr &&
	               pState->pClearNamedFramebuffer != nullptr && pState->pGetNativeError != nullptr ?
	           SIR_GRAPHICS_SUCCESS :
	           SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirGLPreparePresentationDisplay( sirGLDevice_t *pState,
                                                 const sirGraphicsABINativeSurface_t *pNative ) noexcept
{
	if ( pState == nullptr || pNative == nullptr || !pState->bWindowed ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pState->bDisplayBound ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( pState->handles.nActive != 0 || pState->framebufferCache.nCount != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
#if defined( _WIN32 )
	if ( pNative->nPlatform != SIR_GRAPHICS_SURFACE_WIN32 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	pState->bDisplayBound = true;
	return sirGLLoadPresentationDispatch( pState, pState->pContext );
#else
	if ( pNative->nPlatform != SIR_GRAPHICS_SURFACE_X11 && pNative->nPlatform != SIR_GRAPHICS_SURFACE_WAYLAND ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGLNativeContext_t *pWindowContext = nullptr;
	uint32_t nStatus = sirGLCreateNativeContextForWindow( &pState->pDriver->host, pNative, &pWindowContext );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLRestoreNativeContext( pState->pContext );
		return nStatus;
	}
	sirGLProbe_t probe = {};
	nStatus = sirGLReadNativeProbe( pWindowContext, &probe );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLValidateCore46( &probe );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS && !pState->bAllowSoftware ) {
		sirGraphicsABIAdapter_t adapter = {};
		adapter.nSize = sizeof( adapter );
		if ( sirGLBuildAdapter( &probe, &adapter ) != SIR_GRAPHICS_SUCCESS ||
		     adapter.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE ) {
			nStatus = SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	sirGLBufferDispatch_t buffers = {};
	sirGLSamplerDispatch_t samplers = {};
	sirGLTextureDispatch_t textures = {};
	sirGLFormatDispatch_t formats = {};
	sirGLFramebufferDispatch_t framebuffers = {};
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadBufferDispatch( sirGLResolveNativeProcedure, pWindowContext, &buffers );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadSamplerDispatch( sirGLResolveNativeProcedure, pWindowContext, &samplers );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadTextureDispatch( sirGLResolveNativeProcedure, pWindowContext, &textures );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadFormatDispatch( sirGLResolveNativeProcedure, pWindowContext, &formats );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadFramebufferDispatch( sirGLResolveNativeProcedure, pWindowContext, &framebuffers );
	}
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		sirGLDestroyNativeContext( pWindowContext );
		( void )sirGLRestoreNativeContext( pState->pContext );
		return nStatus;
	}
	sirGLNativeContext_t *pOldContext = pState->pContext;
	pState->pContext = pWindowContext;
	pState->probe = probe;
	pState->buffers = buffers;
	pState->samplers = samplers;
	pState->textures = textures;
	pState->formats = formats;
	pState->framebuffers = framebuffers;
	nStatus = sirGLLoadPresentationDispatch( pState, pWindowContext );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		pState->pContext = pOldContext;
		sirGLDestroyNativeContext( pWindowContext );
		( void )sirGLRestoreNativeContext( pOldContext );
		return nStatus;
	}
	sirGLDestroyNativeContext( pOldContext );
	if ( sirGLRestoreNativeContext( pWindowContext ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	pState->bDisplayBound = true;
	return SIR_GRAPHICS_SUCCESS;
#endif
}

static uint32_t sirGLCreatePresentationImage( sirGLDevice_t *pState, uint32_t nWidth, uint32_t nHeight,
                                                uint32_t nFormat, sirGraphicsHandle_t *pImage, uint32_t *pFramebuffer ) noexcept
{
	if ( pState == nullptr || pImage == nullptr || pFramebuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsTextureDesc_t desc = { nWidth, nHeight, 1, 1, 1, 1,
	                                        SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_SAMPLED |
	                                            SIR_GRAPHICS_USAGE_COPY_SOURCE,
	                                        SIR_GRAPHICS_TEXTURE_2D, ( sirGraphicsFormat_t )nFormat,
	                                        SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	sirGraphicsHandle_t hImage = {};
	uint32_t nStatus = sirGLCreateTexture( pState, &desc, &hImage );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGLNativeResource_t image = {};
	nStatus = sirGLHandlePoolFind( &pState->handles, hImage, &image );
	uint32_t nFramebuffer = 0;
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		const sirGLFramebufferDesc_t fboDesc = { image.nName, 0, 0, 0,
		                                         ( sirGraphicsFormat_t )nFormat, SIR_GRAPHICS_FORMAT_UNKNOWN };
		nStatus = sirGLCreateNativeFramebuffer( &pState->framebuffers, &fboDesc, &nFramebuffer );
	}
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		sirGraphicsCompletionSet_t noPending = {};
		noPending.nDeviceGeneration = pState->nGeneration;
		( void )sirGLRetireResource( pState, hImage, &noPending );
		return nStatus;
	}
	*pImage = hImage;
	*pFramebuffer = nFramebuffer;
	return SIR_GRAPHICS_SUCCESS;
}

static sirGLPresentSlot_t *sirGLFindPresentSlot( sirGLDevice_t *pState, sirGraphicsHandle_t hSurface ) noexcept
{
	for ( uint32_t i = 0; i < SIR_GL_PRESENT_SURFACE_COUNT; ++i ) {
		sirGLPresentSlot_t& slot = pState->present[i];
		if ( slot.bActive && slot.hSurface.nSlot == hSurface.nSlot &&
		     slot.hSurface.nGeneration == hSurface.nGeneration &&
		     slot.hSurface.nDeviceGeneration == hSurface.nDeviceGeneration &&
		     hSurface.eType == SIR_GRAPHICS_HANDLE_SURFACE ) {
			return &slot;
		}
	}
	return nullptr;
}

static uint32_t sirGLCreateSurface( void *pDevice, const sirGraphicsABINativeSurface_t *pNative,
                                    const sirGraphicsABIPresentDesc_t *pPresent, sirGraphicsHandle_t *pSurface ) noexcept
{
	if ( pDevice == nullptr || pNative == nullptr || pPresent == nullptr || pSurface == nullptr ||
	     pNative->nSize < sizeof( *pNative ) || pPresent->nSize < sizeof( *pPresent ) ||
	     pNative->pWindow == nullptr || pNative->nWindowGeneration == 0 ||
	     pPresent->nWidth == 0 || pPresent->nHeight == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( !pState->bWindowed || pPresent->nBackbufferCount == 0 || pPresent->nBackbufferCount > 2 ||
	     pPresent->nMode != SIR_GRAPHICS_PRESENT_FIFO || pPresent->nAllowTearing != 0 ||
	     pPresent->nHdr != 0 ||
	     ( pPresent->nFormat != SIR_GRAPHICS_FORMAT_RGBA8_UNORM &&
	       pPresent->nFormat != SIR_GRAPHICS_FORMAT_RGBA8_SRGB ) ||
	     pPresent->nWidth > pState->probe.nMaximumTextureSize || pPresent->nHeight > pState->probe.nMaximumTextureSize ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nSlot = SIR_GL_PRESENT_SURFACE_COUNT;
	for ( uint32_t i = 0; i < SIR_GL_PRESENT_SURFACE_COUNT; ++i ) {
		if ( !pState->present[i].bActive ) {
			nSlot = i;
			break;
		}
	}
	if ( nSlot == SIR_GL_PRESENT_SURFACE_COUNT ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	uint32_t nStatus = sirGLPreparePresentationDisplay( pState, pNative );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGLPresentSlot_t pending = {};
	nStatus = sirGLCreatePresentWindow( pState->pContext, pNative, pPresent->nWidth, pPresent->nHeight, &pending.window );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	nStatus = sirGLCreatePresentationImage( pState, pPresent->nWidth, pPresent->nHeight, pPresent->nFormat,
	                                        &pending.hImage, &pending.nFramebuffer );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLDestroyPresentWindow( pState->pContext, &pending.window );
		return nStatus;
	}
	sirGLNativeResource_t resource = {};
	resource.nName = nSlot + 1; // Internal slot identifier, never a GLuint.
	resource.eType = SIR_GRAPHICS_HANDLE_SURFACE;
	nStatus = sirGLHandlePoolInsert( &pState->handles, &resource, &pending.hSurface );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLDeleteNativeFramebuffer( &pState->framebuffers, pending.nFramebuffer );
		sirGraphicsCompletionSet_t noPending = {};
		noPending.nDeviceGeneration = pState->nGeneration;
		( void )sirGLRetireResource( pState, pending.hImage, &noPending );
		( void )sirGLDestroyPresentWindow( pState->pContext, &pending.window );
		return nStatus;
	}
	pending.nFormat = pPresent->nFormat;
	pending.bActive = true;
	pState->present[nSlot] = pending;
	*pSurface = pending.hSurface;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLResizeSurface( void *pDevice, sirGraphicsHandle_t hSurface,
                                    const sirGraphicsABINativeSurface_t *pNative,
                                    const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	if ( pDevice == nullptr || pNative == nullptr || pPresent == nullptr ||
	     pNative->nSize < sizeof( *pNative ) || pPresent->nSize < sizeof( *pPresent ) ||
	     pPresent->nWidth == 0 || pPresent->nHeight == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirGLPresentSlot_t *pSlot = sirGLFindPresentSlot( pState, hSurface );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->bAcquired || pState->handles.pSlots[pSlot->hImage.nSlot].nChildCount != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pNative->nWindowGeneration != pSlot->window.nWindowGeneration ||
	     pNative->pWindow != pSlot->window.pNativeWindow ||
	     pNative->pDisplayOrInstance != pSlot->window.pNativeDisplay ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	if ( pPresent->nBackbufferCount == 0 || pPresent->nBackbufferCount > 2 ||
	     pPresent->nMode != SIR_GRAPHICS_PRESENT_FIFO || pPresent->nAllowTearing != 0 ||
	     pPresent->nHdr != 0 ||
	     ( pPresent->nFormat != SIR_GRAPHICS_FORMAT_RGBA8_UNORM &&
	       pPresent->nFormat != SIR_GRAPHICS_FORMAT_RGBA8_SRGB ) ||
	     pPresent->nWidth > pState->probe.nMaximumTextureSize || pPresent->nHeight > pState->probe.nMaximumTextureSize ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pSlot->window.nWidth == pPresent->nWidth && pSlot->window.nHeight == pPresent->nHeight &&
	     pSlot->nFormat == pPresent->nFormat ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsHandle_t hReplacement = {};
	uint32_t nReplacementFramebuffer = 0;
	uint32_t nStatus = sirGLCreatePresentationImage( pState, pPresent->nWidth, pPresent->nHeight, pPresent->nFormat,
	                                                 &hReplacement, &nReplacementFramebuffer );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	nStatus = sirGLResizePresentWindow( pState->pContext, &pSlot->window, pPresent->nWidth, pPresent->nHeight );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLDeleteNativeFramebuffer( &pState->framebuffers, nReplacementFramebuffer );
		sirGraphicsCompletionSet_t noPending = {};
		noPending.nDeviceGeneration = pState->nGeneration;
		( void )sirGLRetireResource( pState, hReplacement, &noPending );
		return nStatus;
	}
	const sirGraphicsHandle_t hOldImage = pSlot->hImage;
	const uint32_t nOldFramebuffer = pSlot->nFramebuffer;
	pSlot->hImage = hReplacement;
	pSlot->nFramebuffer = nReplacementFramebuffer;
	pSlot->nFormat = pPresent->nFormat;
	( void )sirGLDeleteNativeFramebuffer( &pState->framebuffers, nOldFramebuffer );
	sirGraphicsCompletionSet_t noPending = {};
	noPending.nDeviceGeneration = pState->nGeneration;
	return sirGLRetireResource( pState, hOldImage, &noPending );
}

static uint32_t sirGLAcquireSurfaceImage( void *pDevice, sirGraphicsHandle_t hSurface,
                                          sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	if ( pDevice == nullptr || pImage == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirGLPresentSlot_t *pSlot = sirGLFindPresentSlot( pState, hSurface );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->bAcquired || pSlot->nAcquisitionSerial == UINT64_MAX ) {
		return SIR_GRAPHICS_BUSY;
	}
	pSlot->nAcquisitionId = ++pSlot->nAcquisitionSerial;
	pSlot->bAcquired = true;
	*pImage = { pSlot->hSurface, pSlot->hImage, pSlot->nAcquisitionId, pSlot->window.nWindowGeneration };
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLPresent( void *pDevice, const sirGraphicsSurfaceImage_t *pImage,
                              const sirGraphicsCompletion_t *pComplete ) noexcept
{
	if ( pDevice == nullptr || pImage == nullptr || pComplete == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirGLPresentSlot_t *pSlot = sirGLFindPresentSlot( pState, pImage->hSurface );
	if ( pSlot == nullptr || !pSlot->bAcquired || pSlot->nAcquisitionId != pImage->nAcquisitionId ||
	     pSlot->hImage.nSlot != pImage->hImage.nSlot ||
	     pSlot->hImage.nGeneration != pImage->hImage.nGeneration ||
	     pSlot->window.nWindowGeneration != pImage->nWindowGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pComplete->nDeviceGeneration != pState->nGeneration || pComplete->eQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
	     pComplete->nValue == 0 || pComplete->nValue > pState->nCompletedDirectValue ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t nStatus = sirGLBeginPresentWindow( pState->pContext, &pSlot->window );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	pState->pBlitNamedFramebuffer( pSlot->nFramebuffer, 0, 0, 0, ( int32_t )pSlot->window.nWidth,
	                              ( int32_t )pSlot->window.nHeight, 0, 0, ( int32_t )pSlot->window.nWidth,
	                              ( int32_t )pSlot->window.nHeight, 0x00004000, 0x2600 );
	const uint32_t nError = pState->pGetNativeError();
	if ( nError != 0 ) {
		( void )sirGLRestoreNativeContext( pState->pContext );
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	nStatus = sirGLSwapPresentWindow( pState->pContext, &pSlot->window );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		pSlot->bAcquired = false;
	}
	return nStatus;
}

static uint32_t sirGLShutdown( void *pDevice ) noexcept
{
	sirGLDevice_t *pState = ( sirGLDevice_t * )pDevice;
	if ( pState == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pState->pContext == nullptr ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( !sirGLContextOwnsThread( pState->pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pState->handles.nActive != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirGraphicsResult_t eStatus = sirGLFramebufferCacheShutdown( &pState->framebufferCache );
	if ( eStatus != SIR_GRAPHICS_SUCCESS ) {
		return eStatus;
	}
	eStatus = sirGLHandlePoolShutdown( &pState->handles );
	if ( eStatus != SIR_GRAPHICS_SUCCESS ) {
		return eStatus;
	}
	sirGLDestroyNativeContext( pState->pContext );
	pState->pContext = nullptr;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLCreateDevice( void *pDriver, const sirGraphicsABIDeviceRequest_t *pRequest, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDriver == nullptr || pRequest == nullptr || pDevice == nullptr || pRequest->nSize < sizeof( *pRequest ) ||
	     pDevice->nSize < sizeof( *pDevice ) || pRequest->nReserved != 0 || pRequest->nFramesInFlight == 0 ||
	     pRequest->nAllowSoftware > 1 || pRequest->nHeadless > 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pRequest->nAdapterIndex != 0 || pRequest->nFramesInFlight != 1 ||
	     ( pRequest->nRequiredFeatures & ~( uint64_t )SIR_GRAPHICS_FEATURE_PRESENT ) != 0 ||
	     ( pRequest->nHeadless != 0 && pRequest->nRequiredFeatures != 0 ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGLDriverState_t *pState = ( sirGLDriverState_t * )pDriver;
	sirGLNativeContext_t *pContext = nullptr;
	uint32_t nStatus = sirGLCreateNativeContext( &pState->host, &pContext );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	sirGLProbe_t probe = {};
	nStatus = sirGLReadNativeProbe( pContext, &probe );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLValidateCore46( &probe );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS && pRequest->nAllowSoftware == 0 ) {
		sirGraphicsABIAdapter_t adapter = {};
		adapter.nSize = sizeof( adapter );
		if ( sirGLBuildAdapter( &probe, &adapter ) != SIR_GRAPHICS_SUCCESS || adapter.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE ) {
			nStatus = SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		sirGLDestroyNativeContext( pContext );
		return nStatus;
	}
	void *pMemory = pState->host.pAllocate( pState->host.pContext, sizeof( sirGLDevice_t ), alignof( sirGLDevice_t ) );
	if ( pMemory == nullptr ) {
		sirGLDestroyNativeContext( pContext );
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	const uint32_t nGeneration = s_nGLDeviceGeneration.fetch_add( 1, std::memory_order_relaxed );
	if ( nGeneration == 0 ) {
		pState->host.pFree( pState->host.pContext, pMemory );
		sirGLDestroyNativeContext( pContext );
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirGLDevice_t *pNew = ( sirGLDevice_t * )pMemory;
	*pNew = {};
	pNew->pDriver = pState;
	pNew->pContext = pContext;
	pNew->probe = probe;
	pNew->nGeneration = nGeneration;
	pNew->nResidentBudgetBytes = pRequest->nResidentBudgetBytes;
	pNew->bWindowed = pRequest->nHeadless == 0;
	pNew->bAllowSoftware = pRequest->nAllowSoftware != 0;
	pNew->commands.pDevice = pNew;
	// Native GL objects will be stored here without per-object CPU allocations.
	nStatus = sirGLHandlePoolInit( &pNew->handles, &pState->host, nGeneration, 1024 );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		pState->host.pFree( pState->host.pContext, pNew );
		sirGLDestroyNativeContext( pContext );
		return nStatus;
	}
	nStatus = sirGLLoadBufferDispatch( sirGLResolveNativeProcedure, pContext, &pNew->buffers );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadSamplerDispatch( sirGLResolveNativeProcedure, pContext, &pNew->samplers );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadTextureDispatch( sirGLResolveNativeProcedure, pContext, &pNew->textures );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadFormatDispatch( sirGLResolveNativeProcedure, pContext, &pNew->formats );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		nStatus = sirGLLoadFramebufferDispatch( sirGLResolveNativeProcedure, pContext, &pNew->framebuffers );
	}
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		sirGLFramebufferCacheConfig_t cacheConfig = {};
		cacheConfig.pHost = &pState->host;
		cacheConfig.pResources = &pNew->handles;
		cacheConfig.pDispatch = &pNew->framebuffers;
		cacheConfig.nCapacity = 64;
		nStatus = sirGLFramebufferCacheInit( &pNew->framebufferCache, &cacheConfig );
	}
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		( void )sirGLHandlePoolShutdown( &pNew->handles );
		pState->host.pFree( pState->host.pContext, pNew );
		sirGLDestroyNativeContext( pContext );
		return nStatus;
	}

	sirGraphicsABIDevice_t result = {};
	result.nSize = sizeof( result );
	result.pDevice = pNew;
	result.pQueryCapabilities = sirGLQueryCapabilities;
	result.pQueryPipelineModelCaps = sirGLQueryPipelineModelCaps;
	result.pQueryFormat = sirGLQueryFormat;
	result.pPoll = sirGLPoll;
	result.pWait = sirGLWait;
	result.pQueryMemoryBudget = sirGLQueryMemoryBudget;
	result.pCreateBuffer = sirGLCreateBuffer;
	result.pCreateTexture = sirGLCreateTexture;
	result.pCreateView = sirGLCreateView;
	result.pCreateSampler = sirGLCreateSampler;
	result.pCreatePipeline = sirGLCreatePipeline;
	result.pCreateBindingSet = sirGLCreateBindingSet;
	result.pUpdateBindings = sirGLUpdateBindings;
	result.pMapBuffer = sirGLMapBuffer;
	result.pUnmapBuffer = sirGLUnmapBuffer;
	result.pFlushBuffer = sirGLFlushBuffer;
	result.pInvalidateBuffer = sirGLInvalidateBuffer;
	result.pAcquireCommands = sirGLAcquireCommands;
	result.pSubmit = sirGLSubmit;
	result.pRetireResource = sirGLRetireResource;
	result.pCreateSurface = sirGLCreateSurface;
	result.pResizeSurface = sirGLResizeSurface;
	result.pAcquireSurfaceImage = sirGLAcquireSurfaceImage;
	result.pPresent = sirGLPresent;
	result.pShutdown = sirGLShutdown;
	pState->nLiveDevices.fetch_add( 1, std::memory_order_relaxed );
	*pDevice = result;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirGLDestroyDevice( void *pDriver, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDriver == nullptr || pDevice == nullptr || pDevice->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLDriverState_t *pState = ( sirGLDriverState_t * )pDriver;
	sirGLDevice_t *pNative = ( sirGLDevice_t * )pDevice->pDevice;
	if ( pNative->pDriver != pState ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint32_t nStatus = sirGLShutdown( pNative );
	if ( nStatus != SIR_GRAPHICS_SUCCESS ) {
		return nStatus;
	}
	pState->nLiveDevices.fetch_sub( 1, std::memory_order_relaxed );
	pState->host.pFree( pState->host.pContext, pNative );
	*pDevice = {};
	return SIR_GRAPHICS_SUCCESS;
}

static void sirGLReleaseDriver( void *pDriver ) noexcept
{
	sirGLDriverState_t *pState = ( sirGLDriverState_t * )pDriver;
	if ( pState == nullptr ) {
		return;
	}
	if ( pState->nLiveDevices.load( std::memory_order_acquire ) != 0 ) {
		pState->host.pLog( pState->host.pContext, 3, "OpenGL ABI module unloaded with live devices" );
		return;
	}
	const sirGraphicsABIHost_t host = pState->host;
	pState->~sirGLDriverState_t();
	host.pFree( host.pContext, pState );
}

extern "C" SIR_GL_EXPORT uint32_t SIRGetGraphicsDriverABI( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                                           sirGraphicsABIDriver_t *pDriver ) noexcept
{
	if ( nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || nMinor > SIR_GRAPHICS_DRIVER_ABI_MINOR ) {
		return SIR_GRAPHICS_VERSION_MISMATCH;
	}
	if ( pHost == nullptr || pDriver == nullptr || pHost->nSize < sizeof( *pHost ) || pDriver->nSize < sizeof( *pDriver ) ||
	     pHost->nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || pHost->nMinor < SIR_GRAPHICS_DRIVER_ABI_MINOR || pHost->nReserved != 0 ||
	     pHost->pAllocate == nullptr || pHost->pFree == nullptr || pHost->pLog == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pMemory = pHost->pAllocate( pHost->pContext, sizeof( sirGLDriverState_t ), alignof( sirGLDriverState_t ) );
	if ( pMemory == nullptr ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	sirGLDriverState_t *pState = new ( pMemory ) sirGLDriverState_t{};
	pState->host = *pHost;
	sirGraphicsABIDriver_t result = {};
	result.nSize = sizeof( result );
	result.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	result.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	result.nBackend = SIR_GRAPHICS_BACKEND_OPENGL;
	result.pDriver = pState;
	result.pAdapterCount = sirGLAdapterCount;
	result.pQueryAdapter = sirGLQueryAdapter;
	result.pCreateDevice = sirGLCreateDevice;
	result.pDestroyDevice = sirGLDestroyDevice;
	result.pReleaseDriver = sirGLReleaseDriver;
	*pDriver = result;
	return SIR_GRAPHICS_SUCCESS;
}
