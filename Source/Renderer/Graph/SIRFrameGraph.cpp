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

#include <Renderer/Graph/SIRFrameGraph.h>

#include <atomic>

static std::atomic<uint64_t> s_nNextGraphIdentity{ 1 };

// Expand only explicit dependency bits into the compiler-owned dense adjacency scratch.
struct sirFrameGraphDenseEdgeWriter_t
{
	uint8_t *pEdges;
	void operator()( uint32_t iEdge ) const noexcept
	{
		pEdges[iEdge] = 1;
	}
};

SIRFrameGraph::SIRFrameGraph( const SIRSharedGPUBackingOwner *pSharedOwner ) noexcept : m_pSharedOwner( pSharedOwner )
{
	m_nIdentity = s_nNextGraphIdentity.fetch_add( 1, std::memory_order_relaxed );
}

uint32_t SIRFrameGraph::getPassCount( void ) const noexcept
{
	return m_nPassCount;
}

uint32_t SIRFrameGraph::getResourceCount( void ) const noexcept
{
	return m_nResourceCount;
}

bool SIRFrameGraph::isCullingEnabled( void ) const noexcept
{
	return m_bCullingEnabled;
}

void SIRFrameGraph::setCullingEnabled( bool bEnabled ) noexcept
{
	if ( m_bCullingEnabled != bEnabled ) {
		m_bCullingEnabled = bEnabled;
		++m_nRevision;
	}
}

void SIRFrameGraph::reset( void ) noexcept
{
	m_nPassCount = 0;
	m_nResourceCount = 0;
	m_nAccessCount = 0;
	m_SharedReads.clear();
	m_Outputs.clear();
	++m_nRevision;
	m_ExplicitEdges.clear();
}

bool SIRFrameGraph::isWritableState( SIRGraphicsState eState ) noexcept
{
	switch ( eState ) {
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return true;
	default:
		return false;
	}
}

bool SIRFrameGraph::isReadableState( SIRGraphicsState eState ) noexcept
{
	switch ( eState ) {
	case SIRGraphicsState::SIR_GRAPHICS_STATE_VERTEX_INDEX:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_CONSTANT:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT:
		return true;
	default:
		return false;
	}
}

static bool sirFrameGraphOverlaps( const sirFrameGraphAccess_t& a, const sirFrameGraphAccess_t& b ) noexcept
{
	if ( a.nResource != b.nResource || a.nBaseMip >= b.nBaseMip + b.nMipCount || b.nBaseMip >= a.nBaseMip + a.nMipCount ) {
		return false;
	}
	return a.nBaseLayer < b.nBaseLayer + b.nLayerCount && b.nBaseLayer < a.nBaseLayer + a.nLayerCount;
}

bool SIRFrameGraph::overlaps( const sirFrameGraphAccess_t& a, const sirFrameGraphAccess_t& b ) noexcept
{
	return sirFrameGraphOverlaps( a, b );
}

bool SIRFrameGraph::covers( const sirFrameGraphAccess_t& a, const sirFrameGraphAccess_t& b ) noexcept
{
	if ( a.nResource != b.nResource || a.nBaseMip > b.nBaseMip || a.nBaseMip + a.nMipCount < b.nBaseMip + b.nMipCount ) {
		return false;
	}
	return a.nBaseLayer <= b.nBaseLayer && a.nBaseLayer + a.nLayerCount >= b.nBaseLayer + b.nLayerCount;
}

// Sorted, duplicate-free subresource boundaries partition arbitrary overlapping
// mip/layer rectangles without allocating a dense texture-sized state table.
static void sirFrameGraphInsertCut( uint32_t *pCuts, uint32_t *pCount, uint32_t nValue ) noexcept
{
	uint32_t iCut = 0;
	while ( iCut < *pCount && pCuts[iCut] < nValue ) {
		++iCut;
	}
	if ( iCut < *pCount && pCuts[iCut] == nValue ) {
		return;
	}
	for ( uint32_t iMove = *pCount; iMove > iCut; --iMove ) {
		pCuts[iMove] = pCuts[iMove - 1];
	}
	pCuts[iCut] = nValue;
	++*pCount;
}

typedef struct sirFrameGraphCompileContext_s
{
	const sirFrameGraphResource_t *pResources;
	const sirFrameGraphPass_t *pPasses;
	uint8_t *pEdges;
	uint32_t nCellsRemaining;
	const uint8_t *pLive;
} sirFrameGraphCompileContext_t;

static bool sirFrameGraphRequiresOrdering( const sirFrameGraphAccess_t& previous, const sirFrameGraphAccess_t& current,
										   const sirFrameGraphPass_t *pPasses ) noexcept
{
	if ( previous.bWrite || current.bWrite || previous.eState != current.eState ) {
		return true;
	}
	return pPasses[previous.nPass].eQueue != pPasses[current.nPass].eQueue;
}

