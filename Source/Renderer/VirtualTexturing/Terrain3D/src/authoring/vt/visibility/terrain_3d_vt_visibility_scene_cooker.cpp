#include "authoring/vt/visibility/terrain_3d_vt_visibility_scene_cooker.h"
#include "authoring/vt/visibility/terrain_3d_vt_visibility_baker.h"
#include "data/model/terrain_3d_data.h"
#include "data/model/terrain_3d_region.h"
#include "data/persistence/terrain_3d_region_codec.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.h"
#include "Terrain3DConstants.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include <iterator>
#include <algorithm>
#include <math.h>
#include <string.h>

using namespace godot;

namespace
{
static uint64_t hashAppend( uint64_t hash, const void *const bytes, const size_t n )
{
	const uint8_t *const p = ( const uint8_t * )bytes;
	for ( size_t i = 0u; i < n; ++i )
		hash = ( hash ^ p[i] ) * 1099511628211ull;
	return hash;
}
static uint32_t hashRevision( const uint64_t source )
{
	const uint32_t value = ( uint32_t )( source ^ ( source >> 32u ) );
	return value == 0u ? 1u : value;
}
static bool validHeight( const Ref<Image>& img )
{
	return img.is_valid() && img->get_width() == T3D_REGION_SIZE && img->get_height() == T3D_REGION_SIZE &&
	       ( img->get_format() == Image::FORMAT_R16 || img->get_format() == Image::FORMAT_RF );
}
static bool validControl( const Ref<Image>& img )
{
	return img.is_valid() && img->get_width() == T3D_REGION_SIZE && img->get_height() == T3D_REGION_SIZE &&
	       img->get_format() == Image::FORMAT_RF;
}
static float sampleHeight( const uint8_t *const data, const Image::Format format, const uint32_t x, const uint32_t z )
{
	return t3dRegionCodecReadHeightBytes( data, ( int )( z * 128u + x ), format );
}
}

