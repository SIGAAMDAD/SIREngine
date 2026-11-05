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
#include <Renderer/SIRGraphicsModule.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>

static_assert( SIR_GRAPHICS_DRIVER_ABI_MAJOR == 1U && SIR_GRAPHICS_DRIVER_ABI_MINOR == 0U, "shared ABI version is 1.0" );
static_assert( sizeof( sirGraphicsHandle_t ) == 16, "handle ABI drift" );
static_assert( sizeof( sirGraphicsCompletion_t ) == 16, "completion ABI drift" );
static_assert( sizeof( sirGraphicsABIBufferRange_t ) == 40, "buffer range ABI drift" );
static_assert( sizeof( sirGraphicsABIDraw_t ) == 16, "draw ABI drift" );
static_assert( sizeof( sirGraphicsABIDrawIndexed_t ) == 24, "indexed draw ABI drift" );
static_assert( sizeof( sirGraphicsViewDesc_t ) == 56, "view ABI drift" );
static_assert( offsetof( sirGraphicsViewDesc_t, nBufferOffsetBytes ) == 40, "buffer view offset drift" );
static_assert( offsetof( sirGraphicsViewDesc_t, nBufferSizeBytes ) == 48, "buffer view size drift" );
static_assert( offsetof( sirGraphicsABIDevice_t, pDevice ) == 8, "device table drift" );
static_assert( offsetof( sirGraphicsABICommandList_t, pCommands ) == 16, "command table drift" );
static_assert( std::is_standard_layout<sirGraphicsABIDriver_t>::value, "driver table layout" );
static_assert( std::is_trivially_copyable<sirGraphicsABIDriver_t>::value, "driver table copy" );
static_assert( std::is_standard_layout<sirGraphicsABIDevice_t>::value, "device table layout" );
static_assert( std::is_trivially_copyable<sirGraphicsABIDevice_t>::value, "device table copy" );
static_assert( std::is_standard_layout<sirGraphicsABIPipeline_t>::value, "pipeline record layout" );
static_assert( std::is_trivially_copyable<sirGraphicsABIPipeline_t>::value, "pipeline record copy" );
static_assert( std::is_standard_layout<sirGraphicsABIFixedRecipe_t>::value, "fixed pipeline layout" );
static_assert( std::is_trivially_copyable<sirGraphicsABIFixedRecipe_t>::value, "fixed pipeline copy" );

static uint32_t s_nAssertions = 0;

static bool check( bool bValue, const char *pszCase ) noexcept
{
	++s_nAssertions;
	if ( !bValue ) {
		fprintf( stderr, "ABI layout / version failure: %s\n", pszCase );
		return false;
	}
	return true;
}

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

static bool testHostVersion( void ) noexcept
{
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = testAllocate;
	host.pFree = testFree;
	host.pLog = testLog;
	if ( !check( sirGraphicsValidateABIHost( &host ) == SIR_GRAPHICS_SUCCESS, "current version" ) ) {
		return false;
	}
	host.nMinor = 1;
	if ( !check( sirGraphicsValidateABIHost( &host ) == SIR_GRAPHICS_SUCCESS, "forward minor" ) ) {
		return false;
	}
	host.nMajor = 2;
	if ( !check( sirGraphicsValidateABIHost( &host ) == SIR_GRAPHICS_VERSION_MISMATCH, "wrong major" ) ) {
		return false;
	}
	host.nMajor = 1;
	host.nSize = sizeof( host ) - 1;
	if ( !check( sirGraphicsValidateABIHost( &host ) == SIR_GRAPHICS_INVALID_ARGUMENT, "short host" ) ) {
		return false;
	}
	host.nSize = sizeof( host );
	host.nReserved = 1;
	return check( sirGraphicsValidateABIHost( &host ) == SIR_GRAPHICS_INVALID_ARGUMENT, "reserved host field" );
}

static bool testAdapterRecords( void ) noexcept
{
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	adapter.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	adapter.nKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	adapter.nApiMajor = 1;
	adapter.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	adapter.nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	if ( !check( sirGraphicsValidateABIAdapter( &adapter ) == SIR_GRAPHICS_SUCCESS, "valid software adapter" ) ) {
		return false;
	}
	adapter.nFeatureMask = 1ULL << 63;
	if ( !check( sirGraphicsValidateABIAdapter( &adapter ) == SIR_GRAPHICS_INVALID_ARGUMENT, "unknown features" ) ) {
		return false;
	}
	adapter.nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	adapter.nQueueMask = 1U << 31;
	if ( !check( sirGraphicsValidateABIAdapter( &adapter ) == SIR_GRAPHICS_INVALID_ARGUMENT, "unknown queue" ) ) {
		return false;
	}
	adapter.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	adapter.nKind = 0;
	if ( !check( sirGraphicsValidateABIAdapter( &adapter ) == SIR_GRAPHICS_INVALID_ARGUMENT, "unknown adapter kind" ) ) {
		return false;
	}
	adapter.nKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	adapter.nSize = 1;
	return check( sirGraphicsValidateABIAdapter( &adapter ) == SIR_GRAPHICS_INVALID_ARGUMENT, "short adapter" );
}

static bool testFormatRecords( void ) noexcept
{
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	format.nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
	format.nSampleCountMask = 1;
	if ( !check( sirGraphicsValidateABIFormat( &format ) == SIR_GRAPHICS_SUCCESS, "valid image format" ) ) {
		return false;
	}
	format.nSampleCountMask = 0;
	if ( !check( sirGraphicsValidateABIFormat( &format ) == SIR_GRAPHICS_INVALID_ARGUMENT, "format samples absent" ) ) {
		return false;
	}
	format.nSampleCountMask = 1;
	format.nOptimalFeatures = 1U << 31;
	if ( !check( sirGraphicsValidateABIFormat( &format ) == SIR_GRAPHICS_INVALID_ARGUMENT, "unknown format feature" ) ) {
		return false;
	}
	format.nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
	format.nReserved = 1;
	return check( sirGraphicsValidateABIFormat( &format ) == SIR_GRAPHICS_INVALID_ARGUMENT, "reserved format field" );
}

static bool testModelIsolation( void ) noexcept
{
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.nSampleCount = 1;
	sirGraphicsABIFixedRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	pipeline.pFixed = &recipe;
	if ( !check( sirGraphicsValidateABIPipelineStructure( &pipeline ) == SIR_GRAPHICS_SUCCESS, "fixed shaderless recipe" ) ) {
		return false;
	}
	pipeline.pModern = ( const sirGraphicsABIModernRecipe_t * )&recipe;
	if ( !check( sirGraphicsValidateABIPipelineStructure( &pipeline ) == SIR_GRAPHICS_INVALID_ARGUMENT, "conflicting recipe" ) ) {
		return false;
	}
	pipeline.pModern = nullptr;
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	if ( !check( sirGraphicsValidateABIPipelineStructure( &pipeline ) == SIR_GRAPHICS_INVALID_ARGUMENT, "wrong model tag" ) ) {
		return false;
	}
	pipeline.nModel = 0;
	return check( sirGraphicsValidateABIPipelineStructure( &pipeline ) == SIR_GRAPHICS_INVALID_ARGUMENT, "untyped recipe" );
}

int main( void )
{
	if ( !testHostVersion() || !testAdapterRecords() || !testFormatRecords() || !testModelIsolation() ) {
		return 1;
	}
	printf( "PASS: ABI 1.0 layout, negotiation, format, adapter and pipeline records (%u assertions)\n", s_nAssertions );
	return 0;
}
