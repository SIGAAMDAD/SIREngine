/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/Drivers/D3D11/SIRD3D11ComOwner.h>
#include <stdio.h>
#include <stdint.h>
#include <type_traits>

struct sirD3D11MockCom_t
{
	uint32_t *pReleases;
	void Release( void ) noexcept
	{
		++*pReleases;
	}
};

static_assert( !std::is_copy_constructible<SIRD3D11ComOwner<sirD3D11MockCom_t>>::value, "COM owner must be unique" );
static_assert( !std::is_move_constructible<SIRD3D11ComOwner<sirD3D11MockCom_t>>::value, "COM owner cannot move implicitly" );

int main( void )
{
	uint32_t nReleasedA = 0;
	uint32_t nReleasedB = 0;
	sirD3D11MockCom_t nativeA = { &nReleasedA };
	sirD3D11MockCom_t nativeB = { &nReleasedB };
	bool bValid = true;
	{
		SIRD3D11ComOwner<sirD3D11MockCom_t> unpublished( &nativeA );
		bValid = bValid && unpublished.get() == &nativeA;
		unpublished.reset( &nativeA );
		bValid = bValid && nReleasedA == 0;
		unpublished.reset( &nativeB );
		bValid = bValid && nReleasedA == 1 && nReleasedB == 0;
		unpublished.reset();
		bValid = bValid && nReleasedB == 1 && unpublished.get() == nullptr;
		unpublished.reset();
		bValid = bValid && nReleasedB == 1;
	}
	{
		SIRD3D11ComOwner<sirD3D11MockCom_t> pending;
		*pending.put() = &nativeA;
		bValid = bValid && pending.get() == &nativeA;
		sirD3D11MockCom_t *pPublished = pending.release();
		bValid = bValid && pPublished == &nativeA && pending.get() == nullptr;
		bValid = bValid && nReleasedA == 1;
		pPublished->Release();
	}
	bValid = bValid && nReleasedA == 2;
	{
		SIRD3D11ComOwner<sirD3D11MockCom_t> rejected( &nativeA );
	}
	bValid = bValid && nReleasedA == 3;
	printf( "D3D11 RAII guard tests: %s\n", bValid ? "PASS" : "FAIL" );
	return bValid ? 0 : 1;
}
