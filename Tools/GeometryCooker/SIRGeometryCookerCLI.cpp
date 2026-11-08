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
#include <Tools/GeometryCooker/SIRDisplacementCooker.h>
#include <Tools/SIRPackCooker/SIRPackShippingGuard.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

struct sirGeometryObjCorner_t
{
	int32_t nPosition = 0;
	int32_t nTexcoord = -1;
	int32_t nNormal = -1;

	bool operator<( const sirGeometryObjCorner_t& other ) const noexcept
	{
		return std::tie( nPosition, nTexcoord, nNormal ) < std::tie( other.nPosition, other.nTexcoord, other.nNormal );
	}
};

struct sirGeometryObjData_t
{
	std::vector<float> positions;
	std::vector<float> uv;
	std::vector<float> normals;
	std::vector<uint32_t> indices;
	std::vector<sirGeometryCookMaterial_t> materials;
	bool bAllNormals = true;
	bool bAnyUV = false;
};

static bool sirGeometryResolveIndex( const std::string& token, size_t nAvailable, int32_t *pIndex ) noexcept
{
	if ( token.empty() || pIndex == nullptr )
		return false;
	errno = 0;
	char *pEnd = nullptr;
	const long long nValue = strtoll( token.c_str(), &pEnd, 10 );
	if ( errno != 0 || pEnd == token.c_str() || *pEnd != 0 || nValue == 0 || nAvailable > 0x7fffffffULL )
		return false;
	const long long nResolved = nValue > 0 ? nValue - 1 : ( long long )nAvailable + nValue;
	if ( nResolved < 0 || ( uint64_t )nResolved >= nAvailable )
		return false;
	*pIndex = ( int32_t )nResolved;
	return true;
}

static bool sirGeometryParseCorner( const std::string& token, const sirGeometryObjData_t& raw, sirGeometryObjCorner_t *pCorner ) noexcept
{
	const size_t nFirstSlash = token.find( '/' );
	const size_t nSecondSlash = nFirstSlash == std::string::npos ? std::string::npos : token.find( '/', nFirstSlash + 1 );
	if ( nSecondSlash != std::string::npos && token.find( '/', nSecondSlash + 1 ) != std::string::npos )
		return false;
	const std::string pszPosition = token.substr( 0, nFirstSlash );
	if ( !sirGeometryResolveIndex( pszPosition, raw.positions.size() / 3, &pCorner->nPosition ) )
		return false;
	if ( nFirstSlash != std::string::npos ) {
		const std::string pszUV =
		    token.substr( nFirstSlash + 1, nSecondSlash == std::string::npos ? std::string::npos : nSecondSlash - nFirstSlash - 1 );
		if ( !pszUV.empty() && !sirGeometryResolveIndex( pszUV, raw.uv.size() / 2, &pCorner->nTexcoord ) )
			return false;
	}
	if ( nSecondSlash != std::string::npos ) {
		const std::string pszNormal = token.substr( nSecondSlash + 1 );
		if ( !pszNormal.empty() && !sirGeometryResolveIndex( pszNormal, raw.normals.size() / 3, &pCorner->nNormal ) )
			return false;
	}
	return true;
}

static uint32_t sirGeometryMaterialHash( const std::string& name ) noexcept
{
	uint32_t nHash = 2166136261U;
	for ( unsigned char c : name )
		nHash = ( nHash ^ c ) * 16777619U;
	return nHash;
}

