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
#include <Tools/GeometryCooker/SIRGeometryCooker.h>
#include <Tools/SIRPackCooker/SIRPackCooker.h>

#include <meshoptimizer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

// A page begins with one u32 count and then cluster payloads; each cluster
// has u16 count pairs, f32 global XYZ bounds, then 24 bytes per vertex:
// u16 XYZ, oct-encoded SNORM16 normal and tangent, i8 tangent sign, padding,
// and f32 UV. Indices are u8 triangle-local values. All multi-byte fields are little-endian.
static void sirGeometryWrite32( std::vector<uint8_t>& out, uint32_t n )
{
	const size_t i = out.size();
	out.resize( i + 4 );
	sirPackWrite32( out.data() + i, n );
}
static void sirGeometryWrite16( std::vector<uint8_t>& out, uint16_t n )
{
	const size_t i = out.size();
	out.resize( i + 2 );
	sirPackWrite16( out.data() + i, n );
}
static void sirGeometryWriteFloat( std::vector<uint8_t>& out, float f )
{
	uint32_t n = 0;
	memcpy( &n, &f, 4 );
	sirGeometryWrite32( out, n );
}

static uint16_t sirGeometryQuantize( float f, float fMin, float fMax ) noexcept
{
	if ( fMax <= fMin )
		return 0;
	const double fScaled = ( double )( f - fMin ) * 65535.0 / ( double )( fMax - fMin );
	return ( uint16_t )std::min( 65535.0, std::max( 0.0, std::floor( fScaled + 0.5 ) ) );
}

static int16_t sirGeometrySnorm16( float f ) noexcept
{
	const float fClamped = std::max( -1.0f, std::min( 1.0f, f ) );
	return ( int16_t )std::round( fClamped * 32767.0f );
}

static void sirGeometryWriteOct( std::vector<uint8_t>& out, const float *pVec )
{
	const float fLen = std::abs( pVec[0] ) + std::abs( pVec[1] ) + std::abs( pVec[2] );
	float x = pVec[0] / fLen;
	float y = pVec[1] / fLen;
	if ( pVec[2] < 0.0f ) {
		const float oldX = x;
		x = ( 1.0f - std::abs( y ) ) * ( oldX >= 0.0f ? 1.0f : -1.0f );
		y = ( 1.0f - std::abs( oldX ) ) * ( y >= 0.0f ? 1.0f : -1.0f );
	}
	sirGeometryWrite16( out, ( uint16_t )sirGeometrySnorm16( x ) );
	sirGeometryWrite16( out, ( uint16_t )sirGeometrySnorm16( y ) );
}

struct sirGeometryCookContext_t
{
	const sirGeometryCookInput_t& input;
	float szMin[3];
	float szMax[3];
	std::vector<float> normals;
	std::vector<float> tangents;
};

struct sirGeometryBuildRange_t
{
	const uint32_t *pIndices;
	uint32_t nIndices;
	uint32_t nMaterialId;
	float fError;
};

struct sirGeometryMeshletPlacement_t
{
	uint32_t iMeshlet;
	uint32_t nGroup;
	uint32_t nPage;
};

struct sirGeometryMeshletOutput_t
{
	std::vector<uint8_t>& payload;
	std::vector<sirMeshPackCluster_t>& clusters;
};

struct sirGeometryMeshletData_t
{
	std::vector<meshopt_Meshlet> meshlets;
	std::vector<unsigned int> vertices;
	std::vector<unsigned char> triangles;
	std::vector<uint32_t> indices;
	std::vector<unsigned int> originalVertices;
	std::vector<float> positions;
	float fError = 0.0f;
	uint32_t nMaterialId = 0;
};

