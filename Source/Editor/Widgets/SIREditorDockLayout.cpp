/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/
#include <Editor/Widgets/SIREditorDockLayout.h>

SIREditorDockLayout::SIREditorDockLayout( void ) noexcept : m_pNodes( nullptr ), m_pStack( nullptr ), m_nCapacity( 0 ), m_nCount( 0 )
{
}
uint32_t SIREditorDockLayout::getCount( void ) const noexcept
{
	return m_nCount;
}

sirEditorDockResult_t SIREditorDockLayout::initialize( sirEditorDockNode_t *pNodes, uint32_t *pStack, uint32_t nCapacity ) noexcept
{
	if ( pNodes == nullptr || pStack == nullptr || nCapacity == 0 ) {
		return SIR_EDITOR_DOCK_INVALID_ARGUMENT;
	}
	if ( m_pNodes != nullptr ) {
		return SIR_EDITOR_DOCK_INVALID_STRUCTURE;
	}
	m_pNodes = pNodes;
	m_pStack = pStack;
	m_nCapacity = nCapacity;
	m_nCount = 0;
	return SIR_EDITOR_DOCK_OK;
}

void SIREditorDockLayout::shutdown( void ) noexcept
{
	m_pNodes = nullptr;
	m_pStack = nullptr;
	m_nCapacity = 0;
	m_nCount = 0;
}

sirEditorDockResult_t SIREditorDockLayout::createPanel( uint32_t nPanelId, uint32_t *pOutIndex ) noexcept
{
	if ( m_pNodes == nullptr ) {
		return SIR_EDITOR_DOCK_NOT_INITIALIZED;
	}
	if ( pOutIndex == nullptr || nPanelId == 0 || m_nCount != 0 ) {
		return SIR_EDITOR_DOCK_INVALID_ARGUMENT;
	}
	m_pNodes[0] = {};
	m_pNodes[0].nPanelId = nPanelId;
	m_pNodes[0].eKind = SIR_EDITOR_DOCK_PANEL;
	m_nCount = 1;
	*pOutIndex = 0;
	return SIR_EDITOR_DOCK_OK;
}

sirEditorDockResult_t SIREditorDockLayout::split( uint32_t iPanel, sirEditorDockKind_t eAxis, uint16_t nPermille,
                                                  uint32_t nPanelId ) noexcept
{
	if ( m_pNodes == nullptr ) {
		return SIR_EDITOR_DOCK_NOT_INITIALIZED;
	}
	if ( iPanel >= m_nCount || m_pNodes[iPanel].eKind != SIR_EDITOR_DOCK_PANEL || eAxis == SIR_EDITOR_DOCK_PANEL ||
	     ( eAxis != SIR_EDITOR_DOCK_SPLIT_HORIZONTAL && eAxis != SIR_EDITOR_DOCK_SPLIT_VERTICAL ) || nPermille == 0 || nPermille >= 1000 ||
	     nPanelId == 0 ) {
		return SIR_EDITOR_DOCK_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		if ( m_pNodes[i].eKind == SIR_EDITOR_DOCK_PANEL && m_pNodes[i].nPanelId == nPanelId ) {
			return SIR_EDITOR_DOCK_INVALID_ARGUMENT;
		}
	}
	if ( m_nCapacity - m_nCount < 2 ) {
		return SIR_EDITOR_DOCK_OUT_OF_CAPACITY;
	}
	const uint32_t iFirst = m_nCount;
	const uint32_t iSecond = m_nCount + 1;
	m_pNodes[iFirst] = m_pNodes[iPanel];
	m_pNodes[iSecond] = {};
	m_pNodes[iSecond].nPanelId = nPanelId;
	m_pNodes[iSecond].eKind = SIR_EDITOR_DOCK_PANEL;
	m_pNodes[iPanel] = {};
	m_pNodes[iPanel].iFirst = iFirst;
	m_pNodes[iPanel].iSecond = iSecond;
	m_pNodes[iPanel].nSplitPermille = nPermille;
	m_pNodes[iPanel].eKind = eAxis;
	m_nCount += 2;
	return SIR_EDITOR_DOCK_OK;
}

