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

#include <Core/Strings/SIRPersistentRope.h>
#include <Core/Strings/SIRStringOps.h>

SIRPersistentRope::SIRPersistentRope( const sirWideRopeStorage_t& storage ) noexcept
    : m_Storage( storage )
{
	if ( storage.pLeaves == nullptr || storage.nLeaves >= LEAF - 1 ) {
		m_Storage.nLeaves = 0;
	}
	if ( storage.pBranches == nullptr || storage.nBranches >= LEAF ) {
		m_Storage.nBranches = 0;
	}
}

sirWideRopeSnapshot_t SIRPersistentRope::empty( void ) const noexcept
{
	return { 0, m_nGeneration };
}

void SIRPersistentRope::reset( void ) noexcept
{
	m_nLeaves = 0;
	m_nBranches = 0;
	if ( m_nGeneration != UINT32_MAX && m_nGeneration != 0 ) {
		++m_nGeneration;
	} else {
		m_nGeneration = 0;
	}
}

bool SIRPersistentRope::isValid( sirWideRopeSnapshot_t snapshot ) const noexcept
{
	if ( snapshot.nGeneration != m_nGeneration || m_nGeneration == 0 || snapshot.iRoot == FAILED ) {
		return false;
	}
	return ( snapshot.iRoot & LEAF ) != 0 ? ( snapshot.iRoot & ~LEAF ) < m_nLeaves : snapshot.iRoot <= m_nBranches;
}

uint32_t SIRPersistentRope::bytes( uint32_t iNode ) const noexcept
{
	if ( iNode == 0 ) {
		return 0;
	}
	if ( ( iNode & LEAF ) != 0 ) {
		return m_Storage.pLeaves[iNode & ~LEAF].nBytes;
	}
	const sirWideRopeBranch_t& branch = m_Storage.pBranches[iNode - 1];
	return branch.nEnds[branch.nCount - 1];
}

uint32_t SIRPersistentRope::height( uint32_t iNode ) const noexcept
{
	return ( iNode & LEAF ) != 0 ? 0 : m_Storage.pBranches[iNode - 1].nHeight;
}

uint32_t SIRPersistentRope::getByteLength( sirWideRopeSnapshot_t snapshot ) const noexcept
{
	return isValid( snapshot ) ? bytes( snapshot.iRoot ) : 0;
}

uint32_t SIRPersistentRope::createLeaf( const sirStringView_t *pFragments, uint32_t nCount ) noexcept
{
	if ( nCount == 0 ) {
		return 0;
	}
	if ( m_nLeaves == m_Storage.nLeaves ) {
		return FAILED;
	}
	const uint32_t iNode = m_nLeaves++;
	sirWideRopeLeaf_t& leaf = m_Storage.pLeaves[iNode];
	leaf.nCount = nCount;
	leaf.nBytes = 0;
	leaf.nNewlines = 0;
	leaf.nUtf8Starts = 0;
	sirWideRopeSummary_t aggregate{};
	for ( uint32_t i = 0; i < nCount; ++i ) {
		leaf.fragments[i] = pFragments[i];
		leaf.nBytes += pFragments[i].nBytes;
		for ( uint32_t j = 0; j < pFragments[i].nBytes; ++j ) {
			const uint8_t nByte = static_cast<uint8_t>( pFragments[i].pBytes[j] );
			leaf.nNewlines += nByte == 10 ? 1u : 0u;
			const uint32_t nStart = ( nByte & 0xc0u ) != 0x80u ? 1u : 0u;
			leaf.nUtf8Starts += nStart;
			if ( hasSummaries() ) {
				aggregate.nHash = aggregate.nHash * 1099511628211ULL + nByte + 1;
				aggregate.nUtf16Units += nStart + ( nByte >= 0xf0 && nByte <= 0xf4 ? 1u : 0u );
			}
		}
	}
	if ( hasSummaries() ) { m_Storage.pLeafSummaries[iNode] = aggregate; }
	return iNode | LEAF;
}