// Tangent frame is generated once from source topology, before any meshlet
// split. Every page sees identical vertex attributes at shared boundaries.
static bool sirGeometryBuildTangentFrame( sirGeometryCookContext_t *pCtx )
{
	const sirGeometryCookInput_t& in = pCtx->input;
	pCtx->normals.assign( ( size_t )in.nVertexCount * 3, 0.0f );
	pCtx->tangents.assign( ( size_t )in.nVertexCount * 4, 0.0f );
	std::vector<float> bitangents( ( size_t )in.nVertexCount * 3, 0.0f );
	std::vector<uint8_t> used( in.nVertexCount, 0 );
	for ( uint32_t i = 0; i < in.nIndexCount; i += 3 ) {
		const uint32_t a = in.pIndices[i], b = in.pIndices[i + 1], c = in.pIndices[i + 2];
		float edge1[3], edge2[3], normal[3];
		for ( uint32_t k = 0; k < 3; ++k ) {
			edge1[k] = in.pPositionsXYZ[b * 3 + k] - in.pPositionsXYZ[a * 3 + k];
			edge2[k] = in.pPositionsXYZ[c * 3 + k] - in.pPositionsXYZ[a * 3 + k];
		}
		normal[0] = edge1[1] * edge2[2] - edge1[2] * edge2[1];
		normal[1] = edge1[2] * edge2[0] - edge1[0] * edge2[2];
		normal[2] = edge1[0] * edge2[1] - edge1[1] * edge2[0];
		const float fArea2 = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
		if ( !std::isfinite( fArea2 ) || fArea2 < 1.0e-20f )
			return false;
		float tangent[3] = {};
		float bitangent[3] = {};
		if ( in.pTexcoordsUV != nullptr ) {
			const float du1 = in.pTexcoordsUV[2 * b] - in.pTexcoordsUV[2 * a];
			const float dv1 = in.pTexcoordsUV[2 * b + 1] - in.pTexcoordsUV[2 * a + 1];
			const float du2 = in.pTexcoordsUV[2 * c] - in.pTexcoordsUV[2 * a];
			const float dv2 = in.pTexcoordsUV[2 * c + 1] - in.pTexcoordsUV[2 * a + 1];
			const float determinant = du1 * dv2 - du2 * dv1;
			if ( std::abs( determinant ) > 1.0e-20f ) {
				const float f = 1.0f / determinant;
				for ( uint32_t k = 0; k < 3; ++k ) {
					tangent[k] = ( edge1[k] * dv2 - edge2[k] * dv1 ) * f;
					bitangent[k] = ( edge2[k] * du1 - edge1[k] * du2 ) * f;
				}
			}
		}
		const uint32_t verts[3] = { a, b, c };
		for ( uint32_t v : verts ) {
			used[v] = 1;
			for ( uint32_t k = 0; k < 3; ++k ) {
				pCtx->normals[3 * v + k] += normal[k];
				pCtx->tangents[4 * v + k] += tangent[k];
				bitangents[3 * v + k] += bitangent[k];
			}
		}
	}
	for ( uint32_t v = 0; v < in.nVertexCount; ++v ) {
		float *pN = pCtx->normals.data() + 3 * v;
		float *pT = pCtx->tangents.data() + 4 * v;
		if ( in.pNormalsXYZ != nullptr )
			memcpy( pN, in.pNormalsXYZ + 3 * v, 3 * sizeof( float ) );
		else if ( !used[v] )
			pN[2] = 1.0f;
		const float nLen = std::sqrt( pN[0] * pN[0] + pN[1] * pN[1] + pN[2] * pN[2] );
		if ( !std::isfinite( nLen ) || nLen < 1.0e-12f )
			return false;
		for ( uint32_t k = 0; k < 3; ++k )
			pN[k] /= nLen;
		const float dot = pT[0] * pN[0] + pT[1] * pN[1] + pT[2] * pN[2];
		for ( uint32_t k = 0; k < 3; ++k )
			pT[k] -= dot * pN[k];
		float tLen = std::sqrt( pT[0] * pT[0] + pT[1] * pT[1] + pT[2] * pT[2] );
		if ( !std::isfinite( tLen ) )
			return false;
		if ( tLen < 1.0e-12f ) {
			const float axis[3] = { std::abs( pN[0] ) < 0.9f ? 1.0f : 0.0f, std::abs( pN[0] ) < 0.9f ? 0.0f : 1.0f, 0.0f };
			pT[0] = axis[1] * pN[2] - axis[2] * pN[1];
			pT[1] = axis[2] * pN[0] - axis[0] * pN[2];
			pT[2] = axis[0] * pN[1] - axis[1] * pN[0];
			tLen = std::sqrt( pT[0] * pT[0] + pT[1] * pT[1] + pT[2] * pT[2] );
		}
		for ( uint32_t k = 0; k < 3; ++k )
			pT[k] /= tLen;
		const float cross[3] = { pN[1] * pT[2] - pN[2] * pT[1], pN[2] * pT[0] - pN[0] * pT[2], pN[0] * pT[1] - pN[1] * pT[0] };
		const float handedness = cross[0] * bitangents[3 * v] + cross[1] * bitangents[3 * v + 1] + cross[2] * bitangents[3 * v + 2];
		pT[3] = handedness < 0.0f ? -1.0f : 1.0f;
	}
	return true;
}

