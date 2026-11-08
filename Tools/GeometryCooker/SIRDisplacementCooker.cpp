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
#include <Tools/GeometryCooker/SIRDisplacementCooker.h>

#include <Resources/SIRPackFormat.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <new>

struct sirDisplacementVertex_t
{
	float base[3];
	float normal[3];
	float uv[2];
	float position[3];
	uint32_t nWeld;
};

struct sirDisplacementTriangle_t
{
	uint32_t vertices[3];
	uint32_t material;
};

static bool sirDisplacementNormalize( float *pVector ) noexcept
{
	const double length2 = ( double )pVector[0] * pVector[0] + ( double )pVector[1] * pVector[1] + ( double )pVector[2] * pVector[2];
	if ( !std::isfinite( length2 ) || length2 < 1.0e-24 )
		return false;
	const float inverse = ( float )( 1.0 / std::sqrt( length2 ) );
	for ( uint32_t i = 0; i < 3; ++i )
		pVector[i] *= inverse;
	return true;
}

static float sirDisplacementCoordinate( float f, sirDisplacementSampler_t sampler ) noexcept
{
	if ( sampler == SIR_DISPLACEMENT_REPEAT ) {
		f -= std::floor( f );
		return f;
	}
	return std::max( 0.0f, std::min( 1.0f, f ) );
}

static float sirDisplacementSample( const sirDisplacementImage_t& image, sirDisplacementSampler_t sampler, const float *pUV ) noexcept
{
	const float u = sirDisplacementCoordinate( pUV[0], sampler );
	const float v = sirDisplacementCoordinate( pUV[1], sampler );
	const float x = u * ( sampler == SIR_DISPLACEMENT_REPEAT ? image.nWidth : image.nWidth - 1 );
	const float y = v * ( sampler == SIR_DISPLACEMENT_REPEAT ? image.nHeight : image.nHeight - 1 );
	const uint32_t ix = ( uint32_t )x;
	const uint32_t iy = ( uint32_t )y;
	const uint32_t nextX = sampler == SIR_DISPLACEMENT_REPEAT ? ( ix + 1 ) % image.nWidth : std::min( ix + 1, image.nWidth - 1 );
	const uint32_t nextY = sampler == SIR_DISPLACEMENT_REPEAT ? ( iy + 1 ) % image.nHeight : std::min( iy + 1, image.nHeight - 1 );
	const float fx = x - ix;
	const float fy = y - iy;
	const float *h = image.pHeights;
	const float a = h[( size_t )iy * image.nWidth + ix];
	const float b = h[( size_t )iy * image.nWidth + nextX];
	const float c = h[( size_t )nextY * image.nWidth + ix];
	const float d = h[( size_t )nextY * image.nWidth + nextX];
	return ( a * ( 1.0f - fx ) + b * fx ) * ( 1.0f - fy ) + ( c * ( 1.0f - fx ) + d * fx ) * fy;
}

static bool sirDisplacementApply( sirDisplacementVertex_t *pVertex, const sirDisplacementImage_t& image,
                                  const sirDisplacementRecipe_t& recipe ) noexcept
{
	const double height = ( double )sirDisplacementSample( image, recipe.sampler, pVertex->uv );
	const double distance = ( height - ( double )recipe.fMidpoint ) * ( double )recipe.fAmplitude;
	for ( uint32_t i = 0; i < 3; ++i ) {
		const double position = ( double )pVertex->base[i] + distance * ( double )pVertex->normal[i];
		if ( !std::isfinite( position ) || std::abs( position ) > std::numeric_limits<float>::max() )
			return false;
		pVertex->position[i] = ( float )position;
	}
	return true;
}

static bool sirDisplacementInterpolate( const sirDisplacementVertex_t& a, const sirDisplacementVertex_t& b,
                                        sirDisplacementVertex_t *pVertex ) noexcept
{
	for ( uint32_t i = 0; i < 3; ++i ) {
		pVertex->base[i] = 0.5f * a.base[i] + 0.5f * b.base[i];
		pVertex->normal[i] = 0.5f * a.normal[i] + 0.5f * b.normal[i];
	}
	for ( uint32_t i = 0; i < 2; ++i )
		pVertex->uv[i] = 0.5f * a.uv[i] + 0.5f * b.uv[i];
	return sirDisplacementNormalize( pVertex->normal );
}