sirEditorDockResult_t SIREditorDockLayout::layout( const sirUIRect_t& viewport, uint32_t nMinPanelExtent ) noexcept
{
	if ( m_pNodes == nullptr ) {
		return SIR_EDITOR_DOCK_NOT_INITIALIZED;
	}
	if ( m_nCount == 0 || nMinPanelExtent == 0 || viewport.nWidth == 0 || viewport.nHeight == 0 || viewport.nWidth > INT32_MAX ||
	     viewport.nHeight > INT32_MAX || ( int64_t )viewport.iX + viewport.nWidth > INT32_MAX ||
	     ( int64_t )viewport.iY + viewport.nHeight > INT32_MAX ) {
		return SIR_EDITOR_DOCK_INVALID_ARGUMENT;
	}
	// Preflight structural integrity and the minimum size required by every
	// split before mutating any cached bounds.
	m_pStack[0] = 0;
	uint32_t nPending = 1;
	while ( nPending != 0 ) {
		const uint32_t i = m_pStack[--nPending];
		const sirEditorDockNode_t& node = m_pNodes[i];
		if ( node.eKind == SIR_EDITOR_DOCK_PANEL ) {
			continue;
		}
		if ( node.iFirst <= i || node.iSecond <= i || node.iFirst >= m_nCount || node.iSecond >= m_nCount || node.iFirst == node.iSecond ||
		     node.nSplitPermille == 0 || node.nSplitPermille >= 1000 ) {
			return SIR_EDITOR_DOCK_INVALID_STRUCTURE;
		}
		if ( nPending + 2 > m_nCapacity ) {
			return SIR_EDITOR_DOCK_OUT_OF_CAPACITY;
		}
		m_pStack[nPending++] = node.iFirst;
		m_pStack[nPending++] = node.iSecond;
	}
	// Use temporary bounds in the same caller-owned nodes. Layout failure can
	// still be diagnosed without publishing a new paint epoch.
	m_pNodes[0].bounds = viewport;
	m_pStack[0] = 0;
	nPending = 1;
	while ( nPending != 0 ) {
		const uint32_t i = m_pStack[--nPending];
		const sirEditorDockNode_t& node = m_pNodes[i];
		if ( node.eKind == SIR_EDITOR_DOCK_PANEL ) {
			if ( node.bounds.nWidth < nMinPanelExtent || node.bounds.nHeight < nMinPanelExtent ) {
				return SIR_EDITOR_DOCK_TOO_SMALL;
			}
			continue;
		}
		const bool bHorizontal = node.eKind == SIR_EDITOR_DOCK_SPLIT_HORIZONTAL;
		const uint32_t nExtent = bHorizontal ? node.bounds.nWidth : node.bounds.nHeight;
		const uint32_t nFirst = ( uint32_t )( ( ( uint64_t )nExtent * node.nSplitPermille ) / 1000 );
		const uint32_t nSecond = nExtent - nFirst;
		if ( nFirst < nMinPanelExtent || nSecond < nMinPanelExtent ) {
			return SIR_EDITOR_DOCK_TOO_SMALL;
		}
		sirUIRect_t first = node.bounds;
		sirUIRect_t second = node.bounds;
		if ( bHorizontal ) {
			first.nWidth = nFirst;
			second.iX += ( int32_t )nFirst;
			second.nWidth = nSecond;
		} else {
			first.nHeight = nFirst;
			second.iY += ( int32_t )nFirst;
			second.nHeight = nSecond;
		}
		m_pNodes[node.iFirst].bounds = first;
		m_pNodes[node.iSecond].bounds = second;
		m_pStack[nPending++] = node.iFirst;
		m_pStack[nPending++] = node.iSecond;
	}
	return SIR_EDITOR_DOCK_OK;
}

sirEditorDockResult_t SIREditorDockLayout::getPanelBounds( uint32_t nPanelId, sirUIRect_t *pOutBounds ) const noexcept
{
	if ( m_pNodes == nullptr ) {
		return SIR_EDITOR_DOCK_NOT_INITIALIZED;
	}
	if ( nPanelId == 0 || pOutBounds == nullptr ) {
		return SIR_EDITOR_DOCK_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		if ( m_pNodes[i].eKind == SIR_EDITOR_DOCK_PANEL && m_pNodes[i].nPanelId == nPanelId ) {
			*pOutBounds = m_pNodes[i].bounds;
			return SIR_EDITOR_DOCK_OK;
		}
	}
	return SIR_EDITOR_DOCK_INVALID_ARGUMENT;
}