static bool sirGeometryReadObj( const char *pszPath, sirGeometryObjData_t *pOutput )
{
	std::ifstream stream( pszPath );
	if ( !stream )
		return false;
	sirGeometryObjData_t raw;
	std::map<sirGeometryObjCorner_t, uint32_t> dedup;
	std::map<uint32_t, std::string> materialNames;
	std::map<uint32_t, std::vector<uint32_t>> trianglesByMaterial;
	uint32_t nCurrentMaterial = 0;
	materialNames.emplace( 0, "" );
	std::string line;
	while ( std::getline( stream, line ) ) {
		const size_t nComment = line.find( '#' );
		if ( nComment != std::string::npos )
			line.erase( nComment );
		std::istringstream input( line );
		std::string operation;
		if ( !( input >> operation ) )
			continue;
		if ( operation == "v" || operation == "vn" ) {
			float x = 0, y = 0, z = 0;
			if ( !( input >> x >> y >> z ) )
				return false;
			std::vector<float>& destination = operation == "v" ? raw.positions : raw.normals;
			destination.insert( destination.end(), { x, y, z } );
		} else if ( operation == "vt" ) {
			float u = 0, v = 0;
			if ( !( input >> u >> v ) )
				return false;
			raw.uv.insert( raw.uv.end(), { u, v } );
		} else if ( operation == "usemtl" ) {
			std::string name;
			if ( !( input >> name ) )
				return false;
			const uint32_t id = sirGeometryMaterialHash( name );
			auto found = materialNames.find( id );
			if ( found != materialNames.end() && found->second != name )
				return false;
			materialNames[id] = name;
			nCurrentMaterial = id;
		} else if ( operation == "f" ) {
			std::vector<uint32_t> face;
			std::string token;
			while ( input >> token ) {
				sirGeometryObjCorner_t corner;
				if ( !sirGeometryParseCorner( token, raw, &corner ) )
					return false;
				auto existing = dedup.find( corner );
				uint32_t index = 0;
				if ( existing == dedup.end() ) {
					if ( pOutput->positions.size() / 3 >= SIR_MESH_PACK_MAX_RECORDS )
						return false;
					index = ( uint32_t )( pOutput->positions.size() / 3 );
					dedup.emplace( corner, index );
					pOutput->positions.insert( pOutput->positions.end(), raw.positions.begin() + 3 * corner.nPosition,
					                           raw.positions.begin() + 3 * corner.nPosition + 3 );
					if ( corner.nTexcoord >= 0 ) {
						pOutput->uv.insert( pOutput->uv.end(), raw.uv.begin() + 2 * corner.nTexcoord,
						                    raw.uv.begin() + 2 * corner.nTexcoord + 2 );
						pOutput->bAnyUV = true;
					} else
						pOutput->uv.insert( pOutput->uv.end(), { 0.0f, 0.0f } );
					if ( corner.nNormal >= 0 ) {
						pOutput->normals.insert( pOutput->normals.end(), raw.normals.begin() + 3 * corner.nNormal,
						                         raw.normals.begin() + 3 * corner.nNormal + 3 );
					} else {
						pOutput->normals.insert( pOutput->normals.end(), { 0.0f, 0.0f, 0.0f } );
						pOutput->bAllNormals = false;
					}
				} else
					index = existing->second;
				face.push_back( index );
			}
			if ( face.size() < 3 || face.size() > 1024 )
				return false;
			std::vector<uint32_t>& triangles = trianglesByMaterial[nCurrentMaterial];
			for ( size_t i = 2; i < face.size(); ++i ) {
				triangles.push_back( face[0] );
				triangles.push_back( face[i - 1] );
				triangles.push_back( face[i] );
			}
		}
	}
	if ( !stream.eof() || trianglesByMaterial.empty() )
		return false;
	for ( const auto& entry : trianglesByMaterial ) {
		if ( entry.second.empty() )
			continue;
		sirGeometryCookMaterial_t material = {};
		material.nMaterialId = entry.first;
		material.nFirstIndex = ( uint32_t )pOutput->indices.size();
		material.nIndexCount = ( uint32_t )entry.second.size();
		pOutput->materials.push_back( material );
		pOutput->indices.insert( pOutput->indices.end(), entry.second.begin(), entry.second.end() );
	}
	return !pOutput->materials.empty();
}

static bool sirGeometryWriteBinary( const std::string& path, const std::vector<uint8_t>& bytes )
{
	const std::string temporary = path + ".tmp";
	std::ofstream stream( temporary, std::ios::binary | std::ios::trunc );
	if ( !stream || bytes.empty() )
		return false;
	stream.write( reinterpret_cast<const char *>( bytes.data() ), ( std::streamsize )bytes.size() );
	stream.flush();
	if ( !stream.good() ) {
		stream.close();
		std::remove( temporary.c_str() );
		return false;
	}
	stream.close();
	if ( std::rename( temporary.c_str(), path.c_str() ) != 0 ) {
		std::remove( temporary.c_str() );
		return false;
	}
	return true;
}

// Prepare every output before touching any previously published package.
// Restore previous catalog/shard/sidecar on filesystem operation failures.
static bool sirGeometryFileExists( const std::string& path )
{
	std::ifstream stream( path, std::ios::binary );
	return ( bool )stream;
}

