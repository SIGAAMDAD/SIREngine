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

#include <Renderer/Drivers/D3D11/SIRD3D11Device.h>
#include <Renderer/Drivers/D3D11/SIRD3D11TextureValidation.h>

#include <d3d11.h>
#include <dxgi.h>
#include <cmath>

static sirGraphicsResult_t sirD3D11TextureError( HRESULT hResult ) noexcept
{
	if ( hResult == E_OUTOFMEMORY ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	if ( hResult == DXGI_ERROR_DEVICE_REMOVED || hResult == DXGI_ERROR_DEVICE_RESET || hResult == DXGI_ERROR_DEVICE_HUNG ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	return SIR_GRAPHICS_DRIVER_ERROR;
}

static DXGI_FORMAT sirD3D11TextureStorageFormat( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	case SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	default:
		return ( DXGI_FORMAT )sirD3D11GetNativeFormatCode( eFormat );
	}
}

static DXGI_FORMAT sirD3D11TextureViewFormat( sirGraphicsFormat_t eFormat, uint32_t nUsage ) noexcept
{
	if ( nUsage != SIR_GRAPHICS_USAGE_SAMPLED ) {
		return ( DXGI_FORMAT )sirD3D11GetNativeFormatCode( eFormat );
	}
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	default:
		return ( DXGI_FORMAT )sirD3D11GetNativeFormatCode( eFormat );
	}
}

