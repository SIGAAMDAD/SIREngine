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
#include <Renderer/Drivers/D3D9/SIRD3D9FixedPipelines.h>
#include <Renderer/Drivers/D3D9/SIRD3D9Mapping.h>
#include <Renderer/Drivers/D3D9/SIRD3D9Barriers.h>
#ifdef _WIN32
#include <Renderer/Drivers/D3D9/SIRD3D9Native.h>
#include <Renderer/Drivers/D3D9/SIRD3D9Formats.h>
#include <string.h>

typedef struct sirD3D9ABIRecord_s
{
	uint32_t nKind;
	sirGraphicsHandle_t hPipeline;
	sirGraphicsABIVertexBuffer_t vertex;
	sirGraphicsABIIndexBuffer_t index;
	sirGraphicsABIDraw_t draw;
	sirGraphicsABIDrawIndexed_t indexed;
	uint32_t nClearColor;
	bool bClear;
	sirGraphicsViewport_t viewport;
	sirGraphicsRect_t scissor;
	sirGraphicsBarrier_t barrier;
	bool bUnbindBarrier;
	bool bHasScissor;
} sirD3D9ABIRecord_t;

typedef struct sirD3D9ABIObject_s
{
	uint32_t nGeneration;
	bool bLive;
	sirGraphicsHandle_t hResource;
	sirGraphicsHandle_t hPipeline;
	sirGraphicsHandle_t szViews[8];
	sirGraphicsHandle_t szSamplers[8];
	sirGraphicsSamplerDesc_t sampler;
} sirD3D9ABIObject_t;

typedef struct sirD3D9ABIDeviceState_s
{
	sirGraphicsABIHost_t host;
	sirD3D9Native_t native;
	sirD3D9FixedPipelines_t fixed;
	IDirect3DVertexDeclaration9 *pFixedDeclarations[SIR_D3D9_MAX_FIXED_PIPELINES];
	D3DCAPS9 availableCaps;
	HMODULE hProbeLibrary;
	IDirect3D9 *pProbeRuntime;
	uint32_t aBufferUsage[128];
	sirGraphicsMemoryClass_t aBufferMemory[128];
	uint64_t aBufferSizes[128];
	sirGraphicsHandle_t aMappedHandles[128];
	uint64_t aMappedOffsets[128];
	uint64_t aMappedSizes[128];
	bool aMapped[128];
	sirGraphicsTextureDesc_t aTextureDescs[128];
	sirD3D9ABIObject_t views[16];
	sirD3D9ABIObject_t samplers[16];
	sirD3D9ABIObject_t bindingSets[16];
	uint32_t nAdapter;
	uint32_t nGeneration;
	uint32_t nSurfaceGeneration;
	uint64_t nWindowGeneration;
	uint64_t nAcquisition;
	uint64_t nBudget;
	uint32_t nRecordCount;
	sirD3D9ABIRecord_t records[256];
	bool bSurface;
	bool bRecording;
	bool bClosed;
	bool bSubmitted;
} sirD3D9ABIDeviceState_t;

enum : uint32_t
{
	SIR_D3D9_ABI_BEGIN = 1,
	SIR_D3D9_ABI_END = 2,
	SIR_D3D9_ABI_PIPELINE = 3,
	SIR_D3D9_ABI_VERTEX = 4,
	SIR_D3D9_ABI_INDEX = 5,
	SIR_D3D9_ABI_DRAW = 6,
	SIR_D3D9_ABI_DRAW_INDEXED = 7,
	SIR_D3D9_ABI_BINDING_SET = 8,
	SIR_D3D9_ABI_VIEWPORT = 9,
	SIR_D3D9_ABI_BARRIER = 10
};

static void sirD3D9ABIReleaseFixedDeclarations( sirD3D9ABIDeviceState_t *p ) noexcept
{
	if ( p == nullptr ) {
		return;
	}
	for ( uint32_t i = 0; i < SIR_D3D9_MAX_FIXED_PIPELINES; ++i ) {
		if ( p->pFixedDeclarations[i] != nullptr ) {
			p->pFixedDeclarations[i]->Release();
			p->pFixedDeclarations[i] = nullptr;
		}
	}
}