uint32_t SIRPersistentRope::createBranch( const uint32_t *pChildren, uint32_t nCount ) noexcept
{
	if ( nCount == 0 ) {
		return 0;
	}
	if ( m_nBranches == m_Storage.nBranches ) {
		return FAILED;
	}
	sirWideRopeBranch_t& branch = m_Storage.pBranches[m_nBranches++];
	branch.nCount = nCount;
	branch.nHeight = height( pChildren[0] ) + 1;
	branch.nNewlines = 0;
	branch.nUtf8Starts = 0;
	sirWideRopeSummary_t aggregate{};
	uint32_t nBytes = 0;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		branch.iChildren[i] = pChildren[i];
		nBytes += bytes( pChildren[i] );
		branch.nEnds[i] = nBytes;
		branch.nNewlines += newlines( pChildren[i] );
		branch.nUtf8Starts += utf8Starts( pChildren[i] );
		if ( hasSummaries() ) {
			const sirWideRopeSummary_t child = summary( pChildren[i] );
			aggregate.nHash = aggregate.nHash * hashPower( bytes( pChildren[i] ) ) + child.nHash;
			aggregate.nUtf16Units += child.nUtf16Units;
		}
	}
	if ( hasSummaries() ) { m_Storage.pBranchSummaries[m_nBranches - 1] = aggregate; }
	return m_nBranches;
}

SIRPersistentRope::sirNodePair_t SIRPersistentRope::packLeaves( const sirStringView_t *pFragments, uint32_t nCount ) noexcept
{
	const uint32_t nFirst = nCount <= 3 ? nCount : nCount / 2;
	const uint32_t iFirst = createLeaf( pFragments, nFirst );
	if ( iFirst == FAILED ) {
		return { FAILED, 0 };
	}
	const uint32_t iSecond = createLeaf( pFragments + nFirst, nCount - nFirst );
	return { iFirst, iSecond };
}

SIRPersistentRope::sirNodePair_t SIRPersistentRope::packBranches( const uint32_t *pChildren, uint32_t nCount ) noexcept
{
	const uint32_t nFirst = nCount <= 6 ? nCount : nCount / 2;
	const uint32_t iFirst = createBranch( pChildren, nFirst );
	if ( iFirst == FAILED ) {
		return { FAILED, 0 };
	}
	return { iFirst, createBranch( pChildren + nFirst, nCount - nFirst ) };
}

SIRPersistentRope::sirNodePair_t SIRPersistentRope::insertNode( uint32_t iNode, uint32_t iPosition, sirStringView_t input ) noexcept
{
	if ( iNode == 0 ) {
		return { createLeaf( &input, 1 ), 0 };
	}
	if ( ( iNode & LEAF ) != 0 ) {
		const sirWideRopeLeaf_t& leaf = m_Storage.pLeaves[iNode & ~LEAF];
		sirStringView_t fragments[5];
		uint32_t nCount = 0;
		bool bInserted = false;
		for ( uint32_t i = 0; i < leaf.nCount; ++i ) {
			const sirStringView_t fragment = leaf.fragments[i];
			if ( !bInserted && iPosition <= fragment.nBytes ) {
				if ( iPosition != 0 ) {
					fragments[nCount++] = { fragment.pBytes, iPosition };
				}
				fragments[nCount++] = input;
				if ( iPosition != fragment.nBytes ) {
					fragments[nCount++] = { fragment.pBytes + iPosition, fragment.nBytes - iPosition };
				}
				bInserted = true;
			} else {
				fragments[nCount++] = fragment;
				if ( !bInserted ) {
					iPosition -= fragment.nBytes;
				}
			}
		}
		return packLeaves( fragments, nCount );
	}
	const sirWideRopeBranch_t& branch = m_Storage.pBranches[iNode - 1];
	uint32_t iChild = 0;
	while ( iPosition > branch.nEnds[iChild] ) {
		++iChild;
	}
	const uint32_t nStart = iChild == 0 ? 0 : branch.nEnds[iChild - 1];
	const sirNodePair_t pair = insertNode( branch.iChildren[iChild], iPosition - nStart, input );
	if ( pair.iFirst == FAILED || pair.iSecond == FAILED ) {
		return { FAILED, 0 };
	}
	uint32_t children[7];
	uint32_t nCount = 0;
	for ( uint32_t i = 0; i < branch.nCount; ++i ) {
		if ( i == iChild ) {
			children[nCount++] = pair.iFirst;
			if ( pair.iSecond != 0 ) {
				children[nCount++] = pair.iSecond;
			}
		} else {
			children[nCount++] = branch.iChildren[i];
		}
	}
	return packBranches( children, nCount );
}

