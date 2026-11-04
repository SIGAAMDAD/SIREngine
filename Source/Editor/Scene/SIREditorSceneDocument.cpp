#include <Editor/Scene/SIREditorSceneDocument.h>

#include <algorithm>
#include <limits.h>

static bool sirEditorNodeLess( const sirEditorNode_t &lhs, const sirEditorNode_t &rhs ) noexcept
{
	return lhs.nId < rhs.nId;
}

static uint32_t sirEditorIndexHash( uint64_t nId, uint32_t nCapacity ) noexcept
{
	constexpr uint64_t HASH_FACTOR = UINT64_C( 11400714819323198485 );
	return ( uint32_t )( nId * HASH_FACTOR ) & ( nCapacity - 1 );
}

static bool sirEditorRangesOverlap( const void *pLeft, size_t nLeftBytes, const void *pRight, size_t nRightBytes ) noexcept
{
	uintptr_t nLeft = ( uintptr_t )pLeft;
	uintptr_t nRight = ( uintptr_t )pRight;
	if ( nLeftBytes > UINTPTR_MAX - nLeft || nRightBytes > UINTPTR_MAX - nRight ) {
		return true;
	}
	return nLeft < nRight + nRightBytes && nRight < nLeft + nLeftBytes;
}

SIREditorSceneDocument::SIREditorSceneDocument( void ) noexcept
    : m_Storage{}, m_nRevision( 0 ), m_nPendingCount( 0 ), m_nHistoryCount( 0 ), m_nHistoryCursor( 0 ), m_nHistoryDeltaCount( 0 ),
      m_bInitialized( false ), m_bEditing( false )
{
}

