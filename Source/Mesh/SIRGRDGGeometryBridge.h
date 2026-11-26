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

#ifndef __SIR_GRDG_GEOMETRY_BRIDGE_H__
#define __SIR_GRDG_GEOMETRY_BRIDGE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCache.h>
#include <Resources/SIRResourceGraph.h>

// Caller-owned pin metadata: one canonical GRDG resource source, no copied
// positions, indices, BVH or other mesh payload. Must outlive cache retirement.
typedef struct sirGRDGMeshPin_s
{
	SIRResourceGraph *pGraph;
	SIRResourcePinHandle hPin;
	bool bActive;
} sirGRDGMeshPin_t;

typedef struct sirGRDGMeshChunkSelection_s
{
	SIRResourceChunkId positionChunk;
	SIRResourceChunkId indexChunk;
	uint32_t nVariant;
	uint32_t nProviderId;
	// Zero chunk ID means that this optional canonical payload is absent.
	SIRResourceChunkId staticBVHChunk;
	SIRResourceChunkId surfaceMaterialChunk;
	SIRResourceChunkId lodHierarchyChunk;
} sirGRDGMeshChunkSelection_t;

class SIRGRDGGeometryBridge
{
public:
	SIRGRDGGeometryBridge( void ) = delete;
	~SIRGRDGGeometryBridge() = delete;

	// Both source chunks must be canonical and retained by a GRDG provider pin.
	// A duplicate key returns the same handle only when the same source bytes
	// are already attached to that cache; no replacement allocation is made.
	static SIRResourceResult attach( SIRResourceGraph& rGraph, SIRMeshCache& rCache, const SIRResourceReadScope& rScope,
	                                 const SIRResourceView& rSource, const sirGRDGMeshChunkSelection_t& rChunks, sirGRDGMeshPin_t *pPin,
	                                 sirMeshHandle_t *pHandle ) noexcept;

	static void releaseSourcePin( void *pContext ) noexcept;
};

#endif // __SIR_GRDG_GEOMETRY_BRIDGE_H__