SIRGraphicsResult SIRFrameGraph::addResource( const sirFrameGraphResource_t& resource, uint32_t *pIndex ) noexcept
{
	return addResourceImpl( resource, pIndex, false );
}

SIRGraphicsResult SIRFrameGraph::addResourceImpl( const sirFrameGraphResource_t& resource, uint32_t *pIndex, bool bSharedLease ) noexcept
{
	if ( ( resource.nCanonicalBackingId != 0 || resource.nCanonicalNativeToken != 0 ) && !bSharedLease ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( bSharedLease && ( !resource.bImported || !resource.nCanonicalBackingId || !resource.nCanonicalNativeToken ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pIndex == nullptr || resource.nMipLevels == 0 || resource.nArrayLayers == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( resource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER && resource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( resource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER && ( resource.nMipLevels != 1 || resource.nArrayLayers != 1 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( resource.bImported && ( resource.hImported.eType != resource.eType || resource.hImported.nGeneration == 0 ||
								 resource.hImported.nDeviceGeneration == 0 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( resource.eInitialQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT || resource.eInitialState > SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !resource.bImported && resource.eInitialState != SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nResourceCount == SIR_FRAMEGRAPH_MAX_RESOURCES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	// A semantic frame product has one producer/storage identity for its entire
	// view/eye/quality/epoch. Duplicate descriptors cannot escape through a
	// second resource name, native handle or pass-local allocation.
	const sirFrameGraphProductKey_t& key = resource.product;
	const bool bNamedProduct = key.nViewId || key.nFrameEpoch || key.nSemanticId || key.nEyeId || key.nQualityId;
	if ( bNamedProduct ) {
		if ( !key.nViewId || !key.nFrameEpoch || !key.nSemanticId || resource.nCanonicalBackingId ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
			const sirFrameGraphProductKey_t& existing = m_szResources[iResource].product;
			if ( existing.nViewId == key.nViewId && existing.nFrameEpoch == key.nFrameEpoch && existing.nSemanticId == key.nSemanticId &&
				 existing.nEyeId == key.nEyeId && existing.nQualityId == key.nQualityId ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	// A physical imported resource must have one graph identity so no alias
	// can evade declared access hazards by registering the same handle twice.
	if ( resource.bImported ) {
		for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
			const sirFrameGraphResource_t& existing = m_szResources[iResource];
			if ( !existing.bImported || existing.eType != resource.eType ) {
				continue;
			}
			const bool bSameSlot =
				existing.hImported.nSlot == resource.hImported.nSlot && existing.hImported.nGeneration == resource.hImported.nGeneration;
			const bool bSameDevice = existing.hImported.nDeviceGeneration == resource.hImported.nDeviceGeneration;
			if ( bSameSlot && bSameDevice ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}

	*pIndex = m_nResourceCount;
	m_szResources[m_nResourceCount++] = resource;
	++m_nRevision;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::importSharedBuffer( const sirNativeGPUBufferLease_t& rLease, SIRGraphicsState eInitialState,
													   SIRGraphicsQueue eInitialQueue, uint32_t *pIndex ) noexcept
{
	if ( !pIndex || !rLease.view.bReadOnly || !rLease.view.nPhysicalBackingId || !rLease.view.nNativeResourceToken || !rLease.view.nBytes ||
		 rLease.view.nOffset > UINT64_MAX - rLease.view.nBytes || !rLease.view.nBorrowGeneration || !rLease.view.nConsumerId ||
		 !rLease.view.nDeviceGeneration || rLease.hNative.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER || !rLease.hNative.nGeneration ||
		 rLease.hNative.nDeviceGeneration != rLease.view.nDeviceGeneration || eInitialQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ||
		 !isReadableState( eInitialState ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// A typed lease alone is not authority: its backing owner must prove it
	// remains issued and bound to the actual live native allocation.
	if ( !m_pSharedOwner || m_pSharedOwner->validateBufferLease( rLease ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		const sirFrameGraphResource_t& rExisting = m_szResources[iResource];
		if ( rExisting.nCanonicalBackingId != rLease.view.nPhysicalBackingId ) {
			continue;
		}
		if ( rExisting.nCanonicalNativeToken != rLease.view.nNativeResourceToken || rExisting.hImported.nSlot != rLease.hNative.nSlot ||
			 rExisting.hImported.nGeneration != rLease.hNative.nGeneration ||
			 rExisting.hImported.nDeviceGeneration != rLease.hNative.nDeviceGeneration || rExisting.eInitialQueue != eInitialQueue ||
			 rExisting.eInitialState != eInitialState ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		*pIndex = iResource;
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	const sirFrameGraphResource_t imported = { rLease.hNative,
											   SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER,
											   eInitialState,
											   eInitialQueue,
											   1,
											   1,
											   true,
											   rLease.view.nPhysicalBackingId,
											   rLease.view.nNativeResourceToken };
	return addResourceImpl( imported, pIndex, true );
}

SIRGraphicsResult SIRFrameGraph::addSharedBufferRead( uint32_t nPass, uint32_t nResource, const sirNativeGPUBufferLease_t& rLease,
														uint64_t nOffsetBytes, uint64_t nBytes, SIRGraphicsState eState ) noexcept
{
	if ( nPass >= m_nPassCount || nResource >= m_nResourceCount || !nBytes || !rLease.view.bReadOnly ||
		 !rLease.view.nCompletionProviderId || !rLease.view.nBorrowGeneration || !isReadableState( eState ) ||
		 m_SharedReads.getSize() == m_SharedReads.getCapacity() || nOffsetBytes < rLease.view.nOffset || nBytes > rLease.view.nBytes ||
		 nOffsetBytes - rLease.view.nOffset > rLease.view.nBytes - nBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !m_pSharedOwner || m_pSharedOwner->validateBufferLease( rLease ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirFrameGraphResource_t& imported = m_szResources[nResource];
	if ( !imported.bImported || imported.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ||
		 imported.nCanonicalBackingId != rLease.view.nPhysicalBackingId ||
		 imported.nCanonicalNativeToken != rLease.view.nNativeResourceToken || imported.hImported.nSlot != rLease.hNative.nSlot ||
		 imported.hImported.nGeneration != rLease.hNative.nGeneration ||
		 imported.hImported.nDeviceGeneration != rLease.hNative.nDeviceGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const SIRGraphicsResult eAdded = addAccessImpl( { nPass, nResource, 0, 1, 0, 1, eState, false }, true );
	if ( eAdded != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAdded;
	}
	const SharedReadLease entry = { { nPass, nResource, rLease.view.nConsumerId, rLease.view.nCompletionProviderId, rLease.view.nBorrowSlot,
									  rLease.view.nBorrowGeneration, rLease.view.nPhysicalBackingId, rLease.view.nNativeResourceToken,
									  nOffsetBytes, nBytes },
									rLease };
	// The capacity check precedes access registration, so this append is infallible.
	const SIRContainerResult eResult = m_SharedReads.tryPushBack( entry );
	assert( eResult == SIRContainerResult::SIR_CONTAINER_OK );
	static_cast<void>(eResult);
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::addPass( const sirFrameGraphPass_t& pass, uint32_t *pIndex ) noexcept
{
	if ( pIndex == nullptr || pass.eQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nPassCount == SIR_FRAMEGRAPH_MAX_PASSES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	*pIndex = m_nPassCount;
	m_szPasses[m_nPassCount++] = pass;
	++m_nRevision;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::addAccess( const sirFrameGraphAccess_t& access ) noexcept
{
	return addAccessImpl( access, false );
}

SIRGraphicsResult SIRFrameGraph::addAccessImpl( const sirFrameGraphAccess_t& access, bool bSharedLease ) noexcept
{
	if ( access.nPass >= m_nPassCount || access.nResource >= m_nResourceCount ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nMipCount == 0 || access.nLayerCount == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirFrameGraphResource_t& resource = m_szResources[access.nResource];
	if ( resource.nCanonicalBackingId != 0 && !bSharedLease ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nBaseMip >= resource.nMipLevels || access.nMipCount > resource.nMipLevels - access.nBaseMip ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nBaseLayer >= resource.nArrayLayers || access.nLayerCount > resource.nArrayLayers - access.nBaseLayer ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Canonical GRDG source imports are immutable throughout this graph's
	// lifetime. Mutable frame products require separately versioned backing.
	if ( access.bWrite && resource.nCanonicalBackingId != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.bWrite ? !isWritableState( access.eState ) : !isReadableState( access.eState ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const SIRGraphicsQueue eQueue = m_szPasses[access.nPass].eQueue;
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY && access.eState != SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE &&
		 access.eState != SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE &&
		 ( access.eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT || access.eState == SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE ||
		   access.eState == SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nAccessCount == SIR_FRAMEGRAPH_MAX_ACCESSES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	// Access declarations follow pass insertion order; hazards retain that order.
	if ( m_nAccessCount != 0 && access.nPass < m_szAccesses[m_nAccessCount - 1].nPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		const sirFrameGraphAccess_t& previous = m_szAccesses[iAccess];
		if ( previous.nPass == access.nPass && overlaps( previous, access ) &&
			 ( previous.bWrite || access.bWrite || previous.eState != access.eState ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	m_szAccesses[m_nAccessCount++] = access;
	++m_nRevision;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::addDependency( uint32_t nBefore, uint32_t nAfter ) noexcept
{
	if ( nBefore >= m_nPassCount || nAfter >= m_nPassCount || nBefore == nAfter ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	static_cast<void>(m_ExplicitEdges.trySet( nBefore * SIR_FRAMEGRAPH_MAX_PASSES + nAfter, true ));
	++m_nRevision;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::addOutput( const sirFrameGraphOutput_t& output ) noexcept
{
	if ( output.nResource >= m_nResourceCount || output.nMipCount == 0 || output.nLayerCount == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirFrameGraphResource_t& resource = m_szResources[output.nResource];
	if ( output.nBaseMip >= resource.nMipLevels || output.nMipCount > resource.nMipLevels - output.nBaseMip ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( output.nBaseLayer >= resource.nArrayLayers || output.nLayerCount > resource.nArrayLayers - output.nBaseLayer ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_Outputs.getSize() == m_Outputs.getCapacity() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( m_Outputs.tryPushBack( output ) != SIRContainerResult::SIR_CONTAINER_OK ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	++m_nRevision;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

// Every cell of an output or live read needs its closest preceding writer.
// Earlier overwritten writers are not dependencies of the output.
SIRGraphicsResult SIRFrameGraph::markProducers( const sirFrameGraphAccess_t& target, uint32_t nBeforePass, uint8_t *pLive,
												  uint32_t *pCellBudget ) const noexcept
{
	uint32_t szMipCuts[SIR_FRAMEGRAPH_MAX_ACCESSES * 2 + 2] = { target.nBaseMip, target.nBaseMip + target.nMipCount };
	uint32_t szLayerCuts[SIR_FRAMEGRAPH_MAX_ACCESSES * 2 + 2] = { target.nBaseLayer, target.nBaseLayer + target.nLayerCount };
	uint32_t nMipCuts = 2;
	uint32_t nLayerCuts = 2;
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		const sirFrameGraphAccess_t& writer = m_szAccesses[iAccess];
		if ( !writer.bWrite || writer.nPass >= nBeforePass || !overlaps( writer, target ) ) {
			continue;
		}
		const uint32_t nMipEnd = writer.nBaseMip + writer.nMipCount;
		const uint32_t nLayerEnd = writer.nBaseLayer + writer.nLayerCount;
		sirFrameGraphInsertCut( szMipCuts, &nMipCuts, writer.nBaseMip > target.nBaseMip ? writer.nBaseMip : target.nBaseMip );
		sirFrameGraphInsertCut( szMipCuts, &nMipCuts,
								nMipEnd < target.nBaseMip + target.nMipCount ? nMipEnd : target.nBaseMip + target.nMipCount );
		sirFrameGraphInsertCut( szLayerCuts, &nLayerCuts, writer.nBaseLayer > target.nBaseLayer ? writer.nBaseLayer : target.nBaseLayer );
		sirFrameGraphInsertCut( szLayerCuts, &nLayerCuts,
								nLayerEnd < target.nBaseLayer + target.nLayerCount ? nLayerEnd : target.nBaseLayer + target.nLayerCount );
	}
	const uint64_t nCells = static_cast<uint64_t>( nMipCuts - 1 ) * ( nLayerCuts - 1 );
	if ( nCells > *pCellBudget ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	*pCellBudget -= static_cast<uint32_t>(nCells);

	const sirFrameGraphResource_t& resource = m_szResources[target.nResource];
	for ( uint32_t iLayer = 0; iLayer + 1 < nLayerCuts; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip + 1 < nMipCuts; ++iMip ) {
			sirFrameGraphAccess_t cell = target;
			cell.nBaseMip = szMipCuts[iMip];
			cell.nMipCount = szMipCuts[iMip + 1] - szMipCuts[iMip];
			cell.nBaseLayer = szLayerCuts[iLayer];
			cell.nLayerCount = szLayerCuts[iLayer + 1] - szLayerCuts[iLayer];
			bool bFound = false;
			for ( uint32_t iAccess = m_nAccessCount; iAccess != 0; --iAccess ) {
				const sirFrameGraphAccess_t& writer = m_szAccesses[iAccess - 1];
				if ( writer.bWrite && writer.nPass < nBeforePass && covers( writer, cell ) ) {
					pLive[writer.nPass] = 1;
					bFound = true;
					break;
				}
			}
			if ( !bFound && ( !resource.bImported || resource.eInitialState == SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED ) ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::findLivePasses( uint8_t *pLive ) const noexcept
{
	if ( !m_bCullingEnabled ) {
		memset( pLive, 1, m_nPassCount );
	}
	uint32_t nCellBudget = 8192;
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		if ( m_szPasses[iPass].bSideEffect ) {
			pLive[iPass] = 1;
		}
	}
	for ( uint32_t iOutput = 0; iOutput < m_Outputs.getSize(); ++iOutput ) {
		const sirFrameGraphOutput_t& output = m_Outputs[iOutput];
		sirFrameGraphAccess_t target {};
		target.nResource = output.nResource;
		target.nBaseMip = output.nBaseMip;
		target.nMipCount = output.nMipCount;
		target.nBaseLayer = output.nBaseLayer;
		target.nLayerCount = output.nLayerCount;
		const SIRGraphicsResult eResult = markProducers( target, m_nPassCount, pLive, &nCellBudget );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}

	if ( !m_bCullingEnabled ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	uint8_t szVisited[SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szVisited, 0, sizeof( szVisited ) );
	for ( uint32_t nIteration = 0; nIteration < m_nPassCount; ++nIteration ) {
		bool bChanged = false;
		for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
			if ( !pLive[iPass] || szVisited[iPass] ) {
				continue;
			}
			szVisited[iPass] = 1;
			bChanged = true;
			for ( uint32_t iPredecessor = 0; iPredecessor < m_nPassCount; ++iPredecessor ) {
				if ( m_ExplicitEdges.test( iPredecessor * SIR_FRAMEGRAPH_MAX_PASSES + iPass ) ) {
					pLive[iPredecessor] = 1;
				}
			}
			for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
				const sirFrameGraphAccess_t& access = m_szAccesses[iAccess];
				if ( access.nPass != iPass || access.bWrite ) {
					continue;
				}
				const SIRGraphicsResult eResult = markProducers( access, iPass, pLive, &nCellBudget );
				if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
					return eResult;
				}
			}
		}
		if ( !bChanged ) {
			break;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

bool SIRFrameGraph::requiresOrdering( const sirFrameGraphAccess_t& previous, const sirFrameGraphAccess_t& current ) const noexcept
{
	if ( previous.bWrite || current.bWrite ) {
		return true;
	}
	if ( previous.eState != current.eState ) {
		return true;
	}
	return m_szPasses[previous.nPass].eQueue != m_szPasses[current.nPass].eQueue;
}

SIRGraphicsResult SIRFrameGraph::validateCapabilities( const sirGraphicsCapabilities_t& caps, const uint8_t *pLive ) const noexcept
{
	if ( caps.nDeviceGeneration == 0 || ( caps.nQueueMask & ( 1U << static_cast<uint32_t>( SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) ) ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		const sirFrameGraphPass_t& pass = m_szPasses[iPass];
		if ( !pLive[iPass] ) {
			continue;
		}
		if ( ( caps.nQueueMask & ( 1U << static_cast<uint32_t>( pass.eQueue ) ) ) == 0 || ( pass.nRequiredFeatures & ~caps.nFeatureMask ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pass.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE && ( caps.nFeatureMask & static_cast<uint64_t>( SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE ) ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pass.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY && ( caps.nFeatureMask & static_cast<uint64_t>( SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COPY_QUEUE ) ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		const sirFrameGraphResource_t& resource = m_szResources[iResource];
		bool bUsed = false;
		for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
			bUsed = bUsed || ( m_szAccesses[iAccess].nResource == iResource && pLive[m_szAccesses[iAccess].nPass] );
		}
		for ( uint32_t iOutput = 0; iOutput < m_Outputs.getSize(); ++iOutput ) {
			bUsed = bUsed || m_Outputs[iOutput].nResource == iResource;
		}
		if ( bUsed && resource.bImported && resource.hImported.nDeviceGeneration != caps.nDeviceGeneration ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

static SIRGraphicsResult sirFrameGraphAppendTransition( const sirFrameGraphTransition_t& transition,
														  sirFrameGraphCompiled_t *pCompiled ) noexcept
{
	if ( pCompiled->nTransitionCount != 0 ) {
		sirFrameGraphTransition_t& previous = pCompiled->szTransitions[pCompiled->nTransitionCount - 1];
		const bool bSameIdentity = previous.nResource == transition.nResource && previous.nBeforePass == transition.nBeforePass &&
								   previous.nAfterPass == transition.nAfterPass;
		const bool bSameStates = previous.eBefore == transition.eBefore && previous.eAfter == transition.eAfter &&
								 previous.bWriteHazard == transition.bWriteHazard;
		const bool bSameQueues = previous.eFromQueue == transition.eFromQueue && previous.eToQueue == transition.eToQueue;
		if ( bSameIdentity && bSameStates && bSameQueues ) {
			const bool bNextMip = previous.nBaseLayer == transition.nBaseLayer && previous.nLayerCount == transition.nLayerCount &&
								  previous.nBaseMip + previous.nMipCount == transition.nBaseMip;
			const bool bNextLayer = previous.nBaseMip == transition.nBaseMip && previous.nMipCount == transition.nMipCount &&
									previous.nBaseLayer + previous.nLayerCount == transition.nBaseLayer;
			if ( bNextMip ) {
				previous.nMipCount += transition.nMipCount;
				return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
			}
			if ( bNextLayer ) {
				previous.nLayerCount += transition.nLayerCount;
				return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
			}
		}
	}
	if ( pCompiled->nTransitionCount >= SIR_FRAMEGRAPH_MAX_TRANSITIONS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	pCompiled->szTransitions[pCompiled->nTransitionCount++] = transition;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

static SIRGraphicsResult sirFrameGraphCompileAccess( const sirFrameGraphAccess_t *pAccesses, uint32_t iAccess,
													   sirFrameGraphCompileContext_t *pContext,
													   sirFrameGraphCompiled_t *pCompiled ) noexcept
{
	const sirFrameGraphAccess_t& access = pAccesses[iAccess];
	const sirFrameGraphResource_t& resource = pContext->pResources[access.nResource];
	uint32_t szMipCuts[SIR_FRAMEGRAPH_MAX_ACCESSES * 2 + 2] = { access.nBaseMip, access.nBaseMip + access.nMipCount };
	uint32_t szLayerCuts[SIR_FRAMEGRAPH_MAX_ACCESSES * 2 + 2] = { access.nBaseLayer, access.nBaseLayer + access.nLayerCount };
	uint32_t nMipCuts = 2;
	uint32_t nLayerCuts = 2;

	for ( uint32_t iPrevious = 0; iPrevious < iAccess; ++iPrevious ) {
		const sirFrameGraphAccess_t& previous = pAccesses[iPrevious];
		if ( !pContext->pLive[previous.nPass] || !sirFrameGraphOverlaps( previous, access ) ) {
			continue;
		}
		if ( previous.nPass != access.nPass && sirFrameGraphRequiresOrdering( previous, access, pContext->pPasses ) ) {
			pContext->pEdges[previous.nPass * SIR_FRAMEGRAPH_MAX_PASSES + access.nPass] = 1;
		}

		const uint32_t nMipBegin = previous.nBaseMip > access.nBaseMip ? previous.nBaseMip : access.nBaseMip;
		const uint32_t nMipEnd = previous.nBaseMip + previous.nMipCount < access.nBaseMip + access.nMipCount
									 ? previous.nBaseMip + previous.nMipCount
									 : access.nBaseMip + access.nMipCount;
		const uint32_t nLayerBegin = previous.nBaseLayer > access.nBaseLayer ? previous.nBaseLayer : access.nBaseLayer;
		const uint32_t nLayerEnd = previous.nBaseLayer + previous.nLayerCount < access.nBaseLayer + access.nLayerCount
									   ? previous.nBaseLayer + previous.nLayerCount
									   : access.nBaseLayer + access.nLayerCount;
		sirFrameGraphInsertCut( szMipCuts, &nMipCuts, nMipBegin );
		sirFrameGraphInsertCut( szMipCuts, &nMipCuts, nMipEnd );
		sirFrameGraphInsertCut( szLayerCuts, &nLayerCuts, nLayerBegin );
		sirFrameGraphInsertCut( szLayerCuts, &nLayerCuts, nLayerEnd );
	}

	const uint64_t nCellCount = static_cast<uint64_t>( nMipCuts - 1 ) * ( nLayerCuts - 1 );
	if ( nCellCount > pContext->nCellsRemaining ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	pContext->nCellsRemaining -= static_cast<uint32_t>(nCellCount);

	for ( uint32_t iLayer = 0; iLayer + 1 < nLayerCuts; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip + 1 < nMipCuts; ++iMip ) {
			sirFrameGraphAccess_t cell = access;
			cell.nBaseMip = szMipCuts[iMip];
			cell.nMipCount = szMipCuts[iMip + 1] - szMipCuts[iMip];
			cell.nBaseLayer = szLayerCuts[iLayer];
			cell.nLayerCount = szLayerCuts[iLayer + 1] - szLayerCuts[iLayer];

			uint32_t iLast = SIR_FRAMEGRAPH_INVALID_INDEX;
			bool bInitialized = resource.bImported && resource.eInitialState != SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED;
			for ( uint32_t iPrevious = iAccess; iPrevious != 0; --iPrevious ) {
				const sirFrameGraphAccess_t& previous = pAccesses[iPrevious - 1];
				if ( !pContext->pLive[previous.nPass] || !sirFrameGraphOverlaps( previous, cell ) ) {
					continue;
				}
				if ( iLast == SIR_FRAMEGRAPH_INVALID_INDEX ) {
					iLast = iPrevious - 1;
				}
				bInitialized = bInitialized || previous.bWrite;
			}
			if ( !access.bWrite && !bInitialized ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}

			sirFrameGraphTransition_t transition {};
			transition.nResource = access.nResource;
			transition.nBeforePass = SIR_FRAMEGRAPH_INVALID_INDEX;
			transition.nAfterPass = access.nPass;
			transition.nBaseMip = cell.nBaseMip;
			transition.nMipCount = cell.nMipCount;
			transition.nBaseLayer = cell.nBaseLayer;
			transition.nLayerCount = cell.nLayerCount;
			transition.eFromQueue = resource.eInitialQueue;
			transition.eToQueue = pContext->pPasses[access.nPass].eQueue;
			transition.eBefore = resource.eInitialState;
			transition.eAfter = access.eState;
			transition.bWriteHazard = access.bWrite;
			if ( iLast != SIR_FRAMEGRAPH_INVALID_INDEX ) {
				const sirFrameGraphAccess_t& previous = pAccesses[iLast];
				transition.nBeforePass = previous.nPass;
				transition.eFromQueue = pContext->pPasses[previous.nPass].eQueue;
				transition.eBefore = previous.eState;
				transition.bWriteHazard = previous.bWrite || access.bWrite;
			}
			const bool bStateChanged = transition.eBefore != transition.eAfter;
			const bool bQueueChanged = transition.eFromQueue != transition.eToQueue;
			if ( !bStateChanged && !bQueueChanged && !transition.bWriteHazard ) {
				continue;
			}
			const SIRGraphicsResult eResult = sirFrameGraphAppendTransition( transition, pCompiled );
			if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::buildTransitions( uint8_t *pszEdges, const uint8_t *pLive,
													 sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	constexpr uint32_t MAX_REGION_CELLS = 8192;
	sirFrameGraphCompileContext_t context = { m_szResources, m_szPasses, pszEdges, MAX_REGION_CELLS, pLive };
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		if ( !pLive[m_szAccesses[iAccess].nPass] ) {
			continue;
		}
		const SIRGraphicsResult eResult = sirFrameGraphCompileAccess( m_szAccesses, iAccess, &context, pCompiled );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::buildSchedule( const uint8_t *pszEdges, const uint8_t *pLive,
												  sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	uint8_t szEmitted[SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szEmitted, 0, sizeof( szEmitted ) );
	uint32_t szIndegree[SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szIndegree, 0, sizeof( szIndegree ) );
	for ( uint32_t iBefore = 0; iBefore < m_nPassCount; ++iBefore ) {
		for ( uint32_t iAfter = 0; iAfter < m_nPassCount; ++iAfter ) {
			if ( !pLive[iBefore] || !pLive[iAfter] || pszEdges[iBefore * SIR_FRAMEGRAPH_MAX_PASSES + iAfter] == 0 ) {
				continue;
			}
			if ( pCompiled->nDependencyCount == SIR_FRAMEGRAPH_MAX_DEPENDENCIES ) {
				return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
			}
			++szIndegree[iAfter];
			sirFrameGraphDependency_t& dependency = pCompiled->szDependencies[pCompiled->nDependencyCount++];
			dependency.nBeforePass = iBefore;
			dependency.nAfterPass = iAfter;
			dependency.bCrossQueue = m_szPasses[iBefore].eQueue != m_szPasses[iAfter].eQueue;
		}
	}

	for ( uint32_t iOrder = 0; iOrder < m_nPassCount; ++iOrder ) {
		uint32_t iReady = SIR_FRAMEGRAPH_INVALID_INDEX;
		for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
			if ( pLive[iPass] && szEmitted[iPass] == 0 && szIndegree[iPass] == 0 ) {
				iReady = iPass;
				break;
			}
		}
		if ( iReady == SIR_FRAMEGRAPH_INVALID_INDEX ) {
			break;
		}
		pCompiled->szPassOrder[pCompiled->nPassCount++] = iReady;
		szEmitted[iReady] = 1;
		for ( uint32_t iNext = 0; iNext < m_nPassCount; ++iNext ) {
			if ( pszEdges[iReady * SIR_FRAMEGRAPH_MAX_PASSES + iNext] != 0 ) {
				--szIndegree[iNext];
			}
		}
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		if ( pLive[iPass] && !szEmitted[iPass] ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

void SIRFrameGraph::calculateLifetimes( sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	uint32_t szOrderIndex[SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szOrderIndex, 0, sizeof( szOrderIndex ) );
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		szOrderIndex[iPass] = SIR_FRAMEGRAPH_INVALID_INDEX;
	}
	for ( uint32_t iOrder = 0; iOrder < pCompiled->nPassCount; ++iOrder ) {
		szOrderIndex[pCompiled->szPassOrder[iOrder]] = iOrder;
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		pCompiled->szFirstUse[iResource] = SIR_FRAMEGRAPH_INVALID_INDEX;
		pCompiled->szLastUse[iResource] = SIR_FRAMEGRAPH_INVALID_INDEX;
	}
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		const sirFrameGraphAccess_t& access = m_szAccesses[iAccess];
		const uint32_t iOrder = szOrderIndex[access.nPass];
		if ( iOrder == SIR_FRAMEGRAPH_INVALID_INDEX ) {
			continue;
		}
		uint32_t& iFirst = pCompiled->szFirstUse[access.nResource];
		uint32_t& iLast = pCompiled->szLastUse[access.nResource];
		if ( iFirst == SIR_FRAMEGRAPH_INVALID_INDEX || iOrder < iFirst ) {
			iFirst = iOrder;
		}
		if ( iLast == SIR_FRAMEGRAPH_INVALID_INDEX || iOrder > iLast ) {
			iLast = iOrder;
		}
	}
}

SIRGraphicsResult SIRFrameGraph::compile( const sirGraphicsCapabilities_t& caps, sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	if ( pCompiled == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	memset( pCompiled, 0, sizeof( *pCompiled ) );
	sirFrameGraphCompiled_t staged {};
	uint8_t szLive[SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szLive, 0, sizeof( szLive ) );
	const SIRGraphicsResult eLiveResult = findLivePasses( szLive );
	if ( eLiveResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eLiveResult;
	}
	const SIRGraphicsResult eCapsResult = validateCapabilities( caps, szLive );
	if ( eCapsResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eCapsResult;
	}

	uint8_t szEdges[SIR_FRAMEGRAPH_MAX_PASSES * SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szEdges, 0, sizeof( szEdges ) );
	m_ExplicitEdges.forEachSetBit( sirFrameGraphDenseEdgeWriter_t{ szEdges } );
	const SIRGraphicsResult eTransitionResult = buildTransitions( szEdges, szLive, &staged );
	if ( eTransitionResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eTransitionResult;
	}
	const SIRGraphicsResult eScheduleResult = buildSchedule( szEdges, szLive, &staged );
	if ( eScheduleResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eScheduleResult;
	}

	calculateLifetimes( &staged );
	// Export only live pass borrows. Callers must release each retained provider
	// lease against that provider's real completion set after GPU submission.
	for ( uint32_t iRead = 0; iRead < m_SharedReads.getSize(); ++iRead ) {
		const sirFrameGraphSharedRead_t& read = m_SharedReads[iRead].read;
		if ( szLive[read.nPass] ) {
			staged.szSharedReads[staged.nSharedReadCount++] = read;
		}
	}
	staged.pOwner = this;
	staged.nOwnerIdentity = m_nIdentity;
	staged.nRevision = m_nRevision;
	staged.nOriginalPassCount = m_nPassCount;
	memcpy( staged.szLivePasses, szLive, sizeof( szLive ) );
	*pCompiled = staged;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRFrameGraph::record( const sirFrameGraphCompiled_t& compiled,
										   const sirFrameGraphCallbacks_t& callbacks ) const noexcept
{
	if ( callbacks.pRecord == nullptr || callbacks.pRollback == nullptr || compiled.pOwner != this ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( compiled.nOwnerIdentity != m_nIdentity || compiled.nRevision != m_nRevision || compiled.nOriginalPassCount != m_nPassCount ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( compiled.nPassCount > m_nPassCount ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint8_t szSeen[SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szSeen, 0, sizeof( szSeen ) );
	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		const uint32_t iPass = compiled.szPassOrder[iOrder];
		if ( iPass >= m_nPassCount || !compiled.szLivePasses[iPass] || szSeen[iPass] ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szSeen[iPass] = 1;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		if ( ( szSeen[iPass] != 0 ) != ( compiled.szLivePasses[iPass] != 0 ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	uint32_t szOrder[SIR_FRAMEGRAPH_MAX_PASSES];
	memset( szOrder, 0, sizeof( szOrder ) );
	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		szOrder[compiled.szPassOrder[iOrder]] = iOrder;
	}
	if ( compiled.nDependencyCount > SIR_FRAMEGRAPH_MAX_DEPENDENCIES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iDependency = 0; iDependency < compiled.nDependencyCount; ++iDependency ) {
		const sirFrameGraphDependency_t& edge = compiled.szDependencies[iDependency];
		if ( edge.nBeforePass >= m_nPassCount || edge.nAfterPass >= m_nPassCount ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !szSeen[edge.nBeforePass] || !szSeen[edge.nAfterPass] || szOrder[edge.nBeforePass] >= szOrder[edge.nAfterPass] ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	// A compiled plan contains metadata, not ownership. Confirm every live
	// GRDG borrow is still issued before executing any recording callback.
	for ( uint32_t iRead = 0; iRead < m_SharedReads.getSize(); ++iRead ) {
		if ( !compiled.szLivePasses[m_SharedReads[iRead].read.nPass] ) {
			continue;
		}
		if ( !m_pSharedOwner || m_pSharedOwner->validateBufferLease( m_SharedReads[iRead].lease ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
	}

	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		const uint32_t iPass = compiled.szPassOrder[iOrder];
		const SIRGraphicsResult eResult = callbacks.pRecord( iPass, callbacks.pContext );
		if ( eResult == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			continue;
		}
		// Include the failed pass: its callback may have staged partial CPU work.
		for ( uint32_t iRollback = iOrder + 1; iRollback != 0; --iRollback ) {
			callbacks.pRollback( compiled.szPassOrder[iRollback - 1], callbacks.pContext );
		}
		return eResult;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