static bool sirGeometryBuildMeshlets( const sirGeometryCookInput_t& in, const sirGeometryBuildRange_t& range,
                                      sirGeometryMeshletData_t *pOut )
{
	const uint32_t *pIndices = range.pIndices;
	const uint32_t nIndices = range.nIndices;
	const uint32_t nMaxV = in.nMaxVerticesPerMeshlet;
	const uint32_t nMaxT = in.nMaxTrianglesPerMeshlet;
	const size_t nBound = meshopt_buildMeshletsBound( nIndices, nMaxV, nMaxT );
	if ( nBound == 0 || nBound > SIR_MESH_PACK_MAX_RECORDS )
		return false;
	std::vector<unsigned int> cacheIndices( nIndices );
	meshopt_optimizeVertexCache( cacheIndices.data(), pIndices, nIndices, in.nVertexCount );
	std::vector<unsigned int> remap( in.nVertexCount, ~0U );
	const size_t nUsed = meshopt_optimizeVertexFetchRemap( remap.data(), cacheIndices.data(), nIndices, in.nVertexCount );
	if ( nUsed == 0 || nUsed > in.nVertexCount )
		return false;
	pOut->originalVertices.assign( nUsed, ~0U );
	pOut->positions.resize( nUsed * 3 );
	for ( uint32_t original = 0; original < in.nVertexCount; ++original ) {
		const uint32_t mapped = remap[original];
		if ( mapped != ~0U ) {
			if ( mapped >= nUsed || pOut->originalVertices[mapped] != ~0U )
				return false;
			pOut->originalVertices[mapped] = original;
			for ( uint32_t k = 0; k < 3; ++k )
				pOut->positions[3 * mapped + k] = in.pPositionsXYZ[3 * original + k];
		}
	}
	pOut->indices.resize( nIndices );
	meshopt_remapIndexBuffer( pOut->indices.data(), cacheIndices.data(), nIndices, remap.data() );
	pOut->meshlets.resize( nBound );
	pOut->vertices.resize( nBound * nMaxV );
	pOut->triangles.resize( nBound * nMaxT * 3 );
	const size_t nMeshlets =
	    meshopt_buildMeshlets( pOut->meshlets.data(), pOut->vertices.data(), pOut->triangles.data(), pOut->indices.data(), nIndices,
		                       pOut->positions.data(), nUsed, sizeof( float ) * 3, nMaxV, nMaxT, 0.0f );
	if ( nMeshlets == 0 || nMeshlets > nBound )
		return false;
	pOut->meshlets.resize( nMeshlets );
	pOut->fError = range.fError;
	pOut->nMaterialId = range.nMaterialId;
	for ( const meshopt_Meshlet& meshlet : pOut->meshlets ) {
		if ( meshlet.vertex_count == 0 || meshlet.triangle_count == 0 || meshlet.vertex_count > nMaxV || meshlet.triangle_count > nMaxT )
			return false;
	}
	return true;
}

static bool sirGeometryEncodeMeshlet( const sirGeometryCookContext_t& ctx, const sirGeometryMeshletData_t& data, const meshopt_Meshlet& m,
                                      std::vector<uint8_t>& page )
{
	const sirGeometryCookInput_t& in = ctx.input;
	for ( uint32_t v = 0; v < m.vertex_count; ++v ) {
		if ( data.vertices[m.vertex_offset + v] >= data.originalVertices.size() )
			return false;
	}
	sirGeometryWrite16( page, ( uint16_t )m.vertex_count );
	sirGeometryWrite16( page, ( uint16_t )m.triangle_count );
	sirGeometryWrite32( page, data.nMaterialId );
	for ( float f : ctx.szMin )
		sirGeometryWriteFloat( page, f );
	for ( float f : ctx.szMax )
		sirGeometryWriteFloat( page, f );
	for ( uint32_t v = 0; v < m.vertex_count; ++v ) {
		const uint32_t iVertex = data.originalVertices[data.vertices[m.vertex_offset + v]];
		for ( uint32_t k = 0; k < 3; ++k )
			sirGeometryWrite16( page, sirGeometryQuantize( in.pPositionsXYZ[3 * iVertex + k], ctx.szMin[k], ctx.szMax[k] ) );
		sirGeometryWriteOct( page, ctx.normals.data() + 3 * iVertex );
		sirGeometryWriteOct( page, ctx.tangents.data() + 4 * iVertex );
		page.push_back( ctx.tangents[4 * iVertex + 3] < 0.0f ? 0xff : 1 );
		page.push_back( 0 );
		sirGeometryWriteFloat( page, in.pTexcoordsUV == nullptr ? 0.0f : in.pTexcoordsUV[2 * iVertex] );
		sirGeometryWriteFloat( page, in.pTexcoordsUV == nullptr ? 0.0f : in.pTexcoordsUV[2 * iVertex + 1] );
	}
	for ( uint32_t i = 0; i < m.triangle_count * 3; ++i ) {
		const uint8_t nLocalVertex = data.triangles[m.triangle_offset + i];
		if ( nLocalVertex >= m.vertex_count )
			return false;
		page.push_back( nLocalVertex );
	}
	return true;
}

