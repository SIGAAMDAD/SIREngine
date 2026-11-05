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

#include <World/HLOD/SIRWorldHLODGRDG.h>

#include <Resources/SIRResourceGraph.h>

sirWorldHLODResult_t sirWorldHLODSnapshotGRDG( const sirWorldHLODGRDGInput_t& input, const sirWorldHLODGRDGOutput_t& output ) noexcept
{
	if ( input.pGraph == nullptr || input.pScope == nullptr || input.pNodes == nullptr || input.pProviderReady == nullptr ||
	     output.pResidency == nullptr || input.nNodeCount == 0 || output.nCapacity < input.nNodeCount || input.nWorldGeneration == 0 ||
	     input.nDeviceGeneration == 0 || !input.pGraph->isReady() || !input.pScope->isActive() ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	// Validate first, so invalid descriptors never partially mutate caller output.
	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		const sirWorldHLODNode_t& node = input.pNodes[iNode];
		if ( node.keyGeometry.m_nAssetId == 0 || node.keyGeometry.m_nTypeId == 0 || node.nSourceRevision == 0 ) {
			return SIR_WORLD_HLOD_INVALID_HIERARCHY;
		}
	}

	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		const sirWorldHLODNode_t& node = input.pNodes[iNode];
		const sirWorldHLODProviderReady_t& provider = input.pProviderReady[iNode];
		sirWorldHLODResidency_t resident = {};
		resident.nWorldGeneration = input.nWorldGeneration;

		if ( provider.bReady && provider.nSourceRevision == node.nSourceRevision && provider.nResourceRevision != 0 &&
		     provider.nWorldGeneration == input.nWorldGeneration && provider.nDeviceGeneration == input.nDeviceGeneration ) {
			SIRResourceHandle handle = {};
			if ( input.pGraph->find( *input.pScope, node.keyGeometry, &handle ) == SIR_RESOURCE_RESULT_SUCCESS ) {
				SIRResourceView view;
				if ( input.pGraph->resolve( *input.pScope, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS ) {
					SIRResourceInfo info = {};
					if ( view.getInfo( &info ) == SIR_RESOURCE_RESULT_SUCCESS && info.m_eKind == SIR_RESOURCE_KIND_GEOMETRY &&
					     info.m_eState == SIR_RESOURCE_STATE_RESIDENT && info.m_nRevision == provider.nResourceRevision &&
					     info.m_Key.m_nAssetId == node.keyGeometry.m_nAssetId && info.m_Key.m_nTypeId == node.keyGeometry.m_nTypeId &&
					     info.m_Key.m_nVariantId == node.keyGeometry.m_nVariantId ) {
						resident.nSourceRevision = node.nSourceRevision;
						resident.bReady = true;
					}
				}
			}
		}
		output.pResidency[iNode] = resident;
	}
	return SIR_WORLD_HLOD_OK;
}
