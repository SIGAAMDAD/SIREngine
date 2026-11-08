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
#include <Renderer/Drivers/D3D9/SIRD3D9Native.h>
#include <Renderer/Drivers/D3D9/SIRD3D9Validation.h>
#include <Renderer/Drivers/D3D9/SIRD3D9Formats.h>
#ifdef _WIN32
#include <stdlib.h>
#include <string.h>

static sirGraphicsResult_t sirD3D9Error( HRESULT hResult ) noexcept
{
	if ( SUCCEEDED( hResult ) ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( hResult == D3DERR_DEVICELOST || hResult == D3DERR_DEVICENOTRESET ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( hResult == D3DERR_INVALIDCALL ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( hResult == D3DERR_OUTOFVIDEOMEMORY || hResult == E_OUTOFMEMORY ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	return SIR_GRAPHICS_DRIVER_ERROR;
}

static void sirD3D9NativeUnbindForReset( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr ) {
		return;
	}
	// The D3D9 state machine retains references to bound DEFAULT-pool objects.
	// Detach them before releasing the owner's references and calling Reset/ResetEx.
	for ( uint32_t i = 0; i < pNative->caps.MaxSimultaneousTextures && i < 16; ++i ) {
		( void )pNative->pDevice->SetTexture( i, nullptr );
	}
	( void )pNative->pDevice->SetStreamSource( 0, nullptr, 0, 0 );
	( void )pNative->pDevice->SetIndices( nullptr );
	( void )pNative->pDevice->SetVertexShader( nullptr );
	( void )pNative->pDevice->SetPixelShader( nullptr );
	( void )pNative->pDevice->SetVertexDeclaration( nullptr );
	for ( uint32_t i = 1; i < pNative->caps.NumSimultaneousRTs && i < 4; ++i ) {
		( void )pNative->pDevice->SetRenderTarget( i, nullptr );
	}
	IDirect3DSurface9 *pBackBuffer = nullptr;
	if ( SUCCEEDED( pNative->pDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer ) ) ) {
		( void )pNative->pDevice->SetRenderTarget( 0, pBackBuffer );
		pBackBuffer->Release();
	}
	( void )pNative->pDevice->SetDepthStencilSurface( nullptr );
}

static void sirD3D9NativeReleaseTrackedResources( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->resources.pSlots == nullptr ) {
		return;
	}
	for ( uint32_t i = 0; i < pNative->resources.nCapacity; ++i ) {
		sirD3D9ResourceSlot_t *pSlot = &pNative->resources.pSlots[i];
		if ( pSlot->pNative != nullptr ) {
			( ( IUnknown * )pSlot->pNative )->Release();
			pSlot->pNative = nullptr;
		}
	}
	sirD3D9ResourcesInvalidate( &pNative->resources );
}

void sirD3D9NativeShutdown( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr ) {
		return;
	}
	if ( pNative->bSceneActive && pNative->pDevice != nullptr ) {
		pNative->pDevice->EndScene();
	}
	sirD3D9NativeReleaseTrackedResources( pNative );
	if ( pNative->pCompletionQuery != nullptr ) {
		pNative->pCompletionQuery->Release();
	}
	if ( pNative->pDeviceEx != nullptr ) {
		pNative->pDeviceEx->Release();
	} else if ( pNative->pDevice != nullptr ) {
		pNative->pDevice->Release();
	}
	if ( pNative->pRuntimeEx != nullptr ) {
		pNative->pRuntimeEx->Release();
	} else if ( pNative->pRuntime != nullptr ) {
		pNative->pRuntime->Release();
	}
	if ( pNative->hRuntime != nullptr ) {
		FreeLibrary( pNative->hRuntime );
	}
	*pNative = {};
}

