/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/D3D10/SIRD3D10Driver.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <Renderer/SIRGraphicsRegistry.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

static void sirD3D10Require( bool bCondition, const char *pszMsg ) noexcept
{
	if ( !bCondition ) {
		std::fprintf( stderr, "FAIL: %s\n", pszMsg );
		abort();
	}
	else {
		std::printf( "PASS: %s\n", pszMsg );
	}
}

static uint32_t s_nAllocations = 0;
static uint32_t s_nReleases = 0;
static bool s_bFailAllocation = false;

static void *sirD3D10TestAllocate( void *pContext, uint64_t nBytes, uint64_t nAlignment ) noexcept
{
	( void )pContext;
	sirD3D10Require( nAlignment <= alignof( std::max_align_t ), "alignment check" );
	if ( s_bFailAllocation ) {
		return nullptr;
	}
	void *pMemory = std::malloc( ( std::size_t )nBytes );
	if ( pMemory != nullptr ) {
		++s_nAllocations;
	}
	return pMemory;
}

static void sirD3D10TestFree( void *pContext, void *pMemory ) noexcept
{
	( void )pContext;
	sirD3D10Require( pMemory != nullptr, "free non-null" );
	++s_nReleases;
	std::free( pMemory );
}

static void sirD3D10TestLog( void *pContext, uint32_t nSeverity, const char *pszMessage ) noexcept
{
	( void )pContext; ( void )nSeverity;
	std::printf( "LOG[%u]: %s\n", nSeverity, pszMessage );
}

int main( void )
{
	static_assert( sizeof( void * ) == 8, "D3D10 module ABI requires x64" );
	static_assert( sizeof( sirGraphicsHandle_t ) == 16, "graphics resource handle ABI drift" );

	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = sirD3D10TestAllocate;
	host.pFree = sirD3D10TestFree;
	host.pLog = sirD3D10TestLog;

	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );

	// Test version negotiation
	sirD3D10Require( SIRGetGraphicsDriverABI( 2, 0, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH, "version mismatch major" );
	sirD3D10Require( s_nAllocations == 0, "no allocation on version mismatch" );
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 1, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH, "version mismatch minor" );
	sirD3D10Require( s_nAllocations == 0, "no allocation on version mismatch" );
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, nullptr, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null host" );
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null driver" );

	const uint32_t nHostSize = host.nSize;
	host.nSize = sizeof( host ) - 1;
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT, "host size too small" );
	host.nSize = nHostSize;
	driver.nSize = sizeof( driver ) - 1;
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT, "driver size too small" );
	driver.nSize = sizeof( driver );

	// Test allocation failure
	s_bFailAllocation = true;
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_DRIVER_ERROR, "allocation failure" );
	s_bFailAllocation = false;
	sirD3D10Require( s_nAllocations == 0, "no allocation on failure" );

	// Test successful negotiation
	struct
	{
		sirGraphicsABIDriver_t driver;
		uint64_t nCanary;
	} result = {};
	result.driver.nSize = sizeof( result );
	result.nCanary = UINT64_C( 0xA551D10A551D10A5 );
	sirD3D10Require( sirGraphicsNegotiateDriverABI( SIRGetGraphicsDriverABI, &host, &result.driver ) == SIR_GRAPHICS_SUCCESS, "negotiate success" );
	sirD3D10Require( result.nCanary == UINT64_C( 0xA551D10A551D10A5 ), "canary intact" );
	sirD3D10Require( result.driver.nBackend == SIR_GRAPHICS_BACKEND_D3D10, "backend D3D10" );
	sirD3D10Require( result.driver.nMajor == 1 && result.driver.nMinor == 0, "ABI version 1.0" );
	sirD3D10Require( sirGraphicsValidateABIDriver( &result.driver ) == SIR_GRAPHICS_SUCCESS, "driver validation" );
	sirD3D10Require( s_nAllocations == 1, "one allocation" );

	// Test adapter enumeration (will return 0 in headless/CI environment)
	uint32_t nAdapters = 99;
	sirD3D10Require( result.driver.pAdapterCount( result.driver.pDriver, &nAdapters ) == SIR_GRAPHICS_SUCCESS, "adapter count success" );
	// In CI/headless, we may have 0 adapters - that's OK
	sirD3D10Require( result.driver.pAdapterCount( result.driver.pDriver, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "adapter count null arg" );

	// Test device creation with invalid args
	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nFramesInFlight = 2;
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_UNSUPPORTED, "create device unsupported (no native impl in ABI)" );
	sirD3D10Require( device.pDevice == nullptr, "no device returned" );

	// Test invalid device creation args
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_UNSUPPORTED, "create device compute unsupported" );
	request.nRequiredFeatures = 0;
	request.nFramesInFlight = 0;
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_INVALID_ARGUMENT, "create device zero frames" );
	request.nFramesInFlight = 2;
	request.nReserved = 1;
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_INVALID_ARGUMENT, "create device reserved" );
	sirD3D10Require( result.driver.pDestroyDevice( result.driver.pDriver, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "destroy null device" );

	// Test release
	result.driver.pReleaseDriver( result.driver.pDriver );
	sirD3D10Require( s_nAllocations == s_nReleases, "all allocations freed" );

	std::printf( "\nAll D3D10 ABI conformance tests passed!\n" );
	return 0;
}