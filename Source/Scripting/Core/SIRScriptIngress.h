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

#ifndef __SIRENGINE_SIRSCRIPTINGRESS_H__
#define __SIRENGINE_SIRSCRIPTINGRESS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptScheduler.h>
#include <Core/Platform/SIRAtomic.h>

// Payload ownership transfers by value; no producer-owned pointer survives
// submission. Event type validation is the caller's responsibility.
typedef struct sirScriptIngressPacket_s
{
	uint64_t nEventTypeId;
	uint32_t nBytes;
	uint32_t nAlignment;
	alignas( 64 ) unsigned char szPayload[256];
} sirScriptIngressPacket_t;

typedef struct sirScriptIngressSlot_s
{
	SIRAtomic<uint64_t> nSequence;
	sirScriptIngressPacket_t packet;
} sirScriptIngressSlot_t;

// Bounded, sequence-tagged MPSC ring per script phase. One owner consumer;
// multiple producers. No allocations, locks, or backend calls on producers.
// The owner must close the ingress before destroying the runtime and join
// producer threads before destroying the enclosing SIRScriptRuntime object.
class SIRScriptIngress
{
public:
	SIRScriptIngress( void ) noexcept;
	SIRScriptIngress( const SIRScriptIngress& ) = delete;
	SIRScriptIngress& operator=( const SIRScriptIngress& ) = delete;

	sirScriptResult_t tryPost( sirScriptPhase_t ePhase, const sirScriptIngressPacket_t& packet ) noexcept;
	bool tryPop( sirScriptPhase_t ePhase, uint64_t nEndTicket, sirScriptIngressPacket_t *pPacket ) noexcept;
	uint64_t snapshotTail( sirScriptPhase_t ePhase ) const noexcept;
	uint32_t pendingCount( sirScriptPhase_t ePhase ) const noexcept;
	void close( void ) noexcept;
	void clear( void ) noexcept;

private:
	static constexpr uint32_t CAPACITY = 64;

	typedef struct sirScriptIngressQueue_s
	{
		alignas( 64 ) uint64_t nHead;
		alignas( 64 ) SIRAtomic<uint64_t> nTail;
		SIRAtomic<uint32_t> nCount;
		sirScriptIngressSlot_t szSlots[CAPACITY];
	} sirScriptIngressQueue_t;

	sirScriptIngressQueue_t m_szQueues[SIR_SCRIPT_PHASE_COUNT];
	SIRAtomic<uint32_t> m_nActiveProducers;
	SIRAtomic<bool> m_bClosed;
};

#endif // __SIRENGINE_SIRSCRIPTINGRESS_H__
