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
#include <Renderer/SIRGraphicsTypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t s_nReleases = 0;
static uint32_t s_nFactoryMode = 0;

static void *testAllocate( void *, uint64_t nBytes, uint64_t ) noexcept
{
	return malloc( ( size_t )nBytes );
}

static void testFree( void *, void *pMemory ) noexcept
{
	free( pMemory );
}

static void testLog( void *, uint32_t, const char * ) noexcept
{
}

static uint32_t testCount( void *, uint32_t *pCount ) noexcept
{
	*pCount = 1;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t testAdapter( void *, sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	pAdapter->nIndex = 0;
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static uint32_t testCreate( void *, const sirGraphicsABIDeviceRequest_t *, sirGraphicsABIDevice_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_UNSUPPORTED;
}

static uint32_t testDestroy( void *, sirGraphicsABIDevice_t * ) noexcept
{
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static void testRelease( void * ) noexcept
{
	++s_nReleases;
}

static uint32_t testFactory( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *, sirGraphicsABIDriver_t *pDriver ) noexcept
{
	if ( nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || nMinor != SIR_GRAPHICS_DRIVER_ABI_MINOR ) {
		return SIR_GRAPHICS_ABI_VERSION_MISMATCH;
	}
	pDriver->nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	pDriver->nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	pDriver->nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	pDriver->pDriver = pDriver;
	pDriver->pAdapterCount = testCount;
	pDriver->pQueryAdapter = testAdapter;
	pDriver->pCreateDevice = testCreate;
	pDriver->pDestroyDevice = testDestroy;
	pDriver->pReleaseDriver = testRelease;
	if ( s_nFactoryMode == 1 ) {
		pDriver->pCreateDevice = nullptr;
	} else if ( s_nFactoryMode == 2 ) {
		pDriver->nMajor = 999;
	} else if ( s_nFactoryMode == 3 ) {
		return SIR_GRAPHICS_ABI_UNSUPPORTED;
	}
	return SIR_GRAPHICS_ABI_SUCCESS;
}

static bool testNegotiation( void ) noexcept
{
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = testAllocate;
	host.pFree = testFree;
	host.pLog = testLog;
	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );

	if ( sirGraphicsNegotiateDriverABI( nullptr, &host, &driver ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	host.nMajor = 2;
	if ( sirGraphicsNegotiateDriverABI( testFactory, &host, &driver ) != SIR_GRAPHICS_ABI_VERSION_MISMATCH ) {
		return false;
	}
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.pFree = nullptr;
	if ( sirGraphicsNegotiateDriverABI( testFactory, &host, &driver ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	host.pFree = testFree;
	s_nFactoryMode = 1;
	if ( sirGraphicsNegotiateDriverABI( testFactory, &host, &driver ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ||
	     driver.pDriver != nullptr || s_nReleases != 1 ) {
		return false;
	}
	s_nFactoryMode = 2;
	if ( sirGraphicsNegotiateDriverABI( testFactory, &host, &driver ) != SIR_GRAPHICS_ABI_VERSION_MISMATCH ||
	     driver.pDriver != nullptr || s_nReleases != 2 ) {
		return false;
	}
	s_nFactoryMode = 3;
	if ( sirGraphicsNegotiateDriverABI( testFactory, &host, &driver ) != SIR_GRAPHICS_ABI_UNSUPPORTED ||
	     driver.pDriver != nullptr ) {
		return false;
	}
	s_nFactoryMode = 0;
	if ( sirGraphicsNegotiateDriverABI( testFactory, &host, &driver ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     driver.pDriver == nullptr || driver.pCreateDevice == nullptr ) {
		return false;
	}
	driver.pReleaseDriver( driver.pDriver );
	return s_nReleases == 3;
}

static bool testCapabilities( void ) noexcept
{
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	caps.nDeviceGeneration = 7;
	caps.nAdapterKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	caps.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nMaximumFramesInFlight = 2;
	caps.nMaximumSamples = 1;
	caps.nEnabledFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	uint32_t nProfile = 99;
	if ( sirGraphicsSelectABIProfile( &caps, &nProfile ) != SIR_GRAPHICS_ABI_SUCCESS || nProfile != 0 ) {
		return false;
	}
	caps.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_PRESENT;
	if ( sirGraphicsSelectABIProfile( &caps, &nProfile ) != SIR_GRAPHICS_ABI_SUCCESS || nProfile != 1 ) {
		return false;
	}
	caps.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE | SIR_GRAPHICS_FEATURE_MRT;
	caps.nMaximumColorAttachments = 4;
	caps.nMaximumBindings = 16;
	if ( sirGraphicsSelectABIProfile( &caps, &nProfile ) != SIR_GRAPHICS_ABI_SUCCESS || nProfile != 2 ) {
		return false;
	}
	caps.nEnabledFeatures = SIR_GRAPHICS_FEATURE_PRESENT;
	if ( sirGraphicsSelectABIProfile( &caps, &nProfile ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT || nProfile != 0 ) {
		return false;
	}
	caps.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nDeviceGeneration = 0;
	if ( sirGraphicsValidateABICapabilities( &caps ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	caps.nDeviceGeneration = 7;
	caps.nEnabledQueues = 1U << 9;
	return sirGraphicsValidateABICapabilities( &caps ) == SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
}

static bool testLayoutsAndFormats( void ) noexcept
{
	static_assert( sizeof( sirGraphicsHandle_t ) == 16, "handle ABI drift" );
	static_assert( sizeof( sirGraphicsCompletion_t ) == 16, "completion ABI drift" );
	static_assert( offsetof( sirGraphicsABIDriver_t, pDriver ) == 16, "binary driver prefix drift" );
	static_assert( offsetof( sirGraphicsABIAdapter_t, nFeatureMask ) == 56, "binary adapter prefix drift" );
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	format.nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
	format.nSampleCountMask = 1;
	if ( sirGraphicsValidateABIFormat( &format ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
	}
	format.nSampleCountMask = 0;
	if ( sirGraphicsValidateABIFormat( &format ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	format.nSampleCountMask = 1;
	format.nFormat = 999;
	return sirGraphicsValidateABIFormat( &format ) == SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
}

int main( void )
{
	if ( !testNegotiation() ) {
		fputs( "graphics.abi.negotiation FAILED\n", stderr );
		return 1;
	}
	if ( !testCapabilities() ) {
		fputs( "graphics.abi.capabilities FAILED\n", stderr );
		return 2;
	}
	if ( !testLayoutsAndFormats() ) {
		fputs( "graphics.abi.formats FAILED\n", stderr );
		return 3;
	}
	puts( "PASS: shared graphics ABI negotiation, layouts, capability profiles and negative cases" );
	return 0;
}
