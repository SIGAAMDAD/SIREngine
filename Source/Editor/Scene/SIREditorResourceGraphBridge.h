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

#ifndef __SIR_EDITORRESOURCEGRAPHBRIDGE_H__
#define __SIR_EDITORRESOURCEGRAPHBRIDGE_H__
#include <Core/SIRPCH.h>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on
#include <Editor/Scene/SIREditorResourceKeys.h>
#include <Resources/SIRResourceTypes.h>
class SIRResourceGraph;

enum class SIREditorGraphBridgeResult : uint8_t {
	SIR_EDITOR_GRAPH_OK = 0,
	SIR_EDITOR_GRAPH_INVALID_ARGUMENT = 1,
	SIR_EDITOR_GRAPH_OUT_OF_CAPACITY = 2,
	SIR_EDITOR_GRAPH_INVALID_CATALOG = 3,
	SIR_EDITOR_GRAPH_REQUEST_FAILED = 4
};

typedef struct sirEditorResourceGraphRequest_s
{
	SIRResourceCatalog *pCatalog;
	SIRResourceGraph *pGraph;
	sirEditorResourceKeyBuffer_t keyBuffer;
	SIRResourceHandle *pOutHandles;
	uint32_t nHandleCapacity;
	uint32_t nRequested;
} sirEditorResourceGraphRequest_t;

// Request canonical resources from GRDG in catalog dependency order. This does
// not own payloads or duplicate graph state. On failure nRequested identifies
// the prefix already requested from GRDG, which remains graph-owned.
SIREditorGraphBridgeResult sirEditorRequestCatalogResources( sirEditorResourceGraphRequest_t *pRequest ) noexcept;
#endif // __SIR_EDITORRESOURCEGRAPHBRIDGE_H__
