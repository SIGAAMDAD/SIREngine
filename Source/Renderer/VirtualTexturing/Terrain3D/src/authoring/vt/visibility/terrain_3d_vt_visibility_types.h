#ifndef TERRAIN3D_VT_VISIBILITY_TYPES_H
#define TERRAIN3D_VT_VISIBILITY_TYPES_H

#include <stdint.h>
#include <vector>

// T01: immutable offline geometry/probe ABI. Coordinates are Godot world metres:
// X right, Y up, Z forward; 128m cells use floor(world X/Z / 128).
// View bin 0 looks +X; bins increment toward +Z in 45-degree steps.
// Cubemap face vectors (+X,-X,+Y,-Y,+Z,-Z) are mapped to those octants
// during the future T02 bake. No RenderingDevice RIDs in these inputs.
#define T3D_VT_DEMAND_BAKED_VISIBILITY_FORMAT_VERSION 1u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_ENDIAN_TAG 0x01020304u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_CELL_METERS 128u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_VIEW_BINS 8u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_MIP_BINS 16u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_GROUP_EDGE 4u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_HEADER_BYTES 136u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_CELL_BYTES 32u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_BUNDLE_BYTES 40u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_RECORD_BYTES 64u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_CELLS 4096u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_RECORDS 1048576u
#define T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_BUNDLE_BYTES ( 1024u * 1024u )
#define T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_FILE_BYTES ( 128u * 1024u * 1024u )

// A material record without visible samples is explicit: no-visible flag and
// zero histogram. A PARTIAL record always retains reactive VT feedback.
enum t3dVTVisibilityRecordFlag_t : uint8_t
{
	T3D_VT_VISIBILITY_PARTIAL = 1u << 0u,
	T3D_VT_VISIBILITY_NO_VISIBLE = 1u << 1u
};

struct t3dVTVisibilityStaticVertex_t
{
	float m_fWorldX, m_fWorldY, m_fWorldZ;
	float m_fU, m_fV; // source UVs, not virtual page coordinates
};

struct t3dVTVisibilityStaticTriangle_t
{
	uint32_t m_nIndices[3];
	uint32_t m_nMaterialKeyIndex; // indexes an immutable, sorted stable-key table
	uint32_t m_nFlags;            // alpha/displacement/animated coverage declared by cooker
};

struct t3dVTVisibilityProbeInput_t
{
	int32_t m_nCellX = 0, m_nCellZ = 0;
	float m_fWorldX = 0.0f, m_fWorldY = 0.0f, m_fWorldZ = 0.0f;
	float m_fFovDegrees = 90.0f;
	uint32_t m_nNominalRenderSize = 256u;
	uint32_t m_nFirstTriangle = 0u;
	uint32_t m_nTriangleCount = 0u;
	uint32_t m_nSourceRevision = 0u;
};

// A borrowed, immutable cook-time scene view. The caller keeps these arrays
// alive until the T02 GPU baker completes its input staging. Stable keys are
// strictly increasing and are referenced by triangle material-key indices;
// there are no Object*, Godot resources, or RenderingDevice handles here.
struct t3dVTVisibilityStaticSceneView_t
{
	const t3dVTVisibilityStaticVertex_t *m_pVertices = nullptr;
	uint32_t m_nVertexCount = 0u;
	const t3dVTVisibilityStaticTriangle_t *m_pTriangles = nullptr;
	uint32_t m_nTriangleCount = 0u;
	const uint64_t *m_pMaterialKeys = nullptr;
	uint32_t m_nMaterialCount = 0u;
	const t3dVTVisibilityProbeInput_t *m_pProbes = nullptr;
	uint32_t m_nProbeCount = 0u;
	uint64_t m_nSourceSignature = 0u;
};

// Histogram weights are 16-bit quantized visible-pixel contributions.
// m_nSampleCount is the original count (before 16-bit quantization);
// confidence is normalized 0..65535 and zero means unknown/absent.
struct t3dVTDemandVisibilityCookConfig_t
{
	uint64_t m_nPackageSignature = 0u;
	uint64_t m_nAssetIdentityHash = 0u;
	uint64_t m_nSourceSignature = 0u;     // exact static geometry/material revisions
	uint64_t m_nBakeConfigSignature = 0u; // deterministic probe selection settings
	uint32_t m_nNominalTextureSize = 4096u;
	uint32_t m_nNominalRenderSize = 256u;
	uint32_t m_nFovMilliDegrees = 90000u;
	uint32_t m_nAlgorithmRevision = 1u;
};

struct t3dVTDemandVisibilityCell_t
{
	int32_t m_nX = 0, m_nZ = 0;
	uint32_t m_nFirstRecord = 0u;
	uint32_t m_nRecordCount = 0u;
	uint32_t m_nFlags = 0u; // bit0 = partial/uncovered
};

struct t3dVTDemandBakedVisibilityRecord_t
{
	uint64_t m_nStableMaterialKey = 0u;
	uint16_t m_szMipWeight[T3D_VT_DEMAND_BAKED_VISIBILITY_MIP_BINS]{};
	uint32_t m_nSampleCount = 0u;
	uint32_t m_nSourceRevision = 0u;
	uint32_t m_nProbeCount = 0u;
	uint32_t m_nCoverageSampleCount = 0u;
	uint16_t m_nCoverageConfidence = 0u;
	uint16_t m_nNominalMipBase = 0u;
	uint8_t m_nViewBin = 0u;
	uint8_t m_nFlags = 0u;
};

// Cells sort by (floor(x/4),floor(z/4),x,z), including negative coordinates.
// Within each cell records sort by (stableMaterialKey,viewBin). Bundles are
// 4x4 cells with codec 0 (raw) or T03 codec 1 (canonical PackBits).
bool t3d_vt_demand_write_baked_visibility( const t3dVTDemandVisibilityCell_t *pCells, uint32_t nCellCount,
                                           const t3dVTDemandBakedVisibilityRecord_t *pRecords, uint32_t nRecordCount,
                                           const t3dVTDemandVisibilityCookConfig_t& rConfig, std::vector<uint8_t>& rBytes );

// Fully validates every directory/record/hash and requires exact VT package
// and authored-source signatures. T01 does not select this path at runtime.
bool t3d_vt_demand_read( const uint8_t *pBytes, uint64_t nBytes, uint64_t nExpectedPackageSignature, uint64_t nExpectedSourceSignature,
                         std::vector<t3dVTDemandVisibilityCell_t>& rCells, std::vector<t3dVTDemandBakedVisibilityRecord_t>& rRecords,
                         t3dVTDemandVisibilityCookConfig_t *pConfig = nullptr );

#endif