static bool sirGeometryPublishPackage( const std::string& prefix, const sirGeometryCookOutput_t& cooked,
                                       const std::vector<uint8_t>& sidecar )
{
	const std::array<std::string, 3> paths = { prefix + ".sirp", prefix + ".sirm", prefix + ".meshrecipe" };
	const std::array<const std::vector<uint8_t> *, 3> payloads = { &cooked.shard, &cooked.catalog, &sidecar };
	std::array<bool, 3> staged = {};
	std::array<bool, 3> backedUp = {};
	std::array<bool, 3> installed = {};
	bool success = true;
	for ( const std::string& path : paths ) {
		if ( sirGeometryFileExists( path + ".bak" ) )
			return false;
	}
	for ( size_t i = 0; i < paths.size(); ++i ) {
		if ( !payloads[i]->empty() ) {
			if ( !sirGeometryWriteBinary( paths[i] + ".stage", *payloads[i] ) ) {
				success = false;
				break;
			}
			staged[i] = true;
		}
	}
	if ( success ) {
		for ( size_t i = 0; i < paths.size(); ++i ) {
			if ( sirGeometryFileExists( paths[i] ) ) {
				if ( std::rename( paths[i].c_str(), ( paths[i] + ".bak" ).c_str() ) != 0 ) {
					success = false;
					break;
				}
				backedUp[i] = true;
			}
		}
	}
	if ( success ) {
		for ( size_t i = 0; i < paths.size(); ++i ) {
			if ( staged[i] ) {
				if ( std::rename( ( paths[i] + ".stage" ).c_str(), paths[i].c_str() ) != 0 ) {
					success = false;
					break;
				}
				installed[i] = true;
			}
		}
	}
	if ( !success ) {
		for ( size_t i = 0; i < paths.size(); ++i ) {
			if ( installed[i] )
				std::remove( paths[i].c_str() );
		}
		for ( size_t i = 0; i < paths.size(); ++i ) {
			if ( backedUp[i] && std::rename( ( paths[i] + ".bak" ).c_str(), paths[i].c_str() ) != 0 )
				fprintf( stderr, "geometry cooker: rollback failed for %s\n", paths[i].c_str() );
		}
	} else {
		for ( size_t i = 0; i < paths.size(); ++i ) {
			if ( backedUp[i] )
				std::remove( ( paths[i] + ".bak" ).c_str() );
		}
	}
	for ( size_t i = 0; i < paths.size(); ++i ) {
		if ( staged[i] )
			std::remove( ( paths[i] + ".stage" ).c_str() );
	}
	return success;
}

static bool sirGeometryReadPGMToken( std::istream& stream, std::string *pToken )
{
	while ( stream >> *pToken ) {
		if ( !pToken->empty() && ( *pToken )[0] == '#' ) {
			std::string rest;
			std::getline( stream, rest );
			continue;
		}
		return true;
	}
	return false;
}

static bool sirGeometryParseUnsigned( const std::string& token, uint32_t *pResult ) noexcept
{
	if ( token.empty() || pResult == nullptr || token[0] == '-' )
		return false;
	errno = 0;
	char *pEnd = nullptr;
	const unsigned long n = strtoul( token.c_str(), &pEnd, 10 );
	if ( errno || pEnd == token.c_str() || *pEnd || n > UINT32_MAX )
		return false;
	*pResult = ( uint32_t )n;
	return true;
}

static bool sirGeometryReadPGM( const char *pszPath, std::vector<float> *pHeights, uint32_t *pWidth, uint32_t *pHeight )
{
	std::ifstream stream( pszPath );
	if ( !stream )
		return false;
	std::string token;
	if ( !sirGeometryReadPGMToken( stream, &token ) || token != "P2" )
		return false;
	uint32_t width = 0, height = 0, maximum = 0;
	if ( !sirGeometryReadPGMToken( stream, &token ) || !sirGeometryParseUnsigned( token, &width ) ||
	     !sirGeometryReadPGMToken( stream, &token ) || !sirGeometryParseUnsigned( token, &height ) ||
	     !sirGeometryReadPGMToken( stream, &token ) || !sirGeometryParseUnsigned( token, &maximum ) || width < 2 || height < 2 ||
	     width > 8192 || height > 8192 || maximum == 0 || maximum > 65535 )
		return false;
	std::vector<float> values;
	values.reserve( ( size_t )width * height );
	for ( uint64_t i = 0; i < ( uint64_t )width * height; ++i ) {
		uint32_t sample = 0;
		if ( !sirGeometryReadPGMToken( stream, &token ) || !sirGeometryParseUnsigned( token, &sample ) || sample > maximum )
			return false;
		values.push_back( ( float )sample / ( float )maximum );
	}
	if ( sirGeometryReadPGMToken( stream, &token ) )
		return false;
	pHeights->swap( values );
	*pWidth = width;
	*pHeight = height;
	return true;
}

