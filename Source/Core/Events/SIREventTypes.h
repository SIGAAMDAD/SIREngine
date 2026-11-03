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

#ifndef __SIRENGINE_SIREVENTTYPES_H__
#define __SIRENGINE_SIREVENTTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

typedef struct sirSubscriptionHandle_s
{
	const void *pOwner;
	uint64_t nToken;

	constexpr bool isValid( void ) const noexcept
	{
		return pOwner != nullptr && nToken != 0;
	}
} sirSubscriptionHandle_t;

// Compact, eight-byte resource locator. Generation belongs to the owning
// subsystem, which validates it at the point of consumption.
typedef struct sirCompactEventRef_s
{
	uint32_t nSlot;
	uint32_t nGeneration;

	constexpr bool isValid( void ) const noexcept
	{
		return nSlot != 0 && nGeneration != 0;
	}
} sirCompactEventRef_t;

typedef struct sirEventHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;

	constexpr bool isValid( void ) const noexcept
	{
		return nSlot != 0 && nGeneration != 0;
	}
} sirEventHandle_t;

typedef struct sirEventScheduleHandle_s
{
	const void *pOwner;
	uint32_t nSlot;
	uint32_t nGeneration;

	constexpr bool isValid( void ) const noexcept
	{
		return pOwner != nullptr && nSlot != 0 && nGeneration != 0;
	}
} sirEventScheduleHandle_t;

typedef enum sirEventPriority_e : uint8_t
{
	SIR_EVENT_PRIORITY_VERY_LOW = 0,
	SIR_EVENT_PRIORITY_LOW = 1,
	SIR_EVENT_PRIORITY_NORMAL = 2,
	SIR_EVENT_PRIORITY_HIGH = 3,
	SIR_EVENT_PRIORITY_VERY_HIGH = 4,
	SIR_EVENT_PRIORITY_CRITICAL = 5,
	SIR_EVENT_PRIORITY_COUNT = 6
} sirEventPriority_t;

typedef enum sirEventEnqueueStatus_e : uint8_t
{
	SIR_EVENT_ENQUEUE_OK = 0,
	SIR_EVENT_ENQUEUE_FULL = 1,
	SIR_EVENT_ENQUEUE_STOPPED = 2,
	SIR_EVENT_ENQUEUE_NO_LISTENERS = 3
} sirEventEnqueueStatus_t;

typedef enum sirEventEntryState_e : uint8_t
{
	SIR_EVENT_ENTRY_DEAD = 0,
	SIR_EVENT_ENTRY_ACTIVE = 1,
	SIR_EVENT_ENTRY_PENDING = 2
} sirEventEntryState_t;

template <typename TPayload>
struct sirEventDelegate_s
{
	void *pContext;
	void ( *pInvoke )( void *, const TPayload & ) noexcept;
};

template <typename TPayload>
using sirEventDelegate_t = sirEventDelegate_s<TPayload>;
#endif // __SIRENGINE_SIREVENTTYPES_H__