sirGraphicsResult_t sirD3D9NativeInitialize( sirD3D9Native_t *pNative, HWND hFocusWindow, uint32_t nAdapterIndex, bool bPreferEx ) noexcept
{
	if ( pNative == nullptr || hFocusWindow == nullptr || !IsWindow( hFocusWindow ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pNative = {};
	pNative->hRuntime = LoadLibraryW( L"d3d9.dll" );
	if ( pNative->hRuntime == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	typedef IDirect3D9 *( WINAPI * sirD3D9Create_t )( UINT );
	typedef HRESULT( WINAPI * sirD3D9CreateEx_t )( UINT, IDirect3D9Ex ** );
	sirD3D9Create_t pCreate = ( sirD3D9Create_t )GetProcAddress( pNative->hRuntime, "Direct3DCreate9" );
	sirD3D9CreateEx_t pCreateEx = ( sirD3D9CreateEx_t )GetProcAddress( pNative->hRuntime, "Direct3DCreate9Ex" );
	if ( bPreferEx && pCreateEx != nullptr && SUCCEEDED( pCreateEx( D3D_SDK_VERSION, &pNative->pRuntimeEx ) ) ) {
		pNative->pRuntime = pNative->pRuntimeEx;
		pNative->bEx = true;
	} else if ( pCreate != nullptr ) {
		pNative->pRuntime = pCreate( D3D_SDK_VERSION );
	}
	if ( pNative->pRuntime == nullptr ) {
		sirD3D9NativeShutdown( pNative );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nAdapterIndex >= pNative->pRuntime->GetAdapterCount() ||
	     FAILED( pNative->pRuntime->GetDeviceCaps( nAdapterIndex, D3DDEVTYPE_HAL, &pNative->caps ) ) ) {
		sirD3D9NativeShutdown( pNative );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	pNative->hFocusWindow = hFocusWindow;
	pNative->nAdapterIndex = nAdapterIndex;
	pNative->present.Windowed = TRUE;
	pNative->present.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pNative->present.hDeviceWindow = hFocusWindow;
	pNative->present.BackBufferFormat = D3DFMT_UNKNOWN;
	pNative->present.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	const DWORD nFlags =
	    D3DCREATE_FPU_PRESERVE | ( pNative->caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ? D3DCREATE_HARDWARE_VERTEXPROCESSING
		                                                                                  : D3DCREATE_SOFTWARE_VERTEXPROCESSING );
	HRESULT hResult = E_FAIL;
	if ( pNative->bEx ) {
		hResult = pNative->pRuntimeEx->CreateDeviceEx( nAdapterIndex, D3DDEVTYPE_HAL, hFocusWindow, nFlags, &pNative->present, nullptr,
		                                               &pNative->pDeviceEx );
		pNative->pDevice = pNative->pDeviceEx;
	} else {
		hResult =
		    pNative->pRuntime->CreateDevice( nAdapterIndex, D3DDEVTYPE_HAL, hFocusWindow, nFlags, &pNative->present, &pNative->pDevice );
	}
	if ( FAILED( hResult ) ) {
		const sirGraphicsResult_t eError = sirD3D9Error( hResult );
		sirD3D9NativeShutdown( pNative );
		return eError;
	}
	pNative->nDeviceGeneration = 1;
	if ( sirD3D9ResourcesInitialize( &pNative->resources, pNative->aResourceSlots, 128, 256ULL * 1024 * 1024 ) != SIR_GRAPHICS_SUCCESS ) {
		sirD3D9NativeShutdown( pNative );
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	// Query completion is optional; a missing query must never be mistaken for a completed operation.
	pNative->pDevice->CreateQuery( D3DQUERYTYPE_EVENT, &pNative->pCompletionQuery );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9NativeClear( sirD3D9Native_t *pNative, uint32_t nColor ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return sirD3D9Error( pNative->pDevice->Clear( 0, nullptr, D3DCLEAR_TARGET, nColor, 1.0f, 0 ) );
}

sirGraphicsResult_t sirD3D9NativeBeginPass( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pNative->bSceneActive ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const HRESULT hResult = pNative->pDevice->BeginScene();
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	pNative->bSceneActive = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9NativeEndPass( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || !pNative->bSceneActive ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const HRESULT hResult = pNative->pDevice->EndScene();
	pNative->bSceneActive = false;
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeDrawTriangle( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pNative->bSceneActive ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	struct sirD3D9TestVertex_s
	{
		float x, y, z, rhw;
		DWORD color;
	};
	const sirD3D9TestVertex_s vertices[] = {
	    { 16, 16, 0.5f, 1.0f, 0xffff0000 }, { 160, 16, 0.5f, 1.0f, 0xff00ff00 }, { 80, 160, 0.5f, 1.0f, 0xff0000ff } };
	HRESULT hResult = pNative->pDevice->BeginScene();
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	pNative->bSceneActive = true;
	hResult = pNative->pDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
	if ( SUCCEEDED( hResult ) ) {
		hResult = pNative->pDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, 1, vertices, sizeof( vertices[0] ) );
	}
	const HRESULT hEnd = pNative->pDevice->EndScene();
	pNative->bSceneActive = false;
	return sirD3D9Error( FAILED( hResult ) ? hResult : hEnd );
}

sirGraphicsResult_t sirD3D9NativePresent( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pNative->bSceneActive ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// One event query cannot independently track more than one outstanding serial.
	// Do not overwrite its fence until the previous issue has completed.
	if ( pNative->bCompletionPending ) {
		bool bComplete = false;
		const sirGraphicsResult_t ePoll = sirD3D9NativePoll( pNative, &bComplete );
		if ( ePoll != SIR_GRAPHICS_SUCCESS ) {
			return ePoll;
		}
		if ( !bComplete ) {
			return SIR_GRAPHICS_BUSY;
		}
	}
	const bool bVertexPending = pNative->vertexRing.nPendingWrites != 0;
	const bool bIndexPending = pNative->indexRing.nPendingWrites != 0;
	if ( pNative->nLastSubmittedSerial == UINT64_MAX || ( ( bVertexPending || bIndexPending ) && pNative->pCompletionQuery == nullptr ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pNative->pCompletionQuery != nullptr ) {
		const HRESULT hIssued = pNative->pCompletionQuery->Issue( D3DISSUE_END );
		if ( FAILED( hIssued ) ) {
			return sirD3D9Error( hIssued );
		}
		pNative->bCompletionPending = true;
		const uint64_t nSerial = pNative->nLastSubmittedSerial + 1;
		if ( bVertexPending ) {
			sirD3D9RingSubmit( &pNative->vertexRing, nSerial );
		}
		if ( bIndexPending ) {
			sirD3D9RingSubmit( &pNative->indexRing, nSerial );
		}
		pNative->nLastSubmittedSerial = nSerial;
	}
	return sirD3D9Error( pNative->pDevice->Present( nullptr, nullptr, nullptr, nullptr ) );
}

sirGraphicsResult_t sirD3D9NativePoll( sirD3D9Native_t *pNative, bool *pComplete ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pComplete == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pComplete = false;
	if ( pNative->pCompletionQuery == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !pNative->bCompletionPending ) {
		*pComplete = true;
		return SIR_GRAPHICS_SUCCESS;
	}
	const HRESULT hResult = pNative->pCompletionQuery->GetData( nullptr, 0, D3DGETDATA_FLUSH );
	if ( hResult == S_FALSE ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	pNative->bCompletionPending = false;
	*pComplete = true;
	pNative->nLastCompletedSerial = pNative->nLastSubmittedSerial;
	if ( pNative->vertexRing.nCapacity != 0 ) {
		sirD3D9RingComplete( &pNative->vertexRing, pNative->vertexRing.nLatestSubmission );
	}
	if ( pNative->indexRing.nCapacity != 0 ) {
		sirD3D9RingComplete( &pNative->indexRing, pNative->indexRing.nLatestSubmission );
	}
	sirD3D9NativeCollectResources( pNative, pNative->nLastCompletedSerial );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9NativeResize( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || nWidth == 0 || nHeight == 0 || pNative->bSceneActive ||
	     pNative->nDeviceGeneration == UINT32_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pNative->bCompletionPending ) {
		const sirGraphicsResult_t eStatus = sirD3D9NativeDeviceStatus( pNative );
		if ( eStatus == SIR_GRAPHICS_SUCCESS ) {
			bool bComplete = false;
			const sirGraphicsResult_t ePoll = sirD3D9NativePoll( pNative, &bComplete );
			if ( ePoll != SIR_GRAPHICS_SUCCESS ) {
				return ePoll;
			}
			if ( !bComplete ) {
				return SIR_GRAPHICS_BUSY;
			}
		} else if ( eStatus != SIR_GRAPHICS_DEVICE_LOST && eStatus != SIR_GRAPHICS_SURFACE_CHANGED ) {
			return eStatus;
		}
	}
	if ( pNative->pCompletionQuery != nullptr ) {
		pNative->pCompletionQuery->Release();
		pNative->pCompletionQuery = nullptr;
	}
	// All DEFAULT-pool resources must be retired before standard D3D9 Reset.
	sirD3D9NativeUnbindForReset( pNative );
	sirD3D9NativeReleaseTrackedResources( pNative );
	pNative->hUploadVertex = {};
	pNative->hUploadIndex = {};
	sirD3D9RingReset( &pNative->vertexRing );
	sirD3D9RingReset( &pNative->indexRing );
	pNative->nLastSubmittedSerial = 0;
	pNative->nLastCompletedSerial = 0;
	pNative->bCompletionPending = false;
	pNative->nActiveFixedLights = 0;
	pNative->nActiveFixedStages = 0;
	pNative->nActiveTextureBindings = 0;
	pNative->present.BackBufferWidth = nWidth;
	pNative->present.BackBufferHeight = nHeight;
	const HRESULT hResult =
	    pNative->bEx ? pNative->pDeviceEx->ResetEx( &pNative->present, nullptr ) : pNative->pDevice->Reset( &pNative->present );
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	++pNative->nDeviceGeneration;
	pNative->pDevice->CreateQuery( D3DQUERYTYPE_EVENT, &pNative->pCompletionQuery );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9NativeCreateBufferDesc( sirD3D9Native_t *pNative, const sirGraphicsBufferDesc_t *pDesc, bool bIndex32,
                                                   IDirect3DResource9 **ppResource ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || ppResource == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppResource = nullptr;
	const sirGraphicsResult_t eValidation = sirD3D9ValidateBuffer( pDesc );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	const bool bIndex = pDesc->nUsage == SIR_GRAPHICS_USAGE_INDEX;
	if ( bIndex32 && !bIndex ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( bIndex32 && pNative->caps.MaxVertexIndex <= UINT16_MAX ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const DWORD nUsage = pDesc->eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD ? D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY : D3DUSAGE_WRITEONLY;
	HRESULT hResult = E_FAIL;
	if ( bIndex ) {
		IDirect3DIndexBuffer9 *pBuffer = nullptr;
		hResult = pNative->pDevice->CreateIndexBuffer( ( UINT )pDesc->nSizeBytes, nUsage, bIndex32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
		                                               D3DPOOL_DEFAULT, &pBuffer, nullptr );
		*ppResource = pBuffer;
	} else {
		IDirect3DVertexBuffer9 *pBuffer = nullptr;
		hResult = pNative->pDevice->CreateVertexBuffer( ( UINT )pDesc->nSizeBytes, nUsage, 0, D3DPOOL_DEFAULT, &pBuffer, nullptr );
		*ppResource = pBuffer;
	}
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeCreateBuffer( sirD3D9Native_t *pNative, uint32_t nBytes, bool bIndex,
                                               IDirect3DResource9 **ppResource ) noexcept
{
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = nBytes;
	desc.nAlignmentBytes = bIndex ? 2 : 4;
	desc.nUsage = bIndex ? SIR_GRAPHICS_USAGE_INDEX : SIR_GRAPHICS_USAGE_VERTEX;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	return sirD3D9NativeCreateBufferDesc( pNative, &desc, false, ppResource );
}

sirGraphicsResult_t sirD3D9NativeCreateTextureResourceDesc( sirD3D9Native_t *pNative, const sirGraphicsTextureDesc_t *pDesc,
                                                            IDirect3DBaseTexture9 **ppTexture ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pNative->pRuntime == nullptr || ppTexture == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppTexture = nullptr;
	const sirGraphicsResult_t eValidation = sirD3D9ValidateTexture(
	    pDesc,
	    pNative->caps.MaxTextureWidth > pNative->caps.MaxTextureHeight ? pNative->caps.MaxTextureWidth : pNative->caps.MaxTextureHeight,
	    1 );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	if ( pDesc->nWidth > pNative->caps.MaxTextureWidth || pDesc->nHeight > pNative->caps.MaxTextureHeight ||
	     ( pDesc->eDimension == SIR_GRAPHICS_TEXTURE_3D && pDesc->nDepth > pNative->caps.MaxVolumeExtent ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eDimension == SIR_GRAPHICS_TEXTURE_CUBE && ( pNative->caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->eDimension == SIR_GRAPHICS_TEXTURE_3D && ( pNative->caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->nMipLevels > 1 ) {
		const DWORD nMipCapability = pDesc->eDimension == SIR_GRAPHICS_TEXTURE_CUBE ? D3DPTEXTURECAPS_MIPCUBEMAP
		                             : pDesc->eDimension == SIR_GRAPHICS_TEXTURE_3D ? D3DPTEXTURECAPS_MIPVOLUMEMAP
		                                                                            : D3DPTEXTURECAPS_MIPMAP;
		if ( ( pNative->caps.TextureCaps & nMipCapability ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	if ( pDesc->eDimension == SIR_GRAPHICS_TEXTURE_3D && ( pDesc->nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	D3DFORMAT eFormat = D3DFMT_UNKNOWN;
	const sirGraphicsResult_t eFormatResult = sirD3D9NativeMapFormat( pDesc->eFormat, &eFormat );
	if ( eFormatResult != SIR_GRAPHICS_SUCCESS ) {
		return eFormatResult;
	}
	D3DDISPLAYMODE mode = {};
	if ( FAILED( pNative->pRuntime->GetAdapterDisplayMode( pNative->nAdapterIndex, &mode ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	const DWORD nUsage = ( pDesc->nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ? D3DUSAGE_RENDERTARGET : 0;
	const D3DRESOURCETYPE eResourceType = pDesc->eDimension == SIR_GRAPHICS_TEXTURE_CUBE ? D3DRTYPE_CUBETEXTURE
	                                      : pDesc->eDimension == SIR_GRAPHICS_TEXTURE_3D ? D3DRTYPE_VOLUMETEXTURE
	                                                                                     : D3DRTYPE_TEXTURE;
	if ( FAILED( pNative->pRuntime->CheckDeviceFormat( pNative->nAdapterIndex, D3DDEVTYPE_HAL, mode.Format, nUsage, eResourceType,
	                                                   eFormat ) ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	HRESULT hResult = E_FAIL;
	if ( eResourceType == D3DRTYPE_CUBETEXTURE ) {
		IDirect3DCubeTexture9 *pCube = nullptr;
		hResult =
		    pNative->pDevice->CreateCubeTexture( pDesc->nWidth, pDesc->nMipLevels, nUsage, eFormat, D3DPOOL_DEFAULT, &pCube, nullptr );
		*ppTexture = pCube;
	} else if ( eResourceType == D3DRTYPE_VOLUMETEXTURE ) {
		IDirect3DVolumeTexture9 *pVolume = nullptr;
		hResult = pNative->pDevice->CreateVolumeTexture( pDesc->nWidth, pDesc->nHeight, pDesc->nDepth, pDesc->nMipLevels, nUsage, eFormat,
		                                                 D3DPOOL_DEFAULT, &pVolume, nullptr );
		*ppTexture = pVolume;
	} else {
		IDirect3DTexture9 *pTexture = nullptr;
		hResult = pNative->pDevice->CreateTexture( pDesc->nWidth, pDesc->nHeight, pDesc->nMipLevels, nUsage, eFormat, D3DPOOL_DEFAULT,
		                                           &pTexture, nullptr );
		*ppTexture = pTexture;
	}
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeCreateTextureDesc( sirD3D9Native_t *pNative, const sirGraphicsTextureDesc_t *pDesc,
                                                    IDirect3DTexture9 **ppTexture ) noexcept
{
	if ( ppTexture == nullptr || pDesc == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppTexture = nullptr;
	if ( pDesc->eDimension != SIR_GRAPHICS_TEXTURE_2D ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	IDirect3DBaseTexture9 *pBase = nullptr;
	const sirGraphicsResult_t eResult = sirD3D9NativeCreateTextureResourceDesc( pNative, pDesc, &pBase );
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		*ppTexture = static_cast<IDirect3DTexture9 *>( pBase );
	}
	return eResult;
}

sirGraphicsResult_t sirD3D9NativeUploadTexture( sirD3D9Native_t *pNative, const sirD3D9TextureUpload_t *pUpload ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pUpload == nullptr || pUpload->pTexture == nullptr ||
	     pUpload->pData == nullptr || pUpload->nMipLevel >= pUpload->pTexture->GetLevelCount() ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	D3DSURFACE_DESC nativeDesc = {};
	const HRESULT hDesc = pUpload->pTexture->GetLevelDesc( pUpload->nMipLevel, &nativeDesc );
	if ( FAILED( hDesc ) ) {
		return sirD3D9Error( hDesc );
	}
	sirD3D9TextureUploadLayout_t layout = {};
	layout.nWidth = nativeDesc.Width;
	layout.nHeight = nativeDesc.Height;
	layout.nRowPitch = pUpload->nRowPitch;
	layout.nBytes = pUpload->nBytes;
	switch ( nativeDesc.Format ) {
	case D3DFMT_A8R8G8B8:
		layout.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
		break;
	case D3DFMT_A8B8G8R8:
		layout.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		break;
	case D3DFMT_DXT1:
		layout.eFormat = SIR_GRAPHICS_FORMAT_BC1_UNORM;
		break;
	case D3DFMT_DXT5:
		layout.eFormat = SIR_GRAPHICS_FORMAT_BC3_UNORM;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nRowBytes = 0;
	uint32_t nRows = 0;
	const sirGraphicsResult_t eLayout = sirD3D9ValidateTextureUploadLayout( &layout, &nRowBytes, &nRows );
	if ( eLayout != SIR_GRAPHICS_SUCCESS ) {
		return eLayout;
	}
	if ( nativeDesc.Pool != D3DPOOL_DEFAULT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	IDirect3DTexture9 *pStaging = nullptr;
	HRESULT hResult = pNative->pDevice->CreateTexture( nativeDesc.Width, nativeDesc.Height, 1, 0, nativeDesc.Format, D3DPOOL_SYSTEMMEM,
	                                                   &pStaging, nullptr );
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	D3DLOCKED_RECT locked = {};
	hResult = pStaging->LockRect( 0, &locked, nullptr, 0 );
	if ( SUCCEEDED( hResult ) ) {
		if ( locked.Pitch < 0 || ( uint32_t )locked.Pitch < nRowBytes ) {
			hResult = D3DERR_INVALIDCALL;
		} else {
			for ( uint32_t i = 0; i < nRows; ++i ) {
				memcpy( ( uint8_t * )locked.pBits + ( size_t )i * locked.Pitch,
				        ( const uint8_t * )pUpload->pData + ( size_t )i * pUpload->nRowPitch, nRowBytes );
			}
		}
		const HRESULT hUnlock = pStaging->UnlockRect( 0 );
		if ( SUCCEEDED( hResult ) ) {
			hResult = hUnlock;
		}
	}
	if ( SUCCEEDED( hResult ) ) {
		IDirect3DSurface9 *pSourceSurface = nullptr;
		IDirect3DSurface9 *pTargetSurface = nullptr;
		hResult = pStaging->GetSurfaceLevel( 0, &pSourceSurface );
		if ( SUCCEEDED( hResult ) ) {
			hResult = pUpload->pTexture->GetSurfaceLevel( pUpload->nMipLevel, &pTargetSurface );
		}
		if ( SUCCEEDED( hResult ) ) {
			hResult = pNative->pDevice->UpdateSurface( pSourceSurface, nullptr, pTargetSurface, nullptr );
		}
		if ( pTargetSurface != nullptr ) {
			pTargetSurface->Release();
		}
		if ( pSourceSurface != nullptr ) {
			pSourceSurface->Release();
		}
	}
	pStaging->Release();
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeCreateTexture( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight,
                                                IDirect3DTexture9 **ppTexture ) noexcept
{
	sirGraphicsTextureDesc_t desc = {};
	desc.nWidth = nWidth;
	desc.nHeight = nHeight;
	desc.nDepth = 1;
	desc.nArrayLayers = 1;
	desc.nMipLevels = 1;
	desc.nSampleCount = 1;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	return sirD3D9NativeCreateTextureDesc( pNative, &desc, ppTexture );
}
#endif

#ifdef _WIN32
sirGraphicsResult_t sirD3D9NativeUploadBuffer( sirD3D9Native_t *pNative, const sirD3D9BufferUpdate_t *pUpdate ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pUpdate == nullptr || pUpdate->pBuffer == nullptr ||
	     pUpdate->pData == nullptr || pUpdate->nBytes == 0 ||
	     ( pUpdate->nLockFlags != 0 && pUpdate->nLockFlags != SIR_D3D9_RING_DISCARD &&
	       pUpdate->nLockFlags != SIR_D3D9_RING_NOOVERWRITE ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pUpdate->nLockFlags == 0 && ( pNative->bCompletionPending || pNative->bSceneActive ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	const DWORD nNativeFlags = pUpdate->nLockFlags == SIR_D3D9_RING_DISCARD       ? D3DLOCK_DISCARD
	                           : pUpdate->nLockFlags == SIR_D3D9_RING_NOOVERWRITE ? D3DLOCK_NOOVERWRITE
	                                                                              : 0;
	void *pDestination = nullptr;
	HRESULT hResult = E_FAIL;
	if ( pUpdate->pBuffer->GetType() == D3DRTYPE_VERTEXBUFFER ) {
		IDirect3DVertexBuffer9 *pBuffer = nullptr;
		if ( FAILED( pUpdate->pBuffer->QueryInterface( IID_IDirect3DVertexBuffer9, ( void ** )&pBuffer ) ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		D3DVERTEXBUFFER_DESC desc = {};
		hResult = pBuffer->GetDesc( &desc );
		if ( SUCCEEDED( hResult ) &&
		     sirD3D9ValidateBufferWrite( desc.Size, pUpdate->nOffset, pUpdate->nBytes, 1 ) == SIR_GRAPHICS_SUCCESS &&
		     ( pUpdate->nLockFlags == 0 || ( desc.Usage & D3DUSAGE_DYNAMIC ) != 0 ) ) {
			hResult = pBuffer->Lock( pUpdate->nOffset, pUpdate->nBytes, &pDestination, nNativeFlags );
			if ( SUCCEEDED( hResult ) ) {
				memcpy( pDestination, pUpdate->pData, pUpdate->nBytes );
				hResult = pBuffer->Unlock();
			}
		} else {
			hResult = D3DERR_INVALIDCALL;
		}
		pBuffer->Release();
	} else if ( pUpdate->pBuffer->GetType() == D3DRTYPE_INDEXBUFFER ) {
		IDirect3DIndexBuffer9 *pBuffer = nullptr;
		if ( FAILED( pUpdate->pBuffer->QueryInterface( IID_IDirect3DIndexBuffer9, ( void ** )&pBuffer ) ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		D3DINDEXBUFFER_DESC desc = {};
		hResult = pBuffer->GetDesc( &desc );
		const uint32_t nElementBytes = desc.Format == D3DFMT_INDEX32 ? 4 : desc.Format == D3DFMT_INDEX16 ? 2 : 0;
		if ( SUCCEEDED( hResult ) && nElementBytes != 0 &&
		     sirD3D9ValidateBufferWrite( desc.Size, pUpdate->nOffset, pUpdate->nBytes, nElementBytes ) == SIR_GRAPHICS_SUCCESS &&
		     ( pUpdate->nLockFlags == 0 || ( desc.Usage & D3DUSAGE_DYNAMIC ) != 0 ) ) {
			hResult = pBuffer->Lock( pUpdate->nOffset, pUpdate->nBytes, &pDestination, nNativeFlags );
			if ( SUCCEEDED( hResult ) ) {
				memcpy( pDestination, pUpdate->pData, pUpdate->nBytes );
				hResult = pBuffer->Unlock();
			}
		} else {
			hResult = D3DERR_INVALIDCALL;
		}
		pBuffer->Release();
	} else {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeUpdateTrackedBuffer( sirD3D9Native_t *pNative, const sirD3D9TrackedBufferWrite_t *pWrite ) noexcept
{
	if ( pNative == nullptr || pWrite == nullptr || pWrite->pData == nullptr || pWrite->hBuffer.eType != SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pResource = nullptr;
	const sirGraphicsResult_t eLookup = sirD3D9NativeLookupResource( pNative, pWrite->hBuffer, &pResource );
	if ( eLookup != SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	sirD3D9BufferUpdate_t update = {};
	update.pBuffer = ( IDirect3DResource9 * )pResource;
	update.pData = pWrite->pData;
	update.nOffset = pWrite->nOffset;
	update.nBytes = pWrite->nBytes;
	return sirD3D9NativeUploadBuffer( pNative, &update );
}

sirGraphicsResult_t sirD3D9NativeCreateShaderTokens( sirD3D9Native_t *pNative, const void *pTokens, uint64_t nBytes, uint32_t nStage,
                                                     IUnknown **ppShader ) noexcept
{
	if ( ppShader == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppShader = nullptr;
	if ( pNative == nullptr || pNative->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nProfile = nStage == SIR_GRAPHICS_STAGE_VERTEX     ? pNative->caps.VertexShaderVersion & 0xffffU
	                          : nStage == SIR_GRAPHICS_STAGE_FRAGMENT ? pNative->caps.PixelShaderVersion & 0xffffU
	                                                                  : 0;
	const sirGraphicsResult_t eCheck = sirD3D9ValidateShaderTokens( pTokens, nBytes, nStage, nProfile );
	if ( eCheck != SIR_GRAPHICS_SUCCESS ) {
		return eCheck;
	}
	// Never cast an unaligned borrowed shader blob to DWORD. Copy to aligned
	// storage before the native device parses the token stream.
	DWORD *pAligned = ( DWORD * )malloc( ( size_t )nBytes );
	if ( pAligned == nullptr ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	memcpy( pAligned, pTokens, ( size_t )nBytes );
	HRESULT hResult = E_FAIL;
	if ( nStage == SIR_GRAPHICS_STAGE_VERTEX ) {
		IDirect3DVertexShader9 *pShader = nullptr;
		hResult = pNative->pDevice->CreateVertexShader( pAligned, &pShader );
		if ( SUCCEEDED( hResult ) ) {
			*ppShader = pShader;
		} else if ( pShader != nullptr ) {
			pShader->Release();
		}
	} else {
		IDirect3DPixelShader9 *pShader = nullptr;
		hResult = pNative->pDevice->CreatePixelShader( pAligned, &pShader );
		if ( SUCCEEDED( hResult ) ) {
			*ppShader = pShader;
		} else if ( pShader != nullptr ) {
			pShader->Release();
		}
	}
	free( pAligned );
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeCreateShader( sirD3D9Native_t *pNative, const sirGraphicsABIShaderCode_t *pShader, uint32_t nStage,
                                               IUnknown **ppShader ) noexcept
{
	if ( ppShader == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppShader = nullptr;
	if ( pNative == nullptr || pShader == nullptr || pShader->nSize < sizeof( sirGraphicsABIShaderCode_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// D3D9's SM2/SM3 token streams are not DXBC containers. The common ABI
	// currently exposes no distinct D3D9 token format, so fail closed.
	return SIR_GRAPHICS_UNSUPPORTED;
}

sirGraphicsResult_t sirD3D9NativeSetShaderConstants( sirD3D9Native_t *pNative, const sirD3D9ShaderConstants_t *pConstants ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pConstants == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nProfile = pConstants->nStage == SIR_GRAPHICS_STAGE_VERTEX     ? pNative->caps.VertexShaderVersion & 0xffffU
	                          : pConstants->nStage == SIR_GRAPHICS_STAGE_FRAGMENT ? pNative->caps.PixelShaderVersion & 0xffffU
	                                                                              : 0;
	const sirGraphicsResult_t eCheck = sirD3D9ValidateShaderConstants( pConstants, nProfile, pNative->caps.MaxVertexShaderConst );
	if ( eCheck != SIR_GRAPHICS_SUCCESS ) {
		return eCheck;
	}
	const uint32_t nRegister = pConstants->nFirstRegister;
	const uint32_t nCount = pConstants->nRegisterCount;
	HRESULT hResult = E_FAIL;
	if ( pConstants->nStage == SIR_GRAPHICS_STAGE_VERTEX ) {
		hResult = pConstants->pFloat4 != nullptr ? pNative->pDevice->SetVertexShaderConstantF( nRegister, pConstants->pFloat4, nCount )
		          : pConstants->pInt4 != nullptr
		              ? pNative->pDevice->SetVertexShaderConstantI( nRegister, pConstants->pInt4, nCount )
		              : pNative->pDevice->SetVertexShaderConstantB( nRegister, ( const BOOL * )pConstants->pBool, nCount );
	} else {
		hResult = pConstants->pFloat4 != nullptr ? pNative->pDevice->SetPixelShaderConstantF( nRegister, pConstants->pFloat4, nCount )
		          : pConstants->pInt4 != nullptr
		              ? pNative->pDevice->SetPixelShaderConstantI( nRegister, pConstants->pInt4, nCount )
		              : pNative->pDevice->SetPixelShaderConstantB( nRegister, ( const BOOL * )pConstants->pBool, nCount );
	}
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeReadback( sirD3D9Native_t *pNative, sirD3D9Readback_t *pReadback ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pReadback == nullptr || pReadback->pPixels == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	IDirect3DSurface9 *pBackBuffer = nullptr;
	HRESULT hResult = pNative->pDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	D3DSURFACE_DESC desc = {};
	hResult = pBackBuffer->GetDesc( &desc );
	if ( FAILED( hResult ) || ( desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_X8R8G8B8 ) ||
	     desc.MultiSampleType != D3DMULTISAMPLE_NONE || desc.Width > UINT32_MAX / 4 || pReadback->nRowStride < desc.Width * 4 ||
	     ( uint64_t )pReadback->nRowStride * desc.Height > pReadback->nCapacityBytes ) {
		pBackBuffer->Release();
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	IDirect3DSurface9 *pStaging = nullptr;
	hResult = pNative->pDevice->CreateOffscreenPlainSurface( desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &pStaging, nullptr );
	if ( SUCCEEDED( hResult ) ) {
		hResult = pNative->pDevice->GetRenderTargetData( pBackBuffer, pStaging );
	}
	if ( SUCCEEDED( hResult ) ) {
		D3DLOCKED_RECT locked = {};
		hResult = pStaging->LockRect( &locked, nullptr, D3DLOCK_READONLY );
		if ( SUCCEEDED( hResult ) ) {
			for ( uint32_t y = 0; y < desc.Height; ++y ) {
				memcpy( ( uint8_t * )pReadback->pPixels + y * pReadback->nRowStride, ( const uint8_t * )locked.pBits + y * locked.Pitch,
				        desc.Width * 4 );
			}
			hResult = pStaging->UnlockRect();
		}
	}
	if ( pStaging != nullptr ) {
		pStaging->Release();
	}
	pBackBuffer->Release();
	if ( SUCCEEDED( hResult ) ) {
		pReadback->nWidth = desc.Width;
		pReadback->nHeight = desc.Height;
	}
	return sirD3D9Error( hResult );
}
#endif

#ifdef _WIN32
sirGraphicsResult_t sirD3D9NativeDeviceStatus( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const HRESULT hResult =
	    pNative->bEx ? pNative->pDeviceEx->CheckDeviceState( pNative->hFocusWindow ) : pNative->pDevice->TestCooperativeLevel();
#ifdef S_PRESENT_OCCLUDED
	if ( hResult == S_PRESENT_OCCLUDED ) {
		return SIR_GRAPHICS_BUSY;
	}
#endif
#ifdef S_PRESENT_MODE_CHANGED
	if ( hResult == S_PRESENT_MODE_CHANGED ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
#endif
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeApplySampler( sirD3D9Native_t *pNative, uint32_t nStage,
                                               const sirGraphicsSamplerDesc_t *pSampler ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || nStage >= pNative->caps.MaxSimultaneousTextures ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eValid =
	    sirD3D9ValidateSampler( pSampler, pNative->caps.MaxAnisotropy == 0 ? 1U : pNative->caps.MaxAnisotropy );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( pSampler->eFilter == SIR_GRAPHICS_FILTER_ANISOTROPIC &&
	     ( pNative->caps.TextureFilterCaps & ( D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC ) ) !=
	         ( D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const DWORD aAddress[] = { D3DTADDRESS_WRAP, D3DTADDRESS_MIRROR, D3DTADDRESS_CLAMP, D3DTADDRESS_BORDER };
	DWORD nMinMag = D3DTEXF_POINT;
	DWORD nMip = D3DTEXF_POINT;
	switch ( pSampler->eFilter ) {
	case SIR_GRAPHICS_FILTER_NEAREST:
		break;
	case SIR_GRAPHICS_FILTER_LINEAR:
		nMinMag = D3DTEXF_LINEAR;
		break;
	case SIR_GRAPHICS_FILTER_TRILINEAR:
		nMinMag = D3DTEXF_LINEAR;
		nMip = D3DTEXF_LINEAR;
		break;
	case SIR_GRAPHICS_FILTER_ANISOTROPIC:
		nMinMag = D3DTEXF_ANISOTROPIC;
		nMip = D3DTEXF_LINEAR;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	// Check the actual enabled device filter modes before changing state.
	const DWORD nMinCap = nMinMag == D3DTEXF_ANISOTROPIC ? D3DPTFILTERCAPS_MINFANISOTROPIC
	                      : nMinMag == D3DTEXF_LINEAR    ? D3DPTFILTERCAPS_MINFLINEAR
	                                                     : D3DPTFILTERCAPS_MINFPOINT;
	const DWORD nMagCap = nMinMag == D3DTEXF_ANISOTROPIC ? D3DPTFILTERCAPS_MAGFANISOTROPIC
	                      : nMinMag == D3DTEXF_LINEAR    ? D3DPTFILTERCAPS_MAGFLINEAR
	                                                     : D3DPTFILTERCAPS_MAGFPOINT;
	const DWORD nMipCap = nMip == D3DTEXF_LINEAR ? D3DPTFILTERCAPS_MIPFLINEAR : D3DPTFILTERCAPS_MIPFPOINT;
	if ( ( pNative->caps.TextureFilterCaps & ( nMinCap | nMagCap | nMipCap ) ) != ( nMinCap | nMagCap | nMipCap ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	HRESULT hResult = pNative->pDevice->SetSamplerState( nStage, D3DSAMP_ADDRESSU, aAddress[pSampler->eAddressU] );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetSamplerState( nStage, D3DSAMP_ADDRESSV, aAddress[pSampler->eAddressV] );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetSamplerState( nStage, D3DSAMP_ADDRESSW, aAddress[pSampler->eAddressW] );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetSamplerState( nStage, D3DSAMP_MINFILTER, nMinMag );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetSamplerState( nStage, D3DSAMP_MAGFILTER, nMinMag );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetSamplerState( nStage, D3DSAMP_MIPFILTER, nMip );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetSamplerState( nStage, D3DSAMP_MAXANISOTROPY, ( DWORD )pSampler->fMaximumAnisotropy );
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeDraw( sirD3D9Native_t *pNative, const sirD3D9DrawPacket_t *pPacket ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pPacket == nullptr || pPacket->pVertexBuffer == nullptr ||
	     pPacket->nVertexStride == 0 || pPacket->nVertexCount == 0 || pPacket->nPrimitiveCount == 0 ||
	     ( pPacket->pVertexShader != nullptr && pPacket->pVertexDeclaration == nullptr ) ||
	     ( pPacket->pVertexShader == nullptr && pPacket->nFVF == 0 ) ||
	     ( pPacket->pIndexBuffer != nullptr && pPacket->nVertexCount > INT_MAX ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	D3DPRIMITIVETYPE eType = D3DPT_FORCE_DWORD;
	switch ( pPacket->nTopology ) {
	case SIR_GRAPHICS_TOPOLOGY_POINTS:
		eType = D3DPT_POINTLIST;
		break;
	case SIR_GRAPHICS_TOPOLOGY_LINES:
		eType = D3DPT_LINELIST;
		break;
	case SIR_GRAPHICS_TOPOLOGY_LINE_STRIP:
		eType = D3DPT_LINESTRIP;
		break;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLES:
		eType = D3DPT_TRIANGLELIST;
		break;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP:
		eType = D3DPT_TRIANGLESTRIP;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pPacket->nPrimitiveCount > pNative->caps.MaxPrimitiveCount || ( pPacket->nVertexOffset % pPacket->nVertexStride ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	D3DVERTEXBUFFER_DESC vertexDesc = {};
	if ( FAILED( pPacket->pVertexBuffer->GetDesc( &vertexDesc ) ) || pPacket->nVertexOffset > vertexDesc.Size ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nVertexCapacity = ( vertexDesc.Size - pPacket->nVertexOffset ) / pPacket->nVertexStride;
	if ( pPacket->pIndexBuffer != nullptr ) {
		D3DINDEXBUFFER_DESC indexDesc = {};
		if ( FAILED( pPacket->pIndexBuffer->GetDesc( &indexDesc ) ) ||
		     ( indexDesc.Format != D3DFMT_INDEX16 && indexDesc.Format != D3DFMT_INDEX32 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint32_t nIndexCapacity = indexDesc.Size / ( indexDesc.Format == D3DFMT_INDEX32 ? 4U : 2U );
		const int64_t iFirstVertex = ( int64_t )pPacket->iBaseVertex + pPacket->nFirstVertex;
		if ( iFirstVertex < 0 || ( uint64_t )iFirstVertex + pPacket->nVertexCount > nVertexCapacity ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirGraphicsResult_t eIndexRange =
		    sirD3D9ValidateDrawRange( pPacket->nTopology, pPacket->nPrimitiveCount, pPacket->nFirstIndex, nIndexCapacity );
		if ( eIndexRange != SIR_GRAPHICS_SUCCESS ) {
			return eIndexRange;
		}
	} else {
		const sirGraphicsResult_t eVertexRange =
		    sirD3D9ValidateDrawRange( pPacket->nTopology, pPacket->nPrimitiveCount, pPacket->nFirstVertex, nVertexCapacity );
		if ( eVertexRange != SIR_GRAPHICS_SUCCESS ) {
			return eVertexRange;
		}
	}
	if ( pPacket->nSamplerCount >
	         ( pPacket->ppTextures != nullptr ? pPacket->nTextureCount : ( pPacket->pTexture != nullptr ? 1U : 0U ) ) ||
	     ( pPacket->nSamplerCount != 0 && pPacket->pSamplers == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pPacket->nSamplerCount; ++i ) {
		const sirGraphicsResult_t eSampler =
		    sirD3D9ValidateSampler( &pPacket->pSamplers[i], pNative->caps.MaxAnisotropy == 0 ? 1U : pNative->caps.MaxAnisotropy );
		if ( eSampler != SIR_GRAPHICS_SUCCESS ) {
			return eSampler;
		}
	}
	if ( pPacket->nTextureCount > pNative->caps.MaxSimultaneousTextures ||
	     ( pPacket->nTextureCount != 0 && pPacket->ppTextures == nullptr ) ||
	     ( pPacket->ppTextures == nullptr && pPacket->pTexture != nullptr && pNative->caps.MaxSimultaneousTextures == 0 ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pPacket->nShaderConstantBatchCount != 0 && pPacket->pShaderConstants == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pPacket->pVertexShader != nullptr && pPacket->pVertexDeclaration == nullptr ) ||
	     ( pPacket->pVertexShader == nullptr && pPacket->nFVF == 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pPacket->nShaderConstantBatchCount; ++i ) {
		const sirD3D9ShaderConstants_t *pBatch = &pPacket->pShaderConstants[i];
		const uint32_t nProfile = pBatch->nStage == SIR_GRAPHICS_STAGE_VERTEX     ? pNative->caps.VertexShaderVersion & 0xffffU
		                          : pBatch->nStage == SIR_GRAPHICS_STAGE_FRAGMENT ? pNative->caps.PixelShaderVersion & 0xffffU
		                                                                          : 0;
		if ( ( pBatch->nStage == SIR_GRAPHICS_STAGE_VERTEX && pPacket->pVertexShader == nullptr ) ||
		     ( pBatch->nStage == SIR_GRAPHICS_STAGE_FRAGMENT && pPacket->pPixelShader == nullptr ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirGraphicsResult_t eBatch = sirD3D9ValidateShaderConstants( pBatch, nProfile, pNative->caps.MaxVertexShaderConst );
		if ( eBatch != SIR_GRAPHICS_SUCCESS ) {
			return eBatch;
		}
	}
	for ( uint32_t i = 0; i < pPacket->nShaderConstantBatchCount; ++i ) {
		const sirGraphicsResult_t eBatch = sirD3D9NativeSetShaderConstants( pNative, &pPacket->pShaderConstants[i] );
		if ( eBatch != SIR_GRAPHICS_SUCCESS ) {
			return eBatch;
		}
	}
	HRESULT hResult = pNative->pDevice->SetStreamSource( 0, pPacket->pVertexBuffer, pPacket->nVertexOffset, pPacket->nVertexStride );
	if ( SUCCEEDED( hResult ) ) {
		hResult = pPacket->pVertexShader == nullptr ? pNative->pDevice->SetFVF( pPacket->nFVF )
		                                            : pNative->pDevice->SetVertexDeclaration( pPacket->pVertexDeclaration );
	}
	if ( SUCCEEDED( hResult ) ) {
		hResult = pNative->pDevice->SetVertexShader( pPacket->pVertexShader );
	}
	if ( SUCCEEDED( hResult ) ) {
		hResult = pNative->pDevice->SetPixelShader( pPacket->pPixelShader );
	}
	const uint32_t nTextures = pPacket->ppTextures != nullptr ? pPacket->nTextureCount : ( pPacket->pTexture != nullptr ? 1U : 0U );
	for ( uint32_t i = 0; i < pPacket->nSamplerCount && SUCCEEDED( hResult ); ++i ) {
		const sirGraphicsResult_t eSampler = sirD3D9NativeApplySampler( pNative, i, &pPacket->pSamplers[i] );
		if ( eSampler != SIR_GRAPHICS_SUCCESS ) {
			return eSampler;
		}
	}
	for ( uint32_t i = 0; i < nTextures && SUCCEEDED( hResult ); ++i ) {
		hResult = pNative->pDevice->SetTexture( i, pPacket->ppTextures != nullptr ? pPacket->ppTextures[i] : pPacket->pTexture );
	}
	for ( uint32_t i = nTextures; i < pNative->nActiveTextureBindings && SUCCEEDED( hResult ); ++i ) {
		hResult = pNative->pDevice->SetTexture( i, nullptr );
	}
	if ( SUCCEEDED( hResult ) && pPacket->pIndexBuffer != nullptr ) {
		hResult = pNative->pDevice->SetIndices( pPacket->pIndexBuffer );
	}
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	pNative->nActiveTextureBindings = nTextures;
	const bool bOpenedHere = !pNative->bSceneActive;
	if ( bOpenedHere ) {
		hResult = pNative->pDevice->BeginScene();
	} else {
		hResult = S_OK;
	}
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	pNative->bSceneActive = true;
	if ( pPacket->pIndexBuffer != nullptr ) {
		hResult = pNative->pDevice->DrawIndexedPrimitive( eType, pPacket->iBaseVertex, pPacket->nFirstVertex, pPacket->nVertexCount,
		                                                  pPacket->nFirstIndex, pPacket->nPrimitiveCount );
	} else {
		hResult = pNative->pDevice->DrawPrimitive( eType, pPacket->nFirstVertex, pPacket->nPrimitiveCount );
	}
	if ( !bOpenedHere ) {
		return sirD3D9Error( hResult );
	}
	const HRESULT hEnd = pNative->pDevice->EndScene();
	pNative->bSceneActive = false;
	return sirD3D9Error( FAILED( hResult ) ? hResult : hEnd );
}
#endif

#ifdef _WIN32
sirGraphicsResult_t sirD3D9NativeConfigureResidentBudget( sirD3D9Native_t *pNative, uint64_t nBytes ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || nBytes == 0 || pNative->resources.nUsedBytes != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pNative->resources.nBudgetBytes = nBytes;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9NativeCreateTrackedBufferDesc( sirD3D9Native_t *pNative, const sirGraphicsBufferDesc_t *pDesc, bool bIndex32,
                                                          sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pNative == nullptr || pHandle == nullptr || pNative->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eValidation = sirD3D9ValidateBuffer( pDesc );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	const sirGraphicsResult_t eBudget = sirD3D9ResourcesCanAllocate( &pNative->resources, pDesc->nSizeBytes );
	if ( eBudget != SIR_GRAPHICS_SUCCESS ) {
		return eBudget;
	}
	IDirect3DResource9 *pResource = nullptr;
	const sirGraphicsResult_t eCreate = sirD3D9NativeCreateBufferDesc( pNative, pDesc, bIndex32, &pResource );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	const sirGraphicsResult_t eInsert =
	    sirD3D9ResourcesInsert( &pNative->resources, SIR_GRAPHICS_HANDLE_BUFFER, pDesc->nSizeBytes, pResource, pHandle );
	if ( eInsert != SIR_GRAPHICS_SUCCESS ) {
		pResource->Release();
	}
	return eInsert;
}

sirGraphicsResult_t sirD3D9NativeCreateTrackedBuffer( sirD3D9Native_t *pNative, uint32_t nBytes, bool bIndex,
                                                      sirGraphicsHandle_t *pHandle ) noexcept
{
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = nBytes;
	desc.nAlignmentBytes = bIndex ? 2 : 4;
	desc.nUsage = bIndex ? SIR_GRAPHICS_USAGE_INDEX : SIR_GRAPHICS_USAGE_VERTEX;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	return sirD3D9NativeCreateTrackedBufferDesc( pNative, &desc, false, pHandle );
}

sirGraphicsResult_t sirD3D9NativeCreateTrackedTextureDesc( sirD3D9Native_t *pNative, const sirGraphicsTextureDesc_t *pDesc,
                                                           sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pNative == nullptr || pHandle == nullptr || pNative->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nEstimatedBytes = 0;
	const sirGraphicsResult_t eSize = sirD3D9EstimateTextureBytes( pDesc, &nEstimatedBytes );
	if ( eSize != SIR_GRAPHICS_SUCCESS ) {
		return eSize;
	}
	const sirGraphicsResult_t eBudget = sirD3D9ResourcesCanAllocate( &pNative->resources, nEstimatedBytes );
	if ( eBudget != SIR_GRAPHICS_SUCCESS ) {
		return eBudget;
	}
	IDirect3DBaseTexture9 *pTexture = nullptr;
	const sirGraphicsResult_t eCreate = sirD3D9NativeCreateTextureResourceDesc( pNative, pDesc, &pTexture );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	const sirGraphicsResult_t eInsert =
	    sirD3D9ResourcesInsert( &pNative->resources, SIR_GRAPHICS_HANDLE_TEXTURE, nEstimatedBytes, pTexture, pHandle );
	if ( eInsert != SIR_GRAPHICS_SUCCESS ) {
		pTexture->Release();
	}
	return eInsert;
}

sirGraphicsResult_t sirD3D9NativeCreateTrackedTexture( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight,
                                                       sirGraphicsHandle_t *pHandle ) noexcept
{
	sirGraphicsTextureDesc_t desc = {};
	desc.nWidth = nWidth;
	desc.nHeight = nHeight;
	desc.nDepth = 1;
	desc.nArrayLayers = 1;
	desc.nMipLevels = 1;
	desc.nSampleCount = 1;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	return sirD3D9NativeCreateTrackedTextureDesc( pNative, &desc, pHandle );
}

sirGraphicsResult_t sirD3D9NativeLookupResource( sirD3D9Native_t *pNative, sirGraphicsHandle_t hHandle, void **ppNative ) noexcept
{
	if ( pNative == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return sirD3D9ResourcesLookup( &pNative->resources, hHandle, ppNative );
}

sirGraphicsResult_t sirD3D9NativeRetireResource( sirD3D9Native_t *pNative, sirGraphicsHandle_t hHandle, uint64_t nCompletionValue ) noexcept
{
	if ( pNative == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pNative->bSceneActive || pNative->vertexRing.nPendingWrites != 0 || pNative->indexRing.nPendingWrites != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( nCompletionValue < pNative->nLastSubmittedSerial ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nCompletionValue > pNative->nLastSubmittedSerial ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pNative->pCompletionQuery == nullptr && nCompletionValue > pNative->nLastCompletedSerial ) {
		return SIR_GRAPHICS_BUSY;
	}
	return sirD3D9ResourcesRetire( &pNative->resources, hHandle, nCompletionValue );
}

void sirD3D9NativeCollectResources( sirD3D9Native_t *pNative, uint64_t nCompletedValue ) noexcept
{
	if ( pNative == nullptr ) {
		return;
	}
	while ( true ) {
		IUnknown *pResource = ( IUnknown * )sirD3D9ResourcesCollect( &pNative->resources, nCompletedValue );
		if ( pResource == nullptr ) {
			break;
		}
		pResource->Release();
	}
}
#endif

#ifdef _WIN32
static uint32_t sirD3D9NativeTextureOp( uint32_t nOp ) noexcept
{
	switch ( nOp ) {
	case SIR_GRAPHICS_FIXED_OP_DISABLE:
		return D3DTOP_DISABLE;
	case SIR_GRAPHICS_FIXED_OP_SELECT:
		return D3DTOP_SELECTARG1;
	case SIR_GRAPHICS_FIXED_OP_MODULATE:
		return D3DTOP_MODULATE;
	case SIR_GRAPHICS_FIXED_OP_ADD:
		return D3DTOP_ADD;
	case SIR_GRAPHICS_FIXED_OP_SUBTRACT:
		return D3DTOP_SUBTRACT;
	case SIR_GRAPHICS_FIXED_OP_INTERPOLATE:
		return D3DTOP_LERP;
	case SIR_GRAPHICS_FIXED_OP_DOT3:
		return D3DTOP_DOTPRODUCT3;
	default:
		return UINT32_MAX;
	}
}

static uint32_t sirD3D9NativeTextureArg( uint32_t nArg ) noexcept
{
	switch ( nArg ) {
	case SIR_GRAPHICS_FIXED_ARG_TEXTURE:
		return D3DTA_TEXTURE;
	case SIR_GRAPHICS_FIXED_ARG_CURRENT:
		return D3DTA_CURRENT;
	case SIR_GRAPHICS_FIXED_ARG_DIFFUSE:
		return D3DTA_DIFFUSE;
	case SIR_GRAPHICS_FIXED_ARG_CONSTANT:
		return D3DTA_CONSTANT;
	default:
		return UINT32_MAX;
	}
}

static uint32_t sirD3D9NativeCompare( uint32_t nCompare ) noexcept
{
	switch ( nCompare ) {
	case SIR_GRAPHICS_COMPARE_NEVER:
		return D3DCMP_NEVER;
	case SIR_GRAPHICS_COMPARE_LESS:
		return D3DCMP_LESS;
	case SIR_GRAPHICS_COMPARE_EQUAL:
		return D3DCMP_EQUAL;
	case SIR_GRAPHICS_COMPARE_LESS_EQUAL:
		return D3DCMP_LESSEQUAL;
	case SIR_GRAPHICS_COMPARE_GREATER:
		return D3DCMP_GREATER;
	case SIR_GRAPHICS_COMPARE_NOT_EQUAL:
		return D3DCMP_NOTEQUAL;
	case SIR_GRAPHICS_COMPARE_GREATER_EQUAL:
		return D3DCMP_GREATEREQUAL;
	case SIR_GRAPHICS_COMPARE_ALWAYS:
		return D3DCMP_ALWAYS;
	default:
		return UINT32_MAX;
	}
}

static D3DCOLOR sirD3D9NativeColor( const float *pColor ) noexcept
{
	uint32_t szValues[4] = {};
	for ( uint32_t i = 0; i < 4; ++i ) {
		const float fValue = pColor[i];
		szValues[i] = fValue <= 0.0f ? 0 : fValue >= 1.0f ? 255 : ( uint32_t )( fValue * 255.0f );
	}
	return D3DCOLOR_ARGB( szValues[3], szValues[0], szValues[1], szValues[2] );
}

sirGraphicsResult_t sirD3D9NativeApplyFixedRecipe( sirD3D9Native_t *pNative, const sirGraphicsABIFixedRecipe_t *pRecipe ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pRecipe == nullptr ||
	     pRecipe->nSize < sizeof( sirGraphicsABIFixedRecipe_t ) || pRecipe->nReserved != 0 ||
	     ( pRecipe->nTextureStageCount != 0 && pRecipe->pStages == nullptr ) ||
	     ( pRecipe->nLightCount != 0 && pRecipe->pLights == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pRecipe->nTextureStageCount > pNative->caps.MaxTextureBlendStages || pRecipe->nLightCount > pNative->caps.MaxActiveLights ||
	     ( pRecipe->nRequiredFeatures & ~( SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsResult_t ePreflight = sirD3D9ValidateFixedRecipe(
	    pRecipe, pNative->caps.MaxTextureBlendStages, pNative->caps.MaxSimultaneousTextures, pNative->caps.MaxActiveLights );
	if ( ePreflight != SIR_GRAPHICS_SUCCESS ) {
		return ePreflight;
	}
	for ( uint32_t i = 0; i < pRecipe->nTextureStageCount; ++i ) {
		const sirGraphicsABIFixedStage_t *pStage = &pRecipe->pStages[i];
		if ( pStage->nTextureUnit >= pNative->caps.MaxSimultaneousTextures ||
		     sirD3D9NativeTextureOp( pStage->nColorOperation ) == UINT32_MAX ||
		     sirD3D9NativeTextureOp( pStage->nAlphaOperation ) == UINT32_MAX ||
		     sirD3D9NativeTextureArg( pStage->nColorArgument0 ) == UINT32_MAX ||
		     sirD3D9NativeTextureArg( pStage->nColorArgument1 ) == UINT32_MAX ||
		     sirD3D9NativeTextureArg( pStage->nAlphaArgument0 ) == UINT32_MAX ||
		     sirD3D9NativeTextureArg( pStage->nAlphaArgument1 ) == UINT32_MAX ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	// Validate hardware texture combiner support before touching any device state.
	for ( uint32_t i = 0; i < pRecipe->nTextureStageCount; ++i ) {
		const sirGraphicsABIFixedStage_t *pStage = &pRecipe->pStages[i];
		const uint32_t aOps[] = { pStage->nColorOperation, pStage->nAlphaOperation };
		for ( uint32_t j = 0; j < 2; ++j ) {
			DWORD nCapability = 0;
			switch ( aOps[j] ) {
			case SIR_GRAPHICS_FIXED_OP_DISABLE:
				nCapability = D3DTEXOPCAPS_DISABLE;
				break;
			case SIR_GRAPHICS_FIXED_OP_SELECT:
				nCapability = D3DTEXOPCAPS_SELECTARG1;
				break;
			case SIR_GRAPHICS_FIXED_OP_MODULATE:
				nCapability = D3DTEXOPCAPS_MODULATE;
				break;
			case SIR_GRAPHICS_FIXED_OP_ADD:
				nCapability = D3DTEXOPCAPS_ADD;
				break;
			case SIR_GRAPHICS_FIXED_OP_SUBTRACT:
				nCapability = D3DTEXOPCAPS_SUBTRACT;
				break;
			case SIR_GRAPHICS_FIXED_OP_DOT3:
				nCapability = D3DTEXOPCAPS_DOTPRODUCT3;
				break;
			default:
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			if ( ( pNative->caps.TextureOpCaps & nCapability ) == 0 ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
		}
	}
	const uint32_t nAlphaCompare = sirD3D9NativeCompare( pRecipe->nAlphaCompare );
	if ( nAlphaCompare == UINT32_MAX || pRecipe->fAlphaReference < 0.0f || pRecipe->fAlphaReference > 1.0f ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	DWORD nFogMode = D3DFOG_NONE;
	switch ( pRecipe->nFogMode ) {
	case SIR_GRAPHICS_FIXED_FOG_NONE:
		nFogMode = D3DFOG_NONE;
		break;
	case SIR_GRAPHICS_FIXED_FOG_LINEAR:
		nFogMode = D3DFOG_LINEAR;
		break;
	case SIR_GRAPHICS_FIXED_FOG_EXP:
		nFogMode = D3DFOG_EXP;
		break;
	case SIR_GRAPHICS_FIXED_FOG_EXP2:
		nFogMode = D3DFOG_EXP2;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nFogMode != D3DFOG_NONE && ( pNative->caps.RasterCaps & ( D3DPRASTERCAPS_FOGTABLE | D3DPRASTERCAPS_FOGVERTEX ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bTableFog = ( pNative->caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE ) != 0;
	DWORD nFogStartBits = 0;
	DWORD nFogEndBits = 0;
	DWORD nFogDensityBits = 0;
	memcpy( &nFogStartBits, &pRecipe->fFogStart, sizeof( nFogStartBits ) );
	memcpy( &nFogEndBits, &pRecipe->fFogEnd, sizeof( nFogEndBits ) );
	memcpy( &nFogDensityBits, &pRecipe->fFogDensity, sizeof( nFogDensityBits ) );
	HRESULT hResult = pNative->pDevice->SetVertexShader( nullptr );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetPixelShader( nullptr );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetTransform( D3DTS_WORLD, ( const D3DMATRIX * )pRecipe->szWorld );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetTransform( D3DTS_VIEW, ( const D3DMATRIX * )pRecipe->szView );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetTransform( D3DTS_PROJECTION, ( const D3DMATRIX * )pRecipe->szProjection );
	if ( FAILED( hResult ) )
		return sirD3D9Error( hResult );
	D3DMATERIAL9 material = {};
	memcpy( &material.Ambient, pRecipe->material.szAmbient, sizeof( material.Ambient ) );
	memcpy( &material.Diffuse, pRecipe->material.szDiffuse, sizeof( material.Diffuse ) );
	memcpy( &material.Specular, pRecipe->material.szSpecular, sizeof( material.Specular ) );
	memcpy( &material.Emissive, pRecipe->material.szEmissive, sizeof( material.Emissive ) );
	material.Power = pRecipe->material.fPower;
	hResult = pNative->pDevice->SetMaterial( &material );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_LIGHTING, pRecipe->nLightCount == 0 ? FALSE : TRUE );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_ALPHATESTENABLE, pRecipe->nAlphaCompare != SIR_GRAPHICS_COMPARE_ALWAYS );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_ALPHAFUNC, nAlphaCompare );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_ALPHAREF, ( DWORD )( pRecipe->fAlphaReference * 255.0f ) );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGENABLE, nFogMode != D3DFOG_NONE );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGTABLEMODE, bTableFog ? nFogMode : D3DFOG_NONE );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGVERTEXMODE, bTableFog ? D3DFOG_NONE : nFogMode );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGSTART, nFogStartBits );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGEND, nFogEndBits );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGDENSITY, nFogDensityBits );
	if ( SUCCEEDED( hResult ) )
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGCOLOR, sirD3D9NativeColor( pRecipe->szFogColor ) );
	if ( FAILED( hResult ) )
		return sirD3D9Error( hResult );
	for ( uint32_t i = 0; i < pRecipe->nTextureStageCount; ++i ) {
		const sirGraphicsABIFixedStage_t *pStage = &pRecipe->pStages[i];
		const DWORD nStage = pStage->nTextureUnit;
		hResult = pNative->pDevice->SetTextureStageState( nStage, D3DTSS_COLOROP, sirD3D9NativeTextureOp( pStage->nColorOperation ) );
		if ( SUCCEEDED( hResult ) )
			hResult = pNative->pDevice->SetTextureStageState( nStage, D3DTSS_ALPHAOP, sirD3D9NativeTextureOp( pStage->nAlphaOperation ) );
		if ( SUCCEEDED( hResult ) )
			hResult =
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_COLORARG2, sirD3D9NativeTextureArg( pStage->nColorArgument1 ) );
		if ( SUCCEEDED( hResult ) )
			hResult =
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_COLORARG1, sirD3D9NativeTextureArg( pStage->nColorArgument0 ) );
		if ( SUCCEEDED( hResult ) )
			hResult =
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_ALPHAARG2, sirD3D9NativeTextureArg( pStage->nAlphaArgument1 ) );
		if ( SUCCEEDED( hResult ) )
			hResult =
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_ALPHAARG1, sirD3D9NativeTextureArg( pStage->nAlphaArgument0 ) );
		if ( SUCCEEDED( hResult ) )
			hResult = pNative->pDevice->SetTextureStageState( nStage, D3DTSS_CONSTANT, sirD3D9NativeColor( pStage->szConstantColor ) );
		if ( SUCCEEDED( hResult ) )
			hResult = pNative->pDevice->SetTextureStageState( nStage, D3DTSS_TEXCOORDINDEX, pStage->nCoordinateSet );
		if ( FAILED( hResult ) )
			return sirD3D9Error( hResult );
	}
	for ( uint32_t i = 0; i < pRecipe->nLightCount; ++i ) {
		const sirGraphicsABIFixedLight_t *pSource = &pRecipe->pLights[i];
		D3DLIGHT9 light = {};
		light.Type = pSource->szPosition[3] == 0 ? D3DLIGHT_DIRECTIONAL : D3DLIGHT_POINT;
		memcpy( &light.Position, pSource->szPosition, sizeof( light.Position ) );
		memcpy( &light.Direction, pSource->szDirection, sizeof( light.Direction ) );
		memcpy( &light.Diffuse, pSource->szDiffuse, sizeof( light.Diffuse ) );
		memcpy( &light.Specular, pSource->szSpecular, sizeof( light.Specular ) );
		light.Range = pSource->fRange;
		light.Attenuation0 = pSource->fConstantAttenuation;
		light.Attenuation1 = pSource->fLinearAttenuation;
		light.Attenuation2 = pSource->fQuadraticAttenuation;
		hResult = pNative->pDevice->SetLight( i, &light );
		if ( SUCCEEDED( hResult ) )
			hResult = pNative->pDevice->LightEnable( i, TRUE );
		if ( FAILED( hResult ) )
			return sirD3D9Error( hResult );
	}
	// Explicitly terminate the fixed-function texture combiner chain.
	if ( pRecipe->nTextureStageCount == 0 || pRecipe->nTextureStageCount < pNative->nActiveFixedStages ) {
		const HRESULT hDisable = pNative->pDevice->SetTextureStageState( pRecipe->nTextureStageCount, D3DTSS_COLOROP, D3DTOP_DISABLE );
		if ( FAILED( hDisable ) ) {
			return sirD3D9Error( hDisable );
		}
	}
	for ( uint32_t i = pRecipe->nLightCount; i < pNative->nActiveFixedLights; ++i ) {
		const HRESULT hDisable = pNative->pDevice->LightEnable( i, FALSE );
		if ( FAILED( hDisable ) ) {
			return sirD3D9Error( hDisable );
		}
	}
	pNative->nActiveFixedStages = pRecipe->nTextureStageCount;
	pNative->nActiveFixedLights = pRecipe->nLightCount;
	return SIR_GRAPHICS_SUCCESS;
}
#endif

#ifdef _WIN32
sirGraphicsResult_t sirD3D9NativeConfigureUploadRings( sirD3D9Native_t *pNative, uint32_t nVertexCapacity,
                                                       uint32_t nIndexCapacity ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pNative->pCompletionQuery == nullptr || nVertexCapacity == 0 ||
	     nIndexCapacity == 0 || pNative->vertexRing.nCapacity != 0 || pNative->indexRing.nCapacity != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsHandle_t hVertex = {};
	sirGraphicsHandle_t hIndex = {};
	sirGraphicsResult_t eResult = sirD3D9NativeCreateTrackedBuffer( pNative, nVertexCapacity, false, &hVertex );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = sirD3D9NativeCreateTrackedBuffer( pNative, nIndexCapacity, true, &hIndex );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		// No work was submitted, so this resource can be released immediately.
		sirD3D9NativeRetireResource( pNative, hVertex, 0 );
		sirD3D9NativeCollectResources( pNative, 0 );
		return eResult;
	}
	sirD3D9RingInitialize( &pNative->vertexRing, nVertexCapacity );
	sirD3D9RingInitialize( &pNative->indexRing, nIndexCapacity );
	pNative->hUploadVertex = hVertex;
	pNative->hUploadIndex = hIndex;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9NativeUpload( sirD3D9Native_t *pNative, const sirD3D9UploadRequest_t *pRequest,
                                         sirD3D9UploadRange_t *pRange ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pRequest == nullptr || pRange == nullptr || pRequest->pData == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D9UploadRing_t *pRing = pRequest->bIndex ? &pNative->indexRing : &pNative->vertexRing;
	const sirGraphicsHandle_t hBuffer = pRequest->bIndex ? pNative->hUploadIndex : pNative->hUploadVertex;
	sirD3D9UploadRing_t previous = *pRing;
	sirGraphicsResult_t eResult = sirD3D9RingReserve( pRing, pRequest->nBytes, pRequest->nAlignment, pRange );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	void *pResource = nullptr;
	eResult = sirD3D9NativeLookupResource( pNative, hBuffer, &pResource );
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		sirD3D9BufferUpdate_t update = {};
		update.pBuffer = ( IDirect3DResource9 * )pResource;
		update.pData = pRequest->pData;
		update.nOffset = pRange->nOffset;
		update.nBytes = pRange->nBytes;
		update.nLockFlags = pRange->nLockFlags;
		eResult = sirD3D9NativeUploadBuffer( pNative, &update );
	}
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		*pRing = previous;
	}
	return eResult;
}
#endif