static bool sirGeometryParseFinite( const char *pszText, float *pResult ) noexcept
{
	if ( pszText == nullptr || pResult == nullptr )
		return false;
	errno = 0;
	char *pEnd = nullptr;
	const float f = strtof( pszText, &pEnd );
	if ( errno || pEnd == pszText || *pEnd || !std::isfinite( f ) )
		return false;
	*pResult = f;
	return true;
}

static std::vector<uint8_t> sirGeometryRecipeSidecar( const char *pszImagePath, const sirDisplacementRecipe_t& recipe,
                                                      const sirDisplacementBakeResult_t& baked )
{
	// Editor-only sidecar: this file is never registered in SIRPack.
	std::ostringstream stream;
	stream << "SIR-DISPLACEMENT-RECIPE-V1\n";
	stream << "source=" << pszImagePath << "\n";
	stream << "amplitude=" << recipe.fAmplitude << "\n";
	stream << "midpoint=" << recipe.fMidpoint << "\n";
	stream << "max-error=" << recipe.fMaxGeometricError << "\n";
	stream << "depth=" << recipe.nMaxDepth << "\n";
	stream << "sampler=" << ( recipe.sampler == SIR_DISPLACEMENT_REPEAT ? "repeat" : "clamp" ) << "\n";
	stream << "revision=" << baked.nRevision << "\n";
	stream << "sha256=";
	static const char digits[] = "0123456789abcdef";
	for ( uint8_t b : baked.szAuthoringDigest )
		stream << digits[b >> 4] << digits[b & 15];
	stream << "\n";
	const std::string source = stream.str();
	return std::vector<uint8_t>( source.begin(), source.end() );
}

