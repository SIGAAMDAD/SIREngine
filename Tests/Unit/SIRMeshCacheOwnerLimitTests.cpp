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

#include <Mesh/SIRMeshCache.h>

#include <stdio.h>
#include <stdlib.h>

static void requireOwner( bool bCondition, const char *pszReason ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "owner limit FAILED: %s\n", pszReason );
		abort();
	}
}

int main( void )
{
	alignas( 64 ) uint8_t szStorageA[4096] = {};
	alignas( 64 ) uint8_t szStorageB[4096] = {};
	SIRMeshCache first;
	SIRMeshCache second;
	requireOwner( first.initialize( szStorageA, sizeof( szStorageA ), 1 ) == SIR_MESH_OK, "last available owner token accepted" );
	requireOwner( second.initialize( szStorageB, sizeof( szStorageB ), 1 ) == SIR_MESH_OUT_OF_CAPACITY,
	              "owner-token exhaustion fails closed" );
	requireOwner( second.getCapacity() == 0 && second.getResidentCount() == 0, "failed cache initialization remains empty" );
	requireOwner( first.shutdown() == SIR_MESH_OK, "first cache shutdown" );
	requireOwner( second.initialize( szStorageB, sizeof( szStorageB ), 1 ) == SIR_MESH_OUT_OF_CAPACITY,
	              "owner tokens cannot be recycled after shutdown" );
	return 0;
}
