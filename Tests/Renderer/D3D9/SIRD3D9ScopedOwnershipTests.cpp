/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/D3D9/SIRD3D9ScopedOwnership.h>
#ifdef _WIN32
#include <stdlib.h>
#include <type_traits>

struct sirD3D9MockRef_t
{
	uint32_t *pReleaseCount;
	ULONG Release( void ) noexcept
	{
		++*pReleaseCount;
		return 0;
	}
};

static void sirD3D9Require( bool bCondition )
{
	if ( !bCondition ) {
		abort();
	}
}

int main( void )
{
	static_assert( !std::is_copy_constructible<SIRD3D9ScopedComRef<sirD3D9MockRef_t>>::value );
	static_assert( !std::is_copy_assignable<SIRD3D9ScopedComRef<sirD3D9MockRef_t>>::value );
	static_assert( !std::is_copy_constructible<SIRD3D9ScopedModule>::value );
	static_assert( sizeof( SIRD3D9ScopedComRef<sirD3D9MockRef_t> ) == sizeof( void * ) );

	uint32_t nReleasedA = 0;
	uint32_t nReleasedB = 0;
	sirD3D9MockRef_t a = { &nReleasedA };
	sirD3D9MockRef_t b = { &nReleasedB };
	{
		SIRD3D9ScopedComRef<sirD3D9MockRef_t> ref( &a );
		sirD3D9Require( ref.get() == &a );
		ref.reset( &b );
		sirD3D9Require( nReleasedA == 1 && nReleasedB == 0 );
		sirD3D9MockRef_t *pTransferred = ref.detach();
		sirD3D9Require( pTransferred == &b && ref.get() == nullptr );
		pTransferred->Release();
	}
	sirD3D9Require( nReleasedA == 1 && nReleasedB == 1 );
	{
		SIRD3D9ScopedComRef<sirD3D9MockRef_t> ref( &a );
		*ref.put() = &b;
		sirD3D9Require( nReleasedA == 2 );
	}
	sirD3D9Require( nReleasedB == 2 );
	{
		SIRD3D9ScopedModule library( LoadLibraryW( L"kernel32.dll" ) );
		sirD3D9Require( library.get() != nullptr );
		HMODULE hTransferred = library.detach();
		sirD3D9Require( library.get() == nullptr && hTransferred != nullptr );
		library.reset( hTransferred );
	}
	return 0;
}
#endif
