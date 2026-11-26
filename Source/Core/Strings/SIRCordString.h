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

#ifndef __SIR_CORDSTRING_H__
#define __SIR_CORDSTRING_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRString.h>
#include <Core/Strings/SIRSharedString.h>

// Bounded circular sequence of shared immutable chunks. The caller provides an array
// of default-constructed, exclusively reserved SIRSharedString slots for the cord lifetime.
// Append/prepend share bytes, never flatten or move existing chunks. Single writer.
class SIRCordString final
{
public:
	explicit SIRCordString( void ) noexcept;
	explicit SIRCordString( SIRSharedString *pSlots, uint32_t nCapacity ) noexcept;
	explicit SIRCordString( const SIRCordString& ) = delete;
	explicit SIRCordString( SIRCordString&& ) = delete;
	~SIRCordString() noexcept;
	SIRCordString& operator=( const SIRCordString& ) = delete;
	SIRCordString& operator=( SIRCordString&& ) = delete;

	[[nodiscard]] uint32_t getByteLength( void ) const noexcept;
	[[nodiscard]] uint32_t getChunkCount( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryGetChunk( uint32_t iChunk, sirStringView_t& view ) const noexcept;
	[[nodiscard]] SIRContainerResult tryAppend( const SIRSharedString& chunk ) noexcept;
	// Optional caller-owned exclusive array of nCapacity uint32_t entries.
	[[nodiscard]] SIRContainerResult tryEnableIndex( uint32_t *pIndex, uint32_t nEntries ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppendOwned( SIRSharedString& chunk ) noexcept;
	[[nodiscard]] SIRContainerResult tryGetByte( uint32_t iByte, char& nByte ) const noexcept;
	[[nodiscard]] SIRContainerResult tryPrepend( const SIRSharedString& chunk ) noexcept;
	[[nodiscard]] SIRContainerResult tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept;
	[[nodiscard]] SIRContainerResult tryPopFront( void ) noexcept;
	[[nodiscard]] SIRContainerResult tryPopBack( void ) noexcept;
	void clear( void ) noexcept;

private:
	[[nodiscard]] uint32_t indexOf( uint32_t iChunk ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInsert( const SIRSharedString& chunk, bool bPrepend ) noexcept;
	void updateIndex( uint32_t iSlot, uint32_t nBytes, bool bRemove ) noexcept;
	[[nodiscard]] uint32_t prefixBytes( uint32_t nSlots ) const noexcept;
	[[nodiscard]] uint32_t indexedSlot( uint32_t& iByte ) const noexcept;

	SIRSharedString m_Inline[2];
	uint32_t *m_pIndex{};
	SIRSharedString *m_pSlots;
	uint32_t m_nCapacity;
	uint32_t m_iHead{};
	uint32_t m_nCount{};
	uint32_t m_nLength{};
};

#endif // __SIR_CORDSTRING_H__
