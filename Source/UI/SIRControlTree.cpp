/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2014-present Godot Engine contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <UI/SIRControlTree.h>

#include <string.h>

sirUIResult_t SIRControlTree::initialize( sirUIControl_t *pControls, uint32_t *pStack, uint32_t nCapacity ) noexcept
{
	if ( m_pControls != nullptr || pControls == nullptr || pStack == nullptr || nCapacity == 0 ||
	     nCapacity > UINT32_MAX / sizeof( *pControls ) ) {
		return SIR_UI_INVALID_ARGUMENT;
	}
	m_pControls = pControls;
	m_pStack = pStack;
	m_nCapacity = nCapacity;
	m_nCount = 0;
	m_Focus = { UINT32_MAX, 0 };
	m_nLayoutEpoch = 1;
	m_nPaintEpoch = 1;
	memset( pControls, 0, nCapacity * sizeof( *pControls ) );
	return SIR_UI_OK;
}

void SIRControlTree::shutdown( void ) noexcept
{
	m_pControls = nullptr;
	m_pStack = nullptr;
	m_nCapacity = 0;
	m_nCount = 0;
	m_Focus = {};
	m_nLayoutEpoch = 0;
	m_nPaintEpoch = 0;
}

bool SIRControlTree::isValid( const sirUIHandle_t &handle ) const noexcept
{
	return m_pControls != nullptr && handle.iSlot < m_nCapacity && m_pControls[handle.iSlot].bLive &&
	       m_pControls[handle.iSlot].nGeneration == handle.nGeneration;
}

sirUIResult_t SIRControlTree::create( const sirUIHandle_t *pParent, const sirUIRect_t &requested, sirUILayout_t eLayout, bool bFocusable,
                                      bool bClipChildren, sirUIHandle_t *pOutHandle ) noexcept
{
	if ( m_pControls == nullptr ) {
		return SIR_UI_NOT_INITIALIZED;
	}
	if ( pOutHandle == nullptr || requested.nWidth > INT32_MAX || requested.nHeight > INT32_MAX || eLayout > SIR_UI_LAYOUT_VERTICAL ) {
		return SIR_UI_INVALID_ARGUMENT;
	}
	if ( pParent != nullptr && !isValid( *pParent ) ) {
		return SIR_UI_STALE_HANDLE;
	}
	uint32_t iSlot = UINT32_MAX;
	for ( uint32_t iCurrent = 0; iCurrent < m_nCapacity; ++iCurrent ) {
		if ( !m_pControls[iCurrent].bLive && m_pControls[iCurrent].nGeneration != UINT32_MAX ) {
			iSlot = iCurrent;
			break;
		}
	}
	if ( iSlot == UINT32_MAX ) {
		return SIR_UI_OUT_OF_CAPACITY;
	}
	sirUIControl_t &control = m_pControls[iSlot];
	const uint32_t nGeneration = control.nGeneration + 1;
	control = {};
	control.nGeneration = nGeneration;
	control.bLive = true;
	control.bVisible = true;
	control.bFocusable = bFocusable;
	control.bClipChildren = bClipChildren;
	control.requested = requested;
	control.eLayout = eLayout;
	control.iParent = pParent != nullptr ? pParent->iSlot : UINT32_MAX;
	control.iFirstChild = UINT32_MAX;
	control.iLastChild = UINT32_MAX;
	control.iNextSibling = UINT32_MAX;
	if ( pParent != nullptr ) {
		sirUIControl_t &parent = m_pControls[pParent->iSlot];
		if ( parent.iLastChild != UINT32_MAX ) {
			m_pControls[parent.iLastChild].iNextSibling = iSlot;
		} else {
			parent.iFirstChild = iSlot;
		}
		parent.iLastChild = iSlot;
	}
	*pOutHandle = { iSlot, nGeneration };
	++m_nCount;
	++m_nLayoutEpoch;
	return SIR_UI_OK;
}

