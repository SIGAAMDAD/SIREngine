/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Editor/Scene/SIREditorResourceGraphBridge.h>
#include <Resources/SIRResourceGraph.h>

SIREditorGraphBridgeResult sirEditorRequestCatalogResources( sirEditorResourceGraphRequest_t *pRequest ) noexcept
{
	if ( pRequest == nullptr ) {
		return SIREditorGraphBridgeResult::SIR_EDITOR_GRAPH_INVALID_ARGUMENT;
	}
	pRequest->nRequested = 0;
	if ( pRequest->pCatalog == nullptr || pRequest->pGraph == nullptr || !pRequest->pGraph->isReady() ||
	     pRequest->pGraph->getWriter() == nullptr ) {
		return SIREditorGraphBridgeResult::SIR_EDITOR_GRAPH_INVALID_ARGUMENT;
	}
	if ( !sirEditorCollectCatalogKeys( *pRequest->pCatalog, &pRequest->keyBuffer ) ) {
		return SIREditorGraphBridgeResult::SIR_EDITOR_GRAPH_INVALID_CATALOG;
	}
	if ( pRequest->pOutHandles == nullptr || pRequest->nHandleCapacity < pRequest->keyBuffer.nCount ) {
		return SIREditorGraphBridgeResult::SIR_EDITOR_GRAPH_OUT_OF_CAPACITY;
	}
	SIRResourceGraphWriter *pWriter = pRequest->pGraph->getWriter();
	for ( uint32_t i = 0; i < pRequest->keyBuffer.nCount; ++i ) {
		const sirEditorResourceRequestKey_t& entry = pRequest->keyBuffer.pKeys[i];
		const SIRResourceKey key = { entry.nCanonicalId, entry.nTypeId, 0 };
		SIRResourceHandle handle {};
		if ( pWriter->request( key, &handle ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIREditorGraphBridgeResult::SIR_EDITOR_GRAPH_REQUEST_FAILED;
		}
		pRequest->pOutHandles[pRequest->nRequested++] = handle;
	}
	return SIREditorGraphBridgeResult::SIR_EDITOR_GRAPH_OK;
}
