/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/OpenGL/SIRGLScopedOwnership.h>
#include <assert.h>
#include <stdlib.h>
#include <new>

struct sirGLTestDispatch_t
{
	uint32_t nReclaimed = 0;
	uint32_t nLastName = 0;
};

static sirGraphicsResult_t sirGLTestDestroy( const sirGLTestDispatch_t *pDispatch, uint32_t nName ) noexcept
{
	sirGLTestDispatch_t *pMutable = ( sirGLTestDispatch_t * )pDispatch;
	++pMutable->nReclaimed;
	pMutable->nLastName = nName;
	return SIR_GRAPHICS_SUCCESS;
}

struct sirGLTestObject_t final
{
	uint32_t *pCount;
	explicit sirGLTestObject_t( uint32_t *pValue ) noexcept : pCount( pValue )
	{
	}
	~sirGLTestObject_t( void ) noexcept
	{
		++*pCount;
	}
	sirGLTestObject_t( const sirGLTestObject_t& ) = delete;
	sirGLTestObject_t& operator=( const sirGLTestObject_t& ) = delete;
};

static uint32_t s_nHostAllocations = 0;
static uint32_t s_nHostFrees = 0;
static void *sirGLTestAllocate( void *, uint64_t nBytes, uint64_t ) noexcept
{
	++s_nHostAllocations;
	return malloc( ( size_t )nBytes );
}
static void sirGLTestFree( void *, void *pMemory ) noexcept
{
	++s_nHostFrees;
	free( pMemory );
}

int main( void )
{
	sirGLTestDispatch_t dispatch = {};
	{
		SIRGLScopedName<sirGLTestDispatch_t, sirGLTestDestroy> owned( &dispatch );
		owned.adopt( 101 );
	}
	assert( dispatch.nReclaimed == 1 && dispatch.nLastName == 101 );

	{
		SIRGLScopedName<sirGLTestDispatch_t, sirGLTestDestroy> transferred( &dispatch );
		transferred.adopt( 102 );
		assert( transferred.release() == 102 );
	}
	assert( dispatch.nReclaimed == 1 );

	{
		SIRGLScopedName<sirGLTestDispatch_t, sirGLTestDestroy> empty( &dispatch );
	}
	assert( dispatch.nReclaimed == 1 );

	sirGraphicsABIHost_t host = {};
	host.pAllocate = sirGLTestAllocate;
	host.pFree = sirGLTestFree;
	uint32_t nDestructed = 0;
	void *pMemory = host.pAllocate( nullptr, sizeof( sirGLTestObject_t ), alignof( sirGLTestObject_t ) );
	assert( pMemory != nullptr );
	{
		sirGLTestObject_t *pObject = new ( pMemory ) sirGLTestObject_t( &nDestructed );
		SIRGLScopedHostObject<sirGLTestObject_t> owned( &host, pObject );
	}
	assert( nDestructed == 1 && s_nHostAllocations == s_nHostFrees );

	pMemory = host.pAllocate( nullptr, sizeof( sirGLTestObject_t ), alignof( sirGLTestObject_t ) );
	assert( pMemory != nullptr );
	sirGLTestObject_t *pObject = new ( pMemory ) sirGLTestObject_t( &nDestructed );
	{
		SIRGLScopedHostObject<sirGLTestObject_t> published( &host, pObject );
		assert( published.release() == pObject );
	}
	assert( nDestructed == 1 && s_nHostAllocations == s_nHostFrees + 1 );
	pObject->~sirGLTestObject_t();
	host.pFree( nullptr, pObject );
	assert( nDestructed == 2 && s_nHostAllocations == s_nHostFrees );
	return 0;
}
