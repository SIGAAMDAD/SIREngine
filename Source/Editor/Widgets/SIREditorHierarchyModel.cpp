#include <Editor/Widgets/SIREditorHierarchyModel.h>

#include <algorithm>
#include <limits.h>

static bool sirEditorHierarchyLinkLess( const sirEditorHierarchyLink_t& a, const sirEditorHierarchyLink_t& b ) noexcept
{
	if ( a.nParentId != b.nParentId ) {
		return a.nParentId < b.nParentId;
	}
	if ( a.nOrder != b.nOrder ) {
		return a.nOrder < b.nOrder;
	}
	return a.nId < b.nId;
}

static bool sirEditorHierarchyOverlap( const void *pA, size_t nA, const void *pB, size_t nB ) noexcept
{
	uintptr_t nLeft = ( uintptr_t )pA;
	uintptr_t nRight = ( uintptr_t )pB;
	if ( nA > UINTPTR_MAX - nLeft || nB > UINTPTR_MAX - nRight ) {
		return true;
	}
	return nLeft < nRight + nB && nRight < nLeft + nA;
}

static bool sirEditorHierarchyOrderLess( const sirEditorHierarchyOrder_t& a, const sirEditorHierarchyOrder_t& b ) noexcept
{
	return a.nId < b.nId;
}

