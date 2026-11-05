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

#include <Renderer/Drivers/D3D11/SIRD3D11Formats.h>

#include <d3d11.h>

static DXGI_FORMAT sirD3D11NativeFormat( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case SIR_GRAPHICS_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM;
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM;
	case SIR_GRAPHICS_FORMAT_BC5_UNORM:
		return DXGI_FORMAT_BC5_UNORM;
	case SIR_GRAPHICS_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

sirGraphicsResult_t sirD3D11QueryFormatSupport( void *pNativeDevice, sirGraphicsFormat_t eFormat,
                                                sirD3D11FormatSupport_t *pSupport ) noexcept
{
	if ( pNativeDevice == nullptr || pSupport == nullptr || pSupport->nSize < sizeof( *pSupport ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const DXGI_FORMAT eNativeFormat = sirD3D11NativeFormat( eFormat );
	if ( eNativeFormat == DXGI_FORMAT_UNKNOWN ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	ID3D11Device *pDevice = ( ID3D11Device * )pNativeDevice;
	UINT nNativeFlags = 0;
	const HRESULT hSupport = pDevice->CheckFormatSupport( eNativeFormat, &nNativeFlags );
	if ( FAILED( hSupport ) ) {
		return hSupport == DXGI_ERROR_DEVICE_REMOVED || hSupport == DXGI_ERROR_DEVICE_RESET ? SIR_GRAPHICS_DEVICE_LOST
		                                                                                    : SIR_GRAPHICS_UNSUPPORTED;
	}
	sirD3D11FormatSupport_t support = {};
	support.nSize = sizeof( support );
	support.eFormat = eFormat;
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_TEXTURE1D ) != 0 ) {
		support.nDimensionMask |= 1U << ( SIR_GRAPHICS_TEXTURE_1D - 1U );
	}
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_TEXTURE2D ) != 0 ) {
		support.nDimensionMask |= 1U << ( SIR_GRAPHICS_TEXTURE_2D - 1U );
	}
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_TEXTURE3D ) != 0 ) {
		support.nDimensionMask |= 1U << ( SIR_GRAPHICS_TEXTURE_3D - 1U );
	}
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_TEXTURECUBE ) != 0 ) {
		support.nDimensionMask |= 1U << ( SIR_GRAPHICS_TEXTURE_CUBE - 1U );
	}
	if ( support.nDimensionMask != 0 ) {
		support.nSupportedUsages |= SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	}
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE ) != 0 ) {
		support.nSupportedUsages |= SIR_GRAPHICS_USAGE_SAMPLED;
	}
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW ) != 0 ) {
		support.nSupportedUsages |= SIR_GRAPHICS_USAGE_STORAGE;
	}
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_RENDER_TARGET ) != 0 ) {
		support.nSupportedUsages |= SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	}
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL ) != 0 ) {
		support.nSupportedUsages |= SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	}
	support.nSampleCountMask = support.nDimensionMask != 0 ? 1U : 0U;
	if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_TEXTURE2D ) != 0 ) {
		if ( ( nNativeFlags & ( D3D11_FORMAT_SUPPORT_RENDER_TARGET | D3D11_FORMAT_SUPPORT_DEPTH_STENCIL ) ) != 0 ) {
			for ( uint32_t nSampleExponent = 1; nSampleExponent <= 5; ++nSampleExponent ) {
				const UINT nCount = 1U << nSampleExponent;
				UINT nQuality = 0;
				if ( SUCCEEDED( pDevice->CheckMultisampleQualityLevels( eNativeFormat, nCount, &nQuality ) ) && nQuality != 0 ) {
					if ( ( nNativeFlags & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL ) != 0 ||
					     ( nNativeFlags & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET ) != 0 ) {
						support.nSampleCountMask |= 1U << nSampleExponent;
					}
				}
			}
		}
	}
}
*pSupport = support;
return SIR_GRAPHICS_SUCCESS;
}