sirUIResult_t SIRControlTree::remove( sirUIHandle_t handle ) noexcept
{
	if ( !isValid( handle ) ) {
		return SIR_UI_STALE_HANDLE;
	}
	sirUIControl_t &root = m_pControls[handle.iSlot];
	if ( root.iParent != UINT32_MAX ) {
		sirUIControl_t &parent = m_pControls[root.iParent];
		uint32_t *pCurrent = &parent.iFirstChild;
		uint32_t iPrevious = UINT32_MAX;
		while ( *pCurrent != UINT32_MAX && *pCurrent != handle.iSlot ) {
			iPrevious = *pCurrent;
			pCurrent = &m_pControls[*pCurrent].iNextSibling;
		}
		if ( *pCurrent != handle.iSlot ) {
			return SIR_UI_INVALID_LAYOUT;
		}
		*pCurrent = root.iNextSibling;
		if ( parent.iLastChild == handle.iSlot ) {
			parent.iLastChild = iPrevious;
		}
	}
	uint32_t nTop = 0;
	m_pStack[nTop++] = handle.iSlot;
	while ( nTop > 0 ) {
		const uint32_t iSlot = m_pStack[--nTop];
		sirUIControl_t &control = m_pControls[iSlot];
		for ( uint32_t iChild = control.iFirstChild; iChild != UINT32_MAX; iChild = m_pControls[iChild].iNextSibling ) {
			m_pStack[nTop++] = iChild;
		}
		if ( m_Focus.iSlot == iSlot ) {
			m_Focus = { UINT32_MAX, 0 };
		}
		control.bLive = false;
		control.iParent = UINT32_MAX;
		control.iFirstChild = UINT32_MAX;
		control.iLastChild = UINT32_MAX;
		control.iNextSibling = UINT32_MAX;
		--m_nCount;
	}
	++m_nLayoutEpoch;
	++m_nPaintEpoch;
	return SIR_UI_OK;
}

sirUIResult_t SIRControlTree::setVisible( sirUIHandle_t handle, bool bVisible ) noexcept
{
	if ( !isValid( handle ) ) {
		return SIR_UI_STALE_HANDLE;
	}
	sirUIControl_t &control = m_pControls[handle.iSlot];
	if ( control.bVisible != bVisible ) {
		control.bVisible = bVisible;
		if ( !bVisible ) {
			uint32_t iAncestor = m_Focus.iSlot;
			while ( iAncestor != UINT32_MAX ) {
				if ( iAncestor == handle.iSlot ) {
					m_Focus = { UINT32_MAX, 0 };
					break;
				}
				iAncestor = m_pControls[iAncestor].iParent;
			}
		}
		++m_nPaintEpoch;
	}
	return SIR_UI_OK;
}

sirUIResult_t SIRControlTree::setRequestedRect( sirUIHandle_t handle, const sirUIRect_t &requested ) noexcept
{
	if ( !isValid( handle ) || requested.nWidth > INT32_MAX || requested.nHeight > INT32_MAX ) {
		return SIR_UI_INVALID_ARGUMENT;
	}
	m_pControls[handle.iSlot].requested = requested;
	++m_nLayoutEpoch;
	return SIR_UI_OK;
}

bool SIRControlTree::contains( const sirUIRect_t &bounds, int32_t iX, int32_t iY ) noexcept
{
	return ( int64_t )iX >= bounds.iX && ( int64_t )iY >= bounds.iY && ( int64_t )iX < ( int64_t )bounds.iX + bounds.nWidth &&
	       ( int64_t )iY < ( int64_t )bounds.iY + bounds.nHeight;
}

sirUIRect_t SIRControlTree::intersection( const sirUIRect_t &a, const sirUIRect_t &b ) noexcept
{
	const int64_t nLeft = a.iX > b.iX ? a.iX : b.iX;
	const int64_t nTop = a.iY > b.iY ? a.iY : b.iY;
	const int64_t nRightA = ( int64_t )a.iX + a.nWidth;
	const int64_t nRightB = ( int64_t )b.iX + b.nWidth;
	const int64_t nBottomA = ( int64_t )a.iY + a.nHeight;
	const int64_t nBottomB = ( int64_t )b.iY + b.nHeight;
	const int64_t nRight = nRightA < nRightB ? nRightA : nRightB;
	const int64_t nBottom = nBottomA < nBottomB ? nBottomA : nBottomB;
	if ( nRight <= nLeft || nBottom <= nTop ) {
		return { ( int32_t )nLeft, ( int32_t )nTop, 0, 0 };
	}
	return { ( int32_t )nLeft, ( int32_t )nTop, ( uint32_t )( nRight - nLeft ), ( uint32_t )( nBottom - nTop ) };
}

