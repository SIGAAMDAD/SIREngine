/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRSHAREDGEOMETRYBVH_H__
#define __SIRENGINE_SIRSHAREDGEOMETRYBVH_H__

#include <Resources/SIRPackFormat.h>
#include <Resources/SIRResourceGraph.h>

// One pointer-free canonical static-mesh BVH and surface/material table.
// Render, physics, audio and navigation all borrow this exact immutable
// payload using a single GRDG read scope. Native acceleration is derived.
constexpr uint32_t SIR_SHARED_BVH_FORMAT = 0x42524953U; // 'SIRB', little endian
constexpr uint16_t SIR_SHARED_BVH_VERSION = 1;
constexpr uint64_t SIR_SHARED_BVH_HEADER_BYTES = 128;
constexpr uint64_t SIR_SHARED_BVH_NODE_BYTES = 48;
constexpr uint64_t SIR_SHARED_BVH_TRIANGLE_BYTES = 16;
constexpr uint64_t SIR_SHARED_BVH_PROXY_BYTES = 24;
constexpr uint64_t SIR_SHARED_BVH_MATERIAL_BYTES = 80;
constexpr uint32_t SIR_SHARED_BVH_MAX_RECORDS = 1048576;
constexpr uint32_t SIR_SHARED_BVH_LEAF = 1;
constexpr uint32_t SIR_SHARED_BVH_NO_CHILD = UINT32_MAX;

// A leaf references triangles by stable triangle ID; the index remap points
// into the original immutable mesh triangle array. Every system observes the
// same triangle and surface identity independent of native query backends.
typedef struct sirSharedBvhNode_s
{
	float szMinimum[3];
	float szMaximum[3];
	uint32_t nFirstChild;
	uint32_t nSecondChild;
	uint32_t nFirstTriangle;
	uint32_t nTriangleCount;
	float fGeometricError;
	uint32_t nFlags;
} sirSharedBvhNode_t;

typedef struct sirSharedBvhTriangle_s
{
	uint32_t nStableTriangleId;
	uint32_t nSurfaceMaterialId;
	uint32_t nSurfaceSemanticId;
	uint32_t nSourceTriangleIndex;
} sirSharedBvhTriangle_t;

// Domain mask: 1 renderer, 2 collision, 4 acoustic, 8 navigation.
// Domain-specific policy can select proxy ranges without copying the BVH.
typedef struct sirSharedBvhProxy_s
{
	uint32_t nDomainMask;
	uint32_t nProfileId;
	uint32_t nFirstTriangle;
	uint32_t nTriangleCount;
	float fMaximumError;
	uint32_t nFlags;
} sirSharedBvhProxy_t;

// Four spectral bands use [0,1] energy fractions, not arbitrary decibels.
// Absorbed plus transmitted energy is at most one for every band.
typedef struct sirSharedSurfaceMaterial_s
{
	uint32_t nMaterialId;
	uint32_t nSurfaceSemantics;
	uint64_t nVisualResourceAssetId;
	float fFriction;
	float fRestitution;
	float fDensity;
	float fNavigationCost;
	uint32_t nNavigationAgentMask;
	uint32_t nVisualFacetId;
	float szAcousticAbsorption[4];
	float szAcousticTransmission[4];
	float fAcousticScattering;
} sirSharedSurfaceMaterial_t;

typedef enum sirSharedBvhResult_e : uint8_t
{
	SIR_SHARED_BVH_OK = 0,
	SIR_SHARED_BVH_INVALID_ARGUMENT,
	SIR_SHARED_BVH_INVALID_FORMAT,
	SIR_SHARED_BVH_UNSUPPORTED_VERSION,
	SIR_SHARED_BVH_TRUNCATED,
	SIR_SHARED_BVH_BAD_DIGEST,
	SIR_SHARED_BVH_OUT_OF_CAPACITY
} sirSharedBvhResult_t;

typedef struct sirSharedBvhDescriptor_s
{
	uint64_t nAssetId;
	uint64_t nRevision;
	const sirSharedBvhNode_t *pNodes;
	const sirSharedBvhTriangle_t *pTriangles;
	const sirSharedBvhProxy_t *pProxies;
	const sirSharedSurfaceMaterial_t *pMaterials;
	uint32_t nNodeCount;
	uint32_t nTriangleCount;
	uint32_t nProxyCount;
	uint32_t nMaterialCount;
	// Caller-owned scratch: nNodeCount + 2 * nTriangleCount uint32 values.
	uint32_t *pParentScratch;
	uint32_t nParentCapacity;
} sirSharedBvhDescriptor_t;

class SIRSharedGeometryBVH final
{
private:
	const uint8_t *m_pBytes;
	uint64_t m_nBytes;
	uint32_t m_nNodes;
	uint32_t m_nTriangles;
	uint32_t m_nProxies;
	uint32_t m_nMaterials;

public:
	SIRSharedGeometryBVH( void ) noexcept;
	static uint64_t requiredBytes( uint32_t nNodes, uint32_t nTriangles, uint32_t nProxies, uint32_t nMaterials ) noexcept;
	static uint32_t requiredValidationWords( uint32_t nNodes, uint32_t nTriangles ) noexcept;
	static sirSharedBvhResult_t write( const sirSharedBvhDescriptor_t& desc, void *pOutput, uint64_t nCapacity ) noexcept;
	// Caller must retain GRDG source read scope until this borrowed view is discarded.
	sirSharedBvhResult_t open( const void *pBytes, uint64_t nBytes, uint32_t *pParentScratch, uint32_t nParentCapacity ) noexcept;
	// The caller holds the GRDG read scope owning this immutable source view.
	// All subsystems use this same accessor; no domain-specific BVH copy.
	sirSharedBvhResult_t openPublished( const SIRResourceView& resource, uint32_t nChunkId, uint32_t *pScratch,
	                                    uint32_t nScratchWords ) noexcept;
	void reset( void ) noexcept;
	bool isOpen( void ) const noexcept;
	uint64_t getAssetId( void ) const noexcept;
	uint64_t getRevision( void ) const noexcept;
	uint32_t getNodeCount( void ) const noexcept;
	uint32_t getTriangleCount( void ) const noexcept;
	sirSharedBvhResult_t nodeAt( uint32_t iNode, sirSharedBvhNode_t *pNode ) const noexcept;
	sirSharedBvhResult_t triangleAt( uint32_t iTriangle, sirSharedBvhTriangle_t *pTriangle ) const noexcept;
	sirSharedBvhResult_t proxyAt( uint32_t iProxy, sirSharedBvhProxy_t *pProxy ) const noexcept;
	sirSharedBvhResult_t materialAt( uint32_t iMaterial, sirSharedSurfaceMaterial_t *pMaterial ) const noexcept;
};

#endif // __SIRENGINE_SIRSHAREDGEOMETRYBVH_H__
