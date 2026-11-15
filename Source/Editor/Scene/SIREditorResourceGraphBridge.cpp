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

sirEditorGraphBridgeResult_t sirEditorRequestCatalogResources( sirEditorResourceGraphRequest_t *pRequest ) noexcept
{
	if ( pRequest == nullptr ) {
		return SIR_EDITOR_GRAPH_INVALID_ARGUMENT;
	}
	pRequest->nRequested = 0;
	if ( pRequest->pCatalog == nullptr || pRequest->pGraph == nullptr || !pRequest->pGraph->isReady() ||
	     pRequest->pGraph->getWriter() == nullptr ) {
		return SIR_EDITOR_GRAPH_INVALID_ARGUMENT;
	}
	if ( !sirEditorCollectCatalogKeys( *pRequest->pCatalog, &pRequest->keyBuffer ) ) {
		return SIR_EDITOR_GRAPH_INVALID_CATALOG;
	}
	if ( pRequest->pOutHandles == nullptr || pRequest->nHandleCapacity < pRequest->keyBuffer.nCount ) {
		return SIR_EDITOR_GRAPH_OUT_OF_CAPACITY;
	}
	SIRResourceGraphWriter *pWriter = pRequest->pGraph->getWriter();
	for ( uint32_t i = 0; i < pRequest->keyBuffer.nCount; ++i ) {
		const sirEditorResourceRequestKey_t& entry = pRequest->keyBuffer.pKeys[i];
		const SIRResourceKey key = { entry.nCanonicalId, entry.nTypeId, 0 };
		SIRResourceHandle handle = {};
		if ( pWriter->request( key, &handle ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_EDITOR_GRAPH_REQUEST_FAILED;
		}
		pRequest->pOutHandles[pRequest->nRequested++] = handle;
	}
	return SIR_EDITOR_GRAPH_OK;
}
