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

#include <Core/Strings/SIRRopeString.h>
#include <Core/Strings/SIRStringOps.h>

SIRRopeString::SIRRopeString( sirRopeNode_t *pNodes, uint32_t nCapacity ) noexcept
    : m_pNodes( pNodes ),
      m_nCapacity( pNodes == nullptr || nCapacity > UINT32_MAX / sizeof( sirRopeNode_t ) ? 0 : nCapacity )
{
	clear();
}

uint32_t SIRRopeString::getByteLength( void ) const noexcept
{
	return bytes( m_iRoot );
}

uint32_t SIRRopeString::getFreeNodeCount( void ) const noexcept
{
	return m_nFree;
}

uint32_t SIRRopeString::getHeight( void ) const noexcept
{
	return height( m_iRoot );
}

sirRopeNode_t& SIRRopeString::node( uint32_t iNode ) noexcept
{
	return m_pNodes[iNode - 1];
}

const sirRopeNode_t& SIRRopeString::node( uint32_t iNode ) const noexcept
{
	return m_pNodes[iNode - 1];
}

uint32_t SIRRopeString::bytes( uint32_t iNode ) const noexcept
{
	return iNode == 0 ? 0 : node( iNode ).nSubtreeBytes;
}

uint32_t SIRRopeString::height( uint32_t iNode ) const noexcept
{
	return iNode == 0 ? 0 : node( iNode ).nHeight;
}

uint32_t SIRRopeString::allocate( sirStringView_t view ) noexcept
{
	// Public edit operations preflight all nodes before entering the mutation phase.
	assert( m_nFree != 0 );
	const uint32_t iNode = m_iFree == 0 ? ++m_nVirgin : m_iFree;
	if ( m_iFree != 0 ) {
		m_iFree = node( iNode ).iLeft;
	}
	--m_nFree;
	node( iNode ) = { view, 0, 0, view.nBytes, 1 };
	return iNode;
}

void SIRRopeString::update( uint32_t iRoot ) noexcept
{
	sirRopeNode_t& root = node( iRoot );
	root.nSubtreeBytes = bytes( root.iLeft ) + root.view.nBytes + bytes( root.iRight );
	const uint32_t nLeftHeight = height( root.iLeft );
	const uint32_t nRightHeight = height( root.iRight );
	root.nHeight = 1 + ( nLeftHeight > nRightHeight ? nLeftHeight : nRightHeight );
}

uint32_t SIRRopeString::rotateLeft( uint32_t iRoot ) noexcept
{
	const uint32_t iRight = node( iRoot ).iRight;
	node( iRoot ).iRight = node( iRight ).iLeft;
	node( iRight ).iLeft = iRoot;
	update( iRoot );
	update( iRight );
	return iRight;
}

uint32_t SIRRopeString::rotateRight( uint32_t iRoot ) noexcept
{
	const uint32_t iLeft = node( iRoot ).iLeft;
	node( iRoot ).iLeft = node( iLeft ).iRight;
	node( iLeft ).iRight = iRoot;
	update( iRoot );
	update( iLeft );
	return iLeft;
}

uint32_t SIRRopeString::rebalance( uint32_t iRoot ) noexcept
{
	update( iRoot );
	const uint32_t iLeft = node( iRoot ).iLeft;
	const uint32_t iRight = node( iRoot ).iRight;
	if ( height( iLeft ) > height( iRight ) + 1 ) {
		if ( height( node( iLeft ).iRight ) > height( node( iLeft ).iLeft ) ) {
			node( iRoot ).iLeft = rotateLeft( iLeft );
		}
		return rotateRight( iRoot );
	}
	if ( height( iRight ) > height( iLeft ) + 1 ) {
		if ( height( node( iRight ).iLeft ) > height( node( iRight ).iRight ) ) {
			node( iRoot ).iRight = rotateRight( iRight );
		}
		return rotateLeft( iRoot );
	}
	return iRoot;
}

uint32_t SIRRopeString::join( uint32_t iLeft, uint32_t iMiddle, uint32_t iRight ) noexcept
{
	if ( height( iLeft ) > height( iRight ) + 1 ) {
		node( iLeft ).iRight = join( node( iLeft ).iRight, iMiddle, iRight );
		return rebalance( iLeft );
	}
	if ( height( iRight ) > height( iLeft ) + 1 ) {
		node( iRight ).iLeft = join( iLeft, iMiddle, node( iRight ).iLeft );
		return rebalance( iRight );
	}
	node( iMiddle ).iLeft = iLeft;
	node( iMiddle ).iRight = iRight;
	update( iMiddle );
	return iMiddle;
}

