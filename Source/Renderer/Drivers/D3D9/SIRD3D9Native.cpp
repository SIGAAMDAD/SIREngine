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
#ifdef _WIN32
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
	if ( sirD3D9ResourcesInitialize( &pNative->resources, pNative->szResourceSlots, 128, 256ULL * 1024 * 1024 ) != SIR_GRAPHICS_SUCCESS ) {
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

sirGraphicsResult_t sirD3D9NativeDrawTriangle( sirD3D9Native_t *pNative ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr ) {
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
	if ( pNative->vertexRing.nPendingWrites != 0 || pNative->indexRing.nPendingWrites != 0 ) {
		if ( pNative->pCompletionQuery == nullptr || pNative->nLastSubmittedSerial == UINT64_MAX ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		const uint64_t nSerial = pNative->nLastSubmittedSerial + 1;
		if ( pNative->vertexRing.nPendingWrites != 0 ) {
			sirD3D9RingSubmit( &pNative->vertexRing, nSerial );
		}
		if ( pNative->indexRing.nPendingWrites != 0 ) {
			sirD3D9RingSubmit( &pNative->indexRing, nSerial );
		}
		pNative->nLastSubmittedSerial = nSerial;
	}
	if ( pNative->pCompletionQuery != nullptr ) {
		const HRESULT hIssued = pNative->pCompletionQuery->Issue( D3DISSUE_END );
		if ( FAILED( hIssued ) ) {
			return sirD3D9Error( hIssued );
		}
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
	const HRESULT hResult = pNative->pCompletionQuery->GetData( nullptr, 0, D3DGETDATA_FLUSH );
	if ( hResult == S_FALSE ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	*pComplete = hResult == S_OK;
	if ( *pComplete ) {
		pNative->nLastCompletedSerial = pNative->nLastSubmittedSerial;
		if ( pNative->vertexRing.nCapacity != 0 ) {
			sirD3D9RingComplete( &pNative->vertexRing, pNative->vertexRing.nLatestSubmission );
		}
		if ( pNative->indexRing.nCapacity != 0 ) {
			sirD3D9RingComplete( &pNative->indexRing, pNative->indexRing.nLatestSubmission );
		}
	}
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeResize( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || nWidth == 0 || nHeight == 0 || pNative->bSceneActive ||
	     pNative->nDeviceGeneration == UINT32_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pNative->pCompletionQuery != nullptr ) {
		pNative->pCompletionQuery->Release();
		pNative->pCompletionQuery = nullptr;
	}
	// All DEFAULT-pool resources must be retired before standard D3D9 Reset.
	sirD3D9NativeReleaseTrackedResources( pNative );
	pNative->hUploadVertex = {};
	pNative->hUploadIndex = {};
	sirD3D9RingReset( &pNative->vertexRing );
	sirD3D9RingReset( &pNative->indexRing );
	pNative->nLastSubmittedSerial = 0;
	pNative->nLastCompletedSerial = 0;
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

sirGraphicsResult_t sirD3D9NativeCreateBuffer( sirD3D9Native_t *pNative, uint32_t nBytes, bool bIndex,
                                               IDirect3DResource9 **ppResource ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || ppResource == nullptr || nBytes == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppResource = nullptr;
	HRESULT hResult = E_FAIL;
	if ( bIndex ) {
		IDirect3DIndexBuffer9 *pBuffer = nullptr;
		hResult = pNative->pDevice->CreateIndexBuffer( nBytes, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
		                                               &pBuffer, nullptr );
		*ppResource = pBuffer;
	} else {
		IDirect3DVertexBuffer9 *pBuffer = nullptr;
		hResult =
		    pNative->pDevice->CreateVertexBuffer( nBytes, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &pBuffer, nullptr );
		*ppResource = pBuffer;
	}
	return sirD3D9Error( hResult );
}

sirGraphicsResult_t sirD3D9NativeCreateTexture( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight,
                                                IDirect3DTexture9 **ppTexture ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || ppTexture == nullptr || nWidth == 0 || nHeight == 0 ||
	     nWidth > pNative->caps.MaxTextureWidth || nHeight > pNative->caps.MaxTextureHeight ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppTexture = nullptr;
	const HRESULT hResult = pNative->pDevice->CreateTexture( nWidth, nHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, ppTexture, nullptr );
	return sirD3D9Error( hResult );
}
#endif

#ifdef _WIN32
sirGraphicsResult_t sirD3D9NativeUploadBuffer( sirD3D9Native_t *pNative, const sirD3D9BufferUpdate_t *pUpdate ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pUpdate == nullptr || pUpdate->pBuffer == nullptr ||
	     pUpdate->pData == nullptr || pUpdate->nBytes == 0 ||
	     ( pUpdate->nLockFlags != SIR_D3D9_RING_DISCARD && pUpdate->nLockFlags != SIR_D3D9_RING_NOOVERWRITE ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const DWORD nNativeFlags = pUpdate->nLockFlags == SIR_D3D9_RING_DISCARD ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
	void *pDestination = nullptr;
	HRESULT hResult = E_FAIL;
	if ( pUpdate->pBuffer->GetType() == D3DRTYPE_VERTEXBUFFER ) {
		IDirect3DVertexBuffer9 *pBuffer = nullptr;
		if ( FAILED( pUpdate->pBuffer->QueryInterface( IID_IDirect3DVertexBuffer9, ( void ** )&pBuffer ) ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		D3DVERTEXBUFFER_DESC desc = {};
		hResult = pBuffer->GetDesc( &desc );
		if ( SUCCEEDED( hResult ) && pUpdate->nOffset <= desc.Size && pUpdate->nBytes <= desc.Size - pUpdate->nOffset ) {
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
		if ( SUCCEEDED( hResult ) && pUpdate->nOffset <= desc.Size && pUpdate->nBytes <= desc.Size - pUpdate->nOffset ) {
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

sirGraphicsResult_t sirD3D9NativeCreateShader( sirD3D9Native_t *pNative, const sirGraphicsABIShaderCode_t *pShader, uint32_t nStage,
                                               IUnknown **ppShader ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pShader == nullptr || ppShader == nullptr ||
	     pShader->nSize < sizeof( sirGraphicsABIShaderCode_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppShader = nullptr;
	if ( pShader->nFormat != SIR_GRAPHICS_SHADER_DXBC ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nProfile = nStage == SIR_GRAPHICS_STAGE_VERTEX     ? pNative->caps.VertexShaderVersion & 0xffffU
	                          : nStage == SIR_GRAPHICS_STAGE_FRAGMENT ? pNative->caps.PixelShaderVersion & 0xffffU
	                                                                  : 0;
	const sirGraphicsResult_t eValidation = sirD3D9ValidateShaderTokens( pShader->pBytes, pShader->nByteCount, nStage, nProfile );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	// D3D9 token streams are DWORD-aligned; do not reinterpret arbitrary borrowed byte buffers.
	if ( ( ( uintptr_t )pShader->pBytes & 3 ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	HRESULT hResult = E_FAIL;
	if ( nStage == SIR_GRAPHICS_STAGE_VERTEX ) {
		IDirect3DVertexShader9 *pNativeShader = nullptr;
		hResult = pNative->pDevice->CreateVertexShader( ( const DWORD * )pShader->pBytes, &pNativeShader );
		*ppShader = pNativeShader;
	} else {
		IDirect3DPixelShader9 *pNativeShader = nullptr;
		hResult = pNative->pDevice->CreatePixelShader( ( const DWORD * )pShader->pBytes, &pNativeShader );
		*ppShader = pNativeShader;
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
	if ( SUCCEEDED( hResult ) ) {
		hResult = pNative->pDevice->SetTexture( 0, pPacket->pTexture );
	}
	if ( SUCCEEDED( hResult ) && pPacket->pIndexBuffer != nullptr ) {
		hResult = pNative->pDevice->SetIndices( pPacket->pIndexBuffer );
	}
	if ( FAILED( hResult ) ) {
		return sirD3D9Error( hResult );
	}
	hResult = pNative->pDevice->BeginScene();
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

sirGraphicsResult_t sirD3D9NativeCreateTrackedBuffer( sirD3D9Native_t *pNative, uint32_t nBytes, bool bIndex,
                                                      sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pNative == nullptr || pHandle == nullptr || pNative->pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	IDirect3DResource9 *pResource = nullptr;
	const sirGraphicsResult_t eCreate = sirD3D9NativeCreateBuffer( pNative, nBytes, bIndex, &pResource );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	const sirGraphicsResult_t eInsert =
	    sirD3D9ResourcesInsert( &pNative->resources, SIR_GRAPHICS_HANDLE_BUFFER, nBytes, pResource, pHandle );
	if ( eInsert != SIR_GRAPHICS_SUCCESS ) {
		pResource->Release();
	}
	return eInsert;
}

sirGraphicsResult_t sirD3D9NativeCreateTrackedTexture( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight,
                                                       sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pNative == nullptr || pHandle == nullptr || pNative->pDevice == nullptr || nWidth == 0 ||
	     ( uint64_t )nWidth * nHeight > UINT64_MAX / 4 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	IDirect3DTexture9 *pTexture = nullptr;
	const sirGraphicsResult_t eCreate = sirD3D9NativeCreateTexture( pNative, nWidth, nHeight, &pTexture );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		return eCreate;
	}
	const sirGraphicsResult_t eInsert =
	    sirD3D9ResourcesInsert( &pNative->resources, SIR_GRAPHICS_HANDLE_TEXTURE, ( uint64_t )nWidth * nHeight * 4, pTexture, pHandle );
	if ( eInsert != SIR_GRAPHICS_SUCCESS ) {
		pTexture->Release();
	}
	return eInsert;
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
		hResult = pNative->pDevice->SetRenderState( D3DRS_FOGTABLEMODE, nFogMode );
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
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_COLORARG1, sirD3D9NativeTextureArg( pStage->nColorArgument1 ) );
		if ( SUCCEEDED( hResult ) )
			hResult =
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_COLORARG0, sirD3D9NativeTextureArg( pStage->nColorArgument0 ) );
		if ( SUCCEEDED( hResult ) )
			hResult =
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_ALPHAARG1, sirD3D9NativeTextureArg( pStage->nAlphaArgument1 ) );
		if ( SUCCEEDED( hResult ) )
			hResult =
			    pNative->pDevice->SetTextureStageState( nStage, D3DTSS_ALPHAARG0, sirD3D9NativeTextureArg( pStage->nAlphaArgument0 ) );
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