static bool sirGeometryValidateInput( const sirGeometryCookInput_t& in ) noexcept
{
	if ( in.key.nAssetId == 0 || in.key.nRevision == 0 || in.key.nReserved != 0 || in.nResourceTypeId == 0 || in.nPackageId == 0 ||
	     in.nGeneration == 0 || in.pPositionsXYZ == nullptr || in.pIndices == nullptr || in.nVertexCount < 3 || in.nIndexCount < 3 ||
	     in.nIndexCount % 3 || in.nIndexCount > 3 * SIR_MESH_PACK_MAX_RECORDS || in.nMaterialCount == 0 ||
	     in.nMaterialCount > in.nIndexCount / 3 || in.pMaterials == nullptr || in.nMaxVerticesPerMeshlet < 32 ||
	     in.nMaxVerticesPerMeshlet > 256 || ( in.nMaxVerticesPerMeshlet & 3 ) || in.nMaxTrianglesPerMeshlet == 0 ||
	     in.nMaxTrianglesPerMeshlet > 512 || ( in.nMaxTrianglesPerMeshlet & 3 ) || in.nTargetRootTriangles == 0 ||
	     !std::isfinite( in.fMaxSimplificationError ) || in.fMaxSimplificationError <= 0.0f )
		return false;
	for ( uint64_t i = 0; i < ( uint64_t )in.nVertexCount * 3; ++i ) {
		if ( !std::isfinite( in.pPositionsXYZ[i] ) || ( in.pNormalsXYZ != nullptr && !std::isfinite( in.pNormalsXYZ[i] ) ) )
			return false;
	}
	if ( in.pTexcoordsUV != nullptr ) {
		for ( uint64_t i = 0; i < ( uint64_t )in.nVertexCount * 2; ++i )
			if ( !std::isfinite( in.pTexcoordsUV[i] ) )
				return false;
	}
	for ( uint32_t i = 0; i < in.nIndexCount; i += 3 ) {
		const uint32_t a = in.pIndices[i], b = in.pIndices[i + 1], c = in.pIndices[i + 2];
		if ( a >= in.nVertexCount || b >= in.nVertexCount || c >= in.nVertexCount || a == b || b == c || a == c )
			return false;
	}
	uint32_t nEnd = 0;
	uint32_t nPriorMaterial = 0;
	for ( uint32_t i = 0; i < in.nMaterialCount; ++i ) {
		const sirGeometryCookMaterial_t& mat = in.pMaterials[i];
		if ( mat.nFirstIndex != nEnd || mat.nIndexCount == 0 || mat.nIndexCount % 3 || mat.nIndexCount > in.nIndexCount - nEnd ||
		     ( i && mat.nMaterialId <= nPriorMaterial ) )
			return false;
		nEnd += mat.nIndexCount;
		nPriorMaterial = mat.nMaterialId;
	}
	return nEnd == in.nIndexCount;
}

static bool sirGeometryAppendCluster( const sirGeometryCookContext_t& ctx, const sirGeometryMeshletData_t& data,
                                      const sirGeometryMeshletPlacement_t& placement, sirGeometryMeshletOutput_t output )
{
	const meshopt_Meshlet& m = data.meshlets[placement.iMeshlet];
	const uint64_t nStart = output.payload.size();
	if ( !sirGeometryEncodeMeshlet( ctx, data, m, output.payload ) )
		return false;
	sirMeshPackCluster_t cluster = {};
	cluster.nGroup = placement.nGroup;
	cluster.nPage = placement.nPage;
	cluster.nVertexCount = m.vertex_count;
	cluster.nTriangleCount = m.triangle_count;
	cluster.nByteOffset = nStart;
	cluster.nByteCount = output.payload.size() - nStart;
	cluster.fError = data.fError;
	output.clusters.push_back( cluster );
	return true;
}

