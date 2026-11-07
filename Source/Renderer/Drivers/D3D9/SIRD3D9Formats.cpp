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
#include <Renderer/Drivers/D3D9/SIRD3D9Formats.h>
#ifdef _WIN32
sirGraphicsResult_t sirD3D9NativeMapFormat( sirGraphicsFormat_t eFormat, D3DFORMAT *pNativeFormat ) noexcept
{
	if ( pNativeFormat == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		*pNativeFormat = D3DFMT_A8R8G8B8;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		*pNativeFormat = D3DFMT_A8B8G8R8;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		*pNativeFormat = D3DFMT_A16B16G16R16F;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		*pNativeFormat = D3DFMT_R16F;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
		*pNativeFormat = D3DFMT_R32F;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		*pNativeFormat = D3DFMT_G32R32F;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		*pNativeFormat = D3DFMT_A32B32G32R32F;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
		*pNativeFormat = D3DFMT_D16;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		*pNativeFormat = D3DFMT_D24S8;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
		*pNativeFormat = D3DFMT_DXT1;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
		*pNativeFormat = D3DFMT_DXT5;
		return SIR_GRAPHICS_SUCCESS;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
}

sirGraphicsResult_t sirD3D9NativeQueryFormat( sirD3D9Native_t *pNative, sirGraphicsABIFormat_t *pFormat ) noexcept
{
	if ( pNative == nullptr || pNative->pRuntime == nullptr || pNative->pDevice == nullptr || pFormat == nullptr ||
	     pFormat->nSize < sizeof( sirGraphicsABIFormat_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	D3DFORMAT eNativeFormat = D3DFMT_UNKNOWN;
	const sirGraphicsResult_t eMap = sirD3D9NativeMapFormat( ( sirGraphicsFormat_t )pFormat->nFormat, &eNativeFormat );
	if ( eMap != SIR_GRAPHICS_SUCCESS ) {
		return eMap;
	}
	D3DDISPLAYMODE mode = {};
	if ( FAILED( pNative->pRuntime->GetAdapterDisplayMode( pNative->nAdapterIndex, &mode ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	const bool bDepth = eNativeFormat == D3DFMT_D16 || eNativeFormat == D3DFMT_D24S8;
	const bool bCompressed = eNativeFormat == D3DFMT_DXT1 || eNativeFormat == D3DFMT_DXT5;
	uint32_t nFeatures = 0;
	if ( !bDepth && SUCCEEDED( pNative->pRuntime->CheckDeviceFormat( pNative->nAdapterIndex, D3DDEVTYPE_HAL, mode.Format, 0,
	                                                                 D3DRTYPE_TEXTURE, eNativeFormat ) ) ) {
		nFeatures |= SIR_GRAPHICS_ABI_FORMAT_SAMPLED | SIR_GRAPHICS_ABI_FORMAT_TRANSFER_DST;
		if ( !bCompressed ) {
			nFeatures |= SIR_GRAPHICS_ABI_FORMAT_LINEAR_FILTER;
		}
	}
	const DWORD nUsage = bDepth ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET;
	if ( !bCompressed && SUCCEEDED( pNative->pRuntime->CheckDeviceFormat( pNative->nAdapterIndex, D3DDEVTYPE_HAL, mode.Format, nUsage,
	                                                                      D3DRTYPE_SURFACE, eNativeFormat ) ) ) {
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
#endif
