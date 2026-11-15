/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRVGCPUPAGEVIEWS_H__
#define __SIRENGINE_SIRVGCPUPAGEVIEWS_H__

#include <Mesh/SIRVGPageDirectory.h>
#include <Resources/SIRResourceViews.h>

// A CPU-addressable page arena for fixed-function client arrays and for
// explicit legacy driver upload. Source SIRPack bytes remain canonical GRDG
// data; this memory contains only bounded, evictable decoded draw derivatives.
typedef struct sirVGCPUPageConfig_s
{
	const SIRMeshPackSchema *pSchema;
	SIRVGPageDirectory *pDirectory;
	uint8_t *pArena;
	uint64_t nArenaBytes;
	uint64_t nPhysicalBackingId;
} sirVGCPUPageConfig_t;

typedef struct sirVGCPUPageLease_s
{
	const sirMeshPackVertex_t *pVertices;
	const uint32_t *pIndices;
	uint32_t nVertices;
	uint32_t nIndices;
	uint64_t nPhysicalBackingId;
	uint64_t nSourceRevision;
	sirVGPagePin_t pin;
} sirVGCPUPageLease_t;

class SIRVGCPUPageViews
{
public:
	SIRVGCPUPageViews( void ) noexcept;
	sirVGResidencyResult_t initialize( const sirVGCPUPageConfig_t& config ) noexcept;
	sirVGResidencyResult_t install( uint32_t nPage, const SIRResourceConstBytes& cooked, uint32_t nRequestEpoch ) noexcept;
	sirVGResidencyResult_t installGRDG( const SIRResourceView& source, uint32_t nPage, uint32_t nRequestEpoch ) noexcept;
	sirVGResidencyResult_t acquire( uint32_t nPage, sirVGCPUPageLease_t *pLease ) noexcept;
	sirVGResidencyResult_t release( const sirVGCPUPageLease_t& lease ) noexcept;
	sirVGResidencyResult_t shutdown( void ) noexcept;

protected:
private:
	sirVGCPUPageConfig_t m_Config;
};

#endif // __SIRENGINE_SIRVGCPUPAGEVIEWS_H__