SIRPersistentRope::sirNodePair_t SIRPersistentRope::eraseNode( uint32_t iNode, uint32_t iPosition, uint32_t nBytes ) noexcept
{
	if ( nBytes == 0 ) {
		return { iNode, 0 };
	}
	if ( nBytes == bytes( iNode ) ) {
		return { 0, 0 };
	}
	const uint32_t iEnd = iPosition + nBytes;
	if ( ( iNode & LEAF ) != 0 ) {
		const sirWideRopeLeaf_t& leaf = m_Storage.pLeaves[iNode & ~LEAF];
		sirStringView_t fragments[4];
		uint32_t nCount = 0;
		uint32_t nStart = 0;
		for ( uint32_t i = 0; i < leaf.nCount; ++i ) {
			const sirStringView_t fragment = leaf.fragments[i];
			const uint32_t nEnd = nStart + fragment.nBytes;
			if ( nEnd <= iPosition || nStart >= iEnd ) {
				fragments[nCount++] = fragment;
			} else {
				if ( nStart < iPosition ) {
					fragments[nCount++] = { fragment.pBytes, iPosition - nStart };
				}
				if ( nEnd > iEnd ) {
					fragments[nCount++] = { fragment.pBytes + iEnd - nStart, nEnd - iEnd };
				}
			}
			nStart = nEnd;
		}
		return packLeaves( fragments, nCount );
	}
	const sirWideRopeBranch_t& branch = m_Storage.pBranches[iNode - 1];
	uint32_t children[7];
	uint32_t nCount = 0;
	uint32_t nStart = 0;
	for ( uint32_t i = 0; i < branch.nCount; ++i ) {
		const uint32_t nEnd = branch.nEnds[i];
		if ( nEnd <= iPosition || nStart >= iEnd ) {
			children[nCount++] = branch.iChildren[i];
		} else {
			const uint32_t nLeft = iPosition > nStart ? iPosition : nStart;
			const uint32_t nRight = iEnd < nEnd ? iEnd : nEnd;
			const sirNodePair_t pair = eraseNode( branch.iChildren[i], nLeft - nStart, nRight - nLeft );
			if ( pair.iFirst == FAILED || pair.iSecond == FAILED ) {
				return { FAILED, 0 };
			}
			if ( pair.iFirst != 0 ) {
				children[nCount++] = pair.iFirst;
			}
			if ( pair.iSecond != 0 ) {
				children[nCount++] = pair.iSecond;
			}
		}
		nStart = nEnd;
	}
	return packBranches( children, nCount );
}

