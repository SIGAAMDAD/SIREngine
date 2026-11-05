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

#include <Resources/SIRProviderCompletion.h>

#include <stdio.h>
#include <stdlib.h>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static void testMultilaneCompletion( void ) noexcept
{
	sirProviderCompletionSet_t required = { 7, 3, 0x5u, 4, { 17, 0, 9, 0 } };
	sirProviderCompletionSet_t observed = { 7, 3, 0x5u, 4, { 17, 0, 8, 0 } };
	REQUIRE( sirProviderCompletionValid( required ) );
	REQUIRE( !sirProviderCompletionSatisfied( required, observed ) );
	observed.szCompletedValues[2] = 9;
	REQUIRE( sirProviderCompletionSatisfied( required, observed ) );
	observed.szCompletedValues[0] = 9;
	observed.szCompletedValues[2] = 17;
	REQUIRE( !sirProviderCompletionSatisfied( required, observed ) );
}

static void testGenerationAndMaskValidation( void ) noexcept
{
	sirProviderCompletionSet_t required = { 7, 3, 0x3u, 3, { 2, 4, 0, 0 } };
	sirProviderCompletionSet_t observed = { 7, 4, 0x3u, 3, { 20, 40, 0, 0 } };
	REQUIRE( !sirProviderCompletionSatisfied( required, observed ) );
	observed.nProviderGeneration = 3;
	observed.nProviderId = 9;
	REQUIRE( !sirProviderCompletionSatisfied( required, observed ) );
	observed.nProviderId = 7;
	observed.nLaneMask = 1u;
	REQUIRE( !sirProviderCompletionSatisfied( required, observed ) );
	observed.nLaneMask = 3u;
	REQUIRE( sirProviderCompletionSatisfied( required, observed ) );
	observed.nLaneMask = 8u;
	REQUIRE( !sirProviderCompletionValid( observed ) );
	observed.nLaneMask = 3u;
	observed.szCompletedValues[3] = 1;
	REQUIRE( !sirProviderCompletionValid( observed ) );
	observed.szCompletedValues[3] = 0;
	observed.nLaneCount = 0;
	REQUIRE( !sirProviderCompletionValid( observed ) );
	observed.nLaneCount = 5;
	REQUIRE( !sirProviderCompletionValid( observed ) );
	observed.nLaneCount = 3;
	observed.nProviderId = 0;
	REQUIRE( !sirProviderCompletionValid( observed ) );
	observed.nProviderId = 7;
	observed.nProviderGeneration = 0;
	REQUIRE( !sirProviderCompletionValid( observed ) );
}

int main( void )
{
	testMultilaneCompletion();
	testGenerationAndMaskValidation();
	puts( "PASS: resource-neutral multiqueue completion and generation rejection" );
	return 0;
}