sirEditorResult_t SIREditorSceneDocument::initialize( const sirEditorDocumentStorage_t &storage ) noexcept
{
	if ( m_bInitialized ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	if ( storage.pNodes == nullptr || storage.pStaging == nullptr || storage.pPending == nullptr || storage.pPendingDeltas == nullptr ||
	     storage.pHistoryDeltas == nullptr || storage.pHistoryFrames == nullptr || storage.pIndex == nullptr ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	if ( storage.nNodeCapacity == 0 || storage.nPendingCapacity == 0 || storage.nDeltaCapacity == 0 || storage.nFrameCapacity == 0 ||
	     storage.nIndexCapacity < ( uint64_t )storage.nNodeCapacity * 2 ||
	     ( storage.nIndexCapacity & ( storage.nIndexCapacity - 1 ) ) != 0 ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	const void *szPointers[7] = { storage.pNodes,         storage.pStaging,       storage.pPending, storage.pPendingDeltas,
	                              storage.pHistoryDeltas, storage.pHistoryFrames, storage.pIndex };
	const size_t szByteCounts[7] = { ( size_t )storage.nNodeCapacity * sizeof( sirEditorNode_t ),
	                                 ( size_t )storage.nNodeCapacity * sizeof( sirEditorNode_t ),
	                                 ( size_t )storage.nPendingCapacity * sizeof( sirEditorMutation_t ),
	                                 ( size_t )storage.nPendingCapacity * sizeof( sirEditorDelta_t ),
	                                 ( size_t )storage.nDeltaCapacity * sizeof( sirEditorDelta_t ),
	                                 ( size_t )storage.nFrameCapacity * sizeof( sirEditorHistoryFrame_t ),
	                                 ( size_t )storage.nIndexCapacity * sizeof( sirEditorIndexEntry_t ) };
	const uint32_t szCapacities[7] = { storage.nNodeCapacity,  storage.nNodeCapacity,  storage.nPendingCapacity, storage.nPendingCapacity,
	                                   storage.nDeltaCapacity, storage.nFrameCapacity, storage.nIndexCapacity };
	const size_t szRecordSizes[7] = { sizeof( sirEditorNode_t ),      sizeof( sirEditorNode_t ),  sizeof( sirEditorMutation_t ),
	                                  sizeof( sirEditorDelta_t ),     sizeof( sirEditorDelta_t ), sizeof( sirEditorHistoryFrame_t ),
	                                  sizeof( sirEditorIndexEntry_t ) };
	for ( uint32_t iRange = 0; iRange < 7; ++iRange ) {
		if ( szCapacities[iRange] > SIZE_MAX / szRecordSizes[iRange] ) {
			return SIR_EDITOR_INVALID_ARGUMENT;
		}
		for ( uint32_t iOther = 0; iOther < iRange; ++iOther ) {
			if ( sirEditorRangesOverlap( szPointers[iRange], szByteCounts[iRange], szPointers[iOther], szByteCounts[iOther] ) ) {
				return SIR_EDITOR_INVALID_ARGUMENT;
			}
		}
	}

	m_Storage = storage;
	memset( m_Storage.pNodes, 0, sizeof( sirEditorNode_t ) * m_Storage.nNodeCapacity );
	memset( m_Storage.pStaging, 0, sizeof( sirEditorNode_t ) * m_Storage.nNodeCapacity );
	memset( m_Storage.pIndex, 0, sizeof( sirEditorIndexEntry_t ) * m_Storage.nIndexCapacity );
	m_nRevision = 1;
	m_bInitialized = true;
	return SIR_EDITOR_OK;
}

void SIREditorSceneDocument::shutdown( void ) noexcept
{
	m_Storage = {};
	m_nRevision = 0;
	m_nPendingCount = 0;
	m_nHistoryCount = 0;
	m_nHistoryCursor = 0;
	m_nHistoryDeltaCount = 0;
	m_bEditing = false;
	m_bInitialized = false;
}

sirEditorResult_t SIREditorSceneDocument::beginEdit( uint64_t nExpectedRevision ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( m_bEditing ) {
		return SIR_EDITOR_TRANSACTION_ACTIVE;
	}
	if ( m_nRevision != nExpectedRevision ) {
		return SIR_EDITOR_STALE_REVISION;
	}
	if ( m_nRevision == UINT64_MAX ) {
		return SIR_EDITOR_REVISION_EXHAUSTED;
	}

	m_bEditing = true;
	m_nPendingCount = 0;
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorSceneDocument::queueUpsert( const sirEditorNode_t &node ) noexcept
{
	if ( !m_bEditing ) {
		return SIR_EDITOR_NO_TRANSACTION;
	}
	if ( node.nId == 0 || node.nTypeId == 0 ) {
		return SIR_EDITOR_INVALID_ID;
	}
	if ( m_nPendingCount >= m_Storage.nPendingCapacity ) {
		return SIR_EDITOR_OUT_OF_CAPACITY;
	}

	sirEditorMutation_t &mutation = m_Storage.pPending[m_nPendingCount++];
	mutation.node = node;
	mutation.eType = SIR_EDITOR_MUTATION_UPSERT;
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorSceneDocument::queueRemove( uint64_t nId ) noexcept
{
	if ( !m_bEditing ) {
		return SIR_EDITOR_NO_TRANSACTION;
	}
	if ( nId == 0 ) {
		return SIR_EDITOR_INVALID_ID;
	}
	if ( m_nPendingCount >= m_Storage.nPendingCapacity ) {
		return SIR_EDITOR_OUT_OF_CAPACITY;
	}

	sirEditorMutation_t &mutation = m_Storage.pPending[m_nPendingCount++];
	mutation.node = {};
	mutation.node.nId = nId;
	mutation.eType = SIR_EDITOR_MUTATION_REMOVE;
	return SIR_EDITOR_OK;
}

uint32_t SIREditorSceneDocument::findSlot( const sirEditorNode_t *pNodes, uint64_t nId ) const noexcept
{
	for ( uint32_t iSlot = 0; iSlot < m_Storage.nNodeCapacity; ++iSlot ) {
		if ( pNodes[iSlot].nId == nId ) {
			return iSlot;
		}
	}
	return UINT32_MAX;
}

sirEditorResult_t SIREditorSceneDocument::applyPending( uint32_t iMutation, sirEditorDelta_t *pDelta ) noexcept
{
	const sirEditorMutation_t &mutation = m_Storage.pPending[iMutation];
	uint32_t iSlot = findSlot( m_Storage.pStaging, mutation.node.nId );

	if ( mutation.eType == SIR_EDITOR_MUTATION_REMOVE ) {
		if ( iSlot == UINT32_MAX ) {
			return SIR_EDITOR_NOT_FOUND;
		}
		pDelta->iSlot = iSlot;
		pDelta->previous = m_Storage.pStaging[iSlot];
		m_Storage.pStaging[iSlot].nId = 0;
		m_Storage.pStaging[iSlot].nParentId = 0;
		m_Storage.pStaging[iSlot].nTypeId = 0;
		m_Storage.pStaging[iSlot].nFlags = 0;
		pDelta->next = m_Storage.pStaging[iSlot];
		return SIR_EDITOR_OK;
	}
	if ( mutation.eType != SIR_EDITOR_MUTATION_UPSERT ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	if ( iSlot == UINT32_MAX ) {
		for ( uint32_t iCandidate = 0; iCandidate < m_Storage.nNodeCapacity; ++iCandidate ) {
			if ( m_Storage.pStaging[iCandidate].nId == 0 ) {
				iSlot = iCandidate;
				break;
			}
		}
	}
	if ( iSlot == UINT32_MAX ) {
		return SIR_EDITOR_OUT_OF_CAPACITY;
	}

	pDelta->iSlot = iSlot;
	pDelta->previous = m_Storage.pStaging[iSlot];
	sirEditorNode_t node = mutation.node;
	if ( pDelta->previous.nId == 0 ) {
		if ( pDelta->previous.nGeneration == UINT32_MAX ) {
			return SIR_EDITOR_OUT_OF_CAPACITY;
		}
		node.nGeneration = pDelta->previous.nGeneration + 1;
	} else {
		node.nGeneration = pDelta->previous.nGeneration;
	}
	node.nReserved = 0;
	m_Storage.pStaging[iSlot] = node;
	pDelta->next = node;
	return SIR_EDITOR_OK;
}

uint32_t SIREditorSceneDocument::indexedSlot( uint64_t nId ) const noexcept
{
	if ( nId == 0 ) {
		return UINT32_MAX;
	}
	uint32_t iHash = sirEditorIndexHash( nId, m_Storage.nIndexCapacity );
	for ( uint32_t nProbes = 0; nProbes < m_Storage.nIndexCapacity; ++nProbes ) {
		const sirEditorIndexEntry_t &entry = m_Storage.pIndex[iHash];
		if ( entry.nId == nId ) {
			return entry.iSlot;
		}
		if ( entry.nId == 0 ) {
			return UINT32_MAX;
		}
		iHash = ( iHash + 1 ) & ( m_Storage.nIndexCapacity - 1 );
	}
	return UINT32_MAX;
}

sirEditorResult_t SIREditorSceneDocument::buildValidatedIndex( const sirEditorNode_t *pNodes ) noexcept
{
	memset( m_Storage.pIndex, 0, sizeof( sirEditorIndexEntry_t ) * m_Storage.nIndexCapacity );
	for ( uint32_t iSlot = 0; iSlot < m_Storage.nNodeCapacity; ++iSlot ) {
		const sirEditorNode_t &node = pNodes[iSlot];
		if ( node.nId == 0 ) {
			continue;
		}
		if ( node.nTypeId == 0 || node.nId == node.nParentId ) {
			return SIR_EDITOR_INVALID_HIERARCHY;
		}
		uint32_t iHash = sirEditorIndexHash( node.nId, m_Storage.nIndexCapacity );
		while ( m_Storage.pIndex[iHash].nId != 0 ) {
			if ( m_Storage.pIndex[iHash].nId == node.nId ) {
				return SIR_EDITOR_INVALID_ID;
			}
			iHash = ( iHash + 1 ) & ( m_Storage.nIndexCapacity - 1 );
		}
		m_Storage.pIndex[iHash].nId = node.nId;
		m_Storage.pIndex[iHash].iSlot = iSlot;
	}

	for ( uint32_t iSlot = 0; iSlot < m_Storage.nNodeCapacity; ++iSlot ) {
		uint64_t nParent = pNodes[iSlot].nId == 0 ? 0 : pNodes[iSlot].nParentId;
		uint32_t nDepth = 0;
		while ( nParent != 0 ) {
			uint32_t iParent = indexedSlot( nParent );
			if ( iParent == UINT32_MAX || ++nDepth >= m_Storage.nNodeCapacity ) {
				return SIR_EDITOR_INVALID_HIERARCHY;
			}
			nParent = pNodes[iParent].nParentId;
		}
	}
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorSceneDocument::commitEdit( void ) noexcept
{
	if ( !m_bEditing ) {
		return SIR_EDITOR_NO_TRANSACTION;
	}
	if ( m_nPendingCount == 0 ) {
		m_bEditing = false;
		return SIR_EDITOR_OK;
	}
	if ( m_nPendingCount > m_Storage.nDeltaCapacity ) {
		return SIR_EDITOR_OUT_OF_CAPACITY;
	}

	memcpy( m_Storage.pStaging, m_Storage.pNodes, sizeof( sirEditorNode_t ) * m_Storage.nNodeCapacity );
	for ( uint32_t iMutation = 0; iMutation < m_nPendingCount; ++iMutation ) {
		sirEditorResult_t eResult = applyPending( iMutation, &m_Storage.pPendingDeltas[iMutation] );
		if ( eResult != SIR_EDITOR_OK ) {
			return eResult;
		}
	}
	sirEditorResult_t eResult = buildValidatedIndex( m_Storage.pStaging );
	if ( eResult != SIR_EDITOR_OK ) {
		buildValidatedIndex( m_Storage.pNodes );
		return eResult;
	}

	uint32_t nRetainedFrames = m_nHistoryCursor;
	uint32_t nRetainedDeltas = nRetainedFrames == m_nHistoryCount
	                               ? m_nHistoryDeltaCount
	                               : ( nRetainedFrames == 0 ? 0
	                                                        : m_Storage.pHistoryFrames[nRetainedFrames - 1].iFirstDelta +
	                                                              m_Storage.pHistoryFrames[nRetainedFrames - 1].nDeltas );
	if ( nRetainedFrames == m_Storage.nFrameCapacity || m_nPendingCount > m_Storage.nDeltaCapacity - nRetainedDeltas ) {
		nRetainedFrames = 0;
		nRetainedDeltas = 0;
	}

	memcpy( m_Storage.pHistoryDeltas + nRetainedDeltas, m_Storage.pPendingDeltas, sizeof( sirEditorDelta_t ) * m_nPendingCount );
	m_Storage.pHistoryFrames[nRetainedFrames] = { nRetainedDeltas, m_nPendingCount };
	m_nHistoryCount = nRetainedFrames + 1;
	m_nHistoryCursor = m_nHistoryCount;
	m_nHistoryDeltaCount = nRetainedDeltas + m_nPendingCount;
	memcpy( m_Storage.pNodes, m_Storage.pStaging, sizeof( sirEditorNode_t ) * m_Storage.nNodeCapacity );
	++m_nRevision;
	m_nPendingCount = 0;
	m_bEditing = false;
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorSceneDocument::cancelEdit( void ) noexcept
{
	if ( !m_bEditing ) {
		return SIR_EDITOR_NO_TRANSACTION;
	}
	m_bEditing = false;
	m_nPendingCount = 0;
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorSceneDocument::applyHistoryFrame( bool bUndo ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( m_bEditing ) {
		return SIR_EDITOR_TRANSACTION_ACTIVE;
	}
	if ( m_nRevision == UINT64_MAX ) {
		return SIR_EDITOR_REVISION_EXHAUSTED;
	}
	if ( ( bUndo && m_nHistoryCursor == 0 ) || ( !bUndo && m_nHistoryCursor == m_nHistoryCount ) ) {
		return SIR_EDITOR_NO_HISTORY;
	}

	uint32_t iFrame = bUndo ? m_nHistoryCursor - 1 : m_nHistoryCursor;
	const sirEditorHistoryFrame_t &frame = m_Storage.pHistoryFrames[iFrame];
	memcpy( m_Storage.pStaging, m_Storage.pNodes, sizeof( sirEditorNode_t ) * m_Storage.nNodeCapacity );
	for ( uint32_t iDelta = 0; iDelta < frame.nDeltas; ++iDelta ) {
		uint32_t iInFrame = bUndo ? frame.nDeltas - iDelta - 1 : iDelta;
		const sirEditorDelta_t &delta = m_Storage.pHistoryDeltas[frame.iFirstDelta + iInFrame];
		m_Storage.pStaging[delta.iSlot] = bUndo ? delta.previous : delta.next;
	}
	sirEditorResult_t eResult = buildValidatedIndex( m_Storage.pStaging );
	if ( eResult != SIR_EDITOR_OK ) {
		buildValidatedIndex( m_Storage.pNodes );
		return eResult;
	}

	memcpy( m_Storage.pNodes, m_Storage.pStaging, sizeof( sirEditorNode_t ) * m_Storage.nNodeCapacity );
	m_nHistoryCursor = bUndo ? m_nHistoryCursor - 1 : m_nHistoryCursor + 1;
	++m_nRevision;
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorSceneDocument::undo( void ) noexcept
{
	return applyHistoryFrame( true );
}

sirEditorResult_t SIREditorSceneDocument::redo( void ) noexcept
{
	return applyHistoryFrame( false );
}

sirEditorResult_t SIREditorSceneDocument::findNode( uint64_t nId, sirEditorNode_t *pOutNode ) const noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( nId == 0 || pOutNode == nullptr ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	uint32_t iSlot = indexedSlot( nId );
	if ( iSlot == UINT32_MAX ) {
		return SIR_EDITOR_NOT_FOUND;
	}
	*pOutNode = m_Storage.pNodes[iSlot];
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorSceneDocument::copySnapshot( sirEditorNode_t *pOutNodes, uint32_t nCapacity, uint32_t *pOutCount,
                                                        uint64_t *pOutRevision ) const noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( pOutCount == nullptr || pOutRevision == nullptr || ( pOutNodes == nullptr && nCapacity != 0 ) ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	uint32_t nCount = 0;
	for ( uint32_t iSlot = 0; iSlot < m_Storage.nNodeCapacity; ++iSlot ) {
		if ( m_Storage.pNodes[iSlot].nId != 0 ) {
			++nCount;
		}
	}
	*pOutCount = nCount;
	*pOutRevision = m_nRevision;
	if ( nCapacity < nCount || ( nCount != 0 && pOutNodes == nullptr ) ) {
		return SIR_EDITOR_OUT_OF_CAPACITY;
	}
	if ( nCount != 0 && sirEditorRangesOverlap( pOutNodes, ( size_t )nCount * sizeof( sirEditorNode_t ), m_Storage.pNodes,
	                                            ( size_t )m_Storage.nNodeCapacity * sizeof( sirEditorNode_t ) ) ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}

	uint32_t iWrite = 0;
	for ( uint32_t iSlot = 0; iSlot < m_Storage.nNodeCapacity; ++iSlot ) {
		if ( m_Storage.pNodes[iSlot].nId != 0 ) {
			pOutNodes[iWrite++] = m_Storage.pNodes[iSlot];
		}
	}
	if ( nCount > 1 ) {
		std::sort( pOutNodes, pOutNodes + nCount, sirEditorNodeLess );
	}
	return SIR_EDITOR_OK;
}

uint64_t SIREditorSceneDocument::getRevision( void ) const noexcept
{
	return m_nRevision;
}

bool SIREditorSceneDocument::canUndo( void ) const noexcept
{
	return m_bInitialized && m_nHistoryCursor != 0;
}

bool SIREditorSceneDocument::canRedo( void ) const noexcept
{
	return m_bInitialized && m_nHistoryCursor < m_nHistoryCount;
}