static bool sirDisplacementCentroid( const sirDisplacementVertex_t& a, const sirDisplacementVertex_t& b, const sirDisplacementVertex_t& c,
                                     sirDisplacementVertex_t *pVertex ) noexcept
{
	for ( uint32_t i = 0; i < 3; ++i ) {
		pVertex->base[i] = ( a.base[i] + b.base[i] + c.base[i] ) / 3.0f;
		pVertex->normal[i] = ( a.normal[i] + b.normal[i] + c.normal[i] ) / 3.0f;
	}
	for ( uint32_t i = 0; i < 2; ++i )
		pVertex->uv[i] = ( a.uv[i] + b.uv[i] + c.uv[i] ) / 3.0f;
	return sirDisplacementNormalize( pVertex->normal );
}

static double sirDisplacementError( const sirDisplacementVertex_t& a, const sirDisplacementVertex_t& b,
                                    const sirDisplacementVertex_t& sampled ) noexcept
{
	double e = 0;
	for ( uint32_t i = 0; i < 3; ++i ) {
		const double delta = ( double )sampled.position[i] - 0.5 * ( ( double )a.position[i] + b.position[i] );
		e += delta * delta;
	}
	return e;
}

static uint64_t sirDisplacementEdge( uint32_t a, uint32_t b ) noexcept
{
	return ( ( uint64_t )std::min( a, b ) << 32 ) | std::max( a, b );
}

// Weld positions with identical source coordinates across independent OBJ
// corners. Separate UV/tangent values stay distinct, but refinement edges
// and final displaced world positions must agree at every physical seam.
static std::array<uint32_t, 3> sirDisplacementPositionKey( const float *pPosition ) noexcept
{
	std::array<uint32_t, 3> key = {};
	for ( uint32_t k = 0; k < 3; ++k ) {
		const float coordinate = pPosition[k] == 0.0f ? 0.0f : pPosition[k];
		memcpy( &key[k], &coordinate, sizeof( coordinate ) );
	}
	return key;
}

static bool sirDisplacementWeldPositions( std::vector<sirDisplacementVertex_t>& vertices )
{
	std::map<uint32_t, std::array<float, 3>> canonical;
	for ( sirDisplacementVertex_t& vertex : vertices ) {
		const auto inserted =
		    canonical.emplace( vertex.nWeld, std::array<float, 3>{ vertex.position[0], vertex.position[1], vertex.position[2] } );
		if ( !inserted.second ) {
			for ( uint32_t k = 0; k < 3; ++k ) {
				const float established = inserted.first->second[k];
				const float tolerance = 1.0e-5f * std::max( 1.0f, std::abs( established ) );
				if ( std::abs( vertex.position[k] - established ) > tolerance )
					return false;
				vertex.position[k] = established;
			}
		}
	}
	return true;
}

static void sirDisplacementHash32( std::vector<uint8_t>& bytes, uint32_t value )
{
	const size_t offset = bytes.size();
	bytes.resize( offset + 4 );
	sirPackWrite32( bytes.data() + offset, value );
}

static void sirDisplacementHash64( std::vector<uint8_t>& bytes, uint64_t value )
{
	sirDisplacementHash32( bytes, ( uint32_t )value );
	sirDisplacementHash32( bytes, ( uint32_t )( value >> 32 ) );
}

static void sirDisplacementHashFloat( std::vector<uint8_t>& bytes, float value )
{
	uint32_t bits = 0;
	memcpy( &bits, &value, sizeof( bits ) );
	sirDisplacementHash32( bytes, bits );
}

