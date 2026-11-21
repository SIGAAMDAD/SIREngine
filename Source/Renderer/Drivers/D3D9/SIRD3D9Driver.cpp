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
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <d3d9.h>
#endif

#if defined( _WIN32 ) && defined( SIR_D3D9_EXPORTS )
#define SIR_D3D9_PUBLIC __declspec( dllexport )
#else
#define SIR_D3D9_PUBLIC
#endif

typedef struct sirD3D9DriverState_s
{
	sirGraphicsABIHost_t host;
} sirD3D9DriverState_t;

static uint32_t sirD3D9AdapterCount( void *pDriver, uint32_t *pCount ) noexcept
{
	if ( pDriver == nullptr || pCount == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
#ifdef _WIN32
	HMODULE hLibrary = LoadLibraryW( L"d3d9.dll" );
	if ( hLibrary == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	typedef IDirect3D9 *( WINAPI * sirCreate9_t )( UINT );
	sirCreate9_t pCreate = ( sirCreate9_t )GetProcAddress( hLibrary, "Direct3DCreate9" );
	IDirect3D9 *pRuntime = pCreate == nullptr ? nullptr : pCreate( D3D_SDK_VERSION );
	if ( pRuntime != nullptr ) {
		*pCount = pRuntime->GetAdapterCount();
		pRuntime->Release();
	}
	FreeLibrary( hLibrary );
	return pRuntime == nullptr ? SIR_GRAPHICS_UNSUPPORTED : SIR_GRAPHICS_SUCCESS;
#else
	return SIR_GRAPHICS_UNSUPPORTED;
#endif
}

static uint32_t sirD3D9QueryAdapter( void *pDriver, sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pDriver == nullptr || pAdapter == nullptr || pAdapter->nSize < sizeof( sirGraphicsABIAdapter_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
#ifdef _WIN32
	HMODULE hLibrary = LoadLibraryW( L"d3d9.dll" );
	if ( hLibrary == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	typedef IDirect3D9 *( WINAPI * sirCreate9_t )( UINT );
	sirCreate9_t pCreate = ( sirCreate9_t )GetProcAddress( hLibrary, "Direct3DCreate9" );
	IDirect3D9 *pRuntime = pCreate == nullptr ? nullptr : pCreate( D3D_SDK_VERSION );
	if ( pRuntime == nullptr ) {
		FreeLibrary( hLibrary );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nIndex = pAdapter->nIndex;
	D3DADAPTER_IDENTIFIER9 adapter = {};
	D3DCAPS9 caps = {};
	const bool bValid = nIndex < pRuntime->GetAdapterCount() && SUCCEEDED( pRuntime->GetAdapterIdentifier( nIndex, 0, &adapter ) ) &&
	                    SUCCEEDED( pRuntime->GetDeviceCaps( nIndex, D3DDEVTYPE_HAL, &caps ) );
	if ( bValid ) {
		sirGraphicsABIAdapter_t info = {};
		info.nSize = sizeof( info );
		info.nIndex = nIndex;
		info.nBackend = SIR_GRAPHICS_BACKEND_D3D9;
		info.nKind = SIR_GRAPHICS_ABI_ADAPTER_OTHER;
		info.nVendorId = adapter.VendorId;
		info.nDeviceId = adapter.DeviceId;
		info.nApiMajor = 9;
		info.nApiMinor = 0;
		info.nStableId = ( ( uint64_t )adapter.VendorId << 32 ) | adapter.DeviceId;
		info.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_PRESENT | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
		if ( caps.NumSimultaneousRTs > 1 ) {
			info.nFeatureMask |= SIR_GRAPHICS_FEATURE_MRT;
		}
		info.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
		memcpy( info.szName, adapter.Description, sizeof( info.szName ) - 1 );
		info.szName[sizeof( info.szName ) - 1] = 0;
		*pAdapter = info;
	}
	pRuntime->Release();
	FreeLibrary( hLibrary );
	return bValid ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_UNSUPPORTED;
#else
	return SIR_GRAPHICS_UNSUPPORTED;
#endif
}

#ifdef _WIN32
uint32_t sirD3D9ABICreateDeviceWindows( const sirGraphicsABIHost_t *pHost, const sirGraphicsABIDeviceRequest_t *pRequest,
                                        sirGraphicsABIDevice_t *pDevice ) noexcept;
uint32_t sirD3D9ABIDestroyDeviceWindows( sirGraphicsABIDevice_t *pDevice ) noexcept;
#endif

static uint32_t sirD3D9CreateDevice( void *pDriver, const sirGraphicsABIDeviceRequest_t *pRequest,
                                     sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDriver == nullptr || pRequest == nullptr || pDevice == nullptr || pRequest->nSize < sizeof( sirGraphicsABIDeviceRequest_t ) ||
	     pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) || pRequest->nFramesInFlight == 0 || pRequest->nReserved != 0 ||
	     pRequest->nHeadless > 1 || pRequest->nAllowSoftware > 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
#ifdef _WIN32
	return sirD3D9ABICreateDeviceWindows( &( ( sirD3D9DriverState_t * )pDriver )->host, pRequest, pDevice );
#else
	return SIR_GRAPHICS_UNSUPPORTED;
#endif
}

static uint32_t sirD3D9DestroyDevice( void *pDriver, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDriver == nullptr || pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
#ifdef _WIN32
	return sirD3D9ABIDestroyDeviceWindows( pDevice );
#else
	return SIR_GRAPHICS_UNSUPPORTED;
#endif
}

static void sirD3D9ReleaseDriver( void *pDriver ) noexcept
{
	if ( pDriver == nullptr ) {
		return;
	}
	sirD3D9DriverState_t *pState = ( sirD3D9DriverState_t * )pDriver;
	pState->host.pFree( pState->host.pContext, pState );
}

extern "C" SIR_D3D9_PUBLIC uint32_t SIRGetGraphicsDriverABI( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                                             sirGraphicsABIDriver_t *pDriver ) noexcept
{
	if ( pDriver == nullptr || pDriver->nSize < sizeof( sirGraphicsABIDriver_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || nMinor > SIR_GRAPHICS_DRIVER_ABI_MINOR ) {
		return SIR_GRAPHICS_VERSION_MISMATCH;
	}
	const sirGraphicsResult_t eError = sirGraphicsValidateABIHost( pHost );
	if ( eError != SIR_GRAPHICS_SUCCESS ) {
		return eError;
	}
	void *pMemory = pHost->pAllocate( pHost->pContext, sizeof( sirD3D9DriverState_t ), alignof( sirD3D9DriverState_t ) );
	if ( pMemory == nullptr ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	if ( ( ( uintptr_t )pMemory % alignof( sirD3D9DriverState_t ) ) != 0 ) {
		pHost->pFree( pHost->pContext, pMemory );
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	sirD3D9DriverState_t *pState = ( sirD3D9DriverState_t * )pMemory;
	pState->host = *pHost;
	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	driver.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	driver.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	driver.nBackend = SIR_GRAPHICS_BACKEND_D3D9;
	driver.pDriver = pState;
	driver.pAdapterCount = sirD3D9AdapterCount;
	driver.pQueryAdapter = sirD3D9QueryAdapter;
	driver.pCreateDevice = sirD3D9CreateDevice;
	driver.pDestroyDevice = sirD3D9DestroyDevice;
	driver.pReleaseDriver = sirD3D9ReleaseDriver;
	memcpy( pDriver, &driver, sizeof( driver ) );
	return SIR_GRAPHICS_SUCCESS;
}
