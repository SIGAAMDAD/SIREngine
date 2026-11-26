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

#ifndef __SIR_STRINGEPOCH_H__
#define __SIR_STRINGEPOCH_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>
#include <Core/Strings/SIRSharedString.h>
#include <atomic>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// A reader gate for an immutable string-storage generation. Readers enter once
// per batch and may borrow any number of views. The owner closes admission,
// then reclaims only after all admitted readers leave. Owner transitions are serialized.
class SIRStringEpoch;

class SIRStringEpochLease final
{
public:
	explicit SIRStringEpochLease() noexcept = default;
	explicit SIRStringEpochLease( const SIRStringEpochLease& ) = delete;
	explicit SIRStringEpochLease( SIRStringEpochLease&& other ) noexcept;
	~SIRStringEpochLease() noexcept;
	SIRStringEpochLease& operator=( const SIRStringEpochLease& ) = delete;
	SIRStringEpochLease& operator=( SIRStringEpochLease&& other ) noexcept;
	[[nodiscard]] bool isValid( void ) const noexcept;
	void reset( void ) noexcept;

private:
	friend class SIRStringEpoch;
	SIRStringEpoch *m_pEpoch{};
};

class SIRStringEpoch final
{
public:
	explicit SIRStringEpoch() noexcept = default;
	explicit SIRStringEpoch( const SIRStringEpoch& ) = delete;
	explicit SIRStringEpoch( SIRStringEpoch&& ) = delete;
	~SIRStringEpoch() noexcept = default;
	SIRStringEpoch& operator=( const SIRStringEpoch& ) = delete;
	SIRStringEpoch& operator=( SIRStringEpoch&& ) = delete;
	[[nodiscard]] SIRContainerResult tryEnter( SIRStringEpochLease& lease ) noexcept;
	// Returns true when closed and no readers remain. May be retried.
	[[nodiscard]] bool close( void ) noexcept;
	[[nodiscard]] bool isQuiescent( void ) const noexcept;
	// Owner calls only after completing retirement and publishing new data.
	[[nodiscard]] bool reopen( void ) noexcept;

private:
	friend class SIRStringEpochLease;
	void leave( void ) noexcept;
	static constexpr uint32_t CLOSED = 0x80000000u;
	std::atomic<uint32_t> m_nState{};
};

// Explicit owner-biased lifetime: local leases are owner-thread-only, remote
// leases may cross threads. The owner object itself must outlive every lease.
// Owner retains one immutable allocation until both counters drain.
class SIRBiasedString;

class SIRBiasedStringLease final
{
public:
	explicit SIRBiasedStringLease() noexcept = default;
	explicit SIRBiasedStringLease( const SIRBiasedStringLease& ) = delete;
	explicit SIRBiasedStringLease( SIRBiasedStringLease&& ) = delete;
	~SIRBiasedStringLease() noexcept;
	SIRBiasedStringLease& operator=( const SIRBiasedStringLease& ) = delete;
	SIRBiasedStringLease& operator=( SIRBiasedStringLease&& ) = delete;
	[[nodiscard]] sirStringView_t getView( void ) const noexcept;
	void reset( void ) noexcept;

private:
	friend class SIRBiasedString;
	SIRBiasedString *m_pOwner{};
	SIRStringEpochLease m_Remote;
};

class SIRBiasedString final
{
public:
	explicit SIRBiasedString() noexcept;
	explicit SIRBiasedString( const SIRBiasedString& ) = delete;
	explicit SIRBiasedString( SIRBiasedString&& ) = delete;
	~SIRBiasedString() noexcept;
	SIRBiasedString& operator=( const SIRBiasedString& ) = delete;
	SIRBiasedString& operator=( SIRBiasedString&& ) = delete;
	// Owner-only, while closed and drained. Initial assignment closes empty state.
	[[nodiscard]] SIRContainerResult tryAssign( const sirContainerAllocator_t *pAllocator, sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAcquireLocal( SIRBiasedStringLease& lease ) noexcept;
	[[nodiscard]] SIRContainerResult tryAcquireRemote( SIRBiasedStringLease& lease ) noexcept;
	[[nodiscard]] bool tryRetire( void ) noexcept;

private:
	friend class SIRBiasedStringLease;
	SIRSharedString m_String;
	SIRStringEpoch m_Epoch;
	uint32_t m_nLocal{};
	bool m_bOpen : 1;
};
#endif // __SIR_STRINGEPOCH_H__
