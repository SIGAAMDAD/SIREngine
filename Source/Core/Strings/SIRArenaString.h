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

#ifndef __SIR_ARENA_STRING_H__
#define __SIR_ARENA_STRING_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRString.h>
#include <Core/Strings/SIROffsetString.h>
#include <atomic>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

class SIRStringArena;

// Stable, non-owning arena reference. Reset/destruction of the arena invalidates all refs.
class SIRArenaString final
{
public:
	explicit SIRArenaString( void ) noexcept = default;
	explicit SIRArenaString( const SIRArenaString& ) noexcept = default;
	explicit SIRArenaString( SIRArenaString&& ) noexcept = default;
	~SIRArenaString() noexcept = default;
	SIRArenaString& operator=( const SIRArenaString& ) noexcept = default;
	SIRArenaString& operator=( SIRArenaString&& ) noexcept = default;

	[[nodiscard]] constexpr sirStringView_t getView( void ) const noexcept
	{
		return m_View;
	}

	[[nodiscard]] constexpr const char *cStr( void ) const noexcept
	{
		return m_View.pBytes;
	}

private:
	friend class SIRStringArena;
	sirStringView_t m_View{ "", 0 };
};

// Caller owns one contiguous character span. Single writer. No per-string allocation.
class SIRStringArena final
{
public:
	explicit SIRStringArena( char *pStorage, uint32_t nCapacity ) noexcept;
	explicit SIRStringArena( const SIRStringArena& ) = delete;
	explicit SIRStringArena( SIRStringArena&& ) = delete;
	~SIRStringArena() noexcept = default;

	SIRStringArena& operator=( const SIRStringArena& ) = delete;
	SIRStringArena& operator=( SIRStringArena&& ) = delete;

	[[nodiscard]] uint32_t getUsedBytes( void ) const noexcept;
	[[nodiscard]] SIRContainerResult trySave( sirStringView_t view, SIRArenaString& output ) noexcept;
	[[nodiscard]] SIRContainerResult trySaveRelative( sirStringView_t view, SIROffsetString& output ) noexcept;
	[[nodiscard]] sirStringView_t getTable( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryGenerate( uint32_t nBytes, void ( *pfnWrite )( char *, uint32_t ) noexcept, SIRArenaString& output ) noexcept;
	void reset( void ) noexcept;

private:
	char *m_pStorage;
	uint32_t m_nCapacity;
	uint32_t m_nUsed{};
};

// Caller provides a persistent backing span. Slabs are disjoint and exclusively
// assigned to one writer. Reset is legal only after every slab reader/writer retires.
typedef struct sirStringArenaSlab_s
{
	char *pBytes;
	uint32_t nCapacity;
} sirStringArenaSlab_t;

class SIRStringArenaSlabSource final
{
public:
	explicit SIRStringArenaSlabSource( char *pStorage, uint32_t nCapacity ) noexcept;
	explicit SIRStringArenaSlabSource( const SIRStringArenaSlabSource& ) = delete;
	explicit SIRStringArenaSlabSource( SIRStringArenaSlabSource&& ) = delete;
	~SIRStringArenaSlabSource() noexcept = default;

	SIRStringArenaSlabSource& operator=( const SIRStringArenaSlabSource& ) = delete;
	SIRStringArenaSlabSource& operator=( SIRStringArenaSlabSource&& ) = delete;

	[[nodiscard]] SIRContainerResult tryAcquire( uint32_t nBytes, sirStringArenaSlab_t& slab ) noexcept;
	void reset( void ) noexcept;

private:
	char *m_pStorage;
	uint32_t m_nCapacity;
	std::atomic<uint32_t> m_nUsed{};
};
#endif // __SIR_ARENA_STRING_H__
