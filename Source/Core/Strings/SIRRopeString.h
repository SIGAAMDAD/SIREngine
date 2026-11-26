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

#ifndef __SIR_ROPESTRING_H__
#define __SIR_ROPESTRING_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// AVL sequence node. Index zero is a sentinel; nodes store borrowed immutable byte
// ranges. A caller-provided arena or other stable owner keeps the ranges alive.
typedef struct sirRopeNode_s
{
	sirStringView_t view;
	uint32_t iLeft;
	uint32_t iRight;
	uint32_t nSubtreeBytes;
	uint32_t nHeight;
} sirRopeNode_t;

// Single writer. The node pool is exclusive to this rope, and its text sources
// must remain alive and immutable until removed or clear(). Edits are byte-based.
// AVL joins/splits provide logarithmic structural work; erased nodes are recycled.
class SIRRopeString final
{
public:
	explicit SIRRopeString( sirRopeNode_t *pNodes, uint32_t nCapacity ) noexcept;
	explicit SIRRopeString( const SIRRopeString& ) = delete;
	explicit SIRRopeString( SIRRopeString&& ) = delete;
	~SIRRopeString() noexcept = default;
	SIRRopeString& operator=( const SIRRopeString& ) = delete;
	SIRRopeString& operator=( SIRRopeString&& ) = delete;

	[[nodiscard]] uint32_t getByteLength( void ) const noexcept;
	[[nodiscard]] uint32_t getFreeNodeCount( void ) const noexcept;
	[[nodiscard]] uint32_t getHeight( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryGetByte( uint32_t iByte, char& nByte ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInsert( uint32_t iPosition, sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppendContiguous( sirStringView_t backing, uint32_t iOffset, uint32_t nBytes ) noexcept;
	[[nodiscard]] SIRContainerResult tryErase( uint32_t iPosition, uint32_t nBytes ) noexcept;
	[[nodiscard]] SIRContainerResult tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept;
	void clear( void ) noexcept;

private:
	[[nodiscard]] sirRopeNode_t& node( uint32_t iNode ) noexcept;
	[[nodiscard]] const sirRopeNode_t& node( uint32_t iNode ) const noexcept;
	[[nodiscard]] uint32_t bytes( uint32_t iNode ) const noexcept;
	[[nodiscard]] uint32_t height( uint32_t iNode ) const noexcept;
	[[nodiscard]] uint32_t allocate( sirStringView_t view ) noexcept;
	[[nodiscard]] uint32_t rotateLeft( uint32_t iRoot ) noexcept;
	[[nodiscard]] uint32_t rotateRight( uint32_t iRoot ) noexcept;
	[[nodiscard]] uint32_t rebalance( uint32_t iRoot ) noexcept;
	[[nodiscard]] uint32_t join( uint32_t iLeft, uint32_t iMiddle, uint32_t iRight ) noexcept;
	[[nodiscard]] uint32_t merge( uint32_t iLeft, uint32_t iRight ) noexcept;
	[[nodiscard]] uint32_t detachFirst( uint32_t iRoot, uint32_t& iFirst ) noexcept;
	[[nodiscard]] bool isBoundary( uint32_t iPosition ) const noexcept;
	[[nodiscard]] bool overlapsTree( uint32_t iRoot, sirStringView_t output ) const noexcept;
	[[nodiscard]] bool extendLast( uint32_t iRoot, sirStringView_t backing, uint32_t iOffset, uint32_t nBytes ) noexcept;
	void update( uint32_t iRoot ) noexcept;
	void split( uint32_t iRoot, uint32_t iPosition, uint32_t& iLeft, uint32_t& iRight ) noexcept;
	void releaseTree( uint32_t iRoot ) noexcept;
	void copyTree( uint32_t iRoot, char *pOutput, uint32_t& iOutput ) const noexcept;

	sirRopeNode_t *m_pNodes;
	uint32_t m_nCapacity;
	uint32_t m_iRoot{};
	uint32_t m_iFree{};
	uint32_t m_nFree{};
	uint32_t m_nVirgin{};
};

#endif // __SIR_ROPESTRING_H__
