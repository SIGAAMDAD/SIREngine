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

#ifndef __SIR_VIRTUALSTRINGARENA_H__
#define __SIR_VIRTUALSTRINGARENA_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Single-writer page-backed arena. Reserve once, commit only newly touched
// pages, keep addresses stable. Reset/release require all borrowers to retire.
class SIRVirtualStringArena final
{
public:
	explicit SIRVirtualStringArena() noexcept = default;
	explicit SIRVirtualStringArena( const SIRVirtualStringArena& ) = delete;
	explicit SIRVirtualStringArena( SIRVirtualStringArena&& ) = delete;
	~SIRVirtualStringArena() noexcept;
	SIRVirtualStringArena& operator=( const SIRVirtualStringArena& ) = delete;
	SIRVirtualStringArena& operator=( SIRVirtualStringArena&& ) = delete;
	[[nodiscard]] uint32_t getUsedBytes( void ) const noexcept;
	[[nodiscard]] size_t getCommittedBytes( void ) const noexcept;
	[[nodiscard]] size_t getReservedBytes( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInitialize( uint32_t nCapacity ) noexcept;
	[[nodiscard]] SIRContainerResult trySave( sirStringView_t input, sirStringView_t& output ) noexcept;
	[[nodiscard]] SIRContainerResult tryReset( bool bDecommit ) noexcept;
	[[nodiscard]] SIRContainerResult tryRelease( void ) noexcept;

private:
	char *m_pBytes{};
	size_t m_nReserved{};
	size_t m_nCommitted{};
	size_t m_nPage{};
	uint32_t m_nCapacity{};
	uint32_t m_nUsed{};
};
#endif // __SIR_VIRTUALSTRINGARENA_H__
