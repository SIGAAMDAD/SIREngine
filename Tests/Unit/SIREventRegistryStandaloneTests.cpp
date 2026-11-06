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

#include <Core/Events/SIREventRegistry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIR_REGISTRY_CHECK( expression )                                                                                                   \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Event registry failure %s:%d: %s\n", __FILE__, __LINE__, #expression );                                      \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static void sirTestEventRegistration( void )
{
	SIREventRegistry registry;
	SIR_REGISTRY_CHECK( registry.count() == 0 );

	sirEventHandle_t handle = { 99, 99 };
	SIR_REGISTRY_CHECK( !registry.registerEvent( 0, "invalid", 4, 4, handle ) );
	SIR_REGISTRY_CHECK( handle.nSlot == 0 && handle.nGeneration == 0 );
	SIR_REGISTRY_CHECK( !registry.registerEvent( 1, nullptr, 4, 4, handle ) );
	SIR_REGISTRY_CHECK( !registry.registerEvent( 1, "invalid", 0, 4, handle ) );
	SIR_REGISTRY_CHECK( !registry.registerEvent( 1, "invalid", 4, 0, handle ) );
	SIR_REGISTRY_CHECK( !registry.registerEvent( 1, "invalid", 4, 3, handle ) );
	SIR_REGISTRY_CHECK( !registry.registerEvent( 1, "", 4, 4, handle ) );
	char szTooLong[49];
	memset( szTooLong, 'x', sizeof( szTooLong ) - 1 );
	szTooLong[sizeof( szTooLong ) - 1] = '\0';
	SIR_REGISTRY_CHECK( !registry.registerEvent( 1, szTooLong, 4, 4, handle ) );
	SIR_REGISTRY_CHECK( registry.count() == 0 );

	SIR_REGISTRY_CHECK( registry.registerEvent( 42, "damage", 4, 4, handle ) );
	SIR_REGISTRY_CHECK( handle.nSlot == 1 && handle.nGeneration == 1 && registry.count() == 1 );
	sirEventHandle_t found = {};
	SIR_REGISTRY_CHECK( registry.findByType( 42, found ) );
	SIR_REGISTRY_CHECK( found.nSlot == handle.nSlot && found.nGeneration == handle.nGeneration );
	SIR_REGISTRY_CHECK( !registry.findByType( 43, found ) && found.nSlot == 0 );
	sirRegisteredEvent_t descriptor = {};
	SIR_REGISTRY_CHECK( registry.getDescriptor( handle, descriptor ) );
	SIR_REGISTRY_CHECK( descriptor.nTypeId == 42 && descriptor.nPayloadBytes == 4 && descriptor.nPayloadAlignment == 4 );
	SIR_REGISTRY_CHECK( strcmp( descriptor.szName, "damage" ) == 0 );
	SIR_REGISTRY_CHECK( !registry.registerEvent( 42, "different", 4, 4, found ) );
	SIR_REGISTRY_CHECK( !registry.registerEvent( 44, "damage", 4, 4, found ) );

	SIR_REGISTRY_CHECK( !registry.getDescriptor( { 0, 1 }, descriptor ) );
	SIR_REGISTRY_CHECK( !registry.getDescriptor( { SIREventRegistry::MAX_EVENTS + 1, 1 }, descriptor ) );
	SIR_REGISTRY_CHECK( !registry.getDescriptor( { handle.nSlot, handle.nGeneration + 1 }, descriptor ) );
	SIR_REGISTRY_CHECK( !registry.unregisterEvent( { 0, 1 } ) );
	SIR_REGISTRY_CHECK( !registry.unregisterEvent( { SIREventRegistry::MAX_EVENTS + 1, 1 } ) );
	SIR_REGISTRY_CHECK( !registry.unregisterEvent( { handle.nSlot, handle.nGeneration + 1 } ) );
	SIR_REGISTRY_CHECK( registry.unregisterEvent( handle ) );
	SIR_REGISTRY_CHECK( !registry.unregisterEvent( handle ) );
	SIR_REGISTRY_CHECK( !registry.getDescriptor( handle, descriptor ) );
	SIR_REGISTRY_CHECK( registry.count() == 0 );

	sirEventHandle_t recycled = {};
	SIR_REGISTRY_CHECK( registry.registerEvent( 42, "damage", 4, 4, recycled ) );
	SIR_REGISTRY_CHECK( recycled.nSlot == handle.nSlot && recycled.nGeneration != handle.nGeneration );
	registry.clear();
	SIR_REGISTRY_CHECK( registry.count() == 0 );
	SIR_REGISTRY_CHECK( !registry.findByType( 42, found ) );
	SIR_REGISTRY_CHECK( !registry.getDescriptor( recycled, descriptor ) );
}

static void sirTestEventCapacity( void )
{
	SIREventRegistry registry;
	sirEventHandle_t handles[SIREventRegistry::MAX_EVENTS] = {};
	char szNames[SIREventRegistry::MAX_EVENTS][16] = {};
	for ( uint32_t iSlot = 0; iSlot < SIREventRegistry::MAX_EVENTS; ++iSlot ) {
		SIR_REGISTRY_CHECK( snprintf( szNames[iSlot], sizeof( szNames[iSlot] ), "event_%u", iSlot ) > 0 );
		SIR_REGISTRY_CHECK( registry.registerEvent( iSlot + 1, szNames[iSlot], 4, 4, handles[iSlot] ) );
		SIR_REGISTRY_CHECK( handles[iSlot].nSlot == iSlot + 1 );
	}
	SIR_REGISTRY_CHECK( registry.count() == SIREventRegistry::MAX_EVENTS );
	sirEventHandle_t rejected = { 123, 123 };
	SIR_REGISTRY_CHECK( !registry.registerEvent( 100, "overflow", 4, 4, rejected ) );
	SIR_REGISTRY_CHECK( rejected.nSlot == 0 );
	SIR_REGISTRY_CHECK( registry.unregisterEvent( handles[3] ) );
	SIR_REGISTRY_CHECK( registry.registerEvent( 100, "reuse", 4, 4, rejected ) );
	SIR_REGISTRY_CHECK( rejected.nSlot == handles[3].nSlot && rejected.nGeneration != handles[3].nGeneration );
	registry.clear();
	SIR_REGISTRY_CHECK( registry.count() == 0 );
	for ( uint32_t iSlot = 0; iSlot < SIREventRegistry::MAX_EVENTS; ++iSlot ) {
		SIR_REGISTRY_CHECK( !registry.unregisterEvent( handles[iSlot] ) );
	}
}

int main( void )
{
	sirTestEventRegistration();
	sirTestEventCapacity();
	puts( "PASS: event registry validation and lifecycle" );
	return 0;
}