uint32_t SIRRopeString::detachFirst( uint32_t iRoot, uint32_t& iFirst ) noexcept
{
	if ( node( iRoot ).iLeft == 0 ) {
		iFirst = iRoot;
		return node( iRoot ).iRight;
	}
	node( iRoot ).iLeft = detachFirst( node( iRoot ).iLeft, iFirst );
	return rebalance( iRoot );
}

uint32_t SIRRopeString::merge( uint32_t iLeft, uint32_t iRight ) noexcept
{
	if ( iLeft == 0 ) {
		return iRight;
	}
	if ( iRight == 0 ) {
		return iLeft;
	}
	uint32_t iMiddle = 0;
	const uint32_t iRemainder = detachFirst( iRight, iMiddle );
	return join( iLeft, iMiddle, iRemainder );
}

void SIRRopeString::split( uint32_t iRoot, uint32_t iPosition, uint32_t& iLeft, uint32_t& iRight ) noexcept
{
	if ( iRoot == 0 ) {
		iLeft = 0;
		iRight = 0;
		return;
	}
	const uint32_t iOldLeft = node( iRoot ).iLeft;
	const uint32_t iOldRight = node( iRoot ).iRight;
	const sirStringView_t view = node( iRoot ).view;
	const uint32_t nLeft = bytes( iOldLeft );
	const uint32_t nEnd = nLeft + view.nBytes;
	if ( iPosition < nLeft ) {
		uint32_t iRemainder = 0;
		split( iOldLeft, iPosition, iLeft, iRemainder );
		iRight = join( iRemainder, iRoot, iOldRight );
	} else if ( iPosition > nEnd ) {
		uint32_t iRemainder = 0;
		split( iOldRight, iPosition - nEnd, iRemainder, iRight );
		iLeft = join( iOldLeft, iRoot, iRemainder );
	} else if ( iPosition == nLeft ) {
		iLeft = iOldLeft;
		iRight = join( 0, iRoot, iOldRight );
	} else if ( iPosition == nEnd ) {
		iLeft = join( iOldLeft, iRoot, 0 );
		iRight = iOldRight;
	} else {
		const uint32_t nPrefix = iPosition - nLeft;
		const uint32_t iSuffix = allocate( { view.pBytes + nPrefix, view.nBytes - nPrefix } );
		node( iRoot ).view.nBytes = nPrefix;
		iLeft = join( iOldLeft, iRoot, 0 );
		iRight = join( 0, iSuffix, iOldRight );
	}
}

bool SIRRopeString::isBoundary( uint32_t iPosition ) const noexcept
{
	uint32_t iNode = m_iRoot;
	while ( iNode != 0 ) {
		const sirRopeNode_t& current = node( iNode );
		const uint32_t nLeft = bytes( current.iLeft );
		const uint32_t nEnd = nLeft + current.view.nBytes;
		if ( iPosition == nLeft || iPosition == nEnd ) {
			return true;
		}
		if ( iPosition < nLeft ) {
			iNode = current.iLeft;
		} else if ( iPosition > nEnd ) {
			iPosition -= nEnd;
			iNode = current.iRight;
		} else {
			return false;
		}
	}
	return iPosition == 0;
}