static sirGraphicsResult_t sirD3D9ABIEnsureFixedDeclaration( sirD3D9ABIDeviceState_t *p, uint32_t nSlot,
                                                             const sirD3D9FixedPipeline_t *pPipeline ) noexcept
{
	if ( p == nullptr || pPipeline == nullptr || nSlot >= SIR_D3D9_MAX_FIXED_PIPELINES || p->native.pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !pPipeline->bNativeDeclaration || p->pFixedDeclarations[nSlot] != nullptr ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	D3DVERTEXELEMENT9 aElements[5] = {};
	for ( uint32_t i = 0; i < pPipeline->nElementCount; ++i ) {
		const sirGraphicsVertexAttribute_t *pAttribute = &pPipeline->elements[i];
		D3DVERTEXELEMENT9 *pElement = &aElements[i];
		pElement->Stream = 0;
		pElement->Offset = ( WORD )pAttribute->nOffsetBytes;
		pElement->Method = D3DDECLMETHOD_DEFAULT;
		pElement->UsageIndex = 0;
		switch ( pAttribute->nLocation ) {
		case 0:
			pElement->Type = D3DDECLTYPE_FLOAT3;
			pElement->Usage = D3DDECLUSAGE_POSITION;
			break;
		case 1:
			pElement->Type = D3DDECLTYPE_D3DCOLOR;
			pElement->Usage = D3DDECLUSAGE_COLOR;
			break;
		case 2:
			pElement->Type = D3DDECLTYPE_FLOAT2;
			pElement->Usage = D3DDECLUSAGE_TEXCOORD;
			break;
		case 3:
			pElement->Type = D3DDECLTYPE_FLOAT3;
			pElement->Usage = D3DDECLUSAGE_NORMAL;
			break;
		default:
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	aElements[pPipeline->nElementCount].Stream = 0xff;
	aElements[pPipeline->nElementCount].Type = D3DDECLTYPE_UNUSED;
	const HRESULT hResult = p->native.pDevice->CreateVertexDeclaration( aElements, &p->pFixedDeclarations[nSlot] );
	return SUCCEEDED( hResult ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

static sirD3D9ABIDeviceState_t *sirD3D9ABIState( void *pState ) noexcept
{
	return ( sirD3D9ABIDeviceState_t * )pState;
}

static uint32_t sirD3D9ABIObjectInsert( sirD3D9ABIObject_t *pObjects, uint32_t nCapacity, uint32_t nDeviceGeneration,
                                        sirGraphicsHandleType_t eType, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pObjects == nullptr || pHandle == nullptr || nDeviceGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nCapacity; ++i ) {
		sirD3D9ABIObject_t *pObject = &pObjects[i];
		if ( !pObject->bLive && pObject->nGeneration != UINT32_MAX ) {
			const uint32_t nGeneration = pObject->nGeneration == 0 ? 1U : pObject->nGeneration;
			*pObject = {};
			pObject->nGeneration = nGeneration;
			pObject->bLive = true;
			*pHandle = { i, pObject->nGeneration, nDeviceGeneration, eType };
			return SIR_GRAPHICS_SUCCESS;
		}
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

static sirD3D9ABIObject_t *sirD3D9ABIObjectLookup( sirD3D9ABIObject_t *pObjects, uint32_t nCapacity, sirGraphicsHandle_t hHandle,
                                                   uint32_t nDeviceGeneration, sirGraphicsHandleType_t eType ) noexcept
{
	if ( pObjects == nullptr || hHandle.nDeviceGeneration != nDeviceGeneration || hHandle.eType != eType || hHandle.nSlot >= nCapacity ) {
		return nullptr;
	}
	sirD3D9ABIObject_t *p = &pObjects[hHandle.nSlot];
	return p->bLive && p->nGeneration == hHandle.nGeneration ? p : nullptr;
}

static uint32_t sirD3D9ABICaps( void *pState, sirGraphicsABICapabilities_t *pOut ) noexcept
{
	const sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pOut == nullptr || pOut->nSize < sizeof( *pOut ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = p->nGeneration;
	caps.nBackend = SIR_GRAPHICS_BACKEND_D3D9;
	caps.nAdapterKind = SIR_GRAPHICS_ABI_ADAPTER_OTHER;
	caps.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION | SIR_GRAPHICS_FEATURE_PRESENT;
	if ( p->availableCaps.NumSimultaneousRTs > 1 ) {
		caps.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_MRT;
	}
	caps.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nMaximumTextureDimension = p->availableCaps.MaxTextureWidth < p->availableCaps.MaxTextureHeight
	                                    ? p->availableCaps.MaxTextureWidth
	                                    : p->availableCaps.MaxTextureHeight;
	caps.nMaximumColorAttachments = p->availableCaps.NumSimultaneousRTs;
	caps.nMaximumBindings = p->availableCaps.MaxSimultaneousTextures;
	caps.nMaximumSamples = 1;
	caps.nMaximumFramesInFlight = 1;
	caps.nShaderProfile = p->availableCaps.VertexShaderVersion < p->availableCaps.PixelShaderVersion ? p->availableCaps.VertexShaderVersion
	                                                                                                 : p->availableCaps.PixelShaderVersion;
	caps.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION - 1U );
	if ( ( p->availableCaps.VertexShaderVersion & 0xffff0000U ) == 0xfffe0000U &&
	     ( ( p->availableCaps.VertexShaderVersion >> 8 ) & 0xffU ) >= 2 && ( ( p->availableCaps.PixelShaderVersion >> 8 ) & 0xffU ) >= 2 ) {
		// Programmable feature reporting is reserved until the public pipeline
		// can carry cooked D3D9 token bytecode (DXBC is a different format).
	}
	*pOut = caps;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABIFormat( void *pState, sirGraphicsABIFormat_t *pFormat ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pFormat == nullptr || pFormat->nSize < sizeof( *pFormat ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	D3DFORMAT eFormat = D3DFMT_UNKNOWN;
	const uint32_t nMap = sirD3D9NativeMapFormat( ( sirGraphicsFormat_t )pFormat->nFormat, &eFormat );
	if ( nMap != SIR_GRAPHICS_SUCCESS ) {
		return nMap;
	}
	if ( p->pProbeRuntime == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	D3DDISPLAYMODE display = {};
	if ( FAILED( p->pProbeRuntime->GetAdapterDisplayMode( p->nAdapter, &display ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	const bool bDepth = eFormat == D3DFMT_D16 || eFormat == D3DFMT_D24S8;
	const bool bCompressed = eFormat == D3DFMT_DXT1 || eFormat == D3DFMT_DXT5;
	uint32_t nFeatures = 0;
	if ( !bDepth &&
	     SUCCEEDED( p->pProbeRuntime->CheckDeviceFormat( p->nAdapter, D3DDEVTYPE_HAL, display.Format, 0, D3DRTYPE_TEXTURE, eFormat ) ) ) {
		nFeatures |= SIR_GRAPHICS_ABI_FORMAT_SAMPLED | SIR_GRAPHICS_ABI_FORMAT_TRANSFER_DST;
		if ( !bCompressed ) {
			nFeatures |= SIR_GRAPHICS_ABI_FORMAT_LINEAR_FILTER;
		}
	}
	const DWORD nUsage = bDepth ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET;
	if ( !bCompressed && SUCCEEDED( p->pProbeRuntime->CheckDeviceFormat( p->nAdapter, D3DDEVTYPE_HAL, display.Format, nUsage,
	                                                                     D3DRTYPE_SURFACE, eFormat ) ) ) {
		nFeatures |= bDepth ? SIR_GRAPHICS_ABI_FORMAT_DEPTH_STENCIL : SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT;
	}
	if ( nFeatures == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	pFormat->nOptimalFeatures = nFeatures;
	pFormat->nLinearFeatures = 0;
	pFormat->nSampleCountMask = 1;
	pFormat->nReserved = 0;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABIModelCaps( void *pState, uint32_t nModel, sirGraphicsABIPipelineModelCaps_t *pOut ) noexcept
{
	const sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pOut == nullptr || pOut->nSize < sizeof( *pOut ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nModel != SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = p->nGeneration;
	caps.nModel = nModel;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	caps.nMaximumTextureUnits = p->availableCaps.MaxSimultaneousTextures < 8 ? p->availableCaps.MaxSimultaneousTextures : 8;
	caps.nMaximumTextureStages = p->availableCaps.MaxTextureBlendStages < caps.nMaximumTextureUnits ? p->availableCaps.MaxTextureBlendStages
	                                                                                                : caps.nMaximumTextureUnits;
	caps.nMaximumFixedLights =
	    p->availableCaps.MaxActiveLights < SIR_D3D9_MAX_FIXED_LIGHTS ? p->availableCaps.MaxActiveLights : SIR_D3D9_MAX_FIXED_LIGHTS;
	caps.nMaximumVertexStreams = 1;
	caps.nMaximumVertexAttributes = 4;
	caps.nMaximumColorAttachments = 1;
	caps.nMaximumSamples = 1;
	caps.nMaximumTextureDimension = p->availableCaps.MaxTextureWidth;
	caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_ALPHA_TEST;
	if ( caps.nMaximumFixedLights != 0 ) {
		caps.nFixedFeatureMask |= SIR_GRAPHICS_FIXED_LIGHTING;
	}
	if ( p->availableCaps.RasterCaps & ( D3DPRASTERCAPS_FOGTABLE | D3DPRASTERCAPS_FOGVERTEX ) ) {
		caps.nFixedFeatureMask |= SIR_GRAPHICS_FIXED_FOG;
	}
	const uint32_t supportedOps[] = { D3DTEXOPCAPS_DISABLE,  D3DTEXOPCAPS_SELECTARG1, D3DTEXOPCAPS_MODULATE,   D3DTEXOPCAPS_ADD,
	                                  D3DTEXOPCAPS_SUBTRACT, D3DTEXOPCAPS_LERP,       D3DTEXOPCAPS_DOTPRODUCT3 };
	for ( uint32_t i = 0; i < 7; ++i ) {
		if ( supportedOps[i] != 0 && ( p->availableCaps.TextureOpCaps & supportedOps[i] ) != 0 ) {
			caps.nFixedTextureOperationMask |= 1U << i;
		}
	}
	caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_NONE;
	if ( caps.nFixedFeatureMask & SIR_GRAPHICS_FIXED_FOG ) {
		caps.nFixedFogModeMask |= 1U << SIR_GRAPHICS_FIXED_FOG_LINEAR;
		caps.nFixedFogModeMask |= 1U << SIR_GRAPHICS_FIXED_FOG_EXP;
		caps.nFixedFogModeMask |= 1U << SIR_GRAPHICS_FIXED_FOG_EXP2;
	}
	*pOut = caps;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABIMemory( void *pState, sirGraphicsMemoryBudget_t *pBudget ) noexcept
{
	const sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pBudget == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBudget = {};
	pBudget->nBudgetBytes = p->nBudget;
	if ( p->bSurface ) {
		pBudget->nCommittedBytes = p->native.resources.nUsedBytes;
		pBudget->nPeakCommittedBytes = p->native.resources.nPeakBytes;
	}
	for ( uint32_t i = 0; i < 128; ++i ) {
		if ( p->aMapped[i] ) {
			pBudget->nUploadMappedBytes += p->aMappedSizes[i];
		}
	}
	// D3D9 has no exact physical residency telemetry.
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABIBuffer( void *pState, const sirGraphicsBufferDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pDesc == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !p->bSurface ) {
		return SIR_GRAPHICS_BUSY;
	}
	const uint32_t nResult = sirD3D9NativeCreateTrackedBufferDesc( &p->native, pDesc, false, pHandle );
	if ( nResult == SIR_GRAPHICS_SUCCESS ) {
		p->aBufferUsage[pHandle->nSlot] = pDesc->nUsage;
		p->aBufferMemory[pHandle->nSlot] = pDesc->eMemory;
		p->aBufferSizes[pHandle->nSlot] = pDesc->nSizeBytes;
		p->aMapped[pHandle->nSlot] = false;
	}
	return nResult;
}

static uint32_t sirD3D9ABITexture( void *pState, const sirGraphicsTextureDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pDesc == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !p->bSurface ) {
		return SIR_GRAPHICS_BUSY;
	}
	const uint32_t nResult = sirD3D9NativeCreateTrackedTextureDesc( &p->native, pDesc, pHandle );
	if ( nResult == SIR_GRAPHICS_SUCCESS ) {
		p->aTextureDescs[pHandle->nSlot] = *pDesc;
	}
	return nResult;
}

static uint32_t sirD3D9ABIPipeline( void *pState, const sirGraphicsABIPipeline_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pDesc == nullptr || pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	const uint32_t nCaps = sirD3D9ABIModelCaps( p, SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION, &caps );
	if ( nCaps != SIR_GRAPHICS_SUCCESS ) {
		return nCaps;
	}
	const uint32_t nValid = sirGraphicsValidateABIPipeline( pDesc, &caps );
	return nValid == SIR_GRAPHICS_SUCCESS ? sirD3D9FixedPipelineCreate( &p->fixed, pDesc, pHandle ) : nValid;
}

static uint32_t sirD3D9ABIView( void *pState, const sirGraphicsViewDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pDesc == nullptr || pHandle == nullptr || !p->bSurface || pDesc->hResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE ||
	     pDesc->nBaseMip != 0 || pDesc->nBaseLayer != 0 || pDesc->nLayers != 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const bool bSurfaceImage = pDesc->hResource.nSlot == UINT32_MAX;
	if ( bSurfaceImage ) {
		if ( pDesc->hResource.nDeviceGeneration != p->nGeneration || pDesc->hResource.nGeneration != p->nSurfaceGeneration ||
		     pDesc->nUsage != SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT || pDesc->nMipLevels != 1 ||
		     pDesc->eFormat != SIR_GRAPHICS_FORMAT_BGRA8_UNORM ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	} else {
		void *pNative = nullptr;
		const uint32_t nLookup = sirD3D9NativeLookupResource( &p->native, pDesc->hResource, &pNative );
		if ( nLookup != SIR_GRAPHICS_SUCCESS ) {
			return nLookup;
		}
		if ( pDesc->nUsage != SIR_GRAPHICS_USAGE_SAMPLED || pDesc->eFormat != p->aTextureDescs[pDesc->hResource.nSlot].eFormat ||
		     pDesc->nMipLevels != p->aTextureDescs[pDesc->hResource.nSlot].nMipLevels ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	const uint32_t nResult = sirD3D9ABIObjectInsert( p->views, 16, p->nGeneration, SIR_GRAPHICS_HANDLE_VIEW, pHandle );
	if ( nResult == SIR_GRAPHICS_SUCCESS ) {
		p->views[pHandle->nSlot].hResource = pDesc->hResource;
	}
	return nResult;
}

static uint32_t sirD3D9ABISampler( void *pState, const sirGraphicsSamplerDesc_t *pDesc, sirGraphicsHandle_t *pHandle ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pDesc == nullptr || pHandle == nullptr || !p->bSurface ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nValid = sirD3D9ValidateSampler( pDesc, p->native.caps.MaxAnisotropy != 0 ? p->native.caps.MaxAnisotropy : 1U );
	if ( nValid != SIR_GRAPHICS_SUCCESS ) {
		return nValid;
	}
	const uint32_t nResult = sirD3D9ABIObjectInsert( p->samplers, 16, p->nGeneration, SIR_GRAPHICS_HANDLE_SAMPLER, pHandle );
	if ( nResult == SIR_GRAPHICS_SUCCESS ) {
		p->samplers[pHandle->nSlot].sampler = *pDesc;
	}
	return nResult;
}

static uint32_t sirD3D9ABIBindingSet( void *pState, sirGraphicsHandle_t hPipeline, uint32_t nSet, sirGraphicsHandle_t *pHandle ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pHandle == nullptr || nSet != 0 || !p->bSurface ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirD3D9FixedPipeline_t *pPipeline = nullptr;
	const uint32_t nValid = sirD3D9FixedPipelineLookup( &p->fixed, hPipeline, &pPipeline );
	if ( nValid != SIR_GRAPHICS_SUCCESS ) {
		return nValid;
	}
	const uint32_t nResult = sirD3D9ABIObjectInsert( p->bindingSets, 16, p->nGeneration, SIR_GRAPHICS_HANDLE_BINDING_SET, pHandle );
	if ( nResult == SIR_GRAPHICS_SUCCESS ) {
		p->bindingSets[pHandle->nSlot].hPipeline = hPipeline;
	}
	return nResult;
}

static uint32_t sirD3D9ABIBindings( void *pState, sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
                                    uint32_t nCount ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || ( nCount != 0 && pWrites == nullptr ) || nCount > 16 || p->bRecording || p->bSubmitted ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D9ABIObject_t *pSet = sirD3D9ABIObjectLookup( p->bindingSets, 16, hSet, p->nGeneration, SIR_GRAPHICS_HANDLE_BINDING_SET );
	if ( pSet == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirD3D9ABIObject_t copy = *pSet;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirGraphicsDescriptorWrite_t *pWrite = &pWrites[i];
		if ( pWrite->nBinding >= 8 || pWrite->nArrayIndex != 0 || pWrite->nOffsetBytes != 0 || pWrite->nRangeBytes != 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pWrite->hResource.eType == SIR_GRAPHICS_HANDLE_VIEW ) {
			sirD3D9ABIObject_t *pView = sirD3D9ABIObjectLookup( p->views, 16, pWrite->hResource, p->nGeneration, SIR_GRAPHICS_HANDLE_VIEW );
			if ( pView == nullptr || pView->hResource.nSlot == UINT32_MAX ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			copy.szViews[pWrite->nBinding] = pWrite->hResource;
		} else if ( pWrite->hResource.eType == SIR_GRAPHICS_HANDLE_SAMPLER ) {
			if ( sirD3D9ABIObjectLookup( p->samplers, 16, pWrite->hResource, p->nGeneration, SIR_GRAPHICS_HANDLE_SAMPLER ) == nullptr ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			copy.szSamplers[pWrite->nBinding] = pWrite->hResource;
		} else {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	*pSet = copy;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t
sirD3D9ABIMapBuffer(void *pState, const sirGraphicsABIBufferRange_t *pRange,
                    sirGraphicsMappedBuffer_t *pMapped) noexcept {
  sirD3D9ABIDeviceState_t *p = sirD3D9ABIState(pState);
  if (p == nullptr || pRange == nullptr || pMapped == nullptr) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  *pMapped = {};
  if (!p->bSurface || p->bRecording || p->bSubmitted ||
      p->native.bCompletionPending) {
    return SIR_GRAPHICS_BUSY;
  }
  if (pRange->hBuffer.nSlot >= 128) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  const uint32_t nSlot = pRange->hBuffer.nSlot;
  const sirGraphicsResult_t eValid =
      sirD3D9ValidateBufferMap(pRange, p->aBufferSizes[nSlot],
                               p->aBufferMemory[nSlot], p->aMapped[nSlot]);
  if (eValid != SIR_GRAPHICS_SUCCESS) {
    return eValid;
  }
  void *pResource = nullptr;
  const uint32_t nLookup =
      sirD3D9NativeLookupResource(&p->native, pRange->hBuffer, &pResource);
  if (nLookup != SIR_GRAPHICS_SUCCESS) {
    return nLookup;
  }
  if (pResource == nullptr) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  IDirect3DResource9 *pBuffer = (IDirect3DResource9 *)pResource;
  void *pBytes = nullptr;
  HRESULT hResult = D3DERR_INVALIDCALL;
  if (pBuffer->GetType() == D3DRTYPE_VERTEXBUFFER) {
    hResult = ((IDirect3DVertexBuffer9 *)pBuffer)
                  ->Lock((UINT)pRange->nOffsetBytes, (UINT)pRange->nBytes,
                         &pBytes, D3DLOCK_NOSYSLOCK);
  } else if (pBuffer->GetType() == D3DRTYPE_INDEXBUFFER) {
    hResult = ((IDirect3DIndexBuffer9 *)pBuffer)
                  ->Lock((UINT)pRange->nOffsetBytes, (UINT)pRange->nBytes,
                         &pBytes, D3DLOCK_NOSYSLOCK);
  } else {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  if (FAILED(hResult)) {
    return hResult == D3DERR_DEVICELOST ? SIR_GRAPHICS_DEVICE_LOST
                                        : SIR_GRAPHICS_DRIVER_ERROR;
  }
  p->aMapped[nSlot] = true;
  p->aMappedHandles[nSlot] = pRange->hBuffer;
  p->aMappedOffsets[nSlot] = pRange->nOffsetBytes;
  p->aMappedSizes[nSlot] = pRange->nBytes;
  pMapped->pData = pBytes;
  pMapped->nOffset = pRange->nOffsetBytes;
  pMapped->nBytes = pRange->nBytes;
  return SIR_GRAPHICS_SUCCESS;
}

static uint32_t
sirD3D9ABIUnmapBuffer(void *pState,
                      const sirGraphicsABIBufferRange_t *pRange) noexcept {
  sirD3D9ABIDeviceState_t *p = sirD3D9ABIState(pState);
  if (p == nullptr || pRange == nullptr || pRange->nSize < sizeof(*pRange) ||
      pRange->nReserved != 0) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  if (pRange->hBuffer.eType != SIR_GRAPHICS_HANDLE_BUFFER ||
      pRange->hBuffer.nSlot >= 128) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  const uint32_t nSlot = pRange->hBuffer.nSlot;
  const sirGraphicsHandle_t hMapped = p->aMappedHandles[nSlot];
  if (!p->aMapped[nSlot] ||
      hMapped.nGeneration != pRange->hBuffer.nGeneration ||
      hMapped.nDeviceGeneration != pRange->hBuffer.nDeviceGeneration ||
      p->aMappedOffsets[nSlot] != pRange->nOffsetBytes ||
      p->aMappedSizes[nSlot] != pRange->nBytes) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  void *pResource = nullptr;
  const uint32_t nLookup =
      sirD3D9NativeLookupResource(&p->native, pRange->hBuffer, &pResource);
  if (nLookup != SIR_GRAPHICS_SUCCESS) {
    return nLookup;
  }
  IDirect3DResource9 *pBuffer = (IDirect3DResource9 *)pResource;
  HRESULT hResult = D3DERR_INVALIDCALL;
  if (pBuffer->GetType() == D3DRTYPE_VERTEXBUFFER) {
    hResult = ((IDirect3DVertexBuffer9 *)pBuffer)->Unlock();
  } else if (pBuffer->GetType() == D3DRTYPE_INDEXBUFFER) {
    hResult = ((IDirect3DIndexBuffer9 *)pBuffer)->Unlock();
  }
  p->aMapped[nSlot] = false;
  p->aMappedOffsets[nSlot] = 0;
  p->aMappedSizes[nSlot] = 0;
  return SUCCEEDED(hResult) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

static uint32_t
sirD3D9ABIFlushBuffer(void *pState,
                      const sirGraphicsABIBufferRange_t *pRange) noexcept {
  sirD3D9ABIDeviceState_t *p = sirD3D9ABIState(pState);
  if (p == nullptr || pRange == nullptr || pRange->nSize < sizeof(*pRange) ||
      pRange->nReserved != 0 || pRange->hBuffer.nSlot >= 128) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  const uint32_t nSlot = pRange->hBuffer.nSlot;
  const uint32_t nValid =
      sirD3D9ValidateBufferMap(pRange, p->aBufferSizes[nSlot],
                               p->aBufferMemory[nSlot], p->aMapped[nSlot]);
  if (nValid != SIR_GRAPHICS_SUCCESS) {
    return nValid;
  }
  void *pNative = nullptr;
  // D3D9 makes CPU writes visible at Unlock; no separate flush exists.
  return sirD3D9NativeLookupResource(&p->native, pRange->hBuffer, &pNative);
}

static uint32_t
sirD3D9ABIUnsupportedRange(void *pState,
                           const sirGraphicsABIBufferRange_t *pRange) noexcept {
  return pState == nullptr || pRange == nullptr ? SIR_GRAPHICS_INVALID_ARGUMENT
                                                : SIR_GRAPHICS_UNSUPPORTED;
}

static void
sirD3D9ABIReleaseMappedBuffers(sirD3D9ABIDeviceState_t *p) noexcept {
  if (p == nullptr || !p->bSurface) {
    return;
  }
  for (uint32_t i = 0; i < 128; ++i) {
    if (!p->aMapped[i]) {
      continue;
    }
    void *pResource = nullptr;
    if (sirD3D9NativeLookupResource(&p->native, p->aMappedHandles[i],
                                    &pResource) == SIR_GRAPHICS_SUCCESS) {
      IDirect3DResource9 *pBuffer = (IDirect3DResource9 *)pResource;
      if (pBuffer->GetType() == D3DRTYPE_VERTEXBUFFER) {
        (void)((IDirect3DVertexBuffer9 *)pBuffer)->Unlock();
      } else if (pBuffer->GetType() == D3DRTYPE_INDEXBUFFER) {
        (void)((IDirect3DIndexBuffer9 *)pBuffer)->Unlock();
      }
    }
    p->aMapped[i] = false;
    p->aMappedOffsets[i] = 0;
    p->aMappedSizes[i] = 0;
  }
}

static uint32_t sirD3D9ABISurface( void *pState, const sirGraphicsABINativeSurface_t *pSurface, const sirGraphicsABIPresentDesc_t *pPresent,
                                   sirGraphicsHandle_t *pHandle ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pSurface == nullptr || pPresent == nullptr || pHandle == nullptr || pSurface->nSize < sizeof( *pSurface ) ||
	     pPresent->nSize < sizeof( *pPresent ) || pSurface->nPlatform != SIR_GRAPHICS_SURFACE_WIN32 || pSurface->pWindow == nullptr ||
	     pSurface->nWindowGeneration == 0 || pPresent->nWidth == 0 || pPresent->nHeight == 0 || pPresent->nBackbufferCount != 1 ||
	     pPresent->nFormat != SIR_GRAPHICS_FORMAT_BGRA8_UNORM || pPresent->nHdr != 0 || pPresent->nAllowTearing != 0 ||
	     pPresent->nMode != SIR_GRAPHICS_PRESENT_FIFO || p->bSurface ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !IsWindow( ( HWND )pSurface->pWindow ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eCreate = sirD3D9NativeInitialize( &p->native, ( HWND )pSurface->pWindow, p->nAdapter, true );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	if ( p->nBudget != 0 ) {
		const sirGraphicsResult_t eBudget = sirD3D9NativeConfigureResidentBudget( &p->native, p->nBudget );
		if ( eBudget != SIR_GRAPHICS_SUCCESS ) {
			sirD3D9NativeShutdown( &p->native );
			return eBudget;
		}
	}
	RECT bounds = {};
	if ( !GetClientRect( ( HWND )pSurface->pWindow, &bounds ) || ( uint32_t )( bounds.right - bounds.left ) != pPresent->nWidth ||
	     ( uint32_t )( bounds.bottom - bounds.top ) != pPresent->nHeight ) {
		sirD3D9NativeShutdown( &p->native );
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	// Initial device creation uses the window's actual client-area extent.
	// Avoid an unnecessary Reset here, which would invalidate the initial
	// shared device generation before the host receives its surface handle.
	p->nSurfaceGeneration = 1;
	p->nWindowGeneration = pSurface->nWindowGeneration;
	p->bSurface = true;
	*pHandle = { 0, p->nSurfaceGeneration, p->nGeneration, SIR_GRAPHICS_HANDLE_SURFACE };
	return SIR_GRAPHICS_SUCCESS;
}

static bool sirD3D9ABISurfaceHandle( const sirD3D9ABIDeviceState_t *p, sirGraphicsHandle_t h ) noexcept
{
	return p != nullptr && p->bSurface && h.eType == SIR_GRAPHICS_HANDLE_SURFACE && h.nSlot == 0 &&
	       h.nGeneration == p->nSurfaceGeneration && h.nDeviceGeneration == p->nGeneration;
}

static uint32_t sirD3D9ABIResize( void *pState, sirGraphicsHandle_t hSurface, const sirGraphicsABINativeSurface_t *pSurface,
                                  const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( !sirD3D9ABISurfaceHandle( p, hSurface ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSurface == nullptr || pPresent == nullptr || pSurface->nSize < sizeof( *pSurface ) || pPresent->nSize < sizeof( *pPresent ) ||
	     pSurface->pWindow != p->native.hFocusWindow || pSurface->nPlatform != SIR_GRAPHICS_SURFACE_WIN32 || pPresent->nWidth == 0 ||
	     pPresent->nHeight == 0 || pPresent->nBackbufferCount != 1 || pPresent->nFormat != SIR_GRAPHICS_FORMAT_BGRA8_UNORM ||
	     pPresent->nHdr != 0 || pPresent->nAllowTearing != 0 || pPresent->nMode == SIR_GRAPHICS_PRESENT_MAILBOX || p->bRecording ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// A Reset destroys DEFAULT-pool resources and advances the native generation.
	// The v1 resize callback cannot replace the caller's surface handle; require
	// an explicit recreation, preserving stale-handle rejection.
	sirD3D9ABIReleaseMappedBuffers( p );
	sirD3D9ABIReleaseFixedDeclarations( p );
	sirD3D9NativeShutdown( &p->native );
	p->bSurface = false;
	p->bRecording = false;
	p->bSubmitted = false;
	p->nAcquisition = 0;
	if ( p->nGeneration == UINT32_MAX ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	++p->nGeneration;
	sirD3D9FixedPipelinesInvalidate( &p->fixed, p->nGeneration );
	return SIR_GRAPHICS_SURFACE_CHANGED;
}

static uint32_t sirD3D9ABIAcquire( void *pState, sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( !sirD3D9ABISurfaceHandle( p, hSurface ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pImage == nullptr || p->nAcquisition == UINT64_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( p->bRecording || p->bSubmitted ) {
		return SIR_GRAPHICS_BUSY;
	}
	*pImage = {};
	pImage->hSurface = hSurface;
	pImage->hImage = { UINT32_MAX, p->nSurfaceGeneration, p->nGeneration, SIR_GRAPHICS_HANDLE_TEXTURE };
	pImage->nAcquisitionId = ++p->nAcquisition;
	pImage->nWindowGeneration = p->nWindowGeneration;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABIPresent( void *pState, const sirGraphicsSurfaceImage_t *pImage,
                                   const sirGraphicsCompletion_t *pCompletion ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pImage == nullptr || !sirD3D9ABISurfaceHandle( p, pImage->hSurface ) ||
	     pImage->nWindowGeneration != p->nWindowGeneration || pImage->nAcquisitionId == 0 || pImage->nAcquisitionId != p->nAcquisition ||
	     !p->bSubmitted || p->bRecording ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pCompletion != nullptr && ( pCompletion->nDeviceGeneration != p->nGeneration || pCompletion->eQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
	                                 pCompletion->nValue > p->native.nLastSubmittedSerial ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t ePresent = sirD3D9NativePresent( &p->native );
	if ( ePresent == SIR_GRAPHICS_SUCCESS ) {
		p->bSubmitted = false;
	}
	return ePresent;
}

static uint32_t sirD3D9ABIPoll( void *pState, const sirGraphicsCompletion_t *pCompletion, uint32_t *pComplete ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pCompletion == nullptr || pComplete == nullptr || pCompletion->nDeviceGeneration != p->nGeneration ||
	     pCompletion->eQueue != SIR_GRAPHICS_QUEUE_DIRECT || pCompletion->nValue > p->native.nLastSubmittedSerial ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	bool bComplete = false;
	const sirGraphicsResult_t ePoll = sirD3D9NativePoll( &p->native, &bComplete );
	if ( ePoll == SIR_GRAPHICS_SUCCESS ) {
		*pComplete = pCompletion->nValue <= p->native.nLastCompletedSerial ? 1U : 0U;
	}
	return ePoll;
}

static uint32_t sirD3D9ABIWait( void *pState, const sirGraphicsCompletion_t *pCompletion, uint64_t nTimeoutNs ) noexcept
{
	uint32_t nComplete = 0;
	const uint32_t nPoll = sirD3D9ABIPoll( pState, pCompletion, &nComplete );
	if ( nPoll != SIR_GRAPHICS_SUCCESS || nComplete != 0 ) {
		return nPoll;
	}
	return nTimeoutNs == 0 ? SIR_GRAPHICS_TIMEOUT : SIR_GRAPHICS_BUSY;
}

static uint32_t sirD3D9ABIRetire( void *pState, sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t *pCompletion ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pCompletion == nullptr || pCompletion->nDeviceGeneration != p->nGeneration ||
	     ( pCompletion->nQueueMask & ~( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_PIPELINE ) {
		if ( p->bRecording || p->bSubmitted || p->native.nLastCompletedSerial < p->native.nLastSubmittedSerial ) {
			return SIR_GRAPHICS_BUSY;
		}
		const sirGraphicsResult_t eRetire = sirD3D9FixedPipelineRetire( &p->fixed, hResource );
		if ( eRetire == SIR_GRAPHICS_SUCCESS && hResource.nSlot < SIR_D3D9_MAX_FIXED_PIPELINES &&
		     p->pFixedDeclarations[hResource.nSlot] != nullptr ) {
			p->pFixedDeclarations[hResource.nSlot]->Release();
			p->pFixedDeclarations[hResource.nSlot] = nullptr;
		}
		return eRetire;
	}
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_VIEW || hResource.eType == SIR_GRAPHICS_HANDLE_SAMPLER ||
	     hResource.eType == SIR_GRAPHICS_HANDLE_BINDING_SET ) {
		if ( p->bRecording || p->bSubmitted || pCompletion->szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] > p->native.nLastCompletedSerial ) {
			return SIR_GRAPHICS_BUSY;
		}
		sirD3D9ABIObject_t *pObjects = hResource.eType == SIR_GRAPHICS_HANDLE_VIEW      ? p->views
		                               : hResource.eType == SIR_GRAPHICS_HANDLE_SAMPLER ? p->samplers
		                                                                                : p->bindingSets;
		sirD3D9ABIObject_t *pObject = sirD3D9ABIObjectLookup( pObjects, 16, hResource, p->nGeneration, hResource.eType );
		if ( pObject == nullptr ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		pObject->bLive = false;
		++pObject->nGeneration;
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( !p->bSurface ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER && hResource.nSlot < 128 && p->aMapped[hResource.nSlot] ) {
		return SIR_GRAPHICS_BUSY;
	}
	return sirD3D9NativeRetireResource( &p->native, hResource, pCompletion->szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] );
}

static uint32_t sirD3D9ABIPush( sirD3D9ABIDeviceState_t *p, uint32_t nKind, sirD3D9ABIRecord_t **ppRecord ) noexcept
{
	if ( p == nullptr || ppRecord == nullptr || !p->bRecording || p->bClosed ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( p->nRecordCount >= 256 ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirD3D9ABIRecord_t *pRecord = &p->records[p->nRecordCount++];
	*pRecord = {};
	pRecord->nKind = nKind;
	*ppRecord = pRecord;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABICommandReset( void *pCommands ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || !p->bSurface || p->bSubmitted ) {
		return SIR_GRAPHICS_BUSY;
	}
	for ( uint32_t i = 0; i < 128; ++i ) {
		if ( p->aMapped[i] ) {
			return SIR_GRAPHICS_BUSY;
		}
	}
	p->nRecordCount = 0;
	p->bRecording = true;
	p->bClosed = false;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABICommandBarrier(void *pCommands,
                                         const sirGraphicsBarrier_t *pBarriers,
                                         uint32_t nCount) noexcept {
  sirD3D9ABIDeviceState_t *p = sirD3D9ABIState(pCommands);
  if (p == nullptr || (nCount != 0 && pBarriers == nullptr) || !p->bRecording ||
      p->bClosed) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  if (nCount > 256 - p->nRecordCount) {
    return SIR_GRAPHICS_CAPACITY_EXCEEDED;
  }
  // Validate the entire batch before appending a single command. A failed
  // transition must never leave a partially recorded FrameGraph barrier.
  for (uint32_t i = 0; i < nCount; ++i) {
    const sirGraphicsBarrier_t *pBarrier = &pBarriers[i];
    uint32_t nMips = 0;
    uint32_t nLayers = 0;
    const sirGraphicsHandle_t h = pBarrier->hResource;
    if (h.eType == SIR_GRAPHICS_HANDLE_TEXTURE && h.nSlot != UINT32_MAX &&
        (pBarrier->eAfter == SIR_GRAPHICS_STATE_PRESENT ||
         pBarrier->eBefore == SIR_GRAPHICS_STATE_PRESENT)) {
      return SIR_GRAPHICS_UNSUPPORTED;
    }
    if (h.eType == SIR_GRAPHICS_HANDLE_TEXTURE && h.nSlot == UINT32_MAX &&
        pBarrier->eBefore != SIR_GRAPHICS_STATE_PRESENT &&
        pBarrier->eBefore != SIR_GRAPHICS_STATE_COLOR_ATTACHMENT &&
        pBarrier->eBefore != SIR_GRAPHICS_STATE_UNDEFINED &&
        pBarrier->eBefore != SIR_GRAPHICS_STATE_COMMON) {
      return SIR_GRAPHICS_UNSUPPORTED;
    }
    if (h.eType == SIR_GRAPHICS_HANDLE_TEXTURE && h.nSlot == UINT32_MAX) {
      if (!p->bSurface || h.nDeviceGeneration != p->nGeneration ||
          h.nGeneration != p->nSurfaceGeneration) {
        return SIR_GRAPHICS_INVALID_HANDLE;
      }
      nMips = 1;
      nLayers = 1;
    } else {
      void *pResource = nullptr;
      const uint32_t nLookup =
          sirD3D9NativeLookupResource(&p->native, h, &pResource);
      if (nLookup != SIR_GRAPHICS_SUCCESS) {
        return nLookup;
      }
      if (h.eType == SIR_GRAPHICS_HANDLE_TEXTURE && h.nSlot < 128) {
        nMips = p->aTextureDescs[h.nSlot].nMipLevels;
        nLayers = p->aTextureDescs[h.nSlot].nArrayLayers;
      }
    }
    bool bUnbind = false;
    const uint32_t nValid =
        sirD3D9ValidateSerialBarrier(pBarrier, nMips, nLayers, &bUnbind);
    if (nValid != SIR_GRAPHICS_SUCCESS) {
      return nValid;
    }
  }
  for (uint32_t i = 0; i < nCount; ++i) {
    sirD3D9ABIRecord_t *pRecord = nullptr;
    const uint32_t nPush = sirD3D9ABIPush(p, SIR_D3D9_ABI_BARRIER, &pRecord);
    if (nPush != SIR_GRAPHICS_SUCCESS) {
      return nPush;
    }
    pRecord->barrier = pBarriers[i];
    bool bUnbind = false;
    const sirGraphicsHandle_t h = pBarriers[i].hResource;
    const uint32_t nMips =
        h.eType == SIR_GRAPHICS_HANDLE_TEXTURE && h.nSlot != UINT32_MAX
            ? p->aTextureDescs[h.nSlot].nMipLevels
            : 1;
    const uint32_t nLayers =
        h.eType == SIR_GRAPHICS_HANDLE_TEXTURE && h.nSlot != UINT32_MAX
            ? p->aTextureDescs[h.nSlot].nArrayLayers
            : 1;
    (void)sirD3D9ValidateSerialBarrier(&pRecord->barrier, nMips, nLayers,
                                       &bUnbind);
    pRecord->bUnbindBarrier = bUnbind;
  }
  return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABICommandCopyBuffer( void *pCommands, const sirGraphicsABICopyBuffer_t *pCopy ) noexcept
{
	return pCommands == nullptr || pCopy == nullptr ? SIR_GRAPHICS_INVALID_ARGUMENT : SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirD3D9ABICommandCopyTexture( void *pCommands, const sirGraphicsABIBufferTextureCopy_t *pCopy ) noexcept
{
	return pCommands == nullptr || pCopy == nullptr ? SIR_GRAPHICS_INVALID_ARGUMENT : SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirD3D9ABICommandBeginPass( void *pCommands, const sirGraphicsABIRenderPass_t *pPass ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || pPass == nullptr || pPass->nSize < sizeof( *pPass ) || pPass->nColorCount > 1 ||
	     pPass->hDepthView.eType != SIR_GRAPHICS_HANDLE_NONE || ( pPass->nColorCount != 0 && pPass->pColors == nullptr ) ||
	     pPass->nClearDepth != 0 || pPass->nClearStencil != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pPass->nColorCount == 1 ) {
		sirD3D9ABIObject_t *pView =
		    sirD3D9ABIObjectLookup( p->views, 16, pPass->pColors[0].hView, p->nGeneration, SIR_GRAPHICS_HANDLE_VIEW );
		if ( pView == nullptr || pView->hResource.nSlot != UINT32_MAX ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nResult = sirD3D9ABIPush( p, SIR_D3D9_ABI_BEGIN, &pRecord );
	if ( nResult == SIR_GRAPHICS_SUCCESS && pPass->nColorCount == 1 && pPass->pColors[0].nClear != 0 ) {
		const float *pColor = pPass->pColors[0].szClearColor;
		DWORD rgba[4] = {};
		for ( uint32_t i = 0; i < 4; ++i ) {
			if ( !( pColor[i] >= 0.0f && pColor[i] <= 1.0f ) ) {
				--p->nRecordCount;
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			rgba[i] = ( DWORD )( pColor[i] * 255.0f );
		}
		pRecord->bClear = true;
		pRecord->nClearColor = D3DCOLOR_ARGB( rgba[3], rgba[0], rgba[1], rgba[2] );
	}
	return nResult;
}

static uint32_t sirD3D9ABICommandEndPass( void *pCommands ) noexcept
{
	sirD3D9ABIRecord_t *pRecord = nullptr;
	return sirD3D9ABIPush( sirD3D9ABIState( pCommands ), SIR_D3D9_ABI_END, &pRecord );
}

static uint32_t sirD3D9ABICommandPipeline( void *pCommands, sirGraphicsHandle_t hPipeline ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	const sirD3D9FixedPipeline_t *pPipeline = nullptr;
	if ( p == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eLookup = sirD3D9FixedPipelineLookup( &p->fixed, hPipeline, &pPipeline );
	if ( eLookup != SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nStatus = sirD3D9ABIPush( p, SIR_D3D9_ABI_PIPELINE, &pRecord );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		pRecord->hPipeline = hPipeline;
	}
	return nStatus;
}

static uint32_t sirD3D9ABICommandBindingSet( void *pCommands, uint32_t nSetIndex, sirGraphicsHandle_t hSet ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || nSetIndex != 0 ||
	     sirD3D9ABIObjectLookup( p->bindingSets, 16, hSet, p->nGeneration, SIR_GRAPHICS_HANDLE_BINDING_SET ) == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nResult = sirD3D9ABIPush( p, SIR_D3D9_ABI_BINDING_SET, &pRecord );
	if ( nResult == SIR_GRAPHICS_SUCCESS ) {
		pRecord->hPipeline = hSet;
	}
	return nResult;
}

static uint32_t sirD3D9ABICommandVertex( void *pCommands, const sirGraphicsABIVertexBuffer_t *pBuffer ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || pBuffer == nullptr || pBuffer->nSize < sizeof( *pBuffer ) || pBuffer->nSlot != 0 || pBuffer->nReserved != 0 ||
	     pBuffer->nStrideBytes == 0 || pBuffer->nOffsetBytes > UINT32_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pResource = nullptr;
	const uint32_t nLookup = sirD3D9NativeLookupResource( &p->native, pBuffer->hBuffer, &pResource );
	if ( nLookup != SIR_GRAPHICS_SUCCESS ) {
		return nLookup;
	}
	if ( p->aBufferUsage[pBuffer->hBuffer.nSlot] != SIR_GRAPHICS_USAGE_VERTEX ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nStatus = sirD3D9ABIPush( p, SIR_D3D9_ABI_VERTEX, &pRecord );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		pRecord->vertex = *pBuffer;
	}
	return nStatus;
}

static uint32_t sirD3D9ABICommandIndex( void *pCommands, const sirGraphicsABIIndexBuffer_t *pBuffer ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || pBuffer == nullptr || pBuffer->nSize < sizeof( *pBuffer ) || pBuffer->nIndex32 > 1 ||
	     pBuffer->nOffsetBytes > UINT32_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pResource = nullptr;
	const uint32_t nLookup = sirD3D9NativeLookupResource( &p->native, pBuffer->hBuffer, &pResource );
	if ( nLookup != SIR_GRAPHICS_SUCCESS ) {
		return nLookup;
	}
	if ( p->aBufferUsage[pBuffer->hBuffer.nSlot] != SIR_GRAPHICS_USAGE_INDEX || pBuffer->nIndex32 != 0 ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nStatus = sirD3D9ABIPush( p, SIR_D3D9_ABI_INDEX, &pRecord );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		pRecord->index = *pBuffer;
	}
	return nStatus;
}

static uint32_t sirD3D9ABICommandViewport( void *pCommands, const sirGraphicsViewport_t *pViewport,
                                           const sirGraphicsRect_t *pScissor ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || pViewport == nullptr || !( pViewport->fWidth >= 1.0f && pViewport->fWidth < 65536.0f ) ||
	     !( pViewport->fHeight >= 1.0f && pViewport->fHeight < 65536.0f ) || !( pViewport->fX >= 0.0f && pViewport->fX < 65536.0f ) ||
	     !( pViewport->fY >= 0.0f && pViewport->fY < 65536.0f ) ||
	     !( pViewport->fMinimumDepth >= 0.0f && pViewport->fMaximumDepth <= 1.0f &&
	        pViewport->fMinimumDepth <= pViewport->fMaximumDepth ) ||
	     ( pScissor != nullptr &&
	       ( pScissor->nLeft < 0 || pScissor->nTop < 0 || pScissor->nRight <= pScissor->nLeft || pScissor->nBottom <= pScissor->nTop ) ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nResult = sirD3D9ABIPush( p, SIR_D3D9_ABI_VIEWPORT, &pRecord );
	if ( nResult == SIR_GRAPHICS_SUCCESS ) {
		pRecord->viewport = *pViewport;
		if ( pScissor != nullptr ) {
			pRecord->scissor = *pScissor;
			pRecord->bHasScissor = true;
		}
	}
	return nResult;
}

static uint32_t sirD3D9ABICommandDraw( void *pCommands, const sirGraphicsABIDraw_t *pDraw ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || pDraw == nullptr || pDraw->nInstances != 1 || pDraw->nFirstInstance != 0 || pDraw->nVertices == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nStatus = sirD3D9ABIPush( p, SIR_D3D9_ABI_DRAW, &pRecord );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		pRecord->draw = *pDraw;
	}
	return nStatus;
}

static uint32_t sirD3D9ABICommandDrawIndexed( void *pCommands, const sirGraphicsABIDrawIndexed_t *pDraw ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || pDraw == nullptr || pDraw->nInstances != 1 || pDraw->nFirstInstance != 0 || pDraw->nIndices == 0 ||
	     pDraw->nReserved != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirD3D9ABIRecord_t *pRecord = nullptr;
	const uint32_t nStatus = sirD3D9ABIPush( p, SIR_D3D9_ABI_DRAW_INDEXED, &pRecord );
	if ( nStatus == SIR_GRAPHICS_SUCCESS ) {
		pRecord->indexed = *pDraw;
	}
	return nStatus;
}

static uint32_t sirD3D9ABICommandDispatch( void *pCommands, uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept
{
	( void )nX;
	( void )nY;
	( void )nZ;
	return pCommands == nullptr ? SIR_GRAPHICS_INVALID_ARGUMENT : SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirD3D9ABICommandClose( void *pCommands ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pCommands );
	if ( p == nullptr || !p->bRecording || p->bClosed ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	p->bClosed = true;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABIAcquireCommands( void *pState, uint32_t nQueue, sirGraphicsABICommandList_t *pCommands ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pCommands == nullptr || pCommands->nSize < sizeof( *pCommands ) || nQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
	     !p->bSurface || p->bSubmitted ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABICommandList_t result = {};
	result.nSize = sizeof( result );
	result.nDeviceGeneration = p->nGeneration;
	result.nQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	result.pCommands = p;
	result.pReset = sirD3D9ABICommandReset;
	result.pBarrier = sirD3D9ABICommandBarrier;
	result.pCopyBuffer = sirD3D9ABICommandCopyBuffer;
	result.pCopyBufferTexture = sirD3D9ABICommandCopyTexture;
	result.pBeginRenderPass = sirD3D9ABICommandBeginPass;
	result.pEndRenderPass = sirD3D9ABICommandEndPass;
	result.pBindPipeline = sirD3D9ABICommandPipeline;
	result.pBindBindingSet = sirD3D9ABICommandBindingSet;
	result.pBindVertexBuffer = sirD3D9ABICommandVertex;
	result.pBindIndexBuffer = sirD3D9ABICommandIndex;
	result.pSetViewport = sirD3D9ABICommandViewport;
	result.pDraw = sirD3D9ABICommandDraw;
	result.pDrawIndexed = sirD3D9ABICommandDrawIndexed;
	result.pDispatch = sirD3D9ABICommandDispatch;
	result.pClose = sirD3D9ABICommandClose;
	*pCommands = result;
	return sirD3D9ABICommandReset( p );
}

static DWORD sirD3D9ABIBlendFactor( uint32_t nFactor ) noexcept
{
	const DWORD mapping[] = { D3DBLEND_ZERO,         D3DBLEND_ONE,      D3DBLEND_SRCCOLOR,    D3DBLEND_INVSRCCOLOR, D3DBLEND_DESTCOLOR,
	                          D3DBLEND_INVDESTCOLOR, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA, D3DBLEND_DESTALPHA,   D3DBLEND_INVDESTALPHA };
	return nFactor < sizeof( mapping ) / sizeof( mapping[0] ) ? mapping[nFactor] : 0;
}

static DWORD sirD3D9ABIBlendOperation( uint32_t nOperation ) noexcept
{
	const DWORD mapping[] = { D3DBLENDOP_ADD, D3DBLENDOP_SUBTRACT, D3DBLENDOP_REVSUBTRACT, D3DBLENDOP_MIN, D3DBLENDOP_MAX };
	return nOperation < sizeof( mapping ) / sizeof( mapping[0] ) ? mapping[nOperation] : 0;
}

static uint32_t sirD3D9ABIApplyPipelineState( sirD3D9ABIDeviceState_t *p, const sirD3D9FixedPipeline_t *pPipeline ) noexcept
{
	// The current public surface has no depth attachment. Reject depth requests
	// until a depth/stencil view is actually bound by this backend.
	if ( pPipeline->nDepthTest || pPipeline->nDepthWrite ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsABIBlendState_t *pBlend = &pPipeline->blend;
	const bool bSeparateAlpha = pBlend->nSourceColor != pBlend->nSourceAlpha || pBlend->nDestinationColor != pBlend->nDestinationAlpha ||
	                            pBlend->nColorOperation != pBlend->nAlphaOperation;
	if ( pBlend->nEnabled &&
	     ( ( bSeparateAlpha && ( p->availableCaps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND ) == 0 ) ||
	       sirD3D9ABIBlendFactor( pBlend->nSourceColor ) == 0 || sirD3D9ABIBlendFactor( pBlend->nDestinationColor ) == 0 ||
	       sirD3D9ABIBlendFactor( pBlend->nSourceAlpha ) == 0 || sirD3D9ABIBlendFactor( pBlend->nDestinationAlpha ) == 0 ||
	       sirD3D9ABIBlendOperation( pBlend->nColorOperation ) == 0 || sirD3D9ABIBlendOperation( pBlend->nAlphaOperation ) == 0 ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	IDirect3DDevice9 *pDevice = p->native.pDevice;
	if ( pDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const DWORD nCull = pPipeline->nCullMode == SIR_GRAPHICS_CULL_BACK    ? D3DCULL_CCW
	                    : pPipeline->nCullMode == SIR_GRAPHICS_CULL_FRONT ? D3DCULL_CW
	                                                                      : D3DCULL_NONE;
	HRESULT hResult = pDevice->SetRenderState( D3DRS_CULLMODE, nCull );
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_COLORVERTEX, ( pPipeline->nFVF & 0x040U ) != 0 ? TRUE : FALSE );
	if ( SUCCEEDED( hResult ) )
		hResult =
		    pDevice->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, ( pPipeline->nFVF & 0x040U ) != 0 ? D3DMCS_COLOR1 : D3DMCS_MATERIAL );
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL );
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL );
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL );
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, pBlend->nEnabled ? TRUE : FALSE );
	if ( SUCCEEDED( hResult ) && pBlend->nEnabled ) {
		hResult = pDevice->SetRenderState( D3DRS_SRCBLEND, sirD3D9ABIBlendFactor( pBlend->nSourceColor ) );
		if ( SUCCEEDED( hResult ) )
			hResult = pDevice->SetRenderState( D3DRS_DESTBLEND, sirD3D9ABIBlendFactor( pBlend->nDestinationColor ) );
		if ( SUCCEEDED( hResult ) )
			hResult = pDevice->SetRenderState( D3DRS_BLENDOP, sirD3D9ABIBlendOperation( pBlend->nColorOperation ) );
		if ( SUCCEEDED( hResult ) )
			hResult = pDevice->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, bSeparateAlpha ? TRUE : FALSE );
		if ( SUCCEEDED( hResult ) && bSeparateAlpha )
			hResult = pDevice->SetRenderState( D3DRS_SRCBLENDALPHA, sirD3D9ABIBlendFactor( pBlend->nSourceAlpha ) );
		if ( SUCCEEDED( hResult ) && bSeparateAlpha )
			hResult = pDevice->SetRenderState( D3DRS_DESTBLENDALPHA, sirD3D9ABIBlendFactor( pBlend->nDestinationAlpha ) );
		if ( SUCCEEDED( hResult ) && bSeparateAlpha )
			hResult = pDevice->SetRenderState( D3DRS_BLENDOPALPHA, sirD3D9ABIBlendOperation( pBlend->nAlphaOperation ) );
	}
	if ( SUCCEEDED( hResult ) && !pBlend->nEnabled ) {
		hResult = pDevice->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, FALSE );
	}
	if ( SUCCEEDED( hResult ) )
		hResult = pDevice->SetRenderState( D3DRS_COLORWRITEENABLE, pBlend->nColorWriteMask );
	return SUCCEEDED( hResult ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

static uint32_t sirD3D9ABIReplay( sirD3D9ABIDeviceState_t *p ) noexcept
{
	sirGraphicsHandle_t hPipeline = {};
	sirGraphicsHandle_t hSet = {};
	sirGraphicsABIVertexBuffer_t vertex = {};
	sirGraphicsABIIndexBuffer_t index = {};
	bool bInPass = false;
	for ( uint32_t i = 0; i < p->nRecordCount; ++i ) {
		const sirD3D9ABIRecord_t *pRecord = &p->records[i];
		if ( pRecord->nKind == SIR_D3D9_ABI_BEGIN ) {
			if ( bInPass ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( pRecord->bClear ) {
				const uint32_t nClear = sirD3D9NativeClear( &p->native, pRecord->nClearColor );
				if ( nClear != SIR_GRAPHICS_SUCCESS ) {
					return nClear;
				}
			}
			const uint32_t nResult = sirD3D9NativeBeginPass( &p->native );
			if ( nResult != SIR_GRAPHICS_SUCCESS ) {
				return nResult;
			}
			bInPass = true;
		} else if ( pRecord->nKind == SIR_D3D9_ABI_END ) {
			if ( !bInPass ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			const uint32_t nResult = sirD3D9NativeEndPass( &p->native );
			if ( nResult != SIR_GRAPHICS_SUCCESS ) {
				return nResult;
			}
			bInPass = false;
		} else if ( pRecord->nKind == SIR_D3D9_ABI_BARRIER ) {
			if ( bInPass && pRecord->bUnbindBarrier ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( pRecord->bUnbindBarrier ) {
				// D3D9 orders direct commands implicitly, but a resource changing
				// to a writable use must not remain bound as a shader input.
				for ( uint32_t slot = 0; slot < 8; ++slot ) {
					if ( FAILED( p->native.pDevice->SetTexture( slot, nullptr ) ) ) {
						return SIR_GRAPHICS_DRIVER_ERROR;
					}
				}
				if ( FAILED( p->native.pDevice->SetStreamSource( 0, nullptr, 0, 0 ) ) ||
				     FAILED( p->native.pDevice->SetIndices( nullptr ) ) ) {
					return SIR_GRAPHICS_DRIVER_ERROR;
				}
			}
		} else if ( pRecord->nKind == SIR_D3D9_ABI_VIEWPORT ) {
			D3DVIEWPORT9 viewport = {};
			viewport.X = ( DWORD )pRecord->viewport.fX;
			viewport.Y = ( DWORD )pRecord->viewport.fY;
			viewport.Width = ( DWORD )pRecord->viewport.fWidth;
			viewport.Height = ( DWORD )pRecord->viewport.fHeight;
			viewport.MinZ = pRecord->viewport.fMinimumDepth;
			viewport.MaxZ = pRecord->viewport.fMaximumDepth;
			HRESULT hResult = p->native.pDevice->SetViewport( &viewport );
			if ( SUCCEEDED( hResult ) && pRecord->bHasScissor ) {
				RECT rect = { pRecord->scissor.nLeft, pRecord->scissor.nTop, pRecord->scissor.nRight, pRecord->scissor.nBottom };
				hResult = p->native.pDevice->SetScissorRect( &rect );
			}
			if ( SUCCEEDED( hResult ) ) {
				hResult = p->native.pDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, pRecord->bHasScissor ? TRUE : FALSE );
			}
			if ( FAILED( hResult ) ) {
				return SIR_GRAPHICS_DRIVER_ERROR;
			}
		} else if ( pRecord->nKind == SIR_D3D9_ABI_PIPELINE ) {
			hPipeline = pRecord->hPipeline;
		} else if ( pRecord->nKind == SIR_D3D9_ABI_BINDING_SET ) {
			hSet = pRecord->hPipeline;
		} else if ( pRecord->nKind == SIR_D3D9_ABI_VERTEX ) {
			vertex = pRecord->vertex;
		} else if ( pRecord->nKind == SIR_D3D9_ABI_INDEX ) {
			index = pRecord->index;
		} else if ( pRecord->nKind == SIR_D3D9_ABI_DRAW || pRecord->nKind == SIR_D3D9_ABI_DRAW_INDEXED ) {
			if ( !bInPass || vertex.nSize == 0 || hPipeline.eType != SIR_GRAPHICS_HANDLE_PIPELINE ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			const sirD3D9FixedPipeline_t *pPipeline = nullptr;
			const uint32_t nPipeline = sirD3D9FixedPipelineLookup( &p->fixed, hPipeline, &pPipeline );
			if ( nPipeline != SIR_GRAPHICS_SUCCESS ) {
				return nPipeline;
			}
			if ( vertex.nStrideBytes != pPipeline->nVertexStride ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			void *pVertex = nullptr;
			const uint32_t nVertex = sirD3D9NativeLookupResource( &p->native, vertex.hBuffer, &pVertex );
			if ( nVertex != SIR_GRAPHICS_SUCCESS ) {
				return nVertex;
			}
			sirD3D9DrawPacket_t packet = {};
			packet.pVertexBuffer = ( IDirect3DVertexBuffer9 * )pVertex;
			packet.nFVF = pPipeline->nFVF;
			const sirGraphicsResult_t eDeclaration = sirD3D9ABIEnsureFixedDeclaration( p, hPipeline.nSlot, pPipeline );
			if ( eDeclaration != SIR_GRAPHICS_SUCCESS ) {
				return eDeclaration;
			}
			packet.pVertexDeclaration = p->pFixedDeclarations[hPipeline.nSlot];
			packet.nVertexStride = vertex.nStrideBytes;
			packet.nVertexOffset = ( uint32_t )vertex.nOffsetBytes;
			packet.nTopology = pPipeline->nTopology;
			const uint32_t nElements = pRecord->nKind == SIR_D3D9_ABI_DRAW ? pRecord->draw.nVertices : pRecord->indexed.nIndices;
			const sirGraphicsResult_t eCount = sirD3D9PrimitiveCount( packet.nTopology, nElements, &packet.nPrimitiveCount );
			if ( eCount != SIR_GRAPHICS_SUCCESS ) {
				return eCount;
			}
			if ( pRecord->nKind == SIR_D3D9_ABI_DRAW ) {
				packet.nFirstVertex = pRecord->draw.nFirstVertex;
				packet.nVertexCount = pRecord->draw.nVertices;
			} else {
				if ( index.nSize == 0 ) {
					return SIR_GRAPHICS_INVALID_ARGUMENT;
				}
				void *pIndices = nullptr;
				const uint32_t nLookup = sirD3D9NativeLookupResource( &p->native, index.hBuffer, &pIndices );
				if ( nLookup != SIR_GRAPHICS_SUCCESS ) {
					return nLookup;
				}
				packet.pIndexBuffer = ( IDirect3DIndexBuffer9 * )pIndices;
				const uint64_t nFirstIndex = ( uint64_t )pRecord->indexed.nFirstIndex + index.nOffsetBytes / ( index.nIndex32 ? 4U : 2U );
				if ( nFirstIndex > UINT32_MAX ) {
					return SIR_GRAPHICS_INVALID_ARGUMENT;
				}
				packet.nFirstIndex = ( uint32_t )nFirstIndex;
				packet.iBaseVertex = pRecord->indexed.nVertexOffset;
				packet.nVertexCount = p->native.aResourceSlots[vertex.hBuffer.nSlot].nBytes / vertex.nStrideBytes;
			}
			IDirect3DBaseTexture9 *textures[8] = {};
			sirGraphicsSamplerDesc_t samplers[8] = {};
			uint32_t nRequiredTextureUnits = 0;
			for ( uint32_t j = 0; j < pPipeline->recipe.nTextureStageCount; ++j ) {
				const sirGraphicsABIFixedStage_t *pStage = &pPipeline->recipe.pStages[j];
				const bool bColorTexture = pStage->nColorOperation != SIR_GRAPHICS_FIXED_OP_DISABLE &&
				                           ( pStage->nColorArgument0 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ||
				                             pStage->nColorArgument1 == SIR_GRAPHICS_FIXED_ARG_TEXTURE );
				const bool bAlphaTexture = pStage->nAlphaOperation != SIR_GRAPHICS_FIXED_OP_DISABLE &&
				                           ( pStage->nAlphaArgument0 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ||
				                             pStage->nAlphaArgument1 == SIR_GRAPHICS_FIXED_ARG_TEXTURE );
				if ( bColorTexture || bAlphaTexture ) {
					nRequiredTextureUnits = j + 1;
				}
			}
			sirD3D9ABIObject_t *pBindings = nullptr;
			if ( hSet.eType == SIR_GRAPHICS_HANDLE_BINDING_SET ) {
				pBindings = sirD3D9ABIObjectLookup( p->bindingSets, 16, hSet, p->nGeneration, SIR_GRAPHICS_HANDLE_BINDING_SET );
				if ( pBindings == nullptr || pBindings->hPipeline.nSlot != hPipeline.nSlot ||
				     pBindings->hPipeline.nGeneration != hPipeline.nGeneration ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
			}
			if ( nRequiredTextureUnits != 0 && pBindings == nullptr ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			for ( uint32_t j = 0; j < nRequiredTextureUnits; ++j ) {
				const sirGraphicsABIFixedStage_t *pStage = &pPipeline->recipe.pStages[j];
				const bool bNeedsTexture = ( pStage->nColorOperation != SIR_GRAPHICS_FIXED_OP_DISABLE &&
				                             ( pStage->nColorArgument0 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ||
				                               pStage->nColorArgument1 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ) ) ||
				                           ( pStage->nAlphaOperation != SIR_GRAPHICS_FIXED_OP_DISABLE &&
				                             ( pStage->nAlphaArgument0 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ||
				                               pStage->nAlphaArgument1 == SIR_GRAPHICS_FIXED_ARG_TEXTURE ) );
				if ( !bNeedsTexture ) {
					// A non-sampling stage between sampled stages still occupies a
					// native unit, but does not require a GRDG texture or sampler.
					samplers[j].eFilter = SIR_GRAPHICS_FILTER_NEAREST;
					samplers[j].eAddressU = SIR_GRAPHICS_ADDRESS_CLAMP;
					samplers[j].eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
					samplers[j].eAddressW = SIR_GRAPHICS_ADDRESS_CLAMP;
					samplers[j].fMaximumAnisotropy = 1.0f;
					continue;
				}
				if ( pBindings->szViews[j].eType != SIR_GRAPHICS_HANDLE_VIEW ||
				     pBindings->szSamplers[j].eType != SIR_GRAPHICS_HANDLE_SAMPLER ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				sirD3D9ABIObject_t *pView =
				    sirD3D9ABIObjectLookup( p->views, 16, pBindings->szViews[j], p->nGeneration, SIR_GRAPHICS_HANDLE_VIEW );
				sirD3D9ABIObject_t *pSampler =
				    sirD3D9ABIObjectLookup( p->samplers, 16, pBindings->szSamplers[j], p->nGeneration, SIR_GRAPHICS_HANDLE_SAMPLER );
				if ( pView == nullptr || pSampler == nullptr ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				void *pTexture = nullptr;
				const uint32_t nLookup = sirD3D9NativeLookupResource( &p->native, pView->hResource, &pTexture );
				if ( nLookup != SIR_GRAPHICS_SUCCESS ) {
					return nLookup;
				}
				textures[j] = ( IDirect3DBaseTexture9 * )pTexture;
				samplers[j] = pSampler->sampler;
			}
			packet.ppTextures = textures;
			packet.nTextureCount = nRequiredTextureUnits;
			packet.pSamplers = samplers;
			packet.nSamplerCount = nRequiredTextureUnits;
			const uint32_t nRecipe = sirD3D9NativeApplyFixedRecipe( &p->native, &pPipeline->recipe );
			if ( nRecipe != SIR_GRAPHICS_SUCCESS ) {
				return nRecipe;
			}
			const uint32_t nState = sirD3D9ABIApplyPipelineState( p, pPipeline );
			if ( nState != SIR_GRAPHICS_SUCCESS ) {
				return nState;
			}
			const uint32_t nDraw = sirD3D9NativeDraw( &p->native, &packet );
			if ( nDraw != SIR_GRAPHICS_SUCCESS ) {
				return nDraw;
			}
		}
	}
	return bInPass ? SIR_GRAPHICS_INVALID_ARGUMENT : SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABISubmit( void *pState, uint32_t nQueue, const sirGraphicsABISubmission_t *pBatch,
                                  sirGraphicsCompletion_t *pCompletion ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr || pBatch == nullptr || pCompletion == nullptr || pBatch->nSize < sizeof( *pBatch ) ||
	     nQueue != SIR_GRAPHICS_QUEUE_DIRECT || !p->bRecording || !p->bClosed || p->bSubmitted || pBatch->nCommandListCount != 1 ||
	     pBatch->ppCommandLists == nullptr || pBatch->ppCommandLists[0] != p || pBatch->nWaitCount != 0 || pBatch->nAcquisitionCount != 1 ||
	     pBatch->nReserved != 0 || p->native.pCompletionQuery == nullptr || p->native.bCompletionPending ||
	     ( pBatch->nAcquisitionCount != 0 && pBatch->pAcquisitions == nullptr ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pBatch->nAcquisitionCount != 0 &&
	     ( !sirD3D9ABISurfaceHandle( p, pBatch->pAcquisitions[0].hSurface ) || pBatch->pAcquisitions[0].nAcquisitionId != p->nAcquisition ||
	       pBatch->pAcquisitions[0].nWindowGeneration != p->nWindowGeneration ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint32_t nReplay = sirD3D9ABIReplay( p );
	if ( nReplay != SIR_GRAPHICS_SUCCESS ) {
		if ( p->native.bSceneActive ) {
			( void )sirD3D9NativeEndPass( &p->native );
		}
		return nReplay;
	}
	if ( FAILED( p->native.pCompletionQuery->Issue( D3DISSUE_END ) ) || p->native.nLastSubmittedSerial == UINT64_MAX ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	p->native.bCompletionPending = true;
	++p->native.nLastSubmittedSerial;
	*pCompletion = { p->nGeneration, SIR_GRAPHICS_QUEUE_DIRECT, p->native.nLastSubmittedSerial };
	p->bSubmitted = true;
	p->bRecording = false;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D9ABIShutdown( void *pState ) noexcept
{
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pState );
	if ( p == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( p->bSurface ) {
		sirD3D9ABIReleaseMappedBuffers( p );
		sirD3D9ABIReleaseFixedDeclarations( p );
		sirD3D9NativeShutdown( &p->native );
		p->bSurface = false;
	}
	p->bRecording = false;
	p->bSubmitted = false;
	return SIR_GRAPHICS_SUCCESS;
}

uint32_t sirD3D9ABICreateDeviceWindows( const sirGraphicsABIHost_t *pHost, const sirGraphicsABIDeviceRequest_t *pRequest,
                                        sirGraphicsABIDevice_t *pOut ) noexcept
{
	if ( pHost == nullptr || pRequest == nullptr || pOut == nullptr || pRequest->nHeadless != 0 ||
	     ( pRequest->nRequiredFeatures &
	       ~( SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION | SIR_GRAPHICS_FEATURE_PRESENT ) ) != 0 ||
	     pRequest->nFramesInFlight != 1 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	void *pMemory = pHost->pAllocate( pHost->pContext, sizeof( sirD3D9ABIDeviceState_t ), alignof( sirD3D9ABIDeviceState_t ) );
	if ( pMemory == nullptr ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	if ( ( ( uintptr_t )pMemory % alignof( sirD3D9ABIDeviceState_t ) ) != 0 ) {
		pHost->pFree( pHost->pContext, pMemory );
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	sirD3D9ABIDeviceState_t *p = ( sirD3D9ABIDeviceState_t * )pMemory;
	memset( p, 0, sizeof( *p ) );
	p->host = *pHost;
	p->nAdapter = pRequest->nAdapterIndex;
	p->nGeneration = 1;
	p->nBudget = pRequest->nResidentBudgetBytes == 0 ? 256ULL * 1024ULL * 1024ULL : pRequest->nResidentBudgetBytes;
	HMODULE hLibrary = LoadLibraryW( L"d3d9.dll" );
	if ( hLibrary == nullptr ) {
		pHost->pFree( pHost->pContext, p );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	typedef IDirect3D9 *( WINAPI * sirCreate9_t )( UINT );
	sirCreate9_t pCreate = ( sirCreate9_t )GetProcAddress( hLibrary, "Direct3DCreate9" );
	IDirect3D9 *pRuntime = pCreate != nullptr ? pCreate( D3D_SDK_VERSION ) : nullptr;
	if ( pRuntime == nullptr || p->nAdapter >= pRuntime->GetAdapterCount() ||
	     FAILED( pRuntime->GetDeviceCaps( p->nAdapter, D3DDEVTYPE_HAL, &p->availableCaps ) ) ) {
		if ( pRuntime != nullptr ) {
			pRuntime->Release();
		}
		FreeLibrary( hLibrary );
		pHost->pFree( pHost->pContext, p );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	p->pProbeRuntime = pRuntime;
	p->hProbeLibrary = hLibrary;
	sirD3D9FixedPipelinesInitialize( &p->fixed, 1, p->availableCaps.MaxTextureBlendStages, p->availableCaps.MaxSimultaneousTextures );
	p->fixed.nMaximumLights =
	    p->availableCaps.MaxActiveLights < SIR_D3D9_MAX_FIXED_LIGHTS ? p->availableCaps.MaxActiveLights : SIR_D3D9_MAX_FIXED_LIGHTS;
	sirGraphicsABIDevice_t result = {};
	result.nSize = sizeof( result );
	result.pDevice = p;
	result.pQueryCapabilities = sirD3D9ABICaps;
	result.pQueryFormat = sirD3D9ABIFormat;
	result.pQueryPipelineModelCaps = sirD3D9ABIModelCaps;
	result.pPoll = sirD3D9ABIPoll;
	result.pWait = sirD3D9ABIWait;
	result.pQueryMemoryBudget = sirD3D9ABIMemory;
	result.pCreateBuffer = sirD3D9ABIBuffer;
	result.pCreateTexture = sirD3D9ABITexture;
	result.pCreateView = sirD3D9ABIView;
	result.pCreateSampler = sirD3D9ABISampler;
	result.pCreatePipeline = sirD3D9ABIPipeline;
	result.pCreateBindingSet = sirD3D9ABIBindingSet;
	result.pUpdateBindings = sirD3D9ABIBindings;
	result.pMapBuffer = sirD3D9ABIMapBuffer;
	result.pUnmapBuffer = sirD3D9ABIUnmapBuffer;
	result.pFlushBuffer = sirD3D9ABIFlushBuffer;
	result.pInvalidateBuffer = sirD3D9ABIUnsupportedRange;
	result.pAcquireCommands = sirD3D9ABIAcquireCommands;
	result.pSubmit = sirD3D9ABISubmit;
	result.pRetireResource = sirD3D9ABIRetire;
	result.pCreateSurface = sirD3D9ABISurface;
	result.pResizeSurface = sirD3D9ABIResize;
	result.pAcquireSurfaceImage = sirD3D9ABIAcquire;
	result.pPresent = sirD3D9ABIPresent;
	result.pShutdown = sirD3D9ABIShutdown;
	*pOut = result;
	return SIR_GRAPHICS_SUCCESS;
}

uint32_t sirD3D9ABIDestroyDeviceWindows( sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDevice == nullptr || pDevice->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D9ABIDeviceState_t *p = sirD3D9ABIState( pDevice->pDevice );
	( void )sirD3D9ABIShutdown( p );
	if ( p->pProbeRuntime != nullptr ) {
		p->pProbeRuntime->Release();
	}
	if ( p->hProbeLibrary != nullptr ) {
		FreeLibrary( p->hProbeLibrary );
	}
	const sirGraphicsABIHost_t host = p->host;
	host.pFree( host.pContext, p );
	*pDevice = {};
	pDevice->nSize = sizeof( *pDevice );
	return SIR_GRAPHICS_SUCCESS;
}
#endif
