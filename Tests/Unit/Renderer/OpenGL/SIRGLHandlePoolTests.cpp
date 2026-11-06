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

#include <Renderer/Drivers/OpenGL/SIRGLHandlePool.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t s_nFailures = 0;
static uint32_t s_nOutstanding = 0;
static bool s_bFailAllocation = false;

static void *sirGLTestAllocate( void *, uint64_t nBytes, uint64_t ) noexcept
{
	if ( s_bFailAllocation ) {
		return nullptr;
	}
	void *pResult = malloc( ( size_t )nBytes );
	if ( pResult != nullptr ) {
		++s_nOutstanding;
	}
	return pResult;
}

static void sirGLTestFree( void *, void *pMemory ) noexcept
{
	if ( pMemory != nullptr ) {
		--s_nOutstanding;
		free( pMemory );
	}
}

static void sirGLCheck( bool bCondition, const char *pszMessage ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "FAIL %s\n", pszMessage );
		++s_nFailures;
	}
}

int main( void )
{
	sirGraphicsABIHost_t host = {};
	host.pAllocate = sirGLTestAllocate;
	host.pFree = sirGLTestFree;
	sirGLHandlePool_t pool = {};
	sirGLCheck( sirGLHandlePoolInit( nullptr, &host, 1, 2 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null pool" );
	sirGLCheck( sirGLHandlePoolInit( &pool, &host, 0, 2 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero device gen" );
	sirGLCheck( sirGLHandlePoolInit( &pool, &host, 1, 0 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero capacity" );
	sirGLCheck( sirGLHandlePoolInit( &pool, &host, 1, UINT32_MAX ) == SIR_GRAPHICS_CAPACITY_EXCEEDED, "overflow" );
	s_bFailAllocation = true;
	sirGLCheck( sirGLHandlePoolInit( &pool, &host, 1, 2 ) == SIR_GRAPHICS_OUT_OF_MEMORY, "host OOM" );
	s_bFailAllocation = false;
	sirGLCheck( sirGLHandlePoolInit( &pool, &host, 41, 2 ) == SIR_GRAPHICS_SUCCESS, "initialize" );
	sirGLCheck( sirGLHandlePoolInit( &pool, &host, 41, 2 ) == SIR_GRAPHICS_BUSY, "double init" );
	sirGLNativeResource_t resource = {};
	resource.nName = 11;
	resource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	resource.nRequestedBytes = 2048;
	sirGraphicsHandle_t first = {};
	sirGraphicsHandle_t second = {};
	sirGraphicsHandle_t third = {};
	sirGLCheck( sirGLHandlePoolInsert( &pool, &resource, &first ) == SIR_GRAPHICS_SUCCESS, "insert first" );
	resource.nName = 12;
	sirGLCheck( sirGLHandlePoolInsert( &pool, &resource, &second ) == SIR_GRAPHICS_SUCCESS, "insert second" );
	sirGLCheck( sirGLHandlePoolInsert( &pool, &resource, &third ) == SIR_GRAPHICS_CAPACITY_EXCEEDED, "pool full" );
	sirGLCheck( sirGLHandlePoolShutdown( &pool ) == SIR_GRAPHICS_BUSY, "cannot leak live resources" );
	sirGLNativeResource_t found = {};
	sirGLCheck( sirGLHandlePoolFind( &pool, first, &found ) == SIR_GRAPHICS_SUCCESS && found.nName == 11, "lookup" );
	sirGraphicsHandle_t wrong = first;
	wrong.nDeviceGeneration = 42;
	sirGLCheck( sirGLHandlePoolFind( &pool, wrong, &found ) == SIR_GRAPHICS_INVALID_HANDLE, "foreign device" );
	wrong = first;
	wrong.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	sirGLCheck( sirGLHandlePoolFind( &pool, wrong, &found ) == SIR_GRAPHICS_INVALID_HANDLE, "wrong type" );
	wrong = first;
	wrong.nSlot = 50;
	sirGLCheck( sirGLHandlePoolFind( &pool, wrong, &found ) == SIR_GRAPHICS_INVALID_HANDLE, "wrong slot" );
	sirGLCheck( sirGLHandlePoolTake( &pool, first, &found ) == SIR_GRAPHICS_SUCCESS && found.nName == 11, "take ownership" );
	sirGLCheck( sirGLHandlePoolFind( &pool, first, &found ) == SIR_GRAPHICS_INVALID_HANDLE, "stale rejected" );
	sirGLCheck( sirGLHandlePoolTake( &pool, first, &found ) == SIR_GRAPHICS_INVALID_HANDLE, "double retire" );
	resource.nName = 13;
	sirGLCheck( sirGLHandlePoolInsert( &pool, &resource, &third ) == SIR_GRAPHICS_SUCCESS && third.nSlot == first.nSlot &&
	                third.nGeneration == first.nGeneration + 1,
	            "reuse bumps generation" );
	sirGLCheck( sirGLHandlePoolTake( &pool, second, &found ) == SIR_GRAPHICS_SUCCESS, "take second" );
	sirGLCheck( sirGLHandlePoolTake( &pool, third, &found ) == SIR_GRAPHICS_SUCCESS, "take reused" );
	sirGLCheck( sirGLHandlePoolShutdown( &pool ) == SIR_GRAPHICS_SUCCESS, "shutdown" );
	sirGLCheck( sirGLHandlePoolShutdown( &pool ) == SIR_GRAPHICS_SUCCESS, "idempotent shutdown" );
	sirGLCheck( s_nOutstanding == 0, "no allocations left" );
	sirGLCheck( sirGLHandlePoolInit( &pool, &host, 55, 3 ) == SIR_GRAPHICS_SUCCESS, "dependent pool initialized" );
	sirGLNativeResource_t parent = {};
	parent.nName = 81;
	parent.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	parent.nRequestedBytes = 4096;
	sirGraphicsHandle_t hParent = {};
	sirGLCheck( sirGLHandlePoolInsert( &pool, &parent, &hParent ) == SIR_GRAPHICS_SUCCESS, "texture parent inserted" );
	sirGLNativeResource_t child = {};
	child.nName = 82;
	child.eType = SIR_GRAPHICS_HANDLE_VIEW;
	child.hParent = hParent;
	sirGraphicsHandle_t hView = {};
	sirGLCheck( sirGLHandlePoolInsert( &pool, &child, &hView ) == SIR_GRAPHICS_SUCCESS, "dependent view inserted" );
	sirGLCheck( pool.pSlots[hParent.nSlot].nChildCount == 1, "parent reference retained" );
	sirGLCheck( sirGLHandlePoolTake( &pool, hParent, &found ) == SIR_GRAPHICS_BUSY, "live view prevents parent retirement" );
	sirGLCheck( sirGLHandlePoolShutdown( &pool ) == SIR_GRAPHICS_BUSY, "dependent resources prevent shutdown" );
	sirGLCheck( sirGLHandlePoolTake( &pool, hView, &found ) == SIR_GRAPHICS_SUCCESS, "view retirement" );
	sirGLCheck( pool.pSlots[hParent.nSlot].nChildCount == 0, "view releases parent reference" );
	sirGLCheck( sirGLHandlePoolInsert( &pool, &child, &hView ) == SIR_GRAPHICS_SUCCESS, "view slot reusable" );
	sirGLCheck( sirGLHandlePoolTake( &pool, hView, &found ) == SIR_GRAPHICS_SUCCESS, "second view retirement" );
	sirGLCheck( sirGLHandlePoolTake( &pool, hParent, &found ) == SIR_GRAPHICS_SUCCESS, "parent retired after views" );
	sirGLCheck( sirGLHandlePoolInsert( &pool, &child, &hView ) == SIR_GRAPHICS_INVALID_HANDLE, "stale parent rejected" );
	child.hParent = {};
	sirGLCheck( sirGLHandlePoolInsert( &pool, &child, &hView ) == SIR_GRAPHICS_INVALID_HANDLE, "missing parent rejected" );
	sirGLCheck( sirGLHandlePoolShutdown( &pool ) == SIR_GRAPHICS_SUCCESS, "dependent pool shutdown" );
	sirGLCheck( s_nOutstanding == 0, "dependent pool allocation released" );
	if ( s_nFailures == 0 ) {
		printf( "GL46 generation-safe handle pool tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
