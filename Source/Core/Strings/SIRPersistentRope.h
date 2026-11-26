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

#ifndef __SIR_PERSISTENTROPE_H__
#define __SIR_PERSISTENTROPE_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Immutable leaves and branches occupy separate pools. Six-way branch records
// have cumulative byte boundaries and fit in one 64-byte cache line. Snapshots
// are arena-scoped indices; all borrowed text and pools must outlive snapshots.
typedef struct sirWideRopeLeaf_s
{
	sirStringView_t fragments[3];
	uint32_t nCount;
	uint32_t nBytes;
	uint32_t nNewlines;
	uint32_t nUtf8Starts;
} sirWideRopeLeaf_t;

typedef struct sirWideRopeBranch_s
{
	uint32_t iChildren[6];
	uint32_t nEnds[6];
	uint32_t nCount;
	uint32_t nHeight;
	uint32_t nNewlines;
	uint32_t nUtf8Starts;
} sirWideRopeBranch_t;

typedef struct sirWideRopeSummary_s
{
	uint64_t nHash;
	uint32_t nUtf16Units;
} sirWideRopeSummary_t;

typedef struct sirWideRopeStorage_s
{
	sirWideRopeLeaf_t *pLeaves;
	sirWideRopeBranch_t *pBranches;
	uint32_t nLeaves;
	uint32_t nBranches;
	// Optional pair of non-overlapping summary arrays, sized to the respective pools.
	sirWideRopeSummary_t *pLeafSummaries{};
	sirWideRopeSummary_t *pBranchSummaries{};
} sirWideRopeStorage_t;

typedef struct sirWideRopeSnapshot_s
{
	uint32_t iRoot;
	uint32_t nGeneration;
} sirWideRopeSnapshot_t;

// Single writer; snapshots may be read concurrently only after publication,
// and while pool allocation/reset are externally synchronized with readers.
// Failed edits roll back all new nodes. Reset invalidates every snapshot.
class SIRPersistentRope final
{
public:
	explicit SIRPersistentRope( const sirWideRopeStorage_t& storage ) noexcept;
	explicit SIRPersistentRope( const SIRPersistentRope& ) = delete;
	explicit SIRPersistentRope( SIRPersistentRope&& ) = delete;
	~SIRPersistentRope() noexcept = default;
	SIRPersistentRope& operator=( const SIRPersistentRope& ) = delete;
	SIRPersistentRope& operator=( SIRPersistentRope&& ) = delete;
	[[nodiscard]] sirWideRopeSnapshot_t empty( void ) const noexcept;
	[[nodiscard]] uint32_t getByteLength( sirWideRopeSnapshot_t snapshot ) const noexcept;
	[[nodiscard]] SIRContainerResult tryGetByte( sirWideRopeSnapshot_t snapshot, uint32_t iByte, char& nByte ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInsert( sirWideRopeSnapshot_t& snapshot, uint32_t iPosition, sirStringView_t input ) noexcept;
	[[nodiscard]] SIRContainerResult tryErase( sirWideRopeSnapshot_t& snapshot, uint32_t iPosition, uint32_t nBytes ) noexcept;
	[[nodiscard]] uint32_t getNewlineCount( sirWideRopeSnapshot_t snapshot ) const noexcept;
	// Counts non-continuation bytes, exact code-point count for valid UTF-8 text.
	[[nodiscard]] uint32_t getUtf8Starts( sirWideRopeSnapshot_t snapshot ) const noexcept;
	[[nodiscard]] SIRContainerResult tryGetSummary( sirWideRopeSnapshot_t snapshot, sirWideRopeSummary_t& output ) const noexcept;
	[[nodiscard]] SIRContainerResult tryGetLineStart( sirWideRopeSnapshot_t snapshot, uint32_t iLine, uint32_t& iByte ) const noexcept;
	// Copy only reachable nodes into a distinct pool; both snapshots remain valid.
	[[nodiscard]] SIRContainerResult tryCompactTo( sirWideRopeSnapshot_t snapshot, SIRPersistentRope& target, sirWideRopeSnapshot_t& output ) const noexcept;
	void reset( void ) noexcept;

private:
	static constexpr uint32_t LEAF = 0x80000000u;
	static constexpr uint32_t FAILED = UINT32_MAX;

	typedef struct sirNodePair_s
	{
		uint32_t iFirst;
		uint32_t iSecond;
	} sirNodePair_t;

	[[nodiscard]] bool isValid( sirWideRopeSnapshot_t snapshot ) const noexcept;
	[[nodiscard]] uint32_t bytes( uint32_t iNode ) const noexcept;
	[[nodiscard]] uint32_t height( uint32_t iNode ) const noexcept;
	[[nodiscard]] uint32_t newlines( uint32_t iNode ) const noexcept;
	[[nodiscard]] uint32_t utf8Starts( uint32_t iNode ) const noexcept;
	[[nodiscard]] bool hasSummaries( void ) const noexcept;
	[[nodiscard]] sirWideRopeSummary_t summary( uint32_t iNode ) const noexcept;
	[[nodiscard]] static uint64_t hashPower( uint32_t nBytes ) noexcept;
	[[nodiscard]] uint32_t cloneNode( uint32_t iNode, SIRPersistentRope& target ) const noexcept;
	[[nodiscard]] uint32_t createLeaf( const sirStringView_t *pFragments, uint32_t nCount ) noexcept;
	[[nodiscard]] uint32_t createBranch( const uint32_t *pChildren, uint32_t nCount ) noexcept;
	[[nodiscard]] sirNodePair_t packLeaves( const sirStringView_t *pFragments, uint32_t nCount ) noexcept;
	[[nodiscard]] sirNodePair_t packBranches( const uint32_t *pChildren, uint32_t nCount ) noexcept;
	[[nodiscard]] sirNodePair_t insertNode( uint32_t iNode, uint32_t iPosition, sirStringView_t input ) noexcept;
	[[nodiscard]] sirNodePair_t eraseNode( uint32_t iNode, uint32_t iPosition, uint32_t nBytes ) noexcept;
	[[nodiscard]] SIRContainerResult commit( sirWideRopeSnapshot_t& snapshot, sirNodePair_t pair, uint32_t nOldLeaves, uint32_t nOldBranches ) noexcept;
	sirWideRopeStorage_t m_Storage;
	uint32_t m_nLeaves{};
	uint32_t m_nBranches{};
	uint32_t m_nGeneration{ 1 };
};
#endif // __SIR_PERSISTENTROPE_H__
