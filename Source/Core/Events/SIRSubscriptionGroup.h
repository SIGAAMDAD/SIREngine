/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRSUBSCRIPTIONGROUP_H__
#define __SIRENGINE_SIRSUBSCRIPTIONGROUP_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREvent.h>
#include <stdint.h>

// Fixed-capacity subscription owner. Event objects must outlive their group.
// Group mutation is performed by the same owner as the subscribed signals.
typedef struct sirSubscriptionGroupEntry_s
{
	void *pOwner;
	sirSubscriptionHandle_t handle;
	void ( *pUnsubscribe )( void *, sirSubscriptionHandle_t ) noexcept;
} sirSubscriptionGroupEntry_t;

template <uint32_t Capacity = 16>
class SIRSubscriptionGroup
{
public:
	static_assert( Capacity > 0, "Subscription group requires storage" );
	SIRSubscriptionGroup( void ) noexcept : m_nCount( 0 )
	{
	}
	~SIRSubscriptionGroup() noexcept
	{
		clear();
	}
	SIRSubscriptionGroup( const SIRSubscriptionGroup & ) = delete;
	SIRSubscriptionGroup &operator=( const SIRSubscriptionGroup & ) = delete;

	template <typename TPayload, uint32_t EventCapacity>
	bool add( SIREvent<TPayload, EventCapacity> &event, sirSubscriptionHandle_t handle ) noexcept
	{
		if ( !handle.isValid() || handle.pOwner != &event || m_nCount == Capacity || !event.contains( handle ) ) {
			return false;
		}
		m_Entries[m_nCount++] = { &event, handle, []( void *pOwner, sirSubscriptionHandle_t item ) noexcept {
			                         ( ( SIREvent<TPayload, EventCapacity> * )pOwner )->unsubscribe( item );
		                         } };
		return true;
	}

	void clear( void ) noexcept
	{
		// Reversing order allows callbacks to unsubscribe each other safely.
		while ( m_nCount != 0 ) {
			const sirSubscriptionGroupEntry_t entry = m_Entries[--m_nCount];
			entry.pUnsubscribe( entry.pOwner, entry.handle );
		}
	}
	uint32_t count( void ) const noexcept
	{
		return m_nCount;
	}

private:
	sirSubscriptionGroupEntry_t m_Entries[Capacity]{};
	uint32_t m_nCount;
};

#endif // __SIRENGINE_SIRSUBSCRIPTIONGROUP_H__
