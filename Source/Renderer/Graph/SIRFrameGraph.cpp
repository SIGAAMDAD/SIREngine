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

uint32_t SIRFrameGraph::getPassCount( void ) const noexcept
{
	return m_nPassCount;
}

uint32_t SIRFrameGraph::getResourceCount( void ) const noexcept
{
	return m_nResourceCount;
}

void SIRFrameGraph::reset( void ) noexcept
{
	m_nPassCount = 0;
	m_nResourceCount = 0;
	m_nAccessCount = 0;
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
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::addAccess( const sirFrameGraphAccess_t& access ) noexcept
{
	if ( access.nPass >= m_nPassCount || access.nResource >= m_nResourceCount ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nMipCount == 0 || access.nLayerCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirFrameGraphResource_t& resource = m_szResources[access.nResource];
	if ( access.nBaseMip >= resource.nMipLevels || access.nMipCount > resource.nMipLevels - access.nBaseMip ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( access.nBaseLayer >= resource.nArrayLayers || access.nLayerCount > resource.nArrayLayers - access.nBaseLayer ) {
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
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::addDependency( uint32_t nBefore, uint32_t nAfter ) noexcept
{
	if ( nBefore >= m_nPassCount || nAfter >= m_nPassCount || nBefore == nAfter ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	m_szExplicitEdges[nBefore * SIR_FRAMEGRAPH_MAX_PASSES + nAfter] = 1;
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

sirGraphicsResult_t SIRFrameGraph::validateCapabilities( const sirGraphicsCapabilities_t& caps ) const noexcept
{
	if ( caps.nDeviceGeneration == 0 || ( caps.nQueueMask & ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		const sirFrameGraphPass_t& pass = m_szPasses[iPass];
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
		if ( resource.bImported && resource.hImported.nDeviceGeneration != caps.nDeviceGeneration ) {
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
		if ( !sirFrameGraphOverlaps( previous, access ) ) {
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
				if ( !sirFrameGraphOverlaps( previous, cell ) ) {
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

sirGraphicsResult_t SIRFrameGraph::buildTransitions( uint8_t *pszEdges, sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	constexpr uint32_t MAX_REGION_CELLS = 8192;
	sirFrameGraphCompileContext_t context = { m_szResources, m_szPasses, pszEdges, MAX_REGION_CELLS };
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		const sirGraphicsResult_t eResult = sirFrameGraphCompileAccess( m_szAccesses, iAccess, &context, pCompiled );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::buildSchedule( const uint8_t *pszEdges, sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	uint8_t szEmitted[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	uint32_t szIndegree[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iBefore = 0; iBefore < m_nPassCount; ++iBefore ) {
		for ( uint32_t iAfter = 0; iAfter < m_nPassCount; ++iAfter ) {
			if ( pszEdges[iBefore * SIR_FRAMEGRAPH_MAX_PASSES + iAfter] == 0 ) {
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
			if ( szEmitted[iPass] == 0 && szIndegree[iPass] == 0 ) {
				iReady = iPass;
				break;
			}
		}
		if ( iReady == SIR_FRAMEGRAPH_INVALID_INDEX ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		pCompiled->szPassOrder[iOrder] = iReady;
		szEmitted[iReady] = 1;
		for ( uint32_t iNext = 0; iNext < m_nPassCount; ++iNext ) {
			if ( pszEdges[iReady * SIR_FRAMEGRAPH_MAX_PASSES + iNext] != 0 ) {
				--szIndegree[iNext];
			}
		}
	}
	pCompiled->nPassCount = m_nPassCount;
	return SIR_GRAPHICS_SUCCESS;
}

void SIRFrameGraph::calculateLifetimes( sirFrameGraphCompiled_t *pCompiled ) const noexcept
{
	uint32_t szOrderIndex[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iOrder = 0; iOrder < m_nPassCount; ++iOrder ) {
		szOrderIndex[pCompiled->szPassOrder[iOrder]] = iOrder;
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		pCompiled->szFirstUse[iResource] = SIR_FRAMEGRAPH_INVALID_INDEX;
		pCompiled->szLastUse[iResource] = SIR_FRAMEGRAPH_INVALID_INDEX;
	}
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		const sirFrameGraphAccess_t& access = m_szAccesses[iAccess];
		const uint32_t iOrder = szOrderIndex[access.nPass];
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
	const sirGraphicsResult_t eCapsResult = validateCapabilities( caps );
	if ( eCapsResult != SIR_GRAPHICS_SUCCESS ) {
		return eCapsResult;
	}

	sirFrameGraphCompiled_t staged = {};
	uint8_t szEdges[SIR_FRAMEGRAPH_MAX_PASSES * SIR_FRAMEGRAPH_MAX_PASSES] = {};
	memcpy( szEdges, m_szExplicitEdges, sizeof( szEdges ) );
	const sirGraphicsResult_t eTransitionResult = buildTransitions( szEdges, &staged );
	if ( eTransitionResult != SIR_GRAPHICS_SUCCESS ) {
		return eTransitionResult;
	}
	const sirGraphicsResult_t eScheduleResult = buildSchedule( szEdges, &staged );
	if ( eScheduleResult != SIR_GRAPHICS_SUCCESS ) {
		return eScheduleResult;
	}

	calculateLifetimes( &staged );
	*pCompiled = staged;
	return SIR_GRAPHICS_SUCCESS;
}