bool t3d_vt_visibility_cook_cold( const Terrain3DData& data, const uint64_t *const keys, const uint32_t nKeys,
                                  const uint64_t packageSignature, const uint64_t assetHash, PackedByteArray& output,
                                  t3dVTVisibilityAggregateStats_t& outStats, String& error )
{
	output.clear();
	outStats = t3dVTVisibilityAggregateStats_t{};
	error = String();
	if ( !keys || !nKeys || nKeys > T3D_VT_VISIBILITY_MAX_MATERIALS || !packageSignature || !assetHash ) {
		error = "Invalid final VT stable-key table for GPU histogram cook";
		return false;
	}
	for ( uint32_t i = 0u; i < nKeys; ++i )
		if ( !keys[i] || ( i && keys[i - 1u] >= keys[i] ) )
			return false;
	const TypedArray<Vector2i> authored = data.getRegionLocations();
	std::vector<Vector2i> regions;
	regions.reserve( authored.size() );
	for ( int i = 0; i < authored.size(); ++i )
		regions.push_back( authored[i] );
	std::sort( regions.begin(), regions.end(), []( const Vector2i& a, const Vector2i& b ) { return a.x != b.x ? a.x < b.x : a.y < b.y; } );
	if ( regions.empty() || regions.size() > T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_CELLS ) {
		error = "No authored cells or visibility cook exceeds 4096-cell limit";
		return false;
	}
	std::vector<t3dVTVisibilityStaticVertex_t> vertices;
	std::vector<t3dVTVisibilityStaticTriangle_t> triangles;
	std::vector<t3dVTVisibilityProbeInput_t> probes;
	vertices.reserve( regions.size() * 289u );
	triangles.reserve( regions.size() * 512u );
	probes.reserve( regions.size() );
	uint64_t sourceSignature = 14695981039346656037ull;
	sourceSignature = hashAppend( sourceSignature, &packageSignature, sizeof( packageSignature ) );
	for ( uint32_t i = 0u; i < nKeys; ++i )
		sourceSignature = hashAppend( sourceSignature, &keys[i], sizeof( keys[i] ) );
	for ( const auto& coord : regions ) {
		const Ref<Terrain3DRegion> region = data.getRegion( coord );
		const Ref<Image> height = region.is_valid() ? region->getHeightMap() : Ref<Image>();
		const Ref<Image> control = region.is_valid() ? region->getControlMap() : Ref<Image>();
		if ( !validHeight( height ) || !validControl( control ) ) {
			error = "Authored height/control unavailable for GPU visibility; using v2 presence index";
			return false;
		}
		const PackedByteArray heightBytes = height->get_data();
		const PackedByteArray controlBytes = control->get_data();
		if ( heightBytes.size() != ( int64_t )t3dRegionCodecExpectedHeightByteCount( 128, height->get_format() ) ||
		     controlBytes.size() != 128 * 128 * 4 )
			return false;
		const uint8_t *const h = heightBytes.ptr(), *const c = controlBytes.ptr();
		const uint64_t previous = sourceSignature;
		sourceSignature = hashAppend( sourceSignature, &coord.x, sizeof( coord.x ) );
		sourceSignature = hashAppend( sourceSignature, &coord.y, sizeof( coord.y ) );
		sourceSignature = hashAppend( sourceSignature, h, ( size_t )heightBytes.size() );
		sourceSignature = hashAppend( sourceSignature, c, ( size_t )controlBytes.size() );
		uint64_t cellHash = hashAppend( previous, h, ( size_t )heightBytes.size() );
		cellHash = hashAppend( cellHash, c, ( size_t )controlBytes.size() );
		const uint32_t revision = hashRevision( cellHash );
		// A 16x16 quad proxy bounds offline GPU work. It is deliberately
		// PARTIAL because control overlays, 128x128 displacement and material
		// UV transforms are not represented exactly in this first histogram.
		const uint32_t firstVertex = ( uint32_t )vertices.size();
		for ( uint32_t iz = 0u; iz <= 16u; ++iz ) {
			for ( uint32_t ix = 0u; ix <= 16u; ++ix ) {
				const uint32_t sx = std::min( 127u, ix * 8u );
				const uint32_t sz = std::min( 127u, iz * 8u );
				const float y = sampleHeight( h, height->get_format(), sx, sz );
				if ( !isfinite( y ) )
					return false;
				t3dVTVisibilityStaticVertex_t v{};
				v.m_fWorldX = ( float )( coord.x * 128 + ( int )ix * 8 );
				v.m_fWorldY = y;
				v.m_fWorldZ = ( float )( coord.y * 128 + ( int )iz * 8 );
				v.m_fU = ( float )ix / 16.0f;
				v.m_fV = ( float )iz / 16.0f;
				vertices.push_back( v );
			}
		}
		const uint32_t firstTriangle = ( uint32_t )triangles.size();
		for ( uint32_t iz = 0u; iz < 16u; ++iz ) {
			for ( uint32_t ix = 0u; ix < 16u; ++ix ) {
				uint32_t word = 0u;
				const uint32_t sample = ( iz * 8u + 4u ) * 128u + ix * 8u + 4u;
				memcpy( &word, c + ( size_t )sample * 4u, sizeof( word ) );
				if ( t3dIsHole( word ) || t3dIsAuto( word ) )
					continue;
				const uint64_t stable = t3d_vt_terrain_material_stable_key( t3dGetBase( word ) );
				const uint64_t *const key = std::lower_bound( keys, keys + nKeys, stable );
				if ( key == keys + nKeys || *key != stable )
					continue;
				const uint32_t m = ( uint32_t )( key - keys );
				const uint32_t a = firstVertex + iz * 17u + ix;
				const uint32_t b = a + 1u, d = a + 17u, e = d + 1u;
				t3dVTVisibilityStaticTriangle_t one{};
				one.m_nMaterialKeyIndex = m;
				one.m_nIndices[0] = a;
				one.m_nIndices[1] = d;
				one.m_nIndices[2] = b;
				triangles.push_back( one );
				one.m_nIndices[0] = b;
				one.m_nIndices[1] = d;
				one.m_nIndices[2] = e;
				triangles.push_back( one );
			}
		}
		const uint32_t nTriangles = ( uint32_t )triangles.size() - firstTriangle;
		if ( !nTriangles )
			continue; // an entirely unknown cell stays v2/reactive
		std::vector<t3dVTVisibilityProbeInput_t> candidates;
		if ( !Terrain3DVTVisibilityBaker::selectCellProbes( vertices.data(), ( uint32_t )vertices.size(), triangles.data(), firstTriangle,
		                                                    nTriangles, coord.x, coord.y, revision, 128u, candidates ) ||
		     candidates.empty() )
			continue;
		probes.push_back( candidates[0] ); // bounded one surface-aware probe/cell
	}
	if ( probes.empty() || triangles.size() > 1048576u || vertices.size() > 1048576u ) {
		error = "Visibility geometry has no valid probe surface or exceeds native bake bounds";
		return false;
	}
	if ( sourceSignature == 0u )
		sourceSignature = 1u;
	t3dVTDemandVisibilityCookConfig_t config;
	config.m_nPackageSignature = packageSignature;
	config.m_nAssetIdentityHash = assetHash;
	config.m_nSourceSignature = sourceSignature;
	config.m_nNominalTextureSize = 4096u;
	config.m_nNominalRenderSize = 128u;
	config.m_nFovMilliDegrees = 90000u;
	config.m_nAlgorithmRevision = 3u;
	const uint64_t recipe[] = { 3u, 16u, 128u, 1u, config.m_nNominalTextureSize, config.m_nNominalRenderSize, config.m_nFovMilliDegrees };
	config.m_nBakeConfigSignature = hashAppend( 14695981039346656037ull, recipe, sizeof( recipe ) );
	std::vector<t3dVTVisibilityProbeResult_t> raw;
	raw.reserve( probes.size() );
	Terrain3DVTVisibilityBaker baker;
	for ( size_t first = 0u; first < probes.size(); first += 32u ) {
		t3dVTVisibilityStaticSceneView_t scene;
		scene.m_pVertices = vertices.data();
		scene.m_nVertexCount = ( uint32_t )vertices.size();
		scene.m_pTriangles = triangles.data();
		scene.m_nTriangleCount = ( uint32_t )triangles.size();
		scene.m_pMaterialKeys = keys;
		scene.m_nMaterialCount = nKeys;
		scene.m_pProbes = probes.data() + first;
		scene.m_nProbeCount = ( uint32_t )std::min( ( size_t )32u, probes.size() - first );
		scene.m_nSourceSignature = sourceSignature;
		std::vector<t3dVTVisibilityProbeResult_t> batch;
		t3dVTVisibilityBakeStats_t gpuStats;
		if ( !baker.bake( scene, config, batch, gpuStats ) ) {
			error = "Local RenderingDevice visibility probe unavailable/failed; using v2 presence index";
			return false;
		}
		raw.insert( raw.end(), std::make_move_iterator( batch.begin() ), std::make_move_iterator( batch.end() ) );
	}
	std::vector<t3dVTDemandVisibilityCell_t> cells;
	std::vector<t3dVTDemandBakedVisibilityRecord_t> records;
	if ( !Terrain3DVTVisibilityAggregator::reduce( raw.data(), ( uint32_t )raw.size(), keys, nKeys, 64u, true, cells, records,
	                                               outStats ) ) {
		error = "Visibility histogram aggregation produced no safe cell coverage";
		return false;
	}
	std::vector<uint8_t> bytes;
	if ( !t3d_vt_demand_write_baked_visibility( cells.data(), ( uint32_t )cells.size(), records.data(), ( uint32_t )records.size(), config,
	                                            bytes ) ) {
		error = "T3VD v1 byte-bounded bundle encoding failed";
		return false;
	}
	if ( output.resize( ( int64_t )bytes.size() ) != OK )
		return false;
	memcpy( output.ptrw(), bytes.data(), bytes.size() );
	return true;
}
