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

#include <World/HLOD/SIRWorldHLODPins.h>

#include <Resources/SIRResourceGraph.h>

static bool sirWorldHLODPinMatch( const sirWorldHLODPinInput_t& input, uint32_t iNode, const SIRResourceInfo& info ) noexcept
{
	const sirWorldHLODNode_t& node = input.snapshot.pNodes[iNode];
	const sirWorldHLODProviderReady_t& provider = input.snapshot.pProviderReady[iNode];
	return provider.bReady && node.keyGeometry.m_nAssetId != 0 && node.keyGeometry.m_nTypeId != 0 && node.nSourceRevision != 0 &&
	       provider.nSourceRevision == node.nSourceRevision && provider.nResourceRevision != 0 &&
	       provider.nWorldGeneration == input.snapshot.nWorldGeneration && provider.nDeviceGeneration == input.snapshot.nDeviceGeneration &&
	       info.m_eKind == SIRResourceKind::SIR_RESOURCE_KIND_GEOMETRY && info.m_eState == SIRResourceState::SIR_RESOURCE_STATE_RESIDENT &&
	       info.m_nRevision == provider.nResourceRevision && info.m_Key.m_nAssetId == node.keyGeometry.m_nAssetId &&
	       info.m_Key.m_nTypeId == node.keyGeometry.m_nTypeId && info.m_Key.m_nVariantId == node.keyGeometry.m_nVariantId;
}

SIRWorldHLODPinResult sirWorldHLODPinCut( const sirWorldHLODPinInput_t& input, const sirWorldHLODPinOutput_t& output ) noexcept
{
	const sirWorldHLODGRDGInput_t& snapshot = input.snapshot;
	if ( snapshot.pGraph == nullptr || snapshot.pScope == nullptr || snapshot.pNodes == nullptr || snapshot.pProviderReady == nullptr ||
	     input.pSelectedNodes == nullptr || output.pPins == nullptr || output.pScratchPins == nullptr || output.pnPinned == nullptr ||
	     snapshot.nNodeCount == 0 || input.nSelectedCount == 0 || input.nProviderId == 0 || snapshot.nWorldGeneration == 0 ||
	     snapshot.nDeviceGeneration == 0 || !snapshot.pGraph->isReady() || !snapshot.pScope->isActive() ) {
		return SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_INVALID_ARGUMENT;
	}
	if ( input.nSelectedCount > output.nCapacity || input.nSelectedCount > snapshot.nNodeCount ) {
		return SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_CAPACITY;
	}
	for ( uint32_t iSelect = 0; iSelect < input.nSelectedCount; ++iSelect ) {
		const uint32_t iNode = input.pSelectedNodes[iSelect];
		if ( iNode >= snapshot.nNodeCount || ( iSelect != 0 && iNode <= input.pSelectedNodes[iSelect - 1] ) ) {
			return SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_INVALID_CUT;
		}
	}

	uint32_t nAcquired = 0;
	SIRWorldHLODPinResult eFailure = SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_OK;
	for ( uint32_t iSelect = 0; iSelect < input.nSelectedCount; ++iSelect ) {
		const uint32_t iNode = input.pSelectedNodes[iSelect];
		const sirWorldHLODNode_t& node = snapshot.pNodes[iNode];
		const sirWorldHLODProviderReady_t& provider = snapshot.pProviderReady[iNode];
		if ( !provider.bReady || provider.nSourceRevision != node.nSourceRevision || node.nSourceRevision == 0 ||
		     provider.nWorldGeneration != snapshot.nWorldGeneration || provider.nDeviceGeneration != snapshot.nDeviceGeneration ||
		     provider.nResourceRevision == 0 || node.keyGeometry.m_nAssetId == 0 || node.keyGeometry.m_nTypeId == 0 ) {
			eFailure = SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_STALE_RESIDENCY;
			break;
		}
		SIRResourceHandle handle {};
		SIRResourceView view;
		SIRResourceInfo info {};
		if ( snapshot.pGraph->find( *snapshot.pScope, node.keyGeometry, &handle ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ||
		     snapshot.pGraph->resolve( *snapshot.pScope, handle, &view ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ||
		     view.getInfo( &info ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS || !sirWorldHLODPinMatch( input, iNode, info ) ) {
			eFailure = SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_STALE_RESIDENCY;
			break;
		}
		if ( snapshot.pGraph->pinForProvider( *snapshot.pScope, view, input.nProviderId, &output.pScratchPins[nAcquired] ) !=
		     SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			eFailure = SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_PROVIDER_FAILURE;
			break;
		}
		++nAcquired;
	}
	if ( eFailure != SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_OK ) {
		bool bRolledBack = true;
		for ( uint32_t iPin = 0; iPin < nAcquired; ++iPin ) {
			if ( snapshot.pGraph->releaseProviderPin( output.pScratchPins[iPin] ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
				bRolledBack = false;
			}
		}
		return bRolledBack ? eFailure : SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_ROLLBACK_FAILURE;
	}
	for ( uint32_t iPin = 0; iPin < nAcquired; ++iPin ) {
		output.pPins[iPin] = output.pScratchPins[iPin];
	}
	*output.pnPinned = nAcquired;
	return SIRWorldHLODPinResult::SIR_WORLD_HLOD_PIN_OK;
}