sirGraphicsResult_t SIRD3D11Device::createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_pNativeDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D11FormatSupport_t support = {};
	support.nSize = sizeof( support );
	const sirGraphicsResult_t eProbe = queryNativeFormatSupport( desc.eFormat, &support );
	if ( eProbe != SIR_GRAPHICS_SUCCESS ) {
		return eProbe;
	}
	sirD3D11TexturePlan_t plan = {};
	const sirGraphicsResult_t ePlan = sirD3D11PlanTexture( &desc, &support, m_handles.getAllocatedBytes(), m_nResidentBudgetBytes, &plan );
	if ( ePlan != SIR_GRAPHICS_SUCCESS ) {
		return ePlan;
	}
	if ( desc.nWidth > m_profile.nMaximumTexture2D || desc.nHeight > m_profile.nMaximumTexture2D ||
	     desc.nDepth > m_profile.nMaximumTexture2D || m_handles.getActiveCount() >= SIRD3D11Handles::CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 && ( m_profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const DXGI_FORMAT eNativeFormat = sirD3D11TextureStorageFormat( desc.eFormat );
	if ( eNativeFormat == DXGI_FORMAT_UNKNOWN ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	UINT nBindFlags = 0;
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_SAMPLED ) != 0 ) {
		nBindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ) {
		nBindFlags |= D3D11_BIND_RENDER_TARGET;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) {
		nBindFlags |= D3D11_BIND_DEPTH_STENCIL;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) {
		nBindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}
	if ( nBindFlags == 0 ) {
		// D3D11 permits copy-only DEFAULT textures with no bind flags.
		if ( ( desc.nUsage & ( SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_COPY_SOURCE ) ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	ID3D11Resource *pNative = nullptr;
	ID3D11Device *pDevice = ( ID3D11Device * )m_pNativeDevice;
	HRESULT hCreate = E_INVALIDARG;
	if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_1D ) {
		D3D11_TEXTURE1D_DESC native = {};
		native.Width = desc.nWidth;
		native.MipLevels = desc.nMipLevels;
		native.ArraySize = desc.nArrayLayers;
		native.Format = eNativeFormat;
		native.Usage = D3D11_USAGE_DEFAULT;
		native.BindFlags = nBindFlags;
		ID3D11Texture1D *pTexture = nullptr;
		hCreate = pDevice->CreateTexture1D( &native, nullptr, &pTexture );
		pNative = pTexture;
	} else if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_3D ) {
		D3D11_TEXTURE3D_DESC native = {};
		native.Width = desc.nWidth;
		native.Height = desc.nHeight;
		native.Depth = desc.nDepth;
		native.MipLevels = desc.nMipLevels;
		native.Format = eNativeFormat;
		native.Usage = D3D11_USAGE_DEFAULT;
		native.BindFlags = nBindFlags;
		ID3D11Texture3D *pTexture = nullptr;
		hCreate = pDevice->CreateTexture3D( &native, nullptr, &pTexture );
		pNative = pTexture;
	} else {
		D3D11_TEXTURE2D_DESC native = {};
		native.Width = desc.nWidth;
		native.Height = desc.nHeight;
		native.MipLevels = desc.nMipLevels;
		native.ArraySize = desc.nArrayLayers;
		native.Format = eNativeFormat;
		native.SampleDesc.Count = desc.nSampleCount;
		native.SampleDesc.Quality = 0;
		native.Usage = D3D11_USAGE_DEFAULT;
		native.BindFlags = nBindFlags;
		if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_CUBE ) {
			native.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		}
		ID3D11Texture2D *pTexture = nullptr;
		hCreate = pDevice->CreateTexture2D( &native, nullptr, &pTexture );
		pNative = pTexture;
	}
	if ( FAILED( hCreate ) || pNative == nullptr ) {
		if ( pNative != nullptr ) {
			pNative->Release();
		}
		return FAILED( hCreate ) ? sirD3D11TextureError( hCreate ) : SIR_GRAPHICS_DRIVER_ERROR;
	}
	const sirD3D11ObjectInfo_t object = { pNative, plan.nLogicalBytes, SIR_GRAPHICS_HANDLE_TEXTURE, desc.eMemory };
	const sirGraphicsResult_t eRegister = m_handles.create( object, pHandle );
	if ( eRegister != SIR_GRAPHICS_SUCCESS ) {
		pNative->Release();
		return eRegister;
	}
	if ( m_handles.getAllocatedBytes() > m_nPeakCommittedBytes ) {
		m_nPeakCommittedBytes = m_handles.getAllocatedBytes();
	}
	return SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsResult_t sirD3D11ResourceDesc( ID3D11Resource *pResource, sirGraphicsFormat_t eFormat,
                                                 sirGraphicsTextureDesc_t *pDesc ) noexcept
{
	*pDesc = {};
	pDesc->eFormat = eFormat;
	pDesc->eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	D3D11_RESOURCE_DIMENSION eDimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	pResource->GetType( &eDimension );
	DXGI_FORMAT eNativeFormat = DXGI_FORMAT_UNKNOWN;
	UINT nBindFlags = 0;
	if ( eDimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D ) {
		D3D11_TEXTURE1D_DESC native = {};
		( ( ID3D11Texture1D * )pResource )->GetDesc( &native );
		pDesc->eDimension = SIR_GRAPHICS_TEXTURE_1D;
		pDesc->nWidth = native.Width;
		pDesc->nHeight = 1;
		pDesc->nDepth = 1;
		pDesc->nArrayLayers = native.ArraySize;
		pDesc->nMipLevels = native.MipLevels;
		pDesc->nSampleCount = 1;
		eNativeFormat = native.Format;
		nBindFlags = native.BindFlags;
	} else if ( eDimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D ) {
		D3D11_TEXTURE2D_DESC native = {};
		( ( ID3D11Texture2D * )pResource )->GetDesc( &native );
		pDesc->eDimension =
		    ( native.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE ) != 0 ? SIR_GRAPHICS_TEXTURE_CUBE : SIR_GRAPHICS_TEXTURE_2D;
		pDesc->nWidth = native.Width;
		pDesc->nHeight = native.Height;
		pDesc->nDepth = 1;
		pDesc->nArrayLayers = native.ArraySize;
		pDesc->nMipLevels = native.MipLevels;
		pDesc->nSampleCount = native.SampleDesc.Count;
		eNativeFormat = native.Format;
		nBindFlags = native.BindFlags;
	} else if ( eDimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D ) {
		D3D11_TEXTURE3D_DESC native = {};
		( ( ID3D11Texture3D * )pResource )->GetDesc( &native );
		pDesc->eDimension = SIR_GRAPHICS_TEXTURE_3D;
		pDesc->nWidth = native.Width;
		pDesc->nHeight = native.Height;
		pDesc->nDepth = native.Depth;
		pDesc->nArrayLayers = 1;
		pDesc->nMipLevels = native.MipLevels;
		pDesc->nSampleCount = 1;
		eNativeFormat = native.Format;
		nBindFlags = native.BindFlags;
	} else {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( eNativeFormat != sirD3D11TextureStorageFormat( eFormat ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ( nBindFlags & D3D11_BIND_SHADER_RESOURCE ) != 0 ) {
		pDesc->nUsage |= SIR_GRAPHICS_USAGE_SAMPLED;
	}
	if ( ( nBindFlags & D3D11_BIND_RENDER_TARGET ) != 0 ) {
		pDesc->nUsage |= SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	}
	if ( ( nBindFlags & D3D11_BIND_DEPTH_STENCIL ) != 0 ) {
		pDesc->nUsage |= SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	}
	if ( ( nBindFlags & D3D11_BIND_UNORDERED_ACCESS ) != 0 ) {
		pDesc->nUsage |= SIR_GRAPHICS_USAGE_STORAGE;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Device::createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_pNativeDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	const sirGraphicsResult_t eLookup = m_handles.lookup( desc.hResource, &pSlot );
	if ( eLookup != SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	if ( desc.hResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	ID3D11Resource *pResource = ( ID3D11Resource * )pSlot->object.pNativeObject;
	sirGraphicsTextureDesc_t texture = {};
	const sirGraphicsResult_t eTexture = sirD3D11ResourceDesc( pResource, desc.eFormat, &texture );
	if ( eTexture != SIR_GRAPHICS_SUCCESS ) {
		return eTexture;
	}
	const sirGraphicsResult_t eValid = sirD3D11ValidateTextureView( &desc, &texture );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( m_handles.getActiveCount() >= SIRD3D11Handles::CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	ID3D11Device *pDevice = ( ID3D11Device * )m_pNativeDevice;
	const DXGI_FORMAT eFormat = sirD3D11TextureViewFormat( desc.eFormat, desc.nUsage );
	IUnknown *pView = nullptr;
	HRESULT hResult = E_INVALIDARG;
	const bool bArray = texture.nArrayLayers > 1;
	if ( desc.nUsage == SIR_GRAPHICS_USAGE_SAMPLED ) {
		D3D11_SHADER_RESOURCE_VIEW_DESC native = {};
		native.Format = eFormat;
		if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_1D ) {
			native.ViewDimension = bArray ? D3D11_SRV_DIMENSION_TEXTURE1DARRAY : D3D11_SRV_DIMENSION_TEXTURE1D;
			if ( bArray ) {
				native.Texture1DArray.MostDetailedMip = desc.nBaseMip;
				native.Texture1DArray.MipLevels = desc.nMipLevels;
				native.Texture1DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture1DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture1D.MostDetailedMip = desc.nBaseMip;
				native.Texture1D.MipLevels = desc.nMipLevels;
			}
		} else if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_3D ) {
			native.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
			native.Texture3D.MostDetailedMip = desc.nBaseMip;
			native.Texture3D.MipLevels = desc.nMipLevels;
		} else if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_CUBE ) {
			const bool bSingleCube = desc.nBaseLayer == 0 && desc.nLayers == 6 && texture.nArrayLayers == 6;
			native.ViewDimension = bSingleCube ? D3D11_SRV_DIMENSION_TEXTURECUBE : D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
			if ( bSingleCube ) {
				native.TextureCube.MostDetailedMip = desc.nBaseMip;
				native.TextureCube.MipLevels = desc.nMipLevels;
			} else {
				native.TextureCubeArray.MostDetailedMip = desc.nBaseMip;
				native.TextureCubeArray.MipLevels = desc.nMipLevels;
				native.TextureCubeArray.First2DArrayFace = desc.nBaseLayer;
				native.TextureCubeArray.NumCubes = desc.nLayers / 6;
			}
		} else if ( texture.nSampleCount > 1 ) {
			native.ViewDimension = bArray ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DMS;
			if ( bArray ) {
				native.Texture2DMSArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture2DMSArray.ArraySize = desc.nLayers;
			}
		} else {
			native.ViewDimension = bArray ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D;
			if ( bArray ) {
				native.Texture2DArray.MostDetailedMip = desc.nBaseMip;
				native.Texture2DArray.MipLevels = desc.nMipLevels;
				native.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture2DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture2D.MostDetailedMip = desc.nBaseMip;
				native.Texture2D.MipLevels = desc.nMipLevels;
			}
		}
		ID3D11ShaderResourceView *pNative = nullptr;
		hResult = pDevice->CreateShaderResourceView( pResource, &native, &pNative );
		pView = pNative;
	} else if ( desc.nUsage == SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) {
		D3D11_RENDER_TARGET_VIEW_DESC native = {};
		native.Format = eFormat;
		if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_1D ) {
			native.ViewDimension = bArray ? D3D11_RTV_DIMENSION_TEXTURE1DARRAY : D3D11_RTV_DIMENSION_TEXTURE1D;
			if ( bArray ) {
				native.Texture1DArray.MipSlice = desc.nBaseMip;
				native.Texture1DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture1DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture1D.MipSlice = desc.nBaseMip;
			}
		} else if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_3D ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		} else if ( texture.nSampleCount > 1 ) {
			native.ViewDimension = bArray ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DMS;
			if ( bArray ) {
				native.Texture2DMSArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture2DMSArray.ArraySize = desc.nLayers;
			}
		} else {
			native.ViewDimension = bArray ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D;
			if ( bArray ) {
				native.Texture2DArray.MipSlice = desc.nBaseMip;
				native.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture2DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture2D.MipSlice = desc.nBaseMip;
			}
		}
		ID3D11RenderTargetView *pNative = nullptr;
		hResult = pDevice->CreateRenderTargetView( pResource, &native, &pNative );
		pView = pNative;
	} else if ( desc.nUsage == SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) {
		D3D11_DEPTH_STENCIL_VIEW_DESC native = {};
		native.Format = eFormat;
		if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_1D ) {
			native.ViewDimension = bArray ? D3D11_DSV_DIMENSION_TEXTURE1DARRAY : D3D11_DSV_DIMENSION_TEXTURE1D;
			if ( bArray ) {
				native.Texture1DArray.MipSlice = desc.nBaseMip;
				native.Texture1DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture1DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture1D.MipSlice = desc.nBaseMip;
			}
		} else if ( texture.nSampleCount > 1 ) {
			native.ViewDimension = bArray ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DMS;
			if ( bArray ) {
				native.Texture2DMSArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture2DMSArray.ArraySize = desc.nLayers;
			}
		} else {
			native.ViewDimension = bArray ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D;
			if ( bArray ) {
				native.Texture2DArray.MipSlice = desc.nBaseMip;
				native.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture2DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture2D.MipSlice = desc.nBaseMip;
			}
		}
		ID3D11DepthStencilView *pNative = nullptr;
		hResult = pDevice->CreateDepthStencilView( pResource, &native, &pNative );
		pView = pNative;
	} else {
		D3D11_UNORDERED_ACCESS_VIEW_DESC native = {};
		native.Format = eFormat;
		if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_1D ) {
			native.ViewDimension = bArray ? D3D11_UAV_DIMENSION_TEXTURE1DARRAY : D3D11_UAV_DIMENSION_TEXTURE1D;
			if ( bArray ) {
				native.Texture1DArray.MipSlice = desc.nBaseMip;
				native.Texture1DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture1DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture1D.MipSlice = desc.nBaseMip;
			}
		} else if ( texture.eDimension == SIR_GRAPHICS_TEXTURE_3D ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		} else {
			native.ViewDimension = bArray ? D3D11_UAV_DIMENSION_TEXTURE2DARRAY : D3D11_UAV_DIMENSION_TEXTURE2D;
			if ( bArray ) {
				native.Texture2DArray.MipSlice = desc.nBaseMip;
				native.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
				native.Texture2DArray.ArraySize = desc.nLayers;
			} else {
				native.Texture2D.MipSlice = desc.nBaseMip;
			}
		}
		ID3D11UnorderedAccessView *pNative = nullptr;
		hResult = pDevice->CreateUnorderedAccessView( pResource, &native, &pNative );
		pView = pNative;
	}
	if ( FAILED( hResult ) || pView == nullptr ) {
		if ( pView != nullptr ) {
			pView->Release();
		}
		return FAILED( hResult ) ? sirD3D11TextureError( hResult ) : SIR_GRAPHICS_DRIVER_ERROR;
	}
	const sirD3D11ObjectInfo_t object = { pView, 0, SIR_GRAPHICS_HANDLE_VIEW, SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	const sirGraphicsResult_t eRegister = m_handles.create( object, pHandle );
	if ( eRegister != SIR_GRAPHICS_SUCCESS ) {
		pView->Release();
	}
	return eRegister;
}

static D3D11_TEXTURE_ADDRESS_MODE sirD3D11Address( sirGraphicsAddressMode_t eMode ) noexcept
{
	switch ( eMode ) {
	case SIR_GRAPHICS_ADDRESS_REPEAT:
		return D3D11_TEXTURE_ADDRESS_WRAP;
	case SIR_GRAPHICS_ADDRESS_MIRROR:
		return D3D11_TEXTURE_ADDRESS_MIRROR;
	case SIR_GRAPHICS_ADDRESS_CLAMP:
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	case SIR_GRAPHICS_ADDRESS_BORDER:
		return D3D11_TEXTURE_ADDRESS_BORDER;
	default:
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	}
}

sirGraphicsResult_t SIRD3D11Device::createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_pNativeDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( desc.eFilter > SIR_GRAPHICS_FILTER_ANISOTROPIC || desc.eAddressU > SIR_GRAPHICS_ADDRESS_BORDER ||
	     desc.eAddressV > SIR_GRAPHICS_ADDRESS_BORDER || desc.eAddressW > SIR_GRAPHICS_ADDRESS_BORDER ||
	     !std::isfinite( desc.fMaximumAnisotropy ) || !std::isfinite( desc.fMinimumLod ) || !std::isfinite( desc.fMaximumLod ) ||
	     desc.fMinimumLod > desc.fMaximumLod || desc.fMaximumAnisotropy < 1.0f || desc.fMaximumAnisotropy > 16.0f ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_handles.getActiveCount() >= SIRD3D11Handles::CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	D3D11_SAMPLER_DESC native = {};
	native.Filter = desc.eFilter == SIR_GRAPHICS_FILTER_NEAREST     ? D3D11_FILTER_MIN_MAG_MIP_POINT
	                : desc.eFilter == SIR_GRAPHICS_FILTER_LINEAR    ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
	                : desc.eFilter == SIR_GRAPHICS_FILTER_TRILINEAR ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
	                                                                : D3D11_FILTER_ANISOTROPIC;
	native.AddressU = sirD3D11Address( desc.eAddressU );
	native.AddressV = sirD3D11Address( desc.eAddressV );
	native.AddressW = sirD3D11Address( desc.eAddressW );
	native.MaxAnisotropy = desc.eFilter == SIR_GRAPHICS_FILTER_ANISOTROPIC ? ( UINT )desc.fMaximumAnisotropy : 1U;
	native.ComparisonFunc = D3D11_COMPARISON_NEVER;
	native.MinLOD = desc.fMinimumLod;
	native.MaxLOD = desc.fMaximumLod;
	ID3D11SamplerState *pNative = nullptr;
	const HRESULT hResult = ( ( ID3D11Device * )m_pNativeDevice )->CreateSamplerState( &native, &pNative );
	if ( FAILED( hResult ) || pNative == nullptr ) {
		if ( pNative != nullptr ) {
			pNative->Release();
		}
		return FAILED( hResult ) ? sirD3D11TextureError( hResult ) : SIR_GRAPHICS_DRIVER_ERROR;
	}
	const sirD3D11ObjectInfo_t object = { pNative, 0, SIR_GRAPHICS_HANDLE_SAMPLER, SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	const sirGraphicsResult_t eRegister = m_handles.create( object, pHandle );
	if ( eRegister != SIR_GRAPHICS_SUCCESS ) {
		pNative->Release();
	}
	return eRegister;
}
