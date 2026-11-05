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

#include <Renderer/Drivers/D3D11/SIRD3D11Driver.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <new>

static constexpr uint32_t SIR_D3D11_MAX_ADAPTERS = 256;
static constexpr uint32_t SIR_D3D11_MAX_FRAMES_IN_FLIGHT = 8;

static void sirD3D11ReleaseNative( ID3D11Device **ppDevice, ID3D11DeviceContext **ppContext ) noexcept
{
	if ( *ppContext != nullptr ) {
		( *ppContext )->Release();
		*ppContext = nullptr;
	}
	if ( *ppDevice != nullptr ) {
		( *ppDevice )->Release();
		*ppDevice = nullptr;
	}
}

// A requested FL11_1 may be rejected by an older D3D11 runtime with
// E_INVALIDARG. Retry with the identical list minus 11_1, not an arbitrary
// downgrade of a failed hardware or WARP device creation.
static HRESULT sirD3D11CreateNative( IDXGIAdapter *pAdapter, bool bSoftware, ID3D11Device **ppDevice, ID3D11DeviceContext **ppContext,
                                     D3D_FEATURE_LEVEL *pLevel ) noexcept
{
	*ppDevice = nullptr;
	*ppContext = nullptr;
	const D3D_FEATURE_LEVEL szLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
	                                       D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1 };
	const D3D_DRIVER_TYPE eType = bSoftware ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN;
	HRESULT hResult = D3D11CreateDevice( pAdapter, eType, nullptr, 0, szLevels, 7, D3D11_SDK_VERSION, ppDevice, pLevel, ppContext );
	if ( hResult == E_INVALIDARG ) {
		sirD3D11ReleaseNative( ppDevice, ppContext );
		hResult = D3D11CreateDevice( pAdapter, eType, nullptr, 0, szLevels + 1, 6, D3D11_SDK_VERSION, ppDevice, pLevel, ppContext );
	}
	if ( FAILED( hResult ) ) {
		sirD3D11ReleaseNative( ppDevice, ppContext );
	}
	return hResult;
}

