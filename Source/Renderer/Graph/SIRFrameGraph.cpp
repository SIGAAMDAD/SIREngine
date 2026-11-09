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

SIRFrameGraph::SIRFrameGraph( void ) noexcept
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
	m_nSharedByteAccessCount = 0;
	m_nOutputCount = 0;
	++m_nRevision;
	memset( m_szExplicitEdges, 0, sizeof( m_szExplicitEdges ) );
}

bool SIRFrameGraph::isWritableState( sirGraphicsState_t eState ) noexcept
{
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
	case SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
	case SIR_GRAPHICS_STATE_DEPTH_WRITE:
	case SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return true;
	default:
		return false;
	}
}

bool SIRFrameGraph::isReadableState( sirGraphicsState_t eState ) noexcept
{
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_VERTEX_INDEX:
	case SIR_GRAPHICS_STATE_CONSTANT:
	case SIR_GRAPHICS_STATE_SHADER_READ:
	case SIR_GRAPHICS_STATE_DEPTH_READ:
	case SIR_GRAPHICS_STATE_COPY_SOURCE:
	case SIR_GRAPHICS_STATE_PRESENT:
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

sirGraphicsResult_t SIRFrameGraph::addResource( const sirFrameGraphResource_t& resource, uint32_t *pIndex ) noexcept
{
	return addResourceImpl( resource, pIndex, false );
}

sirGraphicsResult_t SIRFrameGraph::addResourceImpl( const sirFrameGraphResource_t& resource, uint32_t *pIndex, bool bSharedLease ) noexcept
{
	if ( ( resource.nCanonicalBackingId != 0 || resource.nCanonicalNativeToken != 0 ) && !bSharedLease ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( bSharedLease && ( !resource.bImported || !resource.nCanonicalBackingId || !resource.nCanonicalNativeToken ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pIndex == nullptr || resource.nMipLevels == 0 || resource.nArrayLayers == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( resource.eType != SIR_GRAPHICS_HANDLE_BUFFER && resource.eType != SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( resource.eType == SIR_GRAPHICS_HANDLE_BUFFER && ( resource.nMipLevels != 1 || resource.nArrayLayers != 1 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( resource.bImported && ( resource.hImported.eType != resource.eType || resource.hImported.nGeneration == 0 ||
								 resource.hImported.nDeviceGeneration == 0 ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( resource.eInitialQueue >= SIR_GRAPHICS_QUEUE_COUNT || resource.eInitialState > SIR_GRAPHICS_STATE_PRESENT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !resource.bImported && resource.eInitialState != SIR_GRAPHICS_STATE_UNDEFINED ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nResourceCount == SIR_FRAMEGRAPH_MAX_RESOURCES ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	// A semantic frame product has one producer/storage identity for its entire
	// view/eye/quality/epoch. Duplicate descriptors cannot escape through a
	// second resource name, native handle or pass-local allocation.
	const sirFrameGraphProductKey_t& key = resource.product;
	const bool bNamedProduct = key.nViewId || key.nFrameEpoch || key.nSemanticId || key.nEyeId || key.nQualityId;
	if ( bNamedProduct ) {
		if ( !key.nViewId || !key.nFrameEpoch || !key.nSemanticId || resource.nCanonicalBackingId ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
			const sirFrameGraphProductKey_t& existing = m_szResources[iResource].product;
			if ( existing.nViewId == key.nViewId && existing.nFrameEpoch == key.nFrameEpoch && existing.nSemanticId == key.nSemanticId &&
				 existing.nEyeId == key.nEyeId && existing.nQualityId == key.nQualityId ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
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
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}

	*pIndex = m_nResourceCount;
	m_szResources[m_nResourceCount++] = resource;
	++m_nRevision;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::importSharedBuffer( const sirNativeGPUBufferLease_t& rLease, sirGraphicsState_t eInitialState,
													   sirGraphicsQueue_t eInitialQueue, uint32_t *pIndex ) noexcept
{
	if ( !pIndex || !rLease.view.bReadOnly || !rLease.view.nPhysicalBackingId || !rLease.view.nNativeResourceToken || !rLease.view.nBytes ||
		 rLease.view.nOffset > UINT64_MAX - rLease.view.nBytes || !rLease.view.nBorrowGeneration || !rLease.view.nConsumerId ||
		 !rLease.view.nDeviceGeneration || rLease.hNative.eType != SIR_GRAPHICS_HANDLE_BUFFER || !rLease.hNative.nGeneration ||
		 rLease.hNative.nDeviceGeneration != rLease.view.nDeviceGeneration || eInitialQueue >= SIR_GRAPHICS_QUEUE_COUNT ||
		 !isReadableState( eInitialState ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
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
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		*pIndex = iResource;
		return SIR_GRAPHICS_SUCCESS;
	}
	const sirFrameGraphResource_t imported = { rLease.hNative,
											   SIR_GRAPHICS_HANDLE_BUFFER,
											   eInitialState,
											   eInitialQueue,
											   1,
											   1,
											   true,
											   rLease.view.nPhysicalBackingId,
											   rLease.view.nNativeResourceToken };
	return addResourceImpl( imported, pIndex, true );
}

sirGraphicsResult_t SIRFrameGraph::addSharedBufferRead( uint32_t nPass, uint32_t nResource, const sirNativeGPUBufferLease_t& rLease,
														uint64_t nOffsetBytes, uint64_t nBytes, sirGraphicsState_t eState ) noexcept
{
	if ( nPass >= m_nPassCount || nResource >= m_nResourceCount || !nBytes || !rLease.view.bReadOnly ||
		 !rLease.view.nCompletionProviderId || !rLease.view.nBorrowGeneration || !isReadableState( eState ) ||
		 m_nSharedByteAccessCount >= SIR_FRAMEGRAPH_MAX_ACCESSES || nOffsetBytes < rLease.view.nOffset || nBytes > rLease.view.nBytes ||
		 nOffsetBytes - rLease.view.nOffset > rLease.view.nBytes - nBytes ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirFrameGraphResource_t& imported = m_szResources[nResource];
	if ( !imported.bImported || imported.eType != SIR_GRAPHICS_HANDLE_BUFFER ||
		 imported.nCanonicalBackingId != rLease.view.nPhysicalBackingId ||
		 imported.nCanonicalNativeToken != rLease.view.nNativeResourceToken || imported.hImported.nSlot != rLease.hNative.nSlot ||
		 imported.hImported.nGeneration != rLease.hNative.nGeneration ||
		 imported.hImported.nDeviceGeneration != rLease.hNative.nDeviceGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirGraphicsResult_t eAdded = addAccessImpl( { nPass, nResource, 0, 1, 0, 1, eState, false }, true );
	if ( eAdded != SIR_GRAPHICS_SUCCESS ) {
		return eAdded;
	}
	m_szSharedByteAccesses[m_nSharedByteAccessCount++] = { nPass,
														   nResource,
														   rLease.view.nConsumerId,
														   rLease.view.nCompletionProviderId,
														   rLease.view.nBorrowSlot,
														   rLease.view.nBorrowGeneration,
														   rLease.view.nPhysicalBackingId,
														   rLease.view.nNativeResourceToken,
														   nOffsetBytes,
														   nBytes };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::addPass( const sirFrameGraphPass_t& pass, uint32_t *pIndex ) noexcept
{
	if ( pIndex == nullptr || pass.eQueue >= SIR_GRAPHICS_QUEUE_COUNT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nPassCount == SIR_FRAMEGRAPH_MAX_PASSES ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	*pIndex = m_nPassCount;
	m_szPasses[m_nPassCount++] = pass;
	++m_nRevision;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::addAccess( const sirFrameGraphAccess_t& access ) noexcept
{
	return addAccessImpl( access, false );
}

sirGraphicsResult_t SIRFrameGraph::addAccessImpl( const sirFrameGraphAccess_t& access, bool bSharedLease ) noexcept
{
	if ( access.nPass >= m_nPassCount || access.nResource >= m_nResourceCount ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nMipCount == 0 || access.nLayerCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirFrameGraphResource_t& resource = m_szResources[access.nResource];
	if ( resource.nCanonicalBackingId != 0 && !bSharedLease ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nBaseMip >= resource.nMipLevels || access.nMipCount > resource.nMipLevels - access.nBaseMip ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nBaseLayer >= resource.nArrayLayers || access.nLayerCount > resource.nArrayLayers - access.nBaseLayer ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Canonical GRDG source imports are immutable throughout this graph's
	// lifetime. Mutable frame products require separately versioned backing.
	if ( access.bWrite && resource.nCanonicalBackingId != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.bWrite ? !isWritableState( access.eState ) : !isReadableState( access.eState ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsQueue_t eQueue = m_szPasses[access.nPass].eQueue;
	if ( eQueue == SIR_GRAPHICS_QUEUE_COPY && access.eState != SIR_GRAPHICS_STATE_COPY_SOURCE &&
		 access.eState != SIR_GRAPHICS_STATE_COPY_DESTINATION ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( eQueue == SIR_GRAPHICS_QUEUE_COMPUTE &&
		 ( access.eState == SIR_GRAPHICS_STATE_COLOR_ATTACHMENT || access.eState == SIR_GRAPHICS_STATE_DEPTH_WRITE ||
		   access.eState == SIR_GRAPHICS_STATE_PRESENT ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nAccessCount == SIR_FRAMEGRAPH_MAX_ACCESSES ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	// Access declarations follow pass insertion order; hazards retain that order.
	if ( m_nAccessCount != 0 && access.nPass < m_szAccesses[m_nAccessCount - 1].nPass ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		const sirFrameGraphAccess_t& previous = m_szAccesses[iAccess];
		if ( previous.nPass == access.nPass && overlaps( previous, access ) &&
			 ( previous.bWrite || access.bWrite || previous.eState != access.eState ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	m_szAccesses[m_nAccessCount++] = access;
	++m_nRevision;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::addDependency( uint32_t nBefore, uint32_t nAfter ) noexcept
{
	if ( nBefore >= m_nPassCount || nAfter >= m_nPassCount || nBefore == nAfter ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	m_szExplicitEdges[nBefore * SIR_FRAMEGRAPH_MAX_PASSES + nAfter] = 1;
	++m_nRevision;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::addOutput( const sirFrameGraphOutput_t& output ) noexcept
{
	if ( output.nResource >= m_nResourceCount || output.nMipCount == 0 || output.nLayerCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirFrameGraphResource_t& resource = m_szResources[output.nResource];
	if ( output.nBaseMip >= resource.nMipLevels || output.nMipCount > resource.nMipLevels - output.nBaseMip ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( output.nBaseLayer >= resource.nArrayLayers || output.nLayerCount > resource.nArrayLayers - output.nBaseLayer ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nOutputCount >= SIR_FRAMEGRAPH_MAX_OUTPUTS ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	m_szOutputs[m_nOutputCount++] = output;
	++m_nRevision;
	return SIR_GRAPHICS_SUCCESS;
}

// Every cell of an output or live read needs its closest preceding writer.
// Earlier overwritten writers are not dependencies of the output.
sirGraphicsResult_t SIRFrameGraph::markProducers( const sirFrameGraphAccess_t& target, uint32_t nBeforePass, uint8_t *pLive,
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
	const uint64_t nCells = ( uint64_t )( nMipCuts - 1 ) * ( nLayerCuts - 1 );
	if ( nCells > *pCellBudget ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	*pCellBudget -= ( uint32_t )nCells;

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
			if ( !bFound && ( !resource.bImported || resource.eInitialState == SIR_GRAPHICS_STATE_UNDEFINED ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::findLivePasses( uint8_t *pLive ) const noexcept
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
	for ( uint32_t iOutput = 0; iOutput < m_nOutputCount; ++iOutput ) {
		const sirFrameGraphOutput_t& output = m_szOutputs[iOutput];
		sirFrameGraphAccess_t target = {};
		target.nResource = output.nResource;
		target.nBaseMip = output.nBaseMip;
		target.nMipCount = output.nMipCount;
		target.nBaseLayer = output.nBaseLayer;
		target.nLayerCount = output.nLayerCount;
		const sirGraphicsResult_t eResult = markProducers( target, m_nPassCount, pLive, &nCellBudget );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}

	if ( !m_bCullingEnabled ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	uint8_t szVisited[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t nIteration = 0; nIteration < m_nPassCount; ++nIteration ) {
		bool bChanged = false;
		for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
			if ( !pLive[iPass] || szVisited[iPass] ) {
				continue;
			}
			szVisited[iPass] = 1;
			bChanged = true;
			for ( uint32_t iPredecessor = 0; iPredecessor < m_nPassCount; ++iPredecessor ) {
				if ( m_szExplicitEdges[iPredecessor * SIR_FRAMEGRAPH_MAX_PASSES + iPass] ) {
					pLive[iPredecessor] = 1;
				}
			}
			for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
				const sirFrameGraphAccess_t& access = m_szAccesses[iAccess];
				if ( access.nPass != iPass || access.bWrite ) {
					continue;
				}
				const sirGraphicsResult_t eResult = markProducers( access, iPass, pLive, &nCellBudget );
				if ( eResult != SIR_GRAPHICS_SUCCESS ) {
					return eResult;
				}
			}
		}
		if ( !bChanged ) {
			break;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
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

sirGraphicsResult_t SIRFrameGraph::validateCapabilities( const sirGraphicsCapabilities_t& caps, const uint8_t *pLive ) const noexcept
{
	if ( caps.nDeviceGeneration == 0 || ( caps.nQueueMask & ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		const sirFrameGraphPass_t& pass = m_szPasses[iPass];
		if ( !pLive[iPass] ) {
			continue;
		}
		if ( ( caps.nQueueMask & ( 1U << pass.eQueue ) ) == 0 || ( pass.nRequiredFeatures & ~caps.nFeatureMask ) != 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pass.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE && ( caps.nFeatureMask & SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pass.eQueue == SIR_GRAPHICS_QUEUE_COPY && ( caps.nFeatureMask & SIR_GRAPHICS_FEATURE_COPY_QUEUE ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		const sirFrameGraphResource_t& resource = m_szResources[iResource];
		bool bUsed = false;
		for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
			bUsed = bUsed || ( m_szAccesses[iAccess].nResource == iResource && pLive[m_szAccesses[iAccess].nPass] );
		}
		for ( uint32_t iOutput = 0; iOutput < m_nOutputCount; ++iOutput ) {
			bUsed = bUsed || m_szOutputs[iOutput].nResource == iResource;
		}
		if ( bUsed && resource.bImported && resource.hImported.nDeviceGeneration != caps.nDeviceGeneration ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsResult_t sirFrameGraphAppendTransition( const sirFrameGraphTransition_t& transition,
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
				return SIR_GRAPHICS_SUCCESS;
			}
			if ( bNextLayer ) {
				previous.nLayerCount += transition.nLayerCount;
				return SIR_GRAPHICS_SUCCESS;
			}
		}
	}
	if ( pCompiled->nTransitionCount >= SIR_FRAMEGRAPH_MAX_TRANSITIONS ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	pCompiled->szTransitions[pCompiled->nTransitionCount++] = transition;
	return SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsResult_t sirFrameGraphCompileAccess( const sirFrameGraphAccess_t *pAccesses, uint32_t iAccess,
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

	const uint64_t nCellCount = ( uint64_t )( nMipCuts - 1 ) * ( nLayerCuts - 1 );
	if ( nCellCount > pContext->nCellsRemaining ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	pContext->nCellsRemaining -= ( uint32_t )nCellCount;

	for ( uint32_t iLayer = 0; iLayer + 1 < nLayerCuts; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip + 1 < nMipCuts; ++iMip ) {
			sirFrameGraphAccess_t cell = access;
			cell.nBaseMip = szMipCuts[iMip];
			cell.nMipCount = szMipCuts[iMip + 1] - szMipCuts[iMip];
			cell.nBaseLayer = szLayerCuts[iLayer];
			cell.nLayerCount = szLayerCuts[iLayer + 1] - szLayerCuts[iLayer];

			uint32_t iLast = SIR_FRAMEGRAPH_INVALID_INDEX;
			bool bInitialized = resource.bImported && resource.eInitialState != SIR_GRAPHICS_STATE_UNDEFINED;
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
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}

			sirFrameGraphTransition_t transition = {};
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
			const sirGraphicsResult_t eResult = sirFrameGraphAppendTransition( transition, pCompiled );
			if ( eResult != SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::buildTransitions( uint8_t *pszEdges, const uint8_t *pLive,
													 sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	constexpr uint32_t MAX_REGION_CELLS = 8192;
	sirFrameGraphCompileContext_t context = { m_szResources, m_szPasses, pszEdges, MAX_REGION_CELLS, pLive };
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		if ( !pLive[m_szAccesses[iAccess].nPass] ) {
			continue;
		}
		const sirGraphicsResult_t eResult = sirFrameGraphCompileAccess( m_szAccesses, iAccess, &context, pCompiled );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::buildSchedule( const uint8_t *pszEdges, const uint8_t *pLive,
												  sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	uint8_t szEmitted[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	uint32_t szIndegree[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iBefore = 0; iBefore < m_nPassCount; ++iBefore ) {
		for ( uint32_t iAfter = 0; iAfter < m_nPassCount; ++iAfter ) {
			if ( !pLive[iBefore] || !pLive[iAfter] || pszEdges[iBefore * SIR_FRAMEGRAPH_MAX_PASSES + iAfter] == 0 ) {
				continue;
			}
			if ( pCompiled->nDependencyCount == SIR_FRAMEGRAPH_MAX_DEPENDENCIES ) {
				return SIR_GRAPHICS_CAPACITY_EXCEEDED;
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
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

void SIRFrameGraph::calculateLifetimes( sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	uint32_t szOrderIndex[SIR_FRAMEGRAPH_MAX_PASSES] = {};
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

sirGraphicsResult_t SIRFrameGraph::compile( const sirGraphicsCapabilities_t& caps, sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	if ( pCompiled == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	memset( pCompiled, 0, sizeof( *pCompiled ) );
	sirFrameGraphCompiled_t staged = {};
	uint8_t szLive[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	const sirGraphicsResult_t eLiveResult = findLivePasses( szLive );
	if ( eLiveResult != SIR_GRAPHICS_SUCCESS ) {
		return eLiveResult;
	}
	const sirGraphicsResult_t eCapsResult = validateCapabilities( caps, szLive );
	if ( eCapsResult != SIR_GRAPHICS_SUCCESS ) {
		return eCapsResult;
	}

	uint8_t szEdges[SIR_FRAMEGRAPH_MAX_PASSES * SIR_FRAMEGRAPH_MAX_PASSES] = {};
	memcpy( szEdges, m_szExplicitEdges, sizeof( szEdges ) );
	const sirGraphicsResult_t eTransitionResult = buildTransitions( szEdges, szLive, &staged );
	if ( eTransitionResult != SIR_GRAPHICS_SUCCESS ) {
		return eTransitionResult;
	}
	const sirGraphicsResult_t eScheduleResult = buildSchedule( szEdges, szLive, &staged );
	if ( eScheduleResult != SIR_GRAPHICS_SUCCESS ) {
		return eScheduleResult;
	}

	calculateLifetimes( &staged );
	// Export only live pass borrows. Callers must release each retained provider
	// lease against that provider's real completion set after GPU submission.
	for ( uint32_t iRead = 0; iRead < m_nSharedByteAccessCount; ++iRead ) {
		const sirFrameGraphSharedRead_t& read = m_szSharedByteAccesses[iRead];
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
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::record( const sirFrameGraphCompiled_t& compiled,
										   const sirFrameGraphCallbacks_t& callbacks ) const noexcept
{
	if ( callbacks.pRecord == nullptr || callbacks.pRollback == nullptr || compiled.pOwner != this ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( compiled.nOwnerIdentity != m_nIdentity || compiled.nRevision != m_nRevision || compiled.nOriginalPassCount != m_nPassCount ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( compiled.nPassCount > m_nPassCount ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint8_t szSeen[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		const uint32_t iPass = compiled.szPassOrder[iOrder];
		if ( iPass >= m_nPassCount || !compiled.szLivePasses[iPass] || szSeen[iPass] ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szSeen[iPass] = 1;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		if ( ( szSeen[iPass] != 0 ) != ( compiled.szLivePasses[iPass] != 0 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	uint32_t szOrder[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		szOrder[compiled.szPassOrder[iOrder]] = iOrder;
	}
	if ( compiled.nDependencyCount > SIR_FRAMEGRAPH_MAX_DEPENDENCIES ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iDependency = 0; iDependency < compiled.nDependencyCount; ++iDependency ) {
		const sirFrameGraphDependency_t& edge = compiled.szDependencies[iDependency];
		if ( edge.nBeforePass >= m_nPassCount || edge.nAfterPass >= m_nPassCount ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !szSeen[edge.nBeforePass] || !szSeen[edge.nAfterPass] || szOrder[edge.nBeforePass] >= szOrder[edge.nAfterPass] ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		const uint32_t iPass = compiled.szPassOrder[iOrder];
		const sirGraphicsResult_t eResult = callbacks.pRecord( iPass, callbacks.pContext );
		if ( eResult == SIR_GRAPHICS_SUCCESS ) {
			continue;
		}
		// Include the failed pass: its callback may have staged partial CPU work.
		for ( uint32_t iRollback = iOrder + 1; iRollback != 0; --iRollback ) {
			callbacks.pRollback( compiled.szPassOrder[iRollback - 1], callbacks.pContext );
		}
		return eResult;
	}
	return SIR_GRAPHICS_SUCCESS;
}
