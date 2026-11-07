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
#include <Renderer/Drivers/D3D9/SIRD3D9Targets.h>
#include <Renderer/Drivers/D3D9/SIRD3D9Formats.h>
#ifdef _WIN32
static sirGraphicsResult_t sirD3D9TargetError( HRESULT hResult ) noexcept
{
	if ( SUCCEEDED( hResult ) )
		return SIR_GRAPHICS_SUCCESS;
	if ( hResult == D3DERR_INVALIDCALL )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	if ( hResult == D3DERR_DEVICELOST || hResult == D3DERR_DEVICENOTRESET )
		return SIR_GRAPHICS_DEVICE_LOST;
	if ( hResult == D3DERR_OUTOFVIDEOMEMORY || hResult == E_OUTOFMEMORY )
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	return SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t sirD3D9NativeCreateTarget( sirD3D9Native_t *pNative, const sirD3D9TargetDesc_t *pDesc,
                                               IDirect3DSurface9 **ppSurface ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pNative->pRuntime == nullptr || pDesc == nullptr || ppSurface == nullptr ||
	     pDesc->nWidth == 0 || pDesc->nHeight == 0 || pDesc->nWidth > pNative->caps.MaxTextureWidth ||
	     pDesc->nHeight > pNative->caps.MaxTextureHeight ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppSurface = nullptr;
	D3DFORMAT eFormat = D3DFMT_UNKNOWN;
	const sirGraphicsResult_t eResult = sirD3D9NativeMapFormat( pDesc->eFormat, &eFormat );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( pDesc->nSamples != 1 && pDesc->nSamples != 2 && pDesc->nSamples != 4 && pDesc->nSamples != 8 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const D3DMULTISAMPLE_TYPE eSamples = pDesc->nSamples == 1 ? D3DMULTISAMPLE_NONE : ( D3DMULTISAMPLE_TYPE )pDesc->nSamples;
	DWORD nQuality = 0;
	if ( FAILED( pNative->pRuntime->CheckDeviceMultiSampleType( pNative->nAdapterIndex, D3DDEVTYPE_HAL, eFormat, TRUE, eSamples,
	                                                            &nQuality ) ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	HRESULT hResult = E_FAIL;
	if ( pDesc->bDepth ) {
		if ( eFormat != D3DFMT_D16 && eFormat != D3DFMT_D24S8 )
			return SIR_GRAPHICS_UNSUPPORTED;
		hResult =
		    pNative->pDevice->CreateDepthStencilSurface( pDesc->nWidth, pDesc->nHeight, eFormat, eSamples, 0, TRUE, ppSurface, nullptr );
	} else {
		if ( eFormat == D3DFMT_D16 || eFormat == D3DFMT_D24S8 || eFormat == D3DFMT_DXT1 || eFormat == D3DFMT_DXT5 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		hResult = pNative->pDevice->CreateRenderTarget( pDesc->nWidth, pDesc->nHeight, eFormat, eSamples, 0, FALSE, ppSurface, nullptr );
	}
	return sirD3D9TargetError( hResult );
}

sirGraphicsResult_t sirD3D9NativeSetTargets( sirD3D9Native_t *pNative, const sirD3D9TargetSet_t *pTargets ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pTargets == nullptr || pTargets->nColorCount == 0 ||
	     pTargets->nColorCount > 4 || pTargets->nColorCount > pNative->caps.NumSimultaneousRTs || pTargets->szColor[0] == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pTargets->nColorCount; ++i ) {
		if ( pTargets->szColor[i] == nullptr )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pNative->caps.NumSimultaneousRTs && i < 4; ++i ) {
		const HRESULT hResult = pNative->pDevice->SetRenderTarget( i, i < pTargets->nColorCount ? pTargets->szColor[i] : nullptr );
		if ( FAILED( hResult ) )
			return sirD3D9TargetError( hResult );
	}
	return sirD3D9TargetError( pNative->pDevice->SetDepthStencilSurface( pTargets->pDepth ) );
}

sirGraphicsResult_t sirD3D9NativeCopySurface( sirD3D9Native_t *pNative, const sirD3D9SurfaceCopy_t *pCopy ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pCopy == nullptr || pCopy->pSource == nullptr ||
	     pCopy->pDestination == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const HRESULT hResult = pCopy->bStretch
	                            ? pNative->pDevice->StretchRect( pCopy->pSource, nullptr, pCopy->pDestination, nullptr, D3DTEXF_NONE )
	                            : pNative->pDevice->UpdateSurface( pCopy->pSource, nullptr, pCopy->pDestination, nullptr );
	return sirD3D9TargetError( hResult );
}

sirGraphicsResult_t sirD3D9NativeUpdateTexture( sirD3D9Native_t *pNative, IDirect3DBaseTexture9 *pSource,
                                                IDirect3DBaseTexture9 *pDestination ) noexcept
{
	if ( pNative == nullptr || pNative->pDevice == nullptr || pSource == nullptr || pDestination == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return sirD3D9TargetError( pNative->pDevice->UpdateTexture( pSource, pDestination ) );
}
#endif
