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

#include <Renderer/SIRGraphicsModule.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <string.h>

static sirGraphicsABIResult_t sirGraphicsResultFromProvider( uint32_t nResult ) noexcept
{
	if ( nResult > SIR_GRAPHICS_ABI_DRIVER_ERROR ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	return ( sirGraphicsABIResult_t )nResult;
}

sirGraphicsABIResult_t sirGraphicsLoadABIModule( const char *pszPath, const sirGraphicsABIHost_t *pHost,
                                                 sirGraphicsABIModule_t *pModule ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' || pModule == nullptr || pModule->library.pNativeHandle != nullptr ||
	     pModule->driver.pDriver != nullptr || pModule->nActiveDevices != 0 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eHost = sirGraphicsValidateABIHost( pHost );
	if ( eHost != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eHost;
	}
	sirDynamicLibrary_t library = {};
	if ( !SIRDynamicLibrary::load( pszPath, &library ) ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	const sirGraphicsABIFactory_t pFactory =
	    ( sirGraphicsABIFactory_t )SIRDynamicLibrary::findSymbol( &library, SIR_GRAPHICS_DRIVER_ABI_ENTRY );
	if ( pFactory == nullptr ) {
		SIRDynamicLibrary::unload( &library );
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	const sirGraphicsABIResult_t eStatus = sirGraphicsNegotiateDriverABI( pFactory, pHost, &driver );
	if ( eStatus != SIR_GRAPHICS_ABI_SUCCESS ) {
		SIRDynamicLibrary::unload( &library );
		return eStatus;
	}
	pModule->library = library;
	pModule->driver = driver;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsUnloadABIModule( sirGraphicsABIModule_t *pModule ) noexcept
{
	if ( pModule == nullptr || pModule->library.pNativeHandle == nullptr ||
	     sirGraphicsValidateABIDriver( &pModule->driver ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	if ( pModule->nActiveDevices != 0 ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	pModule->driver.pReleaseDriver( pModule->driver.pDriver );
	if ( !SIRDynamicLibrary::unload( &pModule->library ) ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	*pModule = {};
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsQueryABIAdapter( const sirGraphicsABIModule_t *pModule, uint32_t nIndex,
                                                   sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pModule == nullptr || pAdapter == nullptr || pAdapter->nSize < sizeof( sirGraphicsABIAdapter_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDriver = sirGraphicsValidateABIDriver( &pModule->driver );
	if ( eDriver != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eDriver;
	}
	uint32_t nCount = 0;
	const sirGraphicsABIResult_t eCount = sirGraphicsResultFromProvider( pModule->driver.pAdapterCount( pModule->driver.pDriver, &nCount ) );
	if ( eCount != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eCount;
	}
	if ( nCount > 256 ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	if ( nIndex >= nCount ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	adapter.nIndex = nIndex;
	const sirGraphicsABIResult_t eProbe = sirGraphicsResultFromProvider( pModule->driver.pQueryAdapter( pModule->driver.pDriver, &adapter ) );
	if ( eProbe != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eProbe;
	}
	if ( sirGraphicsValidateABIAdapter( &adapter ) != SIR_GRAPHICS_ABI_SUCCESS || adapter.nIndex != nIndex ||
	     adapter.nBackend != pModule->driver.nBackend ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	memcpy( pAdapter, &adapter, sizeof( adapter ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsQueryABICapabilities( const sirGraphicsABIDevice_t *pDevice, uint32_t nBackend,
                                                        sirGraphicsABICapabilities_t *pInfo ) noexcept
{
	if ( pInfo == nullptr || pInfo->nSize < sizeof( sirGraphicsABICapabilities_t ) ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABICapabilities_t info = {};
	info.nSize = sizeof( info );
	const sirGraphicsABIResult_t eQuery = sirGraphicsResultFromProvider( pDevice->pQueryCapabilities( pDevice->pDevice, &info ) );
	if ( eQuery != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eQuery;
	}
	if ( sirGraphicsValidateABICapabilities( &info ) != SIR_GRAPHICS_ABI_SUCCESS || info.nBackend != nBackend ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	memcpy( pInfo, &info, sizeof( info ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsQueryABIFormat( const sirGraphicsABIDevice_t *pDevice, uint32_t nFormat,
                                                  sirGraphicsABIFormat_t *pFormat ) noexcept
{
	if ( pFormat == nullptr || pFormat->nSize < sizeof( sirGraphicsABIFormat_t ) ||
	     nFormat <= SIR_GRAPHICS_FORMAT_UNKNOWN || nFormat > SIR_GRAPHICS_FORMAT_BC7_UNORM ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eDevice = sirGraphicsValidateABIDevice( pDevice );
	if ( eDevice != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eDevice;
	}
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = nFormat;
	const sirGraphicsABIResult_t eQuery = sirGraphicsResultFromProvider( pDevice->pQueryFormat( pDevice->pDevice, &format ) );
	if ( eQuery != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eQuery;
	}
	if ( sirGraphicsValidateABIFormat( &format ) != SIR_GRAPHICS_ABI_SUCCESS || format.nFormat != nFormat ) {
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	memcpy( pFormat, &format, sizeof( format ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsCreateABIDevice( sirGraphicsABIModule_t *pModule,
                                                   const sirGraphicsABIDeviceRequest_t *pRequest,
                                                   sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pModule == nullptr || pRequest == nullptr || pRequest->nSize < sizeof( sirGraphicsABIDeviceRequest_t ) ||
	     pRequest->nFramesInFlight == 0 || pRequest->nAllowSoftware > 1 || pDevice == nullptr ||
	     pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) ||
	     pModule->nActiveDevices >= 16 ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	const sirGraphicsABIResult_t eAdapter = sirGraphicsQueryABIAdapter( pModule, pRequest->nAdapterIndex, &adapter );
	if ( eAdapter != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eAdapter;
	}
	if ( adapter.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE && pRequest->nAllowSoftware == 0 ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	if ( ( adapter.nFeatureMask & pRequest->nRequiredFeatures ) != pRequest->nRequiredFeatures ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	const sirGraphicsABIResult_t eCreated =
	    sirGraphicsResultFromProvider( pModule->driver.pCreateDevice( pModule->driver.pDriver, pRequest, &device ) );
	if ( eCreated != SIR_GRAPHICS_ABI_SUCCESS ) {
		return eCreated;
	}
	const sirGraphicsABIResult_t eValid = sirGraphicsValidateABIDevice( &device );
	if ( eValid != SIR_GRAPHICS_ABI_SUCCESS ) {
		pModule->driver.pDestroyDevice( pModule->driver.pDriver, &device );
		return SIR_GRAPHICS_ABI_DRIVER_ERROR;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsABIResult_t eCaps = sirGraphicsQueryABICapabilities( &device, pModule->driver.nBackend, &caps );
	if ( eCaps != SIR_GRAPHICS_ABI_SUCCESS || ( caps.nEnabledFeatures & pRequest->nRequiredFeatures ) != pRequest->nRequiredFeatures ||
	     caps.nAdapterKind != adapter.nKind || caps.nMaximumFramesInFlight < pRequest->nFramesInFlight ) {
		pModule->driver.pDestroyDevice( pModule->driver.pDriver, &device );
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	for ( uint32_t i = 0; i < pModule->nActiveDevices; ++i ) {
		if ( pModule->szActiveDevices[i] == device.pDevice ) {
			pModule->driver.pDestroyDevice( pModule->driver.pDriver, &device );
			return SIR_GRAPHICS_ABI_DRIVER_ERROR;
		}
	}
	pModule->szActiveDevices[pModule->nActiveDevices] = device.pDevice;
	++pModule->nActiveDevices;
	memcpy( pDevice, &device, sizeof( device ) );
	return SIR_GRAPHICS_ABI_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsDestroyABIDevice( sirGraphicsABIModule_t *pModule, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pModule == nullptr || pDevice == nullptr || sirGraphicsValidateABIDevice( pDevice ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pModule->nActiveDevices; ++i ) {
		if ( pModule->szActiveDevices[i] != pDevice->pDevice ) {
			continue;
		}
		const sirGraphicsABIResult_t eDestroy =
		    sirGraphicsResultFromProvider( pModule->driver.pDestroyDevice( pModule->driver.pDriver, pDevice ) );
		if ( eDestroy != SIR_GRAPHICS_ABI_SUCCESS ) {
			return eDestroy;
		}
		--pModule->nActiveDevices;
		pModule->szActiveDevices[i] = pModule->szActiveDevices[pModule->nActiveDevices];
		pModule->szActiveDevices[pModule->nActiveDevices] = nullptr;
		*pDevice = {};
		return SIR_GRAPHICS_ABI_SUCCESS;
	}
	return SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
}