sirUIResult_t SIRControlTree::layout( const sirUIRect_t &viewport ) noexcept
{
	if ( m_pControls == nullptr ) {
		return SIR_UI_NOT_INITIALIZED;
	}
	if ( viewport.nWidth > INT32_MAX || viewport.nHeight > INT32_MAX ) {
		return SIR_UI_INVALID_ARGUMENT;
	}
	uint32_t nTop = 0;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		sirUIControl_t &control = m_pControls[iSlot];
		if ( control.bLive && control.iParent == UINT32_MAX ) {
			control.bounds = viewport;
			control.clip = viewport;
			m_pStack[nTop++] = iSlot;
		}
	}
	while ( nTop > 0 ) {
		const uint32_t iParent = m_pStack[--nTop];
		const sirUIControl_t &parent = m_pControls[iParent];
		uint64_t nFlowY = 0;
		for ( uint32_t iChild = parent.iFirstChild; iChild != UINT32_MAX; iChild = m_pControls[iChild].iNextSibling ) {
			sirUIControl_t &child = m_pControls[iChild];
			const int64_t iX = ( int64_t )parent.bounds.iX + child.requested.iX;
			const int64_t iY = ( int64_t )parent.bounds.iY + child.requested.iY + ( parent.eLayout == SIR_UI_LAYOUT_VERTICAL ? nFlowY : 0 );
			if ( iX < INT32_MIN || iX > INT32_MAX || iY < INT32_MIN || iY > INT32_MAX ) {
				return SIR_UI_INVALID_LAYOUT;
			}
			child.bounds = { ( int32_t )iX, ( int32_t )iY, child.requested.nWidth == 0 ? parent.bounds.nWidth : child.requested.nWidth,
			                 child.requested.nHeight };
			child.clip = parent.bClipChildren ? intersection( parent.clip, parent.bounds ) : parent.clip;
			if ( parent.eLayout == SIR_UI_LAYOUT_VERTICAL ) {
				nFlowY += child.requested.nHeight;
			}
			m_pStack[nTop++] = iChild;
		}
	}
	++m_nPaintEpoch;
	return SIR_UI_OK;
}

sirUIResult_t SIRControlTree::hitTest( int32_t iX, int32_t iY, sirUIHandle_t *pOutHandle ) noexcept
{
	if ( m_pControls == nullptr ) {
		return SIR_UI_NOT_INITIALIZED;
	}
	if ( pOutHandle == nullptr ) {
		return SIR_UI_INVALID_ARGUMENT;
	}
	*pOutHandle = { UINT32_MAX, 0 };
	uint32_t iCandidate = UINT32_MAX;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirUIControl_t &control = m_pControls[iSlot];
		if ( control.bLive && control.iParent == UINT32_MAX && control.bVisible && contains( control.clip, iX, iY ) &&
		     contains( control.bounds, iX, iY ) ) {
			iCandidate = iSlot;
		}
	}
	while ( iCandidate != UINT32_MAX ) {
		const sirUIControl_t &control = m_pControls[iCandidate];
		*pOutHandle = { iCandidate, control.nGeneration };
		uint32_t iNext = UINT32_MAX;
		for ( uint32_t iChild = control.iFirstChild; iChild != UINT32_MAX; iChild = m_pControls[iChild].iNextSibling ) {
			const sirUIControl_t &child = m_pControls[iChild];
			if ( child.bVisible && contains( child.clip, iX, iY ) && contains( child.bounds, iX, iY ) ) {
				iNext = iChild;
			}
		}
		iCandidate = iNext;
	}
	return SIR_UI_OK;
}

sirUIResult_t SIRControlTree::dispatch( const sirUIInput_t &input, sirUIHandle_t *pTarget ) noexcept
{
	if ( m_pControls == nullptr ) {
		return SIR_UI_NOT_INITIALIZED;
	}
	if ( pTarget == nullptr ) {
		return SIR_UI_INVALID_ARGUMENT;
	}
	if ( input.eType == SIR_UI_INPUT_KEY_DOWN ) {
		*pTarget = m_Focus;
		return SIR_UI_OK;
	}
	if ( input.eType != SIR_UI_INPUT_POINTER_DOWN && input.eType != SIR_UI_INPUT_POINTER_MOVE ) {
		return SIR_UI_INVALID_ARGUMENT;
	}
	const sirUIResult_t eResult = hitTest( input.iX, input.iY, pTarget );
	if ( eResult != SIR_UI_OK ) {
		return eResult;
	}
	if ( input.eType == SIR_UI_INPUT_POINTER_DOWN ) {
		sirUIHandle_t focus = { UINT32_MAX, 0 };
		if ( isValid( *pTarget ) && m_pControls[pTarget->iSlot].bFocusable ) {
			focus = *pTarget;
		}
		if ( focus.iSlot != m_Focus.iSlot || focus.nGeneration != m_Focus.nGeneration ) {
			m_Focus = focus;
			++m_nPaintEpoch;
		}
	}
	return SIR_UI_OK;
}

sirUIResult_t SIRControlTree::getBounds( sirUIHandle_t handle, sirUIRect_t *pOutBounds ) const noexcept
{
	if ( !isValid( handle ) || pOutBounds == nullptr ) {
		return SIR_UI_STALE_HANDLE;
	}
	*pOutBounds = m_pControls[handle.iSlot].bounds;
	return SIR_UI_OK;
}

sirUIHandle_t SIRControlTree::getFocus( void ) const noexcept
{
	return m_Focus;
}

uint64_t SIRControlTree::getLayoutEpoch( void ) const noexcept
{
	return m_nLayoutEpoch;
}

uint64_t SIRControlTree::getPaintEpoch( void ) const noexcept
{
	return m_nPaintEpoch;
}

uint32_t SIRControlTree::getCount( void ) const noexcept
{
	return m_nCount;
}