static bool sirDisplacementValidate( const sirGeometryCookInput_t& input, const sirDisplacementImage_t& image,
                                     const sirDisplacementRecipe_t& recipe ) noexcept
{
	if ( input.key.nAssetId == 0 || input.key.nRevision == 0 || input.pPositionsXYZ == nullptr || input.pIndices == nullptr ||
	     input.pTexcoordsUV == nullptr || input.pMaterials == nullptr || input.nVertexCount < 3 || input.nIndexCount < 3 ||
	     input.nIndexCount % 3 != 0 || input.nMaterialCount == 0 || input.nIndexCount > 3 * SIR_MESH_PACK_MAX_RECORDS ||
	     image.pHeights == nullptr || image.nWidth < 2 || image.nHeight < 2 || image.nWidth > 8192 || image.nHeight > 8192 ||
	     recipe.nMaxDepth > 8 || recipe.nMaxTriangles == 0 || recipe.nMaxTriangles > SIR_MESH_PACK_MAX_RECORDS ||
	     recipe.nMaxVertices < input.nVertexCount || recipe.nMaxVertices > SIR_MESH_PACK_MAX_RECORDS ||
	     input.nIndexCount / 3 > recipe.nMaxTriangles || !std::isfinite( recipe.fAmplitude ) || !std::isfinite( recipe.fMidpoint ) ||
	     !std::isfinite( recipe.fMaxGeometricError ) || recipe.fMaxGeometricError <= 0 || recipe.sampler > SIR_DISPLACEMENT_REPEAT )
		return false;
	for ( uint64_t i = 0; i < ( uint64_t )image.nWidth * image.nHeight; ++i )
		if ( !std::isfinite( image.pHeights[i] ) )
			return false;
	for ( uint64_t i = 0; i < ( uint64_t )input.nVertexCount * 3; ++i )
		if ( !std::isfinite( input.pPositionsXYZ[i] ) || ( input.pNormalsXYZ && !std::isfinite( input.pNormalsXYZ[i] ) ) )
			return false;
	for ( uint64_t i = 0; i < ( uint64_t )input.nVertexCount * 2; ++i )
		if ( !std::isfinite( input.pTexcoordsUV[i] ) )
			return false;
	uint32_t end = 0;
	for ( uint32_t material = 0; material < input.nMaterialCount; ++material ) {
		const sirGeometryCookMaterial_t& m = input.pMaterials[material];
		if ( m.nFirstIndex != end || m.nIndexCount == 0 || m.nIndexCount % 3 != 0 || m.nIndexCount > input.nIndexCount - end ||
		     m.nMaterialId == 0xffffffffU || ( material && input.pMaterials[material - 1].nMaterialId >= m.nMaterialId ) )
			return false;
		end += m.nIndexCount;
	}
	if ( end != input.nIndexCount )
		return false;
	for ( uint32_t i = 0; i < input.nIndexCount; i += 3 ) {
		const uint32_t a = input.pIndices[i], b = input.pIndices[i + 1], c = input.pIndices[i + 2];
		if ( a >= input.nVertexCount || b >= input.nVertexCount || c >= input.nVertexCount || a == b || b == c || a == c )
			return false;
	}
	return true;
}