SIRContainerResult SIRRopeString::tryGetByte( uint32_t iByte, char& nByte ) const noexcept
{
	if ( iByte >= getByteLength() ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iNode = m_iRoot;
	while ( iNode != 0 ) {
		const sirRopeNode_t& current = node( iNode );
		const uint32_t nLeft = bytes( current.iLeft );
		if ( iByte < nLeft ) {
			iNode = current.iLeft;
			continue;
		}
		iByte -= nLeft;
		if ( iByte < current.view.nBytes ) {
			nByte = current.view.pBytes[iByte];
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		iByte -= current.view.nBytes;
		iNode = current.iRight;
	}
	return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
}

SIRContainerResult SIRRopeString::tryInsert( uint32_t iPosition, sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) || iPosition > getByteLength() ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( SIRStringOps::overlaps( view, { reinterpret_cast<const char *>( m_pNodes ), static_cast<uint32_t>( m_nCapacity * sizeof( sirRopeNode_t ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes > UINT32_MAX - getByteLength() ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const uint32_t nRequired = isBoundary( iPosition ) ? 1 : 2;
	if ( m_nFree < nRequired ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	uint32_t iLeft = 0;
	uint32_t iRight = 0;
	split( m_iRoot, iPosition, iLeft, iRight );
	m_iRoot = join( iLeft, allocate( view ), iRight );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRRopeString::tryErase( uint32_t iPosition, uint32_t nBytes ) noexcept
{
	if ( iPosition > getByteLength() ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes > getByteLength() - iPosition ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	const uint32_t nRequired = static_cast<uint32_t>( !isBoundary( iPosition ) ) + static_cast<uint32_t>( !isBoundary( iPosition + nBytes ) );
	if ( m_nFree < nRequired ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	uint32_t iLeft = 0;
	uint32_t iRest = 0;
	uint32_t iRemoved = 0;
	uint32_t iRight = 0;
	split( m_iRoot, iPosition, iLeft, iRest );
	split( iRest, nBytes, iRemoved, iRight );
	releaseTree( iRemoved );
	m_iRoot = merge( iLeft, iRight );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRRopeString::releaseTree( uint32_t iRoot ) noexcept
{
	if ( iRoot == 0 ) {
		return;
	}
	releaseTree( node( iRoot ).iLeft );
	releaseTree( node( iRoot ).iRight );
	node( iRoot ) = {};
	node( iRoot ).iLeft = m_iFree;
	m_iFree = iRoot;
	++m_nFree;
}

bool SIRRopeString::overlapsTree( uint32_t iRoot, sirStringView_t output ) const noexcept
{
	if ( iRoot == 0 ) {
		return false;
	}
	const sirRopeNode_t& current = node( iRoot );
	return SIRStringOps::overlaps( output, current.view ) || overlapsTree( current.iLeft, output ) || overlapsTree( current.iRight, output );
}

void SIRRopeString::copyTree( uint32_t iRoot, char *pOutput, uint32_t& iOutput ) const noexcept
{
	if ( iRoot == 0 ) {
		return;
	}
	const sirRopeNode_t& current = node( iRoot );
	copyTree( current.iLeft, pOutput, iOutput );
	memcpy( pOutput + iOutput, current.view.pBytes, current.view.nBytes );
	iOutput += current.view.nBytes;
	copyTree( current.iRight, pOutput, iOutput );
}

SIRContainerResult SIRRopeString::tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept
{
	if ( pszOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint32_t nLength = getByteLength();
	if ( nCapacity <= nLength ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( SIRStringOps::overlaps( { pszOutput, nLength + 1 },
	                             { reinterpret_cast<const char *>( m_pNodes ), static_cast<uint32_t>( m_nCapacity * sizeof( sirRopeNode_t ) ) } ) ||
	     overlapsTree( m_iRoot, { pszOutput, nLength + 1 } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iOutput = 0;
	copyTree( m_iRoot, pszOutput, iOutput );
	pszOutput[nLength] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRRopeString::clear( void ) noexcept
{
	m_iRoot = 0;
	m_iFree = 0;
	m_nVirgin = 0;
	m_nFree = m_nCapacity;
}

bool SIRRopeString::extendLast( uint32_t iRoot, sirStringView_t backing, uint32_t iOffset, uint32_t nBytes ) noexcept
{
	sirRopeNode_t& current = node( iRoot );
	if ( current.iRight != 0 ) {
		if ( !extendLast( current.iRight, backing, iOffset, nBytes ) ) {
			return false;
		}
	} else {
		if ( current.view.nBytes > iOffset || current.view.pBytes != backing.pBytes + iOffset - current.view.nBytes ) {
			return false;
		}
		current.view.nBytes += nBytes;
	}
	update( iRoot );
	return true;
}

SIRContainerResult SIRRopeString::tryAppendContiguous( sirStringView_t backing, uint32_t iOffset, uint32_t nBytes ) noexcept
{
	if ( !SIRStringOps::isValid( backing ) || iOffset > backing.nBytes || nBytes > backing.nBytes - iOffset ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( nBytes > UINT32_MAX - getByteLength() ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const sirStringView_t view{ backing.pBytes + iOffset, nBytes };
	if ( SIRStringOps::overlaps( view, { reinterpret_cast<const char *>( m_pNodes ), static_cast<uint32_t>( m_nCapacity * sizeof( sirRopeNode_t ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( m_iRoot != 0 && extendLast( m_iRoot, backing, iOffset, nBytes ) ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	return tryInsert( getByteLength(), view );
}
