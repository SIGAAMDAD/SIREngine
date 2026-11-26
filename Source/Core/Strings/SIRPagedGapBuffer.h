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

#ifndef __SIR_PAGEDGAPBUFFER_H__
#define __SIR_PAGEDGAPBUFFER_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Fixed-size pages with independent gaps. Storage and descriptors are exclusive
// caller-owned spans; input/output must not alias them. Single writer.
typedef struct sirPagedGapPage_s
{
	uint32_t iGapBegin;
	uint32_t iGapEnd;
	uint32_t iNext;
} sirPagedGapPage_t;

typedef struct sirPagedGapStorage_s
{
	char *pBytes;
	sirPagedGapPage_t *pPages;
	uint32_t nPages;
	uint32_t nPageBytes;
	// Optional arrays, each nPages entries, exclusive and non-overlapping.
	uint32_t *pOrder{};
	uint32_t *pWeights{};
} sirPagedGapStorage_t;

class SIRPagedGapBuffer final
{
public:
	explicit SIRPagedGapBuffer( const sirPagedGapStorage_t& storage ) noexcept;
	explicit SIRPagedGapBuffer( const SIRPagedGapBuffer& ) = delete;
	explicit SIRPagedGapBuffer( SIRPagedGapBuffer&& ) = delete;
	~SIRPagedGapBuffer() noexcept = default;
	SIRPagedGapBuffer& operator=( const SIRPagedGapBuffer& ) = delete;
	SIRPagedGapBuffer& operator=( SIRPagedGapBuffer&& ) = delete;
	[[nodiscard]] uint32_t getByteLength( void ) const noexcept;
	[[nodiscard]] uint32_t getFreePages( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInsert( uint32_t iPosition, sirStringView_t input ) noexcept;
	[[nodiscard]] SIRContainerResult tryErase( uint32_t iPosition, uint32_t nBytes ) noexcept;
	[[nodiscard]] SIRContainerResult tryGetByte( uint32_t iPosition, char& nByte ) const noexcept;
	[[nodiscard]] SIRContainerResult tryCopyTo( char *pOutput, uint32_t nCapacity ) const noexcept;
	void clear( void ) noexcept;

private:
	static constexpr uint32_t INVALID = UINT32_MAX;
	[[nodiscard]] uint32_t length( uint32_t iPage ) const noexcept;
	[[nodiscard]] uint32_t locate( uint32_t& iPosition, uint32_t& iPrevious, uint32_t *pRank = nullptr ) const noexcept;
	[[nodiscard]] uint32_t allocateAfter( uint32_t iPrevious ) noexcept;
	[[nodiscard]] char *data( uint32_t iPage ) const noexcept;
	void moveGap( uint32_t iPage, uint32_t iPosition ) noexcept;
	void release( uint32_t iPage, uint32_t iPrevious ) noexcept;
	void rebuildIndex( void ) noexcept;
	void updateIndex( uint32_t iRank, uint32_t nAdded, uint32_t nRemoved ) noexcept;
	[[nodiscard]] uint32_t rankAt( uint32_t& iPosition ) const noexcept;
	uint32_t m_nIndexed{};
	sirPagedGapStorage_t m_Storage;
	uint32_t m_iHead{ INVALID };
	uint32_t m_iFree{ INVALID };
	uint32_t m_nFree{};
	uint32_t m_nLength{};
};
#endif // __SIR_PAGEDGAPBUFFER_H__