sirPackResult_t SIRDisplacementCooker::bake( const sirGeometryCookInput_t& input, const sirDisplacementImage_t& image,
                                             const sirDisplacementRecipe_t& recipe, sirDisplacementBakeResult_t *pResult ) noexcept
{
	if ( pResult == nullptr || !sirDisplacementValidate( input, image, recipe ) )
		return SIR_PACK_INVALID_ARGUMENT;
	try {
		std::vector<sirDisplacementVertex_t> vertices( input.nVertexCount );
		for ( uint32_t v = 0; v < input.nVertexCount; ++v ) {
			for ( uint32_t k = 0; k < 3; ++k ) {
				vertices[v].base[k] = input.pPositionsXYZ[3 * v + k];
				vertices[v].normal[k] = input.pNormalsXYZ ? input.pNormalsXYZ[3 * v + k] : 0;
			}
			vertices[v].uv[0] = input.pTexcoordsUV[2 * v];
			vertices[v].uv[1] = input.pTexcoordsUV[2 * v + 1];
		}
		if ( input.pNormalsXYZ == nullptr ) {
			for ( uint32_t t = 0; t < input.nIndexCount; t += 3 ) {
				const uint32_t a = input.pIndices[t], b = input.pIndices[t + 1], c = input.pIndices[t + 2];
				const sirDisplacementVertex_t& A = vertices[a];
				const sirDisplacementVertex_t& B = vertices[b];
				const sirDisplacementVertex_t& C = vertices[c];
				const double e1[3] = { ( double )B.base[0] - A.base[0], ( double )B.base[1] - A.base[1], ( double )B.base[2] - A.base[2] };
				const double e2[3] = { ( double )C.base[0] - A.base[0], ( double )C.base[1] - A.base[1], ( double )C.base[2] - A.base[2] };
				const double normal[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
				const double length = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
				if ( length < 1.0e-24 || !std::isfinite( length ) )
					return SIR_PACK_INVALID_FORMAT;
				for ( uint32_t v : { a, b, c } )
					for ( uint32_t k = 0; k < 3; ++k )
						vertices[v].normal[k] += ( float )normal[k];
			}
		}
		for ( sirDisplacementVertex_t& vertex : vertices )
			if ( !sirDisplacementNormalize( vertex.normal ) || !sirDisplacementApply( &vertex, image, recipe ) )
				return SIR_PACK_INVALID_FORMAT;
		std::map<std::array<uint32_t, 3>, uint32_t> weldedPositions;
		uint32_t nextWeld = 0;
		for ( sirDisplacementVertex_t& vertex : vertices ) {
			const auto key = sirDisplacementPositionKey( vertex.base );
			const auto existing = weldedPositions.emplace( key, nextWeld );
			if ( existing.second )
				++nextWeld;
			vertex.nWeld = existing.first->second;
		}
		if ( !sirDisplacementWeldPositions( vertices ) )
			return SIR_PACK_INVALID_FORMAT;
		std::vector<sirDisplacementTriangle_t> triangles;
		triangles.reserve( input.nIndexCount / 3 );
		uint32_t material = 0;
		for ( uint32_t i = 0; i < input.nIndexCount; i += 3 ) {
			while ( material + 1 < input.nMaterialCount && i >= input.pMaterials[material + 1].nFirstIndex )
				++material;
			triangles.push_back( { { input.pIndices[i], input.pIndices[i + 1], input.pIndices[i + 2] }, material } );
		}
		const double threshold2 = ( double )recipe.fMaxGeometricError * recipe.fMaxGeometricError;
		for ( uint32_t depth = 0; depth <= recipe.nMaxDepth; ++depth ) {
			std::map<uint64_t, uint32_t> physicalEdges;
			for ( const sirDisplacementTriangle_t& triangle : triangles ) {
				bool split[3] = {};
				for ( uint32_t edge = 0; edge < 3; ++edge ) {
					const uint32_t a = triangle.vertices[edge], b = triangle.vertices[( edge + 1 ) % 3];
					sirDisplacementVertex_t midpoint = {};
					if ( !sirDisplacementInterpolate( vertices[a], vertices[b], &midpoint ) ||
					     !sirDisplacementApply( &midpoint, image, recipe ) )
						return SIR_PACK_INVALID_FORMAT;
					split[edge] = sirDisplacementError( vertices[a], vertices[b], midpoint ) > threshold2;
				}
				sirDisplacementVertex_t centroid = {};
				if ( !sirDisplacementCentroid( vertices[triangle.vertices[0]], vertices[triangle.vertices[1]],
				                               vertices[triangle.vertices[2]], &centroid ) ||
				     !sirDisplacementApply( &centroid, image, recipe ) )
					return SIR_PACK_INVALID_FORMAT;
				double centerError2 = 0;
				for ( uint32_t k = 0; k < 3; ++k ) {
					const double delta = ( double )centroid.position[k] -
					                     ( ( double )vertices[triangle.vertices[0]].position[k] +
					                       vertices[triangle.vertices[1]].position[k] + vertices[triangle.vertices[2]].position[k] ) /
					                         3.0;
					centerError2 += delta * delta;
				}
				for ( uint32_t edge = 0; edge < 3; ++edge ) {
					if ( split[edge] || centerError2 > threshold2 ) {
						const uint32_t a = triangle.vertices[edge], b = triangle.vertices[( edge + 1 ) % 3];
						physicalEdges.emplace( sirDisplacementEdge( vertices[a].nWeld, vertices[b].nWeld ), 0 );
					}
				}
			}
			if ( physicalEdges.empty() )
				break;
			// A requested geometric-error limit is an actual admission bound.
			// Never silently publish a mesh that still exceeds it at max depth.
			if ( depth == recipe.nMaxDepth )
				return SIR_PACK_OUT_OF_CAPACITY;
			// Every topological edge at the same physical seam gets its own UV
			// midpoint but the same world-space weld identity. Split scheduling
			// is shared across materials, UV islands and independent OBJ corners.
			std::map<uint64_t, uint32_t> topologicalEdges;
			for ( const sirDisplacementTriangle_t& triangle : triangles ) {
				for ( uint32_t edge = 0; edge < 3; ++edge ) {
					const uint32_t a = triangle.vertices[edge], b = triangle.vertices[( edge + 1 ) % 3];
					const uint64_t key = sirDisplacementEdge( vertices[a].nWeld, vertices[b].nWeld );
					if ( physicalEdges.find( key ) != physicalEdges.end() )
						topologicalEdges.emplace( sirDisplacementEdge( a, b ), 0 );
				}
			}
			if ( topologicalEdges.size() > recipe.nMaxVertices - vertices.size() )
				return SIR_PACK_OUT_OF_CAPACITY;
			for ( auto& edge : physicalEdges )
				edge.second = nextWeld++;
			for ( auto& edge : topologicalEdges ) {
				const uint32_t a = ( uint32_t )( edge.first >> 32 );
				const uint32_t b = ( uint32_t )edge.first;
				sirDisplacementVertex_t midpoint = {};
				if ( !sirDisplacementInterpolate( vertices[a], vertices[b], &midpoint ) ||
				     !sirDisplacementApply( &midpoint, image, recipe ) )
					return SIR_PACK_INVALID_FORMAT;
				midpoint.nWeld = physicalEdges.at( sirDisplacementEdge( vertices[a].nWeld, vertices[b].nWeld ) );
				edge.second = ( uint32_t )vertices.size();
				vertices.push_back( midpoint );
			}
			if ( !sirDisplacementWeldPositions( vertices ) )
				return SIR_PACK_INVALID_FORMAT;
			std::vector<sirDisplacementTriangle_t> next;
			for ( const sirDisplacementTriangle_t& triangle : triangles ) {
				uint32_t mid[3] = { UINT32_MAX, UINT32_MAX, UINT32_MAX };
				uint32_t count = 0;
				for ( uint32_t edge = 0; edge < 3; ++edge ) {
					const uint64_t key = sirDisplacementEdge( triangle.vertices[edge], triangle.vertices[( edge + 1 ) % 3] );
					const auto it = topologicalEdges.find( key );
					if ( it != topologicalEdges.end() ) {
						mid[edge] = it->second;
						++count;
					}
				}
				if ( count + 1 > recipe.nMaxTriangles - next.size() )
					return SIR_PACK_OUT_OF_CAPACITY;
				const uint32_t *v = triangle.vertices;
				if ( count == 0 ) {
					next.push_back( triangle );
				} else if ( count == 1 ) {
					const uint32_t e = mid[0] != UINT32_MAX ? 0 : ( mid[1] != UINT32_MAX ? 1 : 2 );
					const uint32_t a = v[e], b = v[( e + 1 ) % 3], c = v[( e + 2 ) % 3];
					next.push_back( { { a, mid[e], c }, triangle.material } );
					next.push_back( { { mid[e], b, c }, triangle.material } );
				} else if ( count == 2 ) {
					const uint32_t missing = mid[0] == UINT32_MAX ? 0 : ( mid[1] == UINT32_MAX ? 1 : 2 );
					const uint32_t e = ( missing + 1 ) % 3;
					const uint32_t a = v[e], b = v[( e + 1 ) % 3], c = v[( e + 2 ) % 3];
					const uint32_t ab = mid[e], bc = mid[( e + 1 ) % 3];
					next.push_back( { { b, bc, ab }, triangle.material } );
					next.push_back( { { a, ab, c }, triangle.material } );
					next.push_back( { { ab, bc, c }, triangle.material } );
				} else {
					next.push_back( { { v[0], mid[0], mid[2] }, triangle.material } );
					next.push_back( { { mid[0], v[1], mid[1] }, triangle.material } );
					next.push_back( { { mid[2], mid[1], v[2] }, triangle.material } );
					next.push_back( { { mid[0], mid[1], mid[2] }, triangle.material } );
				}
			}

			triangles.swap( next );
		}

		sirDisplacementBakeResult_t baked = {};
		baked.positionsXYZ.reserve( vertices.size() * 3 );
		baked.texcoordsUV.reserve( vertices.size() * 2 );
		for ( const sirDisplacementVertex_t& vertex : vertices ) {
			for ( float f : vertex.position )
				baked.positionsXYZ.push_back( f );
			baked.texcoordsUV.push_back( vertex.uv[0] );
			baked.texcoordsUV.push_back( vertex.uv[1] );
		}
		baked.indices.reserve( triangles.size() * 3 );
		baked.materials.resize( input.nMaterialCount );
		for ( uint32_t m = 0; m < input.nMaterialCount; ++m ) {
			baked.materials[m].nMaterialId = input.pMaterials[m].nMaterialId;
			baked.materials[m].nFirstIndex = ( uint32_t )baked.indices.size();
			for ( const sirDisplacementTriangle_t& triangle : triangles ) {
				if ( triangle.material != m )
					continue;
				for ( uint32_t vertex : triangle.vertices )
					baked.indices.push_back( vertex );
			}
			baked.materials[m].nIndexCount = ( uint32_t )baked.indices.size() - baked.materials[m].nFirstIndex;
		}
		// Canonical LE digest of authoring-only inputs, independent of host
		// endianness, address, allocation order and parallel scheduling.
		std::vector<uint8_t> recipeBytes;
		sirDisplacementHash64( recipeBytes, input.key.nAssetId );
		sirDisplacementHash64( recipeBytes, input.key.nRevision );
		sirDisplacementHash32( recipeBytes, input.key.nVariant );
		sirDisplacementHash32( recipeBytes, input.nVertexCount );
		sirDisplacementHash32( recipeBytes, input.nIndexCount );
		sirDisplacementHash32( recipeBytes, image.nWidth );
		sirDisplacementHash32( recipeBytes, image.nHeight );
		sirDisplacementHashFloat( recipeBytes, recipe.fAmplitude );
		sirDisplacementHashFloat( recipeBytes, recipe.fMidpoint );
		sirDisplacementHashFloat( recipeBytes, recipe.fMaxGeometricError );
		sirDisplacementHash32( recipeBytes, recipe.nMaxDepth );
		sirDisplacementHash32( recipeBytes, recipe.nMaxVertices );
		sirDisplacementHash32( recipeBytes, recipe.nMaxTriangles );
		sirDisplacementHash32( recipeBytes, ( uint32_t )recipe.sampler );
		for ( uint64_t i = 0; i < ( uint64_t )input.nVertexCount * 3; ++i ) {
			sirDisplacementHashFloat( recipeBytes, input.pPositionsXYZ[i] );
			if ( input.pNormalsXYZ )
				sirDisplacementHashFloat( recipeBytes, input.pNormalsXYZ[i] );
		}
		for ( uint64_t i = 0; i < ( uint64_t )input.nVertexCount * 2; ++i )
			sirDisplacementHashFloat( recipeBytes, input.pTexcoordsUV[i] );
		for ( uint32_t i = 0; i < input.nIndexCount; ++i )
			sirDisplacementHash32( recipeBytes, input.pIndices[i] );
		for ( uint32_t i = 0; i < input.nMaterialCount; ++i ) {
			sirDisplacementHash32( recipeBytes, input.pMaterials[i].nFirstIndex );
			sirDisplacementHash32( recipeBytes, input.pMaterials[i].nIndexCount );
			sirDisplacementHash32( recipeBytes, input.pMaterials[i].nMaterialId );
		}
		for ( uint64_t i = 0; i < ( uint64_t )image.nWidth * image.nHeight; ++i )
			sirDisplacementHashFloat( recipeBytes, image.pHeights[i] );
		sirPackSha256( recipeBytes.data(), recipeBytes.size(), baked.szAuthoringDigest );
		baked.nRevision = sirPackRead64( baked.szAuthoringDigest );
		if ( baked.nRevision == 0 )
			baked.nRevision = 1;
		pResult->positionsXYZ.swap( baked.positionsXYZ );
		pResult->texcoordsUV.swap( baked.texcoordsUV );
		pResult->indices.swap( baked.indices );
		pResult->materials.swap( baked.materials );
		pResult->nRevision = baked.nRevision;
		memcpy( pResult->szAuthoringDigest, baked.szAuthoringDigest, sizeof( baked.szAuthoringDigest ) );
		return SIR_PACK_OK;
	} catch ( const std::bad_alloc& ) {
		return SIR_PACK_OUT_OF_CAPACITY;
	} catch ( ... ) {
		return SIR_PACK_INVALID_FORMAT;
	}
}
