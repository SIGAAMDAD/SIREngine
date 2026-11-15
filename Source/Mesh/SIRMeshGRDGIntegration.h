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
#ifndef __SIRENGINE_SIRMESHGRDGINTEGRATION_H__
#define __SIRENGINE_SIRMESHGRDGINTEGRATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCache.h>
#include <Resources/SIRResourceTypes.h>

class SIRResourceGraph;
class SIRResourceReadScope;
class SIRResourceView;

// Existing GRDG resource views and provider pins remain the only backing
// authority. The caller owns the scope and view until insert() returns.
typedef struct sirMeshGRDGSourceRequest_s
{
	const SIRResourceGraph *pGraph;
	const SIRResourceReadScope *pScope;
	const SIRResourceView *pView;
	sirMeshKey_t key;
	uint32_t nResourceTypeId;
	uint32_t nProviderId;
	SIRResourceChunkId positionsChunk;
	SIRResourceChunkId indicesChunk;
	uint32_t nVertexCount;
	uint32_t nIndexCount;
	// Optional canonical read-only BVH/triangle-order/material chunks; all
	// three chunks must be requested together (zero IDs omit the derivative).
	SIRResourceChunkId bvhNodesChunk;
	SIRResourceChunkId bvhOrderChunk;
	SIRResourceChunkId surfaceMaterialsChunk;
	// The canonical SIRG V3 format stores one packed mesh/material source
	// chunk and one independently readable BVH chunk. No unpacked copies.
	SIRResourceChunkId packedCollisionChunk;
	SIRResourceChunkId packedBVHChunk;
	uint32_t nCollisionProfileIndex;
} sirMeshGRDGSourceRequest_t;

// Explicit native GRDG provider lease, not a callback adapter. The caller
// retains the token and closes it after the last logical MeshCache reader;
// the source pin cannot be released while the cache slot remains alive.
typedef struct sirMeshGRDGSourceLease_s
{
	const SIRResourceGraph *pGraph;
	SIRMeshCache *pCache;
	sirMeshHandle_t hMesh;
	SIRResourcePinHandle hPin;
	SIRResourceResult eLastRelease;
	bool bActive;
} sirMeshGRDGSourceLease_t;

#endif // __SIRENGINE_SIRMESHGRDGINTEGRATION_H__
