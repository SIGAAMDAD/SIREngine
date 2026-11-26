/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_SCRIPTINGRESS_H__
#define __SIR_SCRIPTINGRESS_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptScheduler.h>
#include <atomic>

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
	std::atomic<uint64_t> nSequence;
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

	SIRScriptResult tryPost( SIRScriptPhase ePhase, const sirScriptIngressPacket_t& packet ) noexcept;
	bool tryPop( SIRScriptPhase ePhase, uint64_t nEndTicket, sirScriptIngressPacket_t *pPacket ) noexcept;
	uint64_t snapshotTail( SIRScriptPhase ePhase ) const noexcept;
	uint32_t pendingCount( SIRScriptPhase ePhase ) const noexcept;
	void close( void ) noexcept;
	void clear( void ) noexcept;

private:
	static constexpr uint32_t CAPACITY = 64;

	typedef struct sirScriptIngressQueue_s
	{
		alignas( 64 ) uint64_t nHead;
		alignas( 64 ) std::atomic<uint64_t> nTail;
		std::atomic<uint32_t> nCount;
		sirScriptIngressSlot_t szSlots[CAPACITY];
	} sirScriptIngressQueue_t;

	sirScriptIngressQueue_t m_szQueues[static_cast<size_t>( SIRScriptPhase::SIR_SCRIPT_PHASE_COUNT )];
	std::atomic<uint32_t> m_nActiveProducers;
	std::atomic<bool> m_bClosed;
};

#endif // __SIR_SCRIPTINGRESS_H__
