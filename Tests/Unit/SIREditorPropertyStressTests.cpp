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

#include <Editor/Scene/SIREditorSceneDocument.h>

#include <assert.h>
#include <stdint.h>

typedef struct sirEditorStressModel_s
{
	uint64_t nValue[4][3];
	bool bPresent[4][3];
} sirEditorStressModel_t;

static uint32_t sirEditorStressNext( uint32_t *pSeed ) noexcept
{
	uint32_t nValue = *pSeed;
	nValue ^= nValue << 13;
	nValue ^= nValue >> 17;
	nValue ^= nValue << 5;
	*pSeed = nValue;
	return nValue;
}

static sirEditorProperty_t sirEditorStressProperty( uint32_t iNode, uint32_t iProperty, uint64_t nValue ) noexcept
{
	sirEditorProperty_t property = {};
	property.nNodeId = iNode + 1;
	property.nPropertyId = iProperty + 1;
	property.value.eKind = SIR_PROPERTY_UINT64;
	property.value.nPayload = nValue;
	return property;
}

static void sirEditorStressCheck( const SIREditorSceneDocument& document, const sirEditorStressModel_t& model ) noexcept
{
	sirEditorProperty_t szSnapshot[16] = {};
	uint32_t nActual = 0;
	uint32_t nExpected = 0;
	assert( document.copyPropertySnapshot( szSnapshot, 16, &nActual ) == SIR_EDITOR_OK );
	for ( uint32_t iNode = 0; iNode < 4; ++iNode ) {
		for ( uint32_t iProperty = 0; iProperty < 3; ++iProperty ) {
			sirPropertyValue_t value = {};
			const sirEditorResult_t eFind = document.findProperty( iNode + 1, iProperty + 1, &value );
			if ( model.bPresent[iNode][iProperty] ) {
				++nExpected;
				assert( eFind == SIR_EDITOR_OK );
				assert( value.eKind == SIR_PROPERTY_UINT64 );
				assert( value.nPayload == model.nValue[iNode][iProperty] );
			} else {
				assert( eFind == SIR_EDITOR_NOT_FOUND );
			}
		}
	}
	assert( nActual == nExpected );
	for ( uint32_t iSlot = 0; iSlot < nActual; ++iSlot ) {
		const sirEditorProperty_t& item = szSnapshot[iSlot];
		assert( item.nNodeId >= 1 && item.nNodeId <= 4 );
		assert( item.nPropertyId >= 1 && item.nPropertyId <= 3 );
		assert( model.bPresent[item.nNodeId - 1][item.nPropertyId - 1] );
		assert( item.value.nPayload == model.nValue[item.nNodeId - 1][item.nPropertyId - 1] );
	}
}