// Root -> per-material middle -> complete leaf meshlet groups. Each page
// depends on its ancestor page, keeping fallback chains acyclic. The global
// quantization range preserves shared vertex positions across page boundaries.
sirPackResult_t SIRGeometryCooker::cook( const sirGeometryCookInput_t& in, sirGeometryCookOutput_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || !sirGeometryValidateInput( in ) )
		return SIR_PACK_INVALID_ARGUMENT;
	try {
		sirGeometryCookContext_t ctx = { in, {}, {}, {}, {} };
		for ( uint32_t k = 0; k < 3; ++k ) {
			ctx.szMin[k] = std::numeric_limits<float>::max();
			ctx.szMax[k] = -std::numeric_limits<float>::max();
		}
		for ( uint32_t v = 0; v < in.nVertexCount; ++v ) {
			for ( uint32_t k = 0; k < 3; ++k ) {
				ctx.szMin[k] = std::min( ctx.szMin[k], in.pPositionsXYZ[3 * v + k] );
				ctx.szMax[k] = std::max( ctx.szMax[k], in.pPositionsXYZ[3 * v + k] );
			}
		}
		for ( uint32_t k = 0; k < 3; ++k ) {
			if ( ( double )ctx.szMax[k] - ( double )ctx.szMin[k] > std::numeric_limits<float>::max() )
				return SIR_PACK_INVALID_FORMAT;
		}
		if ( !sirGeometryBuildTangentFrame( &ctx ) )
			return SIR_PACK_INVALID_FORMAT;
		std::vector<sirGeometryMeshletData_t> root;
		std::vector<sirGeometryMeshletData_t> middle;
		std::vector<sirGeometryMeshletData_t> leaves;
		root.reserve( in.nMaterialCount );
		middle.reserve( in.nMaterialCount );
		leaves.reserve( in.nMaterialCount );
		float fMaxRootError = 0;
		uint32_t nLeafMeshlets = 0;
		for ( uint32_t i = 0; i < in.nMaterialCount; ++i ) {
			const sirGeometryCookMaterial_t& mat = in.pMaterials[i];
			const uint32_t *pMaterialIndices = in.pIndices + mat.nFirstIndex;
			sirGeometryMeshletData_t full;
			if ( !sirGeometryBuildMeshlets( in, { pMaterialIndices, mat.nIndexCount, mat.nMaterialId, 0 }, &full ) )
				return SIR_PACK_INVALID_FORMAT;
			if ( full.meshlets.size() > SIR_MESH_PACK_MAX_RECORDS - nLeafMeshlets )
				return SIR_PACK_OUT_OF_CAPACITY;
			nLeafMeshlets += ( uint32_t )full.meshlets.size();
			leaves.push_back( std::move( full ) );
			std::vector<uint32_t> simplified( mat.nIndexCount );
			const uint32_t nTarget = std::max(
			    3U, ( uint32_t )std::min( ( uint64_t )mat.nIndexCount, ( uint64_t )in.nTargetRootTriangles * 3 / in.nMaterialCount ) );
			float fError = 0;
			size_t nSimplified = meshopt_simplify( simplified.data(), pMaterialIndices, mat.nIndexCount, in.pPositionsXYZ, in.nVertexCount,
			                                       sizeof( float ) * 3, std::max( 3U, ( nTarget / 3 ) * 3 ), in.fMaxSimplificationError,
			                                       meshopt_SimplifyLockBorder, &fError );
			if ( nSimplified == 0 || nSimplified % 3 ) {
				nSimplified = mat.nIndexCount;
				memcpy( simplified.data(), pMaterialIndices, nSimplified * sizeof( uint32_t ) );
				fError = 0;
			}
			if ( !std::isfinite( fError ) || fError < 0.0f )
				return SIR_PACK_INVALID_FORMAT;
			sirGeometryMeshletData_t coarse;
			if ( !sirGeometryBuildMeshlets( in, { simplified.data(), ( uint32_t )nSimplified, mat.nMaterialId, fError }, &coarse ) )
				return SIR_PACK_INVALID_FORMAT;
			fMaxRootError = std::max( fMaxRootError, fError );
			root.push_back( std::move( coarse ) );
			const uint32_t nMidTarget = std::min( mat.nIndexCount, std::max( ( uint32_t )nSimplified * 2, ( mat.nIndexCount / 12 ) * 3 ) );
			float fMidError = 0;
			size_t nMid = meshopt_simplify( simplified.data(), pMaterialIndices, mat.nIndexCount, in.pPositionsXYZ, in.nVertexCount,
			                                sizeof( float ) * 3, std::max( 3U, ( nMidTarget / 3 ) * 3 ), in.fMaxSimplificationError * 0.5f,
			                                meshopt_SimplifyLockBorder, &fMidError );
			if ( nMid == 0 || nMid % 3 ) {
				nMid = mat.nIndexCount;
				memcpy( simplified.data(), pMaterialIndices, nMid * sizeof( uint32_t ) );
				fMidError = 0;
			}
			if ( !std::isfinite( fMidError ) || fMidError < 0.0f )
				return SIR_PACK_INVALID_FORMAT;
			sirGeometryMeshletData_t middleData;
			if ( !sirGeometryBuildMeshlets( in, { simplified.data(), ( uint32_t )nMid, mat.nMaterialId, fMidError }, &middleData ) )
				return SIR_PACK_INVALID_FORMAT;
			fMaxRootError = std::max( fMaxRootError, fMidError );
			middle.push_back( std::move( middleData ) );
		}
		if ( nLeafMeshlets == 0 || ( uint64_t )nLeafMeshlets + in.nMaterialCount + 1 > SIR_MESH_PACK_MAX_RECORDS )
			return SIR_PACK_OUT_OF_CAPACITY;
		std::vector<sirMeshPackGroup_t> groups;
		std::vector<sirMeshPackCluster_t> clusters;
		std::vector<sirMeshPackPage_t> pages;
		std::vector<std::vector<uint8_t>> payloads;
		groups.reserve( ( size_t )nLeafMeshlets + in.nMaterialCount + 1 );
		clusters.reserve( ( size_t )nLeafMeshlets * 3 );
		payloads.emplace_back();
		sirGeometryWrite32( payloads[0], 0 );
		sirMeshPackGroup_t coarseGroup = {};
		coarseGroup.nParent = SIR_MESH_PACK_NO_PARENT;
		coarseGroup.nFirstChild = 1;
		coarseGroup.nChildCount = in.nMaterialCount;
		coarseGroup.nMaterialId = 0xffffffffU;
		coarseGroup.nLevel = 2;
		coarseGroup.fError = std::max( 0.000001f, fMaxRootError );
		groups.push_back( coarseGroup );
		for ( const sirGeometryMeshletData_t& data : root ) {
			for ( uint32_t m = 0; m < data.meshlets.size(); ++m ) {
				if ( !sirGeometryAppendCluster( ctx, data, { m, 0, 0 }, { payloads[0], clusters } ) )
					return SIR_PACK_INVALID_FORMAT;
			}
		}
		groups[0].nClusterCount = ( uint32_t )clusters.size();
		sirPackWrite32( payloads[0].data(), groups[0].nClusterCount );
		sirMeshPackPage_t rootPage = {};
		rootPage.nParent = SIR_MESH_PACK_NO_PARENT;
		rootPage.nClusterCount = groups[0].nClusterCount;
		rootPage.nChunkId = 1;
		rootPage.nByteCount = payloads[0].size();
		pages.push_back( rootPage );
		uint32_t nLeafGroupCursor = 1 + in.nMaterialCount;
		for ( uint32_t i = 0; i < middle.size(); ++i ) {
			const sirGeometryMeshletData_t& data = middle[i];
			const uint32_t nGroup = ( uint32_t )groups.size();
			const uint32_t nPage = ( uint32_t )pages.size();
			sirMeshPackGroup_t g = {};
			g.nParent = 0;
			g.nFirstCluster = ( uint32_t )clusters.size();
			g.nClusterCount = ( uint32_t )data.meshlets.size();
			g.nFirstChild = nLeafGroupCursor;
			g.nChildCount = ( uint32_t )leaves[i].meshlets.size();
			g.nMaterialId = data.nMaterialId;
			g.nLevel = 1;
			g.nPage = nPage;
			g.fError = data.fError;
			groups.push_back( g );
			payloads.emplace_back();
			sirGeometryWrite32( payloads.back(), g.nClusterCount );
			for ( uint32_t m = 0; m < data.meshlets.size(); ++m ) {
				if ( !sirGeometryAppendCluster( ctx, data, { m, nGroup, nPage }, { payloads.back(), clusters } ) )
					return SIR_PACK_INVALID_FORMAT;
			}
			sirMeshPackPage_t page = {};
			page.nParent = 0;
			page.nFirstCluster = g.nFirstCluster;
			page.nClusterCount = g.nClusterCount;
			page.nChunkId = nPage + 1;
			page.nByteCount = payloads.back().size();
			pages.push_back( page );
			nLeafGroupCursor += g.nChildCount;
		}
		// Pack sequential meshlets of the same material and parent into
		// bounded pages. A page is a residency unit; each leaf remains a
		// separately addressable complete group in the universal frame plan.
		static constexpr uint64_t SIR_GEOMETRY_LEAF_PAGE_TARGET_BYTES = 64ULL * 1024;
		for ( uint32_t material = 0; material < leaves.size(); ++material ) {
			const sirGeometryMeshletData_t& data = leaves[material];
			uint32_t nPage = SIR_MESH_PACK_NO_PARENT;
			for ( uint32_t m = 0; m < data.meshlets.size(); ++m ) {
				const meshopt_Meshlet& meshlet = data.meshlets[m];
				const uint64_t nClusterBytes = 32ULL + 24ULL * meshlet.vertex_count + 3ULL * meshlet.triangle_count;
				if ( nPage == SIR_MESH_PACK_NO_PARENT || payloads[nPage].size() + nClusterBytes > SIR_GEOMETRY_LEAF_PAGE_TARGET_BYTES ) {
					if ( pages.size() >= SIR_MESH_PACK_MAX_RECORDS )
						return SIR_PACK_OUT_OF_CAPACITY;
					nPage = ( uint32_t )pages.size();
					payloads.emplace_back();
					sirGeometryWrite32( payloads.back(), 0 );
					sirMeshPackPage_t page = {};
					page.nParent = material + 1;
					page.nFirstCluster = ( uint32_t )clusters.size();
					page.nChunkId = nPage + 1;
					pages.push_back( page );
				}
				const uint32_t nGroup = ( uint32_t )groups.size();
				sirMeshPackGroup_t g = {};
				g.nParent = material + 1;
				g.nFirstCluster = ( uint32_t )clusters.size();
				g.nClusterCount = 1;
				g.nMaterialId = data.nMaterialId;
				g.nPage = nPage;
				g.fError = 0;
				groups.push_back( g );
				if ( !sirGeometryAppendCluster( ctx, data, { m, nGroup, nPage }, { payloads[nPage], clusters } ) )
					return SIR_PACK_INVALID_FORMAT;
				++pages[nPage].nClusterCount;
				pages[nPage].nByteCount = payloads[nPage].size();
				sirPackWrite32( payloads[nPage].data(), pages[nPage].nClusterCount );
			}
		}
		if ( pages.size() > SIR_MESH_PACK_MAX_RECORDS || clusters.size() > SIR_MESH_PACK_MAX_RECORDS )
			return SIR_PACK_OUT_OF_CAPACITY;
		for ( uint32_t i = 0; i < pages.size(); ++i ) {
			uint8_t digest[32] = {};
			sirPackSha256( payloads[i].data(), payloads[i].size(), digest );
			memcpy( pages[i].szDigest, digest, 16 );
		}
		std::vector<uint8_t> collision;
		sirMeshPackCollision_t collisionRecord = {};
		if ( in.bCookCollision ) {
			// Portable source in meters with CCW winding and per-triangle
			// material attribution. No backend-dependent BVH or native shape.
			sirGeometryWrite32( collision, 0x43524953U );
			sirGeometryWrite32( collision, in.nVertexCount );
			sirGeometryWrite32( collision, in.nIndexCount );
			sirGeometryWrite32( collision, 1 );
			for ( uint64_t i = 0; i < ( uint64_t )in.nVertexCount * 3; ++i )
				sirGeometryWriteFloat( collision, in.pPositionsXYZ[i] );
			for ( uint32_t i = 0; i < in.nIndexCount; ++i )
				sirGeometryWrite32( collision, in.pIndices[i] );
			for ( uint32_t m = 0; m < in.nMaterialCount; ++m ) {
				const sirGeometryCookMaterial_t& mat = in.pMaterials[m];
				for ( uint32_t i = 0; i < mat.nIndexCount / 3; ++i )
					sirGeometryWrite32( collision, mat.nMaterialId );
			}
			collisionRecord.nProfile = 1;
			collisionRecord.nFlags = SIR_MESH_PACK_COLLISION_SOURCE_FLAGS;
			collisionRecord.nVertexCount = in.nVertexCount;
			collisionRecord.nIndexCount = in.nIndexCount;
			collisionRecord.nChunkId = ( uint32_t )pages.size() + 1;
			collisionRecord.nRevision = in.key.nRevision;
		}
		sirMeshPackDesc_t desc = {};
		desc.key = in.key;
		desc.pGroups = groups.data();
		desc.pClusters = clusters.data();
		desc.pPages = pages.data();
		desc.pCollisions = in.bCookCollision ? &collisionRecord : nullptr;
		desc.nGroups = ( uint32_t )groups.size();
		desc.nClusters = ( uint32_t )clusters.size();
		desc.nPages = ( uint32_t )pages.size();
		desc.nCollisions = in.bCookCollision ? 1 : 0;
		desc.nVertexCount = in.nVertexCount;
		desc.nTriangleCount = in.nIndexCount / 3;
		std::vector<uint8_t> manifest( ( size_t )SIRMeshPackSchema::requiredBytes( desc ) );
		if ( manifest.empty() || SIRMeshPackSchema::write( desc, manifest.data(), manifest.size() ) != SIR_MESH_PACK_OK )
			return SIR_PACK_INVALID_FORMAT;
		SIRMeshPackSchema view;
		if ( view.open( manifest.data(), manifest.size() ) != SIR_MESH_PACK_OK )
			return SIR_PACK_INVALID_FORMAT;
		// SIRPack chunk 0 contains the manifest, chunks 1..N correspond to
		// stable page ids, and the optional final chunk carries collision.
		std::vector<sirPackCookChunk_t> chunks;
		chunks.reserve( payloads.size() + 2 );
		chunks.push_back( { 0, 0, manifest.data(), manifest.size(), 16 } );
		for ( uint32_t i = 0; i < payloads.size(); ++i )
			chunks.push_back( { i + 1, 0, payloads[i].data(), payloads[i].size(), 16 } );
		if ( in.bCookCollision )
			chunks.push_back( { ( uint32_t )chunks.size(), 0, collision.data(), collision.size(), 16 } );
		sirPackCookResource_t resource = {};
		resource.key = { in.key.nAssetId, in.nResourceTypeId, in.key.nVariant };
		resource.nRevision = in.key.nRevision;
		resource.nFormatId = SIR_MESH_PACK_FORMAT_ID;
		resource.nChunkCount = ( uint32_t )chunks.size();
		resource.nSchemaVersion = SIR_MESH_PACK_VERSION;
		resource.nKind = SIR_RESOURCE_KIND_GEOMETRY;
		std::vector<uint8_t> shard;
		uint64_t nShardCapacity = SIR_PACK_SHARD_HEADER_BYTES;
		for ( const sirPackCookChunk_t& c : chunks ) {
			nShardCapacity = ( nShardCapacity + 15 ) & ~15ULL;
			nShardCapacity += c.nBytes;
		}
		shard.resize( ( size_t )nShardCapacity );
		sirPackCookShard_t shardDesc = { shard.data(), shard.size() };
		sirPackCookDesc_t pack = {};
		pack.nPackageId = in.nPackageId;
		pack.nGeneration = in.nGeneration;
		pack.pResources = &resource;
		pack.nResourceCount = 1;
		pack.pChunks = chunks.data();
		pack.nChunkCount = ( uint32_t )chunks.size();
		pack.pShards = &shardDesc;
		pack.nShardCount = 1;
		std::vector<uint8_t> catalog( ( size_t )SIRPackCooker::requiredCatalogBytes( pack ) );
		std::vector<uint8_t> scratch( ( size_t )SIRPackCooker::requiredScratch( pack ) );
		pack.pScratch = scratch.data();
		pack.nScratchBytes = scratch.size();
		const sirPackResult_t result = SIRPackCooker::build( pack, catalog.data(), catalog.size() );
		if ( result != SIR_PACK_OK )
			return result;
		pOutput->catalog.swap( catalog );
		pOutput->shard.swap( shard );
		pOutput->manifest.swap( manifest );
		pOutput->pages.swap( payloads );
		return SIR_PACK_OK;
	} catch ( const std::bad_alloc& ) {
		return SIR_PACK_OUT_OF_CAPACITY;
	} catch ( ... ) {
		return SIR_PACK_INVALID_FORMAT;
	}
}
