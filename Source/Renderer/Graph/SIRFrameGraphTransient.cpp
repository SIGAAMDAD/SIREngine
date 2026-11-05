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

#include <Renderer/Graph/SIRFrameGraphTransient.h>

// GPU commands on one queue execute serially; independent queues can overlap
// despite a deterministic CPU topological order. Cross-queue aliasing remains
// ineligible until submission/completion-set proof is available to the
// provider.
sirGraphicsResult_t SIRFrameGraph::planTransients( const sirFrameGraphCompiled_t& compiled, const sirFrameGraphTransientInput_t& input,
												   sirFrameGraphTransientPlan_t *pPlan ) const noexcept
{
	if ( pPlan == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	memset( pPlan, 0, sizeof( *pPlan ) );
	if ( compiled.pOwner != this || compiled.nOwnerIdentity != m_nIdentity || compiled.nRevision != m_nRevision ||
		 compiled.nOriginalPassCount != m_nPassCount || compiled.nPassCount > m_nPassCount ||
		 compiled.nDependencyCount > SIR_FRAMEGRAPH_MAX_DEPENDENCIES || input.nDescCount > SIR_FRAMEGRAPH_MAX_RESOURCES ||
		 ( input.nDescCount != 0 && input.pDescs == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	uint32_t szOrder[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	uint32_t szFirst[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	uint32_t szLast[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	sirGraphicsQueue_t szQueue[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	uint8_t szQueueSeen[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	const sirFrameGraphTransientDesc_t *szDesc[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	uint8_t szExported[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		szOrder[iPass] = SIR_FRAMEGRAPH_INVALID_INDEX;
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		szFirst[iResource] = SIR_FRAMEGRAPH_INVALID_INDEX;
		szLast[iResource] = SIR_FRAMEGRAPH_INVALID_INDEX;
		szQueue[iResource] = SIR_GRAPHICS_QUEUE_COUNT;
	}
	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		const uint32_t iPass = compiled.szPassOrder[iOrder];
		if ( iPass >= m_nPassCount || !compiled.szLivePasses[iPass] || szOrder[iPass] != SIR_FRAMEGRAPH_INVALID_INDEX ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szOrder[iPass] = iOrder;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		if ( ( szOrder[iPass] != SIR_FRAMEGRAPH_INVALID_INDEX ) != ( compiled.szLivePasses[iPass] != 0 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iDependency = 0; iDependency < compiled.nDependencyCount; ++iDependency ) {
		const sirFrameGraphDependency_t& dependency = compiled.szDependencies[iDependency];
		if ( dependency.nBeforePass >= m_nPassCount || dependency.nAfterPass >= m_nPassCount ||
			 szOrder[dependency.nBeforePass] == SIR_FRAMEGRAPH_INVALID_INDEX ||
			 szOrder[dependency.nAfterPass] == SIR_FRAMEGRAPH_INVALID_INDEX ||
			 szOrder[dependency.nBeforePass] >= szOrder[dependency.nAfterPass] ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iAccess = 0; iAccess < m_nAccessCount; ++iAccess ) {
		const sirFrameGraphAccess_t& access = m_szAccesses[iAccess];
		const uint32_t iOrder = szOrder[access.nPass];
		if ( iOrder == SIR_FRAMEGRAPH_INVALID_INDEX ) {
			continue;
		}
		uint32_t& nFirst = szFirst[access.nResource];
		uint32_t& nLast = szLast[access.nResource];
		if ( nFirst == SIR_FRAMEGRAPH_INVALID_INDEX || iOrder < nFirst ) {
			nFirst = iOrder;
		}
		if ( nLast == SIR_FRAMEGRAPH_INVALID_INDEX || iOrder > nLast ) {
			nLast = iOrder;
		}
		const sirGraphicsQueue_t eQueue = m_szPasses[access.nPass].eQueue;
		if ( !szQueueSeen[access.nResource] ) {
			szQueue[access.nResource] = eQueue;
			szQueueSeen[access.nResource] = 1;
		} else if ( szQueue[access.nResource] != eQueue ) {
			szQueue[access.nResource] = SIR_GRAPHICS_QUEUE_COUNT;
		}
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		if ( szFirst[iResource] != compiled.szFirstUse[iResource] || szLast[iResource] != compiled.szLastUse[iResource] ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iOutput = 0; iOutput < m_nOutputCount; ++iOutput ) {
		szExported[m_szOutputs[iOutput].nResource] = 1;
	}
	for ( uint32_t iDesc = 0; iDesc < input.nDescCount; ++iDesc ) {
		const sirFrameGraphTransientDesc_t& desc = input.pDescs[iDesc];
		if ( desc.nResource >= m_nResourceCount || m_szResources[desc.nResource].bImported || szDesc[desc.nResource] != nullptr ||
			 desc.nRequiredBytes == 0 || desc.nAlignmentBytes == 0 || ( desc.nAlignmentBytes & ( desc.nAlignmentBytes - 1 ) ) != 0 ||
			 desc.nProviderCompatibilityKey == 0 || desc.eMemory > SIR_GRAPHICS_MEMORY_TRANSIENT || desc.nSampleCount == 0 ||
			 ( m_szResources[desc.nResource].eType == SIR_GRAPHICS_HANDLE_BUFFER &&
			   ( desc.eFormat != SIR_GRAPHICS_FORMAT_UNKNOWN || desc.nSampleCount != 1 ) ) ||
			 ( m_szResources[desc.nResource].eType == SIR_GRAPHICS_HANDLE_TEXTURE &&
			   ( desc.eFormat == SIR_GRAPHICS_FORMAT_UNKNOWN || desc.eFormat > SIR_GRAPHICS_FORMAT_BC7_UNORM ) ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szDesc[desc.nResource] = &desc;
	}

	uint32_t szSorted[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	uint32_t nSorted = 0;
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		if ( szFirst[iResource] == SIR_FRAMEGRAPH_INVALID_INDEX || m_szResources[iResource].bImported ) {
			continue;
		}
		if ( szDesc[iResource] == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		uint32_t iInsert = nSorted;
		while ( iInsert != 0 && szFirst[szSorted[iInsert - 1]] > szFirst[iResource] ) {
			szSorted[iInsert] = szSorted[iInsert - 1];
			--iInsert;
		}
		szSorted[iInsert] = iResource;
		++nSorted;
	}

	sirFrameGraphTransientPlan_t staged = {};
	staged.nResourceCount = m_nResourceCount;
	for ( uint32_t iResource = 0; iResource < SIR_FRAMEGRAPH_MAX_RESOURCES; ++iResource ) {
		staged.szResourceSlot[iResource] = SIR_FRAMEGRAPH_INVALID_INDEX;
	}
	for ( uint32_t iSorted = 0; iSorted < nSorted; ++iSorted ) {
		const uint32_t nResource = szSorted[iSorted];
		const sirFrameGraphTransientDesc_t& desc = *szDesc[nResource];
		if ( UINT64_MAX - staged.nLogicalBytes < desc.nRequiredBytes ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		staged.nLogicalBytes += desc.nRequiredBytes;
		uint32_t iSlot = 0;
		for ( ; iSlot < staged.nSlotCount; ++iSlot ) {
			const sirFrameGraphTransientSlot_t& slot = staged.szSlots[iSlot];
			if ( !slot.bReuseAllowed || !desc.bAllowAlias || slot.eQueue != szQueue[nResource] || slot.nLastUse >= szFirst[nResource] ||
				 slot.nRequiredBytes < desc.nRequiredBytes || slot.nAlignmentBytes < desc.nAlignmentBytes ||
				 slot.nProviderCompatibilityKey != desc.nProviderCompatibilityKey || slot.eMemory != desc.eMemory ||
				 slot.eType != m_szResources[nResource].eType || slot.eFormat != desc.eFormat || slot.nSampleCount != desc.nSampleCount ) {
				continue;
			}
			break;
		}
		if ( iSlot == staged.nSlotCount ) {
			const uint64_t nRound = desc.nAlignmentBytes - 1;
			if ( desc.nRequiredBytes > UINT64_MAX - nRound ) {
				return SIR_GRAPHICS_CAPACITY_EXCEEDED;
			}
			const uint64_t nAligned = ( desc.nRequiredBytes + nRound ) & ~nRound;
			if ( nAligned > input.nMaxBackingBytes - staged.nRequestedBackingBytes ) {
				return SIR_GRAPHICS_OUT_OF_MEMORY;
			}
			staged.nRequestedBackingBytes += nAligned;
			++staged.nSlotCount;
			staged.szSlots[iSlot].nRepresentativeResource = nResource;
			staged.szSlots[iSlot].nRequiredBytes = nAligned;
			staged.szSlots[iSlot].nAlignmentBytes = desc.nAlignmentBytes;
			staged.szSlots[iSlot].nProviderCompatibilityKey = desc.nProviderCompatibilityKey;
			staged.szSlots[iSlot].eMemory = desc.eMemory;
			staged.szSlots[iSlot].eType = m_szResources[nResource].eType;
			staged.szSlots[iSlot].eFormat = desc.eFormat;
			staged.szSlots[iSlot].nSampleCount = desc.nSampleCount;
		}
		staged.szSlots[iSlot].nLastUse = szLast[nResource];
		staged.szSlots[iSlot].eQueue = szQueue[nResource];
		staged.szSlots[iSlot].bReuseAllowed =
			desc.bAllowAlias && szExported[nResource] == 0 && szQueue[nResource] != SIR_GRAPHICS_QUEUE_COUNT;
		staged.szResourceSlot[nResource] = iSlot;
	}
	*pPlan = staged;
	return SIR_GRAPHICS_SUCCESS;
}