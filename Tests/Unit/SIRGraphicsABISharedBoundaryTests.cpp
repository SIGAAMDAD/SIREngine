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
#include <Renderer/SIRGraphicsValidation.h>
#include <stddef.h>
#include <stdio.h>
#include <type_traits>

// The native factory's CPU allocation and diagnostic configuration is not
// part of this shared, driver-neutral descriptor. Guard against restoring
// the historical D3D12-specific fork without an explicit ABI revision.
static_assert( SIR_GRAPHICS_DRIVER_ABI_MAJOR == 1U && SIR_GRAPHICS_DRIVER_ABI_MINOR == 0U, "shared graphics ABI version drift" );
static_assert( sizeof( sirGraphicsDeviceCreateDesc_t ) == 48, "shared native device descriptor layout drift" );
static_assert( offsetof( sirGraphicsDeviceCreateDesc_t, nRequiredFeatures ) == 8, "shared device feature offset drift" );
static_assert( offsetof( sirGraphicsDeviceCreateDesc_t, nResidentBudgetBytes ) == 16, "shared device budget offset drift" );
static_assert( offsetof( sirGraphicsDeviceCreateDesc_t, bHeadless ) == 40, "native device flags offset drift" );
static_assert( sizeof( sirGraphicsABIDeviceRequest_t ) == 56, "binary device request layout drift" );
static_assert( offsetof( sirGraphicsABIDeviceRequest_t, nRequiredFeatures ) == 24, "binary device feature offset drift" );
static_assert( sizeof( sirGraphicsHandle_t ) == 16, "graphics handle layout drift" );
static_assert( sizeof( sirGraphicsCompletion_t ) == 16, "graphics single-queue completion layout drift" );
static_assert( sizeof( sirGraphicsCompletionSet_t ) == 32, "graphics multi-queue completion layout drift" );
static_assert( offsetof( sirGraphicsCompletionSet_t, szQueueValues ) == 8, "graphics completion lanes offset drift" );

using sirDeviceCreationSignature_t = sirGraphicsResult_t ( SIRGraphicsDriver::* )( const sirGraphicsDeviceCreateDesc_t&,
                                                                                   SIRRenderingDevice ** ) noexcept;
using sirResourceRetirementSignature_t = sirGraphicsResult_t ( SIRRenderingDevice::* )( sirGraphicsHandle_t,
                                                                                        const sirGraphicsCompletionSet_t& ) noexcept;
using sirBinaryCreationSignature_t = uint32_t ( * )( void *, const sirGraphicsABIDeviceRequest_t *, sirGraphicsABIDevice_t * );
static_assert( std::is_same<decltype( &SIRGraphicsDriver::createDevice ), sirDeviceCreationSignature_t>::value,
               "shared native factory signature drift" );
static_assert( std::is_same<decltype( &SIRRenderingDevice::retireResource ), sirResourceRetirementSignature_t>::value,
               "shared resource retirement signature drift" );
static_assert( std::is_same<decltype( sirGraphicsABIDriver_t::pCreateDevice ), sirBinaryCreationSignature_t>::value,
               "binary factory signature drift" );

static bool expectValidity( const sirGraphicsCompletionSet_t& completion, uint32_t nGeneration, bool bValid, const char *pszCase ) noexcept
{
	if ( sirGraphicsIsCompletionSetValid( completion, nGeneration ) != bValid ) {
		fprintf( stderr, "graphics completion contract failure: %s\n", pszCase );
		return false;
	}
	return true;
}

static bool testCompletionLanes( void ) noexcept
{
	sirGraphicsCompletionSet_t completion = {};
	completion.nDeviceGeneration = 7;
	completion.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 17;
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 9;
	if ( !expectValidity( completion, 7, true, "independent direct and copy lanes" ) ||
	     !expectValidity( completion, 8, false, "stale device generation" ) ) {
		return false;
	}

	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 0;
	if ( !expectValidity( completion, 7, false, "named lane has no completion value" ) ) {
		return false;
	}
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 9;
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] = 1;
	if ( !expectValidity( completion, 7, false, "unnamed queue carries a completion value" ) ) {
		return false;
	}
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] = 0;
	completion.nQueueMask |= 1U << 31;
	if ( !expectValidity( completion, 7, false, "queue mask includes an undefined lane" ) ) {
		return false;
	}
	completion.nQueueMask &= ~( 1U << 31 );
	completion.nDeviceGeneration = 0;
	return expectValidity( completion, 7, false, "zero device generation" );
}

int main( void )
{
	if ( !testCompletionLanes() ) {
		return 1;
	}
	puts( "PASS: shared graphics ABI v1 factory layouts, signatures and completion lanes" );
	return 0;
}