SIRContainerResult SIRPersistentRope::commit( sirWideRopeSnapshot_t& snapshot, sirNodePair_t pair, uint32_t nOldLeaves, uint32_t nOldBranches ) noexcept
{
	uint32_t iRoot = pair.iFirst;
	if ( iRoot != FAILED && pair.iSecond != 0 ) {
		if ( pair.iSecond == FAILED ) {
			iRoot = FAILED;
		} else {
			const uint32_t children[]{ iRoot, pair.iSecond };
			iRoot = createBranch( children, 2 );
		}
	}
	if ( iRoot == FAILED ) {
		m_nLeaves = nOldLeaves;
		m_nBranches = nOldBranches;
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	while ( iRoot != 0 && ( iRoot & LEAF ) == 0 && m_Storage.pBranches[iRoot - 1].nCount == 1 ) {
		iRoot = m_Storage.pBranches[iRoot - 1].iChildren[0];
	}
	snapshot.iRoot = iRoot;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRPersistentRope::tryInsert( sirWideRopeSnapshot_t& snapshot, uint32_t iPosition, sirStringView_t input ) noexcept
{
	if ( !isValid( snapshot ) || iPosition > bytes( snapshot.iRoot ) || !SIRStringOps::isValid( input ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( input.nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( input.nBytes > UINT32_MAX - bytes( snapshot.iRoot ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const uint32_t nOldLeaves = m_nLeaves;
	const uint32_t nOldBranches = m_nBranches;
	const sirNodePair_t pair = insertNode( snapshot.iRoot, iPosition, input );
	return commit( snapshot, pair, nOldLeaves, nOldBranches );
}

SIRContainerResult SIRPersistentRope::tryErase( sirWideRopeSnapshot_t& snapshot, uint32_t iPosition, uint32_t nBytes ) noexcept
{
	if ( !isValid( snapshot ) || iPosition > bytes( snapshot.iRoot ) || nBytes > bytes( snapshot.iRoot ) - iPosition ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	const uint32_t nOldLeaves = m_nLeaves;
	const uint32_t nOldBranches = m_nBranches;
	const sirNodePair_t pair = eraseNode( snapshot.iRoot, iPosition, nBytes );
	return commit( snapshot, pair, nOldLeaves, nOldBranches );
}

SIRContainerResult SIRPersistentRope::tryGetByte( sirWideRopeSnapshot_t snapshot, uint32_t iByte, char& nByte ) const noexcept
{
	if ( !isValid( snapshot ) || iByte >= bytes( snapshot.iRoot ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iNode = snapshot.iRoot;
	while ( ( iNode & LEAF ) == 0 ) {
		const sirWideRopeBranch_t& branch = m_Storage.pBranches[iNode - 1];
		uint32_t iChild = 0;
		while ( iByte >= branch.nEnds[iChild] ) {
			++iChild;
		}
		if ( iChild != 0 ) {
			iByte -= branch.nEnds[iChild - 1];
		}
		iNode = branch.iChildren[iChild];
	}
	const sirWideRopeLeaf_t& leaf = m_Storage.pLeaves[iNode & ~LEAF];
	for ( uint32_t i = 0; i < leaf.nCount; ++i ) {
		if ( iByte < leaf.fragments[i].nBytes ) {
			nByte = leaf.fragments[i].pBytes[iByte];
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		iByte -= leaf.fragments[i].nBytes;
	}
	return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
}

uint32_t SIRPersistentRope::newlines( uint32_t iNode ) const noexcept
{
	if ( iNode == 0 ) { return 0; }
	return ( iNode & LEAF ) != 0 ? m_Storage.pLeaves[iNode & ~LEAF].nNewlines : m_Storage.pBranches[iNode - 1].nNewlines;
}

uint32_t SIRPersistentRope::utf8Starts( uint32_t iNode ) const noexcept
{
	if ( iNode == 0 ) { return 0; }
	return ( iNode & LEAF ) != 0 ? m_Storage.pLeaves[iNode & ~LEAF].nUtf8Starts : m_Storage.pBranches[iNode - 1].nUtf8Starts;
}

uint32_t SIRPersistentRope::getNewlineCount( sirWideRopeSnapshot_t snapshot ) const noexcept
{
	return isValid( snapshot ) ? newlines( snapshot.iRoot ) : 0;
}

uint32_t SIRPersistentRope::getUtf8Starts( sirWideRopeSnapshot_t snapshot ) const noexcept
{
	return isValid( snapshot ) ? utf8Starts( snapshot.iRoot ) : 0;
}

SIRContainerResult SIRPersistentRope::tryGetLineStart( sirWideRopeSnapshot_t snapshot, uint32_t iLine, uint32_t& iByte ) const noexcept
{
	if ( !isValid( snapshot ) || iLine > newlines( snapshot.iRoot ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	iByte = 0;
	if ( iLine == 0 ) { return SIRContainerResult::SIR_CONTAINER_OK; }
	uint32_t iNode = snapshot.iRoot;
	while ( ( iNode & LEAF ) == 0 ) {
		const sirWideRopeBranch_t& branch = m_Storage.pBranches[iNode - 1];
		for ( uint32_t i = 0; i < branch.nCount; ++i ) {
			const uint32_t iChild = branch.iChildren[i];
			const uint32_t nLines = newlines( iChild );
			if ( iLine <= nLines ) { iNode = iChild; break; }
			iLine -= nLines;
			iByte += bytes( iChild );
		}
	}
	const sirWideRopeLeaf_t& leaf = m_Storage.pLeaves[iNode & ~LEAF];
	for ( uint32_t i = 0; i < leaf.nCount; ++i ) {
		for ( uint32_t j = 0; j < leaf.fragments[i].nBytes; ++j ) {
			++iByte;
			if ( leaf.fragments[i].pBytes[j] == '\n' && --iLine == 0 ) { return SIRContainerResult::SIR_CONTAINER_OK; }
		}
	}
	return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
}

uint32_t SIRPersistentRope::cloneNode( uint32_t iNode, SIRPersistentRope& target ) const noexcept
{
	if ( iNode == 0 ) { return 0; }
	if ( ( iNode & LEAF ) != 0 ) {
		const sirWideRopeLeaf_t& leaf = m_Storage.pLeaves[iNode & ~LEAF];
		return target.createLeaf( leaf.fragments, leaf.nCount );
	}
	const sirWideRopeBranch_t& branch = m_Storage.pBranches[iNode - 1];
	uint32_t iChildren[6]{};
	for ( uint32_t i = 0; i < branch.nCount; ++i ) {
		iChildren[i] = cloneNode( branch.iChildren[i], target );
		if ( iChildren[i] == FAILED ) { return FAILED; }
	}
	return target.createBranch( iChildren, branch.nCount );
}

SIRContainerResult SIRPersistentRope::tryCompactTo( sirWideRopeSnapshot_t snapshot, SIRPersistentRope& target, sirWideRopeSnapshot_t& output ) const noexcept
{
	if ( !isValid( snapshot ) || &target == this || target.m_nGeneration == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	// Distinct pool ranges are a caller contract (including partial overlap).
	if ( target.m_Storage.pLeaves == m_Storage.pLeaves || target.m_Storage.pBranches == m_Storage.pBranches ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint32_t nLeaves = target.m_nLeaves;
	const uint32_t nBranches = target.m_nBranches;
	const uint32_t iRoot = cloneNode( snapshot.iRoot, target );
	if ( iRoot == FAILED ) {
		target.m_nLeaves = nLeaves;
		target.m_nBranches = nBranches;
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	output = { iRoot, target.m_nGeneration };
	return SIRContainerResult::SIR_CONTAINER_OK;
}


bool SIRPersistentRope::hasSummaries( void ) const noexcept
{
	return m_Storage.pLeafSummaries != nullptr && m_Storage.pBranchSummaries != nullptr;
}

sirWideRopeSummary_t SIRPersistentRope::summary( uint32_t iNode ) const noexcept
{
	if ( iNode == 0 ) { return {}; }
	return ( iNode & LEAF ) != 0 ? m_Storage.pLeafSummaries[iNode & ~LEAF] : m_Storage.pBranchSummaries[iNode - 1];
}

uint64_t SIRPersistentRope::hashPower( uint32_t nBytes ) noexcept
{
	uint64_t nFactor = 1099511628211ULL;
	uint64_t nPower = 1;
	while ( nBytes != 0 ) {
		if ( ( nBytes & 1u ) != 0 ) { nPower *= nFactor; }
		nBytes >>= 1;
		nFactor *= nFactor;
	}
	return nPower;
}

SIRContainerResult SIRPersistentRope::tryGetSummary( sirWideRopeSnapshot_t snapshot, sirWideRopeSummary_t& output ) const noexcept
{
	if ( !isValid( snapshot ) || !hasSummaries() ) { return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT; }
	output = summary( snapshot.iRoot );
	return SIRContainerResult::SIR_CONTAINER_OK;
}