int main( int argc, char **argv )
{
	if ( argc < 6 ) {
		fprintf( stderr, "usage: sir-geometry-cooker input.obj output-prefix asset-id revision resource-type-id [variant] "
		                 "[--heightmap height.pgm --amplitude N --midpoint N --error N --depth N --sampler clamp|repeat]\n" );
		return 2;
	}
	try {
		sirGeometryObjData_t obj;
		if ( !sirGeometryReadObj( argv[1], &obj ) ) {
			fprintf( stderr, "geometry cooker: invalid or unreadable OBJ\n" );
			return 1;
		}
		sirGeometryCookInput_t input = {};
		input.key.nAssetId = std::stoull( argv[3] );
		input.key.nRevision = std::stoull( argv[4] );
		uint32_t nextArg = 6;
		if ( nextArg < ( uint32_t )argc && argv[nextArg][0] != '-' )
			input.key.nVariant = ( uint32_t )std::stoul( argv[nextArg++] );
		input.nResourceTypeId = ( uint32_t )std::stoul( argv[5] );
		input.nPackageId = input.key.nAssetId;
		input.nGeneration = input.key.nRevision;
		input.pPositionsXYZ = obj.positions.data();
		input.nVertexCount = ( uint32_t )obj.positions.size() / 3;
		input.pIndices = obj.indices.data();
		input.nIndexCount = ( uint32_t )obj.indices.size();
		input.pMaterials = obj.materials.data();
		input.nMaterialCount = ( uint32_t )obj.materials.size();
		input.nMaxVerticesPerMeshlet = 64;
		input.nMaxTrianglesPerMeshlet = 124;
		input.nTargetRootTriangles = std::max( 1U, input.nIndexCount / 48 );
		input.fMaxSimplificationError = 0.05f;
		input.bCookCollision = true;
		input.pTexcoordsUV = obj.bAnyUV ? obj.uv.data() : nullptr;
		input.pNormalsXYZ = obj.bAllNormals ? obj.normals.data() : nullptr;

		std::string imagePath;
		sirDisplacementRecipe_t recipe = { 1.0f, 0.5f, 0.05f, 6, 1048576, 1048576, SIR_DISPLACEMENT_CLAMP };
		for ( ; nextArg < ( uint32_t )argc; nextArg += 2 ) {
			if ( nextArg + 1 >= ( uint32_t )argc ) {
				fprintf( stderr, "geometry cooker: option requires a value\n" );
				return 2;
			}
			const std::string option = argv[nextArg];
			const char *pszValue = argv[nextArg + 1];
			if ( option == "--heightmap" )
				imagePath = pszValue;
			else if ( option == "--amplitude" ) {
				if ( !sirGeometryParseFinite( pszValue, &recipe.fAmplitude ) )
					return 2;
			} else if ( option == "--midpoint" ) {
				if ( !sirGeometryParseFinite( pszValue, &recipe.fMidpoint ) )
					return 2;
			} else if ( option == "--error" ) {
				if ( !sirGeometryParseFinite( pszValue, &recipe.fMaxGeometricError ) || recipe.fMaxGeometricError <= 0 )
					return 2;
			} else if ( option == "--depth" ) {
				if ( !sirGeometryParseUnsigned( pszValue, &recipe.nMaxDepth ) || recipe.nMaxDepth > 8 )
					return 2;
			} else if ( option == "--sampler" ) {
				if ( strcmp( pszValue, "repeat" ) == 0 )
					recipe.sampler = SIR_DISPLACEMENT_REPEAT;
				else if ( strcmp( pszValue, "clamp" ) == 0 )
					recipe.sampler = SIR_DISPLACEMENT_CLAMP;
				else
					return 2;
			} else {
				fprintf( stderr, "geometry cooker: unknown option %s\n", argv[nextArg] );
				return 2;
			}
		}
		std::vector<float> heights;
		sirDisplacementBakeResult_t baked = {};
		std::vector<uint8_t> recipeSidecar;
		if ( !imagePath.empty() ) {
			if ( input.pTexcoordsUV == nullptr ) {
				fprintf( stderr, "geometry cooker: displacement requires valid source UVs\n" );
				return 1;
			}
			uint32_t width = 0, height = 0;
			if ( !sirGeometryReadPGM( imagePath.c_str(), &heights, &width, &height ) ) {
				fprintf( stderr, "geometry cooker: malformed or missing ASCII PGM heightmap\n" );
				return 1;
			}
			const sirDisplacementImage_t image = { heights.data(), width, height };
			const sirPackResult_t bakeResult = SIRDisplacementCooker::bake( input, image, recipe, &baked );
			if ( bakeResult != SIR_PACK_OK ) {
				fprintf( stderr, "geometry cooker: displacement bake failed with code %u\n", ( unsigned )bakeResult );
				return 1;
			}
			recipeSidecar = sirGeometryRecipeSidecar( imagePath.c_str(), recipe, baked );
			input.key.nRevision = baked.nRevision;
			input.nGeneration = baked.nRevision;
			input.pPositionsXYZ = baked.positionsXYZ.data();
			input.nVertexCount = ( uint32_t )( baked.positionsXYZ.size() / 3 );
			input.pTexcoordsUV = baked.texcoordsUV.data();
			input.pNormalsXYZ = nullptr;
			input.pIndices = baked.indices.data();
			input.nIndexCount = ( uint32_t )baked.indices.size();
			input.pMaterials = baked.materials.data();
			input.nMaterialCount = ( uint32_t )baked.materials.size();
		}
		// The output is screened against authoring-only source bytes and the
		// original source path before any catalog or shard becomes visible.
		std::vector<uint8_t> imageBytes;
		std::vector<sirPackShippingPattern_t> sourcePatterns;
		static const char pszRecipeMarker[] = "SIR-DISPLACEMENT-RECIPE-V1";
		if ( !imagePath.empty() ) {
			std::ifstream source( imagePath, std::ios::binary );
			if ( !source )
				return 1;
			imageBytes.assign( std::istreambuf_iterator<char>( source ), std::istreambuf_iterator<char>() );
			if ( !source.eof() || imageBytes.empty() )
				return 1;
			sourcePatterns.push_back( { imageBytes.data(), imageBytes.size() } );
			sourcePatterns.push_back( { ( const uint8_t * )imagePath.data(), imagePath.size() } );
			sourcePatterns.push_back( { ( const uint8_t * )pszRecipeMarker, sizeof( pszRecipeMarker ) - 1 } );
		}
		const sirPackShippingPolicy_t policy = { nullptr, 0, nullptr, 0, sourcePatterns.data(), ( uint32_t )sourcePatterns.size() };
		sirGeometryCookOutput_t cooked;
		const sirPackResult_t result = SIRGeometryCooker::cookShipping( input, policy, &cooked );
		if ( result != SIR_PACK_OK ) {
			fprintf( stderr, "geometry cooker: failed with code %u\n", ( unsigned )result );
			return 1;
		}
		const std::string prefix = argv[2];
		if ( !sirGeometryPublishPackage( prefix, cooked, recipeSidecar ) ) {
			fprintf( stderr, "geometry cooker: failed to write outputs\n" );
			return 1;
		}
		printf( "geometry cooker: wrote %zu page chunks, %zu shard bytes, %zu catalog bytes\n", cooked.pages.size(), cooked.shard.size(),
		        cooked.catalog.size() );
		return 0;
	} catch ( ... ) {
		fprintf( stderr, "geometry cooker: invalid argument or memory allocation failure\n" );
		return 1;
	}
}
