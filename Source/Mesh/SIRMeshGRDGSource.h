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
#ifndef __SIRENGINE_SIRMESHGRDGSOURCE_H__
#define __SIRENGINE_SIRMESHGRDGSOURCE_H__

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
} sirMeshGRDGSourceRequest_t;

// One fixed caller-owned record per inserted canonical mesh, not per instance.
// Keep this storage and the GRDG alive until MeshCache's final retirement.
// Only MeshCache's final release callback may free the GRDG provider pin.
typedef struct sirMeshGRDGSourceLease_s
{
	const SIRResourceGraph *pGraph;
	SIRResourcePinHandle hPin;
	SIRResourceResult eLastRelease;
	bool bActive;
} sirMeshGRDGSourceLease_t;

class SIRMeshGRDGSource
{
public:
	static sirMeshResult_t insert( SIRMeshCache& cache, const sirMeshGRDGSourceRequest_t& request, sirMeshGRDGSourceLease_t *pLease,
	                               sirMeshHandle_t *pHandle ) noexcept;

protected:
private:
	static void releasePin( void *pContext ) noexcept;
};

#endif // __SIRENGINE_SIRMESHGRDGSOURCE_H__