sirEditorHierarchyResult_t sirEditorHierarchyOrderFromTextScene( const sirEditorTextSceneIndex_t& index, sirEditorHierarchyOrder_t *pOutput,
                                                                 uint32_t nCapacity, uint32_t *pOutCount ) noexcept
{
	if ( pOutCount == nullptr ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	*pOutCount = 0;
	if ( index.nNodeCount > index.storage.nNodeCapacity ) {
		return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
	}


	if ( index.nNodeCount > nCapacity ) {
		return SIR_EDITOR_HIERARCHY_OUT_OF_CAPACITY;
	}
	if ( index.nNodeCount != 0 && ( pOutput == nullptr || index.storage.pNodes == nullptr ) ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	if ( index.nNodeCount != 0 &&
	     sirEditorHierarchyOverlap( pOutput, ( size_t )index.nNodeCount * sizeof( sirEditorHierarchyOrder_t ), index.storage.pNodes,
	                                ( size_t )index.nNodeCount * sizeof( sirEditorTextSceneNode_t ) ) ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	for ( uint32_t iNode = 0; iNode < index.nNodeCount; ++iNode ) {
		const sirEditorTextSceneNode_t& node = index.storage.pNodes[iNode];
		if ( node.node.nId == 0 || node.nSourceLine == 0 ) {
			return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
		}
	}
	for ( uint32_t iNode = 0; iNode < index.nNodeCount; ++iNode ) {
		const sirEditorTextSceneNode_t& node = index.storage.pNodes[iNode];
		pOutput[iNode] = { node.node.nId, node.nSourceLine, 0 };
	}
	std::sort( pOutput, pOutput + index.nNodeCount, sirEditorHierarchyOrderLess );
	for ( uint32_t iNode = 1; iNode < index.nNodeCount; ++iNode ) {
		if ( pOutput[iNode].nId == pOutput[iNode - 1].nId ) {
			*pOutCount = 0;
			return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
		}
	}
	*pOutCount = index.nNodeCount;
	return SIR_EDITOR_HIERARCHY_OK;
}

SIREditorHierarchyModel::SIREditorHierarchyModel( void ) noexcept
    : m_Storage{}, m_List(), m_nSourceRevision( 0 ), m_nLayoutEpoch( 0 ), m_nPaintEpoch( 0 ), m_nSelectedId( 0 ), m_nNodeCount( 0 ),
      m_nVisibleCount( 0 ), m_nDefaultRowHeight( 0 ), m_bInitialized( false )
{
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::initialize( const sirEditorHierarchyStorage_t& storage,
                                                                uint32_t nDefaultRowHeight ) noexcept
{
	if ( m_bInitialized || storage.nCapacity == 0 || storage.nCapacity == UINT32_MAX || nDefaultRowHeight == 0 ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	const void *szPointers[8] = { storage.pSnapshot, storage.pLinks,   storage.pState,    storage.pNextState,
	                              storage.pStack,    storage.pVisible, storage.pListRows, storage.pFenwick };
	const size_t szSizes[8] = { sizeof( sirEditorNode_t ),
	                            sizeof( sirEditorHierarchyLink_t ),
	                            sizeof( sirEditorHierarchyState_t ),
	                            sizeof( sirEditorHierarchyState_t ),
	                            sizeof( sirEditorHierarchyCursor_t ),
	                            sizeof( sirEditorHierarchyRow_t ),
	                            sizeof( sirEditorVirtualRow_t ),
	                            sizeof( uint64_t ) };
	for ( uint32_t iBuffer = 0; iBuffer < 8; ++iBuffer ) {
		if ( szPointers[iBuffer] == nullptr ) {
			return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
		}
		size_t nCount = ( size_t )storage.nCapacity + ( iBuffer == 7 ? 1u : 0u );
		if ( nCount > SIZE_MAX / szSizes[iBuffer] ) {
			return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iBuffer; ++iPrevious ) {
			size_t nPreviousCount = ( size_t )storage.nCapacity + ( iPrevious == 7 ? 1u : 0u );
			if ( sirEditorHierarchyOverlap( szPointers[iBuffer], nCount * szSizes[iBuffer], szPointers[iPrevious],
			                                nPreviousCount * szSizes[iPrevious] ) ) {
				return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
			}
		}
	}

	if ( m_List.initialize( storage.pListRows, storage.pFenwick, storage.nCapacity ) != SIR_EDITOR_LIST_OK ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	m_Storage = storage;
	m_nDefaultRowHeight = nDefaultRowHeight;
	m_bInitialized = true;
	return SIR_EDITOR_HIERARCHY_OK;
}

void SIREditorHierarchyModel::shutdown( void ) noexcept
{
	m_List.shutdown();
	m_Storage = {};
	m_nSourceRevision = 0;
	m_nLayoutEpoch = 0;
	m_nPaintEpoch = 0;
	m_nSelectedId = 0;
	m_nNodeCount = 0;
	m_nVisibleCount = 0;
	m_nDefaultRowHeight = 0;
	m_bInitialized = false;
}

uint32_t SIREditorHierarchyModel::findState( uint64_t nId, const sirEditorHierarchyState_t *pStates, uint32_t nCount ) const noexcept
{
	uint32_t iFirst = 0;
	uint32_t iLast = nCount;
	while ( iFirst < iLast ) {
		uint32_t iMiddle = iFirst + ( iLast - iFirst ) / 2;
		if ( pStates[iMiddle].nId < nId ) {
			iFirst = iMiddle + 1;
		} else {
			iLast = iMiddle;
		}
	}
	return iFirst < nCount && pStates[iFirst].nId == nId ? iFirst : UINT32_MAX;
}

uint32_t SIREditorHierarchyModel::findFirstChild( uint64_t nParentId ) const noexcept
{
	uint32_t iFirst = 0;
	uint32_t iLast = m_nNodeCount;
	while ( iFirst < iLast ) {
		uint32_t iMiddle = iFirst + ( iLast - iFirst ) / 2;
		if ( m_Storage.pLinks[iMiddle].nParentId < nParentId ) {
			iFirst = iMiddle + 1;
		} else {
			iLast = iMiddle;
		}
	}
	return iFirst;
}

uint32_t SIREditorHierarchyModel::findAfterChildren( uint64_t nParentId ) const noexcept
{
	uint32_t iFirst = findFirstChild( nParentId );
	uint32_t iLast = m_nNodeCount;
	while ( iFirst < iLast ) {
		uint32_t iMiddle = iFirst + ( iLast - iFirst ) / 2;
		if ( m_Storage.pLinks[iMiddle].nParentId <= nParentId ) {
			iFirst = iMiddle + 1;
		} else {
			iLast = iMiddle;
		}
	}
	return iFirst;
}

bool SIREditorHierarchyModel::hasChildren( uint64_t nId ) const noexcept
{
	uint32_t iFirst = findFirstChild( nId );
	return iFirst < m_nNodeCount && m_Storage.pLinks[iFirst].nParentId == nId;
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::buildVisibleRows( void ) noexcept
{
	uint32_t nStack = 0;
	uint32_t nVisible = 0;
	uint32_t nFirstRoot = findFirstChild( 0 );
	uint32_t nAfterRoots = findAfterChildren( 0 );
	if ( nFirstRoot < nAfterRoots ) {
		m_Storage.pStack[nStack++] = { nFirstRoot, nAfterRoots, 0 };
	}

	while ( nStack != 0 ) {
		sirEditorHierarchyCursor_t& cursor = m_Storage.pStack[nStack - 1];
		if ( cursor.iNext == cursor.iEnd ) {
			--nStack;
			continue;
		}
		const sirEditorHierarchyLink_t& link = m_Storage.pLinks[cursor.iNext++];
		uint32_t iState = findState( link.nId, m_Storage.pState, m_nNodeCount );
		if ( iState == UINT32_MAX || nVisible >= m_Storage.nCapacity ) {
			return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
		}
		const sirEditorHierarchyState_t& state = m_Storage.pState[iState];
		bool bChildren = hasChildren( link.nId );
		m_Storage.pVisible[nVisible] = { link.nId, link.nParentId, cursor.nDepth, bChildren, state.bExpanded, false };
		m_Storage.pListRows[nVisible] = { link.nId, state.nRowHeight, 0 };
		++nVisible;

		if ( bChildren && state.bExpanded ) {
			if ( nStack >= m_Storage.nCapacity ) {
				return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
			}
			m_Storage.pStack[nStack++] = { findFirstChild( link.nId ), findAfterChildren( link.nId ), cursor.nDepth + 1 };
		}
	}
	if ( m_List.setRows( m_Storage.pListRows, nVisible ) != SIR_EDITOR_LIST_OK ) {
		return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
	}
	m_nVisibleCount = nVisible;
	++m_nLayoutEpoch;
	++m_nPaintEpoch;
	return SIR_EDITOR_HIERARCHY_OK;
}

uint32_t SIREditorHierarchyModel::findVisibleRow( uint64_t nId ) const noexcept
{
	for ( uint32_t iRow = 0; iRow < m_nVisibleCount; ++iRow ) {
		if ( m_Storage.pVisible[iRow].nId == nId ) {
			return iRow;
		}
	}
	return UINT32_MAX;
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::rebuild( const SIREditorSceneDocument& document,
                                                             const sirEditorHierarchyOrder_t *pOrder, uint32_t nOrderCount ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_HIERARCHY_NOT_INITIALIZED;
	}
	uint32_t nCount = 0;
	uint64_t nRevision = 0;
	sirEditorResult_t eResult = document.copySnapshot( m_Storage.pSnapshot, m_Storage.nCapacity, &nCount, &nRevision );
	if ( eResult == SIR_EDITOR_OUT_OF_CAPACITY ) {
		return SIR_EDITOR_HIERARCHY_OUT_OF_CAPACITY;
	}
	if ( eResult != SIR_EDITOR_OK ) {
		return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
	}
	if ( ( pOrder == nullptr && nOrderCount != 0 ) || ( pOrder != nullptr && nOrderCount != nCount ) ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	for ( uint32_t iNode = 0; iNode < nCount; ++iNode ) {
		const sirEditorNode_t& node = m_Storage.pSnapshot[iNode];
		if ( pOrder != nullptr && pOrder[iNode].nId != node.nId ) {
			return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
		}
		uint32_t iPrior = findState( node.nId, m_Storage.pState, m_nNodeCount );
		m_Storage.pNextState[iNode] = iPrior == UINT32_MAX ? sirEditorHierarchyState_t{ node.nId, m_nDefaultRowHeight, node.nParentId == 0 }
		                                                   : m_Storage.pState[iPrior];
		m_Storage.pLinks[iNode] = { node.nId, node.nParentId, pOrder == nullptr ? iNode : pOrder[iNode].nOrder, 0 };
	}

	std::sort( m_Storage.pLinks, m_Storage.pLinks + nCount, sirEditorHierarchyLinkLess );
	memcpy( m_Storage.pState, m_Storage.pNextState, sizeof( sirEditorHierarchyState_t ) * nCount );
	m_nNodeCount = nCount;
	m_nSourceRevision = nRevision;
	if ( m_nSelectedId != 0 && findState( m_nSelectedId, m_Storage.pState, nCount ) == UINT32_MAX ) {
		m_nSelectedId = 0;
	}
	return buildVisibleRows();
}

bool SIREditorHierarchyModel::isCurrent( const SIREditorSceneDocument& document ) const noexcept
{
	return m_bInitialized && m_nSourceRevision != 0 && m_nSourceRevision == document.getRevision();
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::setExpanded( const SIREditorSceneDocument& document, uint64_t nId,
                                                                 bool bExpanded ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_HIERARCHY_NOT_INITIALIZED;
	}
	if ( !isCurrent( document ) ) {
		return SIR_EDITOR_HIERARCHY_STALE_SOURCE;
	}
	uint32_t iState = findState( nId, m_Storage.pState, m_nNodeCount );
	if ( iState == UINT32_MAX ) {
		return SIR_EDITOR_HIERARCHY_NOT_FOUND;
	}
	if ( m_Storage.pState[iState].bExpanded == bExpanded ) {
		return SIR_EDITOR_HIERARCHY_OK;
	}
	m_Storage.pState[iState].bExpanded = bExpanded;
	return buildVisibleRows();
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::setSelected( const SIREditorSceneDocument& document, uint64_t nId ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_HIERARCHY_NOT_INITIALIZED;
	}
	if ( !isCurrent( document ) ) {
		return SIR_EDITOR_HIERARCHY_STALE_SOURCE;
	}
	if ( nId != 0 && findState( nId, m_Storage.pState, m_nNodeCount ) == UINT32_MAX ) {
		return SIR_EDITOR_HIERARCHY_NOT_FOUND;
	}
	if ( m_nSelectedId != nId ) {
		m_nSelectedId = nId;
		++m_nPaintEpoch;
	}
	return SIR_EDITOR_HIERARCHY_OK;
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::setRowHeight( const SIREditorSceneDocument& document, uint64_t nId,
                                                                  uint32_t nHeight ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_HIERARCHY_NOT_INITIALIZED;
	}
	if ( !isCurrent( document ) ) {
		return SIR_EDITOR_HIERARCHY_STALE_SOURCE;
	}
	uint32_t iState = findState( nId, m_Storage.pState, m_nNodeCount );
	if ( iState == UINT32_MAX ) {
		return SIR_EDITOR_HIERARCHY_NOT_FOUND;
	}
	if ( nHeight == 0 ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	if ( m_Storage.pState[iState].nRowHeight == nHeight ) {
		return SIR_EDITOR_HIERARCHY_OK;
	}
	uint32_t iRow = findVisibleRow( nId );
	if ( iRow != UINT32_MAX && m_List.setHeight( iRow, nHeight ) != SIR_EDITOR_LIST_OK ) {
		return SIR_EDITOR_HIERARCHY_INVALID_SOURCE;
	}
	m_Storage.pState[iState].nRowHeight = nHeight;
	if ( iRow != UINT32_MAX ) {
		++m_nLayoutEpoch;
		++m_nPaintEpoch;
	}
	return SIR_EDITOR_HIERARCHY_OK;
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::getVisibleRange( uint64_t nScroll, uint64_t nViewportHeight, uint64_t nOverscan,
                                                                     sirEditorVirtualRange_t *pOutRange ) const noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_HIERARCHY_NOT_INITIALIZED;
	}
	if ( m_List.getVisibleRange( nScroll, nViewportHeight, nOverscan, pOutRange ) != SIR_EDITOR_LIST_OK ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	return SIR_EDITOR_HIERARCHY_OK;
}

sirEditorHierarchyResult_t SIREditorHierarchyModel::getVisibleRow( uint32_t iRow, sirEditorHierarchyRow_t *pOutRow ) const noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_HIERARCHY_NOT_INITIALIZED;
	}
	if ( pOutRow == nullptr || iRow >= m_nVisibleCount ) {
		return SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT;
	}
	*pOutRow = m_Storage.pVisible[iRow];
	pOutRow->bSelected = pOutRow->nId == m_nSelectedId;
	return SIR_EDITOR_HIERARCHY_OK;
}

uint32_t SIREditorHierarchyModel::getVisibleCount( void ) const noexcept
{
	return m_nVisibleCount;
}

uint64_t SIREditorHierarchyModel::getSourceRevision( void ) const noexcept
{
	return m_nSourceRevision;
}

uint64_t SIREditorHierarchyModel::getLayoutEpoch( void ) const noexcept
{
	return m_nLayoutEpoch;
}

uint64_t SIREditorHierarchyModel::getPaintEpoch( void ) const noexcept
{
	return m_nPaintEpoch;
}

uint64_t SIREditorHierarchyModel::getSelectedId( void ) const noexcept
{
	return m_nSelectedId;
}

uint64_t SIREditorHierarchyModel::getTotalHeight( void ) const noexcept
{
	return m_List.getTotalHeight();
}