static sirGraphicsResult_t sirD3D11MapHRESULT( HRESULT hResult ) noexcept
{
	if ( SUCCEEDED( hResult ) ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( hResult == E_OUTOFMEMORY ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	if ( hResult == DXGI_ERROR_DEVICE_REMOVED || hResult == DXGI_ERROR_DEVICE_RESET ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}

// Enumerate only native DXGI hardware adapters. Software selection is a
// single explicit WARP index immediately after the hardware adapter range.
static sirGraphicsResult_t sirD3D11ResolveHardware( uint32_t nRequestedIndex, IDXGIAdapter1 **ppAdapter, uint32_t *pHardwareCount ) noexcept
{
	*ppAdapter = nullptr;
	*pHardwareCount = 0;
	IDXGIFactory1 *pFactory = nullptr;
	const HRESULT hFactory = CreateDXGIFactory1( __uuidof( IDXGIFactory1 ), ( void ** )&pFactory );
	if ( FAILED( hFactory ) || pFactory == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}

	for ( UINT iDxgi = 0; iDxgi < SIR_D3D11_MAX_ADAPTERS; ++iDxgi ) {
		IDXGIAdapter1 *pCurrent = nullptr;
		const HRESULT hEnum = pFactory->EnumAdapters1( iDxgi, &pCurrent );
		if ( hEnum == DXGI_ERROR_NOT_FOUND ) {
			break;
		}
		if ( FAILED( hEnum ) || pCurrent == nullptr ) {
			if ( *ppAdapter != nullptr ) {
				( *ppAdapter )->Release();
				*ppAdapter = nullptr;
			}
			pFactory->Release();
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		DXGI_ADAPTER_DESC1 desc = {};
		const HRESULT hDesc = pCurrent->GetDesc1( &desc );
		if ( FAILED( hDesc ) ) {
			pCurrent->Release();
			if ( *ppAdapter != nullptr ) {
				( *ppAdapter )->Release();
				*ppAdapter = nullptr;
			}
			pFactory->Release();
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		const bool bBasicRenderDriver = desc.VendorId == 0x1414 && desc.DeviceId == 0x008C;
		if ( ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) == 0 && !bBasicRenderDriver ) {
			if ( *pHardwareCount == nRequestedIndex ) {
				*ppAdapter = pCurrent;
			} else {
				pCurrent->Release();
			}
			++*pHardwareCount;
		} else {
			pCurrent->Release();
		}
	}
	pFactory->Release();
	return SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsResult_t sirD3D11GetProfile( ID3D11Device *pDevice, D3D_FEATURE_LEVEL eLevel,
                                               sirD3D11FeatureProfile_t *pProfile ) noexcept
{
	sirD3D11FeatureProbe_t probe = {};
	probe.nSize = sizeof( probe );
	probe.nFeatureLevel = ( uint32_t )eLevel;

	ID3D11Device1 *pDevice1 = nullptr;
	if ( SUCCEEDED( pDevice->QueryInterface( __uuidof( ID3D11Device1 ), ( void ** )&pDevice1 ) ) && pDevice1 != nullptr ) {
		probe.nD3D11_1InterfaceAvailable = 1;
		pDevice1->Release();
	}
	if ( eLevel == D3D_FEATURE_LEVEL_10_0 || eLevel == D3D_FEATURE_LEVEL_10_1 ) {
		D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options = {};
		if ( SUCCEEDED( pDevice->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &options, sizeof( options ) ) ) ) {
			probe.nComputeShader4xAvailable = options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x != FALSE ? 1U : 0U;
		}
	}
	probe.nIndirectDrawAvailable = eLevel >= D3D_FEATURE_LEVEL_11_0 ? 1U : 0U;
	UINT nQualityLevels = 0;
	if ( SUCCEEDED( pDevice->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, 2, &nQualityLevels ) ) ) {
		probe.nMultisampleAvailable = nQualityLevels != 0 ? 1U : 0U;
	}
	// Surface presentation is admitted only after D11-14 implements it.
	probe.nPresentationAvailable = 0;
	pProfile->nSize = sizeof( *pProfile );
	return sirD3D11BuildFeatureProfile( &probe, pProfile );
}

static sirGraphicsResult_t sirD3D11GetAdapterDesc( IDXGIAdapter1 *pSelected, ID3D11Device *pDevice, DXGI_ADAPTER_DESC *pDesc ) noexcept
{
	if ( pSelected != nullptr ) {
		return SUCCEEDED( pSelected->GetDesc( pDesc ) ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
	}
	IDXGIDevice *pDxgiDevice = nullptr;
	if ( FAILED( pDevice->QueryInterface( __uuidof( IDXGIDevice ), ( void ** )&pDxgiDevice ) ) || pDxgiDevice == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	IDXGIAdapter *pWarp = nullptr;
	const HRESULT hAdapter = pDxgiDevice->GetAdapter( &pWarp );
	pDxgiDevice->Release();
	if ( FAILED( hAdapter ) || pWarp == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	const HRESULT hDesc = pWarp->GetDesc( pDesc );
	pWarp->Release();
	return SUCCEEDED( hDesc ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

SIRD3D11Driver::~SIRD3D11Driver( void ) noexcept
{
	if ( m_pActiveDevice != nullptr ) {
		( void )destroyDevice( m_pActiveDevice );
	}
}

sirGraphicsBackend_t SIRD3D11Driver::getBackend( void ) const noexcept
{
	return SIR_GRAPHICS_BACKEND_D3D11;
}

sirGraphicsResult_t SIRD3D11Driver::getAdapterCount( uint32_t *pCount ) const noexcept
{
	if ( pCount == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
	IDXGIAdapter1 *pUnused = nullptr;
	uint32_t nHardware = 0;
	const sirGraphicsResult_t eEnumerate = sirD3D11ResolveHardware( UINT32_MAX, &pUnused, &nHardware );
	if ( eEnumerate != SIR_GRAPHICS_SUCCESS ) {
		return eEnumerate;
	}
	ID3D11Device *pNative = nullptr;
	ID3D11DeviceContext *pContext = nullptr;
	D3D_FEATURE_LEVEL eLevel = D3D_FEATURE_LEVEL_9_1;
	const HRESULT hWarp = sirD3D11CreateNative( nullptr, true, &pNative, &pContext, &eLevel );
	sirD3D11ReleaseNative( &pNative, &pContext );
	*pCount = nHardware + ( nHardware < SIR_D3D11_MAX_ADAPTERS && SUCCEEDED( hWarp ) ? 1U : 0U );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Driver::queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept
{
	if ( pInfo == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pInfo = {};
	IDXGIAdapter1 *pSelected = nullptr;
	uint32_t nHardware = 0;
	const sirGraphicsResult_t eEnumerate = sirD3D11ResolveHardware( nAdapterIndex, &pSelected, &nHardware );
	if ( eEnumerate != SIR_GRAPHICS_SUCCESS ) {
		return eEnumerate;
	}
	if ( nAdapterIndex >= SIR_D3D11_MAX_ADAPTERS || ( pSelected == nullptr && nAdapterIndex != nHardware ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bSoftware = pSelected == nullptr;
	ID3D11Device *pNative = nullptr;
	ID3D11DeviceContext *pContext = nullptr;
	D3D_FEATURE_LEVEL eLevel = D3D_FEATURE_LEVEL_9_1;
	const HRESULT hDevice = sirD3D11CreateNative( pSelected, bSoftware, &pNative, &pContext, &eLevel );
	if ( FAILED( hDevice ) || pNative == nullptr || pContext == nullptr ) {
		sirD3D11ReleaseNative( &pNative, &pContext );
		if ( pSelected != nullptr ) {
			pSelected->Release();
		}
		return sirD3D11MapHRESULT( hDevice );
	}

	DXGI_ADAPTER_DESC nativeDesc = {};
	const sirGraphicsResult_t eDesc = sirD3D11GetAdapterDesc( pSelected, pNative, &nativeDesc );
	sirD3D11FeatureProfile_t profile = {};
	const sirGraphicsResult_t eProfile = sirD3D11GetProfile( pNative, eLevel, &profile );
	sirD3D11ReleaseNative( &pNative, &pContext );
	if ( pSelected != nullptr ) {
		pSelected->Release();
	}
	if ( eDesc != SIR_GRAPHICS_SUCCESS || eProfile != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}

	pInfo->nAdapterIndex = nAdapterIndex;
	pInfo->nVendorId = nativeDesc.VendorId;
	pInfo->nDeviceId = nativeDesc.DeviceId;
	pInfo->nAdapterLuid = ( ( uint64_t )( uint32_t )nativeDesc.AdapterLuid.HighPart << 32 ) | nativeDesc.AdapterLuid.LowPart;
	pInfo->nDedicatedVideoBytes = nativeDesc.DedicatedVideoMemory;
	pInfo->nSharedSystemBytes = nativeDesc.SharedSystemMemory;
	pInfo->nFeatureMask = profile.nEnabledFeatures;
	pInfo->nApiVersion = { 11, profile.nD3D11_1InterfaceAvailable != 0 ? 1U : 0U, 0 };
	pInfo->bSoftwareAdapter = bSoftware;
	if ( WideCharToMultiByte( CP_UTF8, 0, nativeDesc.Description, -1, pInfo->szName, sizeof( pInfo->szName ), nullptr, nullptr ) == 0 ) {
		const char *pszFallback = "Direct3D 11 adapter";
		memcpy( pInfo->szName, pszFallback, sizeof( "Direct3D 11 adapter" ) );
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Driver::createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept
{
	if ( ppDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDevice = nullptr;
	if ( desc.nFramesInFlight == 0 || desc.nFramesInFlight > SIR_D3D11_MAX_FRAMES_IN_FLIGHT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_pActiveDevice != nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	IDXGIAdapter1 *pSelected = nullptr;
	uint32_t nHardware = 0;
	const sirGraphicsResult_t eEnumerate = sirD3D11ResolveHardware( desc.nAdapterIndex, &pSelected, &nHardware );
	if ( eEnumerate != SIR_GRAPHICS_SUCCESS ) {
		return eEnumerate;
	}
	if ( desc.nAdapterIndex >= SIR_D3D11_MAX_ADAPTERS || ( pSelected == nullptr && desc.nAdapterIndex != nHardware ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bSoftware = pSelected == nullptr;
	if ( bSoftware && !desc.bAllowSoftware ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	ID3D11Device *pNative = nullptr;
	ID3D11DeviceContext *pContext = nullptr;
	D3D_FEATURE_LEVEL eLevel = D3D_FEATURE_LEVEL_9_1;
	const HRESULT hDevice = sirD3D11CreateNative( pSelected, bSoftware, &pNative, &pContext, &eLevel );
	if ( pSelected != nullptr ) {
		pSelected->Release();
	}
	if ( FAILED( hDevice ) || pNative == nullptr || pContext == nullptr ) {
		sirD3D11ReleaseNative( &pNative, &pContext );
		return FAILED( hDevice ) ? sirD3D11MapHRESULT( hDevice ) : SIR_GRAPHICS_DRIVER_ERROR;
	}

	sirD3D11FeatureProfile_t profile = {};
	const sirGraphicsResult_t eProfile = sirD3D11GetProfile( pNative, eLevel, &profile );
	if ( eProfile != SIR_GRAPHICS_SUCCESS || ( profile.nEnabledFeatures & desc.nRequiredFeatures ) != desc.nRequiredFeatures ) {
		sirD3D11ReleaseNative( &pNative, &pContext );
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirD3D11NativeDeviceInit_t init = {};
	init.pNativeDevice = pNative;
	init.pImmediateContext = pContext;
	init.profile = profile;
	init.nResidentBudgetBytes = desc.nResidentBudgetBytes;
	init.nFramesInFlight = desc.nFramesInFlight;
	init.nSoftwareAdapter = bSoftware ? 1U : 0U;
	m_pActiveDevice = new ( m_szDeviceStorage ) SIRD3D11Device( init );
	*ppDevice = m_pActiveDevice;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Driver::destroyDevice( SIRRenderingDevice *pDevice ) noexcept
{
	if ( pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDevice != m_pActiveDevice ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	m_pActiveDevice->~SIRD3D11Device();
	m_pActiveDevice = nullptr;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D11RegisterDriver( SIRGraphicsRegistry *pRegistry, SIRD3D11Driver *pDriver ) noexcept
{
	if ( pRegistry == nullptr || pDriver == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return pRegistry->registerDriver( pDriver );
}
