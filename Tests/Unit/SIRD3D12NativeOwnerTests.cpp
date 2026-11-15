/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with the
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/Drivers/D3D12/SIRD3D12Pipelines.h>
#include <type_traits>

static_assert( !std::is_copy_constructible<SIRD3D12ScopedVirtualAllocation>::value, "Native owner must be noncopyable" );
static_assert( !std::is_copy_assignable<SIRD3D12ScopedVirtualAllocation>::value, "Native owner must be noncopyable" );

static bool sirIsCommittedVirtualMemory( void *pAddress ) noexcept
{
	MEMORY_BASIC_INFORMATION info = {};
	return VirtualQuery( pAddress, &info, sizeof( info ) ) == sizeof( info ) && info.State == MEM_COMMIT;
}

int main( void )
{
	constexpr SIZE_T TEST_ALLOCATION_BYTES = 65536;
	void *pFirst = VirtualAlloc( nullptr, TEST_ALLOCATION_BYTES, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	if ( pFirst == nullptr ) {
		return 1;
	}
	{
		SIRD3D12ScopedVirtualAllocation pending( pFirst );
		if ( pending.get() != pFirst || !sirIsCommittedVirtualMemory( pFirst ) ) {
			return 2;
		}
		void *pDetached = pending.release();
		if ( pending.get() != nullptr || pDetached != pFirst || !sirIsCommittedVirtualMemory( pDetached ) ) {
			return 3;
		}
		pending.reset( pDetached );
		pending.reset();
		if ( pending.get() != nullptr || sirIsCommittedVirtualMemory( pFirst ) ) {
			return 4;
		}
		void *pSecond = VirtualAlloc( nullptr, TEST_ALLOCATION_BYTES, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
		if ( pSecond == nullptr ) {
			return 5;
		}
		pending.reset( pSecond );
		if ( !sirIsCommittedVirtualMemory( pSecond ) ) {
			return 6;
		}
	}
	return 0;
}