int main( void )
{
	sirObjectTypeDescriptor_t szTypes[1] = {};
	sirObjectPropertyDescriptor_t szDescriptors[3] = {};
	SIRObjectRegistry registry;
	assert( registry.initialize( szTypes, 1, szDescriptors, 3 ) == SIR_OBJECT_OK );
	assert( registry.registerType( { 1, 0, 100 } ) == SIR_OBJECT_OK );
	for ( uint32_t iProperty = 1; iProperty <= 3; ++iProperty ) {
		assert( registry.registerProperty( { iProperty, 1, iProperty + 100, SIR_PROPERTY_UINT64, {} } ) == SIR_OBJECT_OK );
	}
	assert( registry.seal() == SIR_OBJECT_OK );

	sirEditorNode_t szNodes[8] = {};
	sirEditorNode_t szStaging[8] = {};
	sirEditorMutation_t szPending[8] = {};
	sirEditorDelta_t szPendingDeltas[8] = {};
	sirEditorDelta_t szHistoryDeltas[64] = {};
	sirEditorHistoryFrame_t szFrames[8] = {};
	sirEditorIndexEntry_t szIndex[16] = {};
	sirEditorProperty_t szProperties[16] = {};
	sirEditorProperty_t szPropertyStaging[16] = {};
	sirEditorPropertyMutation_t szPendingProperties[16] = {};
	sirEditorPropertyDelta_t szPendingPropertyDeltas[32] = {};
	sirEditorPropertyDelta_t szHistoryPropertyDeltas[128] = {};
	sirEditorDocumentStorage_t storage = {};
	storage.pNodes = szNodes;
	storage.pStaging = szStaging;
	storage.pPending = szPending;
	storage.pPendingDeltas = szPendingDeltas;
	storage.pHistoryDeltas = szHistoryDeltas;
	storage.pHistoryFrames = szFrames;
	storage.pIndex = szIndex;
	storage.nNodeCapacity = 8;
	storage.nPendingCapacity = 8;
	storage.nDeltaCapacity = 64;
	storage.nFrameCapacity = 8;
	storage.nIndexCapacity = 16;
	storage.pProperties = szProperties;
	storage.pPropertyStaging = szPropertyStaging;
	storage.pPendingProperties = szPendingProperties;
	storage.pPendingPropertyDeltas = szPendingPropertyDeltas;
	storage.pHistoryPropertyDeltas = szHistoryPropertyDeltas;
	storage.nPropertyCapacity = 16;
	storage.nPendingPropertyCapacity = 16;
	storage.nPropertyDeltaCapacity = 128;

	SIREditorSceneDocument document;
	assert( document.initialize( storage ) == SIR_EDITOR_OK );
	assert( document.setPropertyRegistry( &registry ) == SIR_EDITOR_OK );
	assert( document.beginEdit( document.getRevision() ) == SIR_EDITOR_OK );
	for ( uint64_t nId = 1; nId <= 4; ++nId ) {
		sirEditorNode_t node = {};
		node.nId = nId;
		node.nTypeId = 1;
		assert( document.queueUpsert( node ) == SIR_EDITOR_OK );
	}
	assert( document.commitEdit() == SIR_EDITOR_OK );

	sirEditorStressModel_t model = {};
	uint32_t nSeed = 0x91352fcd;
	for ( uint32_t iTrial = 0; iTrial < 2500; ++iTrial ) {
		const uint32_t iNode = sirEditorStressNext( &nSeed ) % 4;
		const uint32_t iProperty = sirEditorStressNext( &nSeed ) % 3;
		const uint64_t nValue = ( ( uint64_t )sirEditorStressNext( &nSeed ) << 32 ) |
		                        sirEditorStressNext( &nSeed );
		const bool bFailure = iTrial % 11 == 0 || iTrial % 13 == 0;
		const bool bRemove = iTrial % 7 == 0 && model.bPresent[iNode][iProperty];
		const sirEditorStressModel_t previous = model;
		const uint64_t nPreviousRevision = document.getRevision();

		assert( document.beginEdit( nPreviousRevision ) == SIR_EDITOR_OK );
		if ( iTrial % 11 == 0 ) {
			sirEditorProperty_t invalid = sirEditorStressProperty( iNode, iProperty, nValue );
			invalid.value.eKind = SIR_PROPERTY_BOOL;
			assert( document.queueSetProperty( invalid ) == SIR_EDITOR_OK );
			assert( document.commitEdit() == SIR_EDITOR_INVALID_PROPERTY );
		} else if ( iTrial % 13 == 0 ) {
			sirEditorProperty_t invalid = sirEditorStressProperty( iNode, iProperty, nValue );
			invalid.nNodeId = 999;
			assert( document.queueSetProperty( invalid ) == SIR_EDITOR_OK );
			assert( document.commitEdit() == SIR_EDITOR_NOT_FOUND );
		} else {
			if ( bRemove ) {
				assert( document.queueRemoveProperty( iNode + 1, iProperty + 1 ) == SIR_EDITOR_OK );
				model.bPresent[iNode][iProperty] = false;
			} else {
				assert( document.queueSetProperty( sirEditorStressProperty( iNode, iProperty, nValue ) ) == SIR_EDITOR_OK );
				model.bPresent[iNode][iProperty] = true;
				model.nValue[iNode][iProperty] = nValue;
			}
			if ( iTrial % 5 == 0 && !bRemove ) {
				const uint64_t nSecondValue = nValue ^ UINT64_C( 0xfedcba9876543210 );
				assert( document.queueSetProperty( sirEditorStressProperty( iNode, iProperty, nSecondValue ) ) == SIR_EDITOR_OK );
				model.nValue[iNode][iProperty] = nSecondValue;
			}
			assert( document.commitEdit() == SIR_EDITOR_OK );
		}

		if ( bFailure ) {
			assert( document.getRevision() == nPreviousRevision );
			assert( document.cancelEdit() == SIR_EDITOR_OK );
			sirEditorStressCheck( document, previous );
		} else {
			sirEditorStressCheck( document, model );
			assert( document.undo() == SIR_EDITOR_OK );
			sirEditorStressCheck( document, previous );
			assert( document.redo() == SIR_EDITOR_OK );
			sirEditorStressCheck( document, model );
		}
		if ( iTrial % 17 == 0 ) {
			assert( document.beginEdit( document.getRevision() - 1 ) == SIR_EDITOR_STALE_REVISION );
		}
	}
	document.shutdown();
	registry.shutdown();
	return 0;
}
