/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/SIRGraphicsDriverABI.h>
#ifdef _WIN32
#include <windows.h>
#include <stddef.h>
#include <stdlib.h>

static uint32_t s_nAllocations = 0;
static uint32_t s_nReleases = 0;

static void *sirD3D9HostAllocate( void *pContext, uint64_t nBytes, uint64_t nAlignment )
{
	( void )pContext;
	if ( nAlignment > alignof( max_align_t ) ) {
		return nullptr;
	}
	void *pMemory = malloc( ( size_t )nBytes );
	if ( pMemory != nullptr ) {
		++s_nAllocations;
	}
	return pMemory;
}

static void sirD3D9HostFree( void *pContext, void *pMemory )
{
	( void )pContext;
	++s_nReleases;
	free( pMemory );
}

static void sirD3D9HostLog( void *pContext, uint32_t nSeverity, const char *szMessage )
{
	( void )pContext;
	( void )nSeverity;
	( void )szMessage;
}

static void sirD3D9Require( bool bCondition )
{
	if ( !bCondition ) {
		abort();
	}
}

int main( void )
{
	HMODULE hLibrary = LoadLibraryW( L"SIRD3D9.dll" );
	sirD3D9Require( hLibrary != nullptr );
	sirGraphicsABIFactory_t pFactory = ( sirGraphicsABIFactory_t )GetProcAddress( hLibrary, "SIRGetGraphicsDriverABI" );
	sirD3D9Require( pFactory != nullptr );

	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = sirD3D9HostAllocate;
	host.pFree = sirD3D9HostFree;
	host.pLog = sirD3D9HostLog;

	for ( uint32_t i = 0; i < 32; ++i ) {
		sirGraphicsABIDriver_t driver = {};
		driver.nSize = sizeof( driver );
		sirD3D9Require( pFactory( SIR_GRAPHICS_DRIVER_ABI_MAJOR, SIR_GRAPHICS_DRIVER_ABI_MINOR, &host, &driver ) == SIR_GRAPHICS_SUCCESS );
		sirD3D9Require( driver.nBackend == SIR_GRAPHICS_BACKEND_D3D9 );
		sirD3D9Require( driver.pAdapterCount != nullptr && driver.pReleaseDriver != nullptr );

		uint32_t nCount = 0;
		const uint32_t nStatus = driver.pAdapterCount( driver.pDriver, &nCount );
		sirD3D9Require( nStatus == SIR_GRAPHICS_SUCCESS || nStatus == SIR_GRAPHICS_UNSUPPORTED );
		driver.pReleaseDriver( driver.pDriver );
	}
	sirD3D9Require( s_nAllocations == s_nReleases );
	sirD3D9Require( FreeLibrary( hLibrary ) != 0 );
	return 0;
}
#endif
