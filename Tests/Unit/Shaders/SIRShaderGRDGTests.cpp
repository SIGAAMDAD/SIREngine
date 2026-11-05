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

#include <Renderer/Shaders/SIRShaderGRDG.h>
#include <Renderer/Shaders/SIRShaderLibrary.h>

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <malloc.h>
#endif

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

typedef struct sirShaderGRDGTestAllocator_s
{
	std::atomic<uint32_t> nOutstanding;
} sirShaderGRDGTestAllocator_t;

static void *testAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nDomain;
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pMemory = nullptr;
#ifdef _WIN32
	pMemory = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nActual = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pMemory, nActual, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pMemory != nullptr ) {
		( ( sirShaderGRDGTestAllocator_t * )pContext )->nOutstanding.fetch_add( 1 );
	}
	return pMemory;
}

static void testFree( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nBytes;
	( void )nAlignment;
	( void )nDomain;
	( ( sirShaderGRDGTestAllocator_t * )pContext )->nOutstanding.fetch_sub( 1 );
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static uint64_t testHash( const uint8_t *pBytes, uint32_t nCount ) noexcept
{
	uint64_t nHash = 14695981039346656037ULL;
	for ( uint32_t iByte = 0; iByte < nCount; ++iByte ) {
		nHash ^= pBytes[iByte];
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

static bool testCookShader( uint8_t *pBytes, uint32_t nCount, uint64_t nAssetId, uint64_t nSourceRevision ) noexcept
{
	if ( nCount < 120 ) {
		return false;
	}
	memset( pBytes, 0, nCount );
	for ( uint32_t iByte = 112; iByte < 120; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( iByte + 11 );
	}
	sirShaderStageRecord_t stage = {};
	stage.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	stage.ePass = SIR_SHADER_PASS_COLOR;
	stage.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	stage.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	stage.nBinaryHash = testHash( pBytes + 112, 8 );
	stage.nEntryPointHash = 17;
	stage.nPayloadOffset = 112;
	stage.nPayloadBytes = 8;
	if ( sirShaderBuildBindingSignature( pBytes + 112, 0, &stage.nBindingSignature ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderEncodeStage( stage, pBytes + 64, 48 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirShaderPackageHeader_t header = {};
	header.nMagic = SIR_SHADER_PACKAGE_MAGIC;
	header.nSchema = SIR_SHADER_PACKAGE_SCHEMA;
	header.nHeaderBytes = 64;
	header.nTotalBytes = 120;
	header.nStageCount = 1;
	header.nBindingCount = 0;
	header.nStageOffset = 64;
	header.nBindingOffset = 112;
	header.nPayloadOffset = 112;
	header.nPayloadBytes = 8;
	header.nAssetId = nAssetId;
	header.nSourceRevision = nSourceRevision;
	header.nTargetFingerprint = 19;
	if ( sirShaderBuildManifestHash( header, pBytes + 64, 48, &header.nManifestHash ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return sirShaderEncodePackageHeader( header, pBytes, 64 ) == SIR_GRAPHICS_SUCCESS;
}

static bool testCookMaterial( uint8_t *pBytes, uint32_t nCount, uint64_t nAssetId, uint32_t iShaderDependency ) noexcept
{
	if ( nCount < 80 ) {
		return false;
	}
	memset( pBytes, 0, nCount );
	sirShaderMaterialPassRecord_t pass = {};
	pass.nPass = SIR_SHADER_PASS_COLOR;
	pass.iShaderDependency = iShaderDependency;
	pass.nRequiredFeatures =
	    iShaderDependency == SIR_SHADER_MATERIAL_NO_SHADER ? SIR_GRAPHICS_FEATURE_FIXED_FUNCTION : SIR_GRAPHICS_FEATURE_GRAPHICS;
	pass.nPermutationKey = 53;
	pass.nRenderStateHash = 29;
	if ( sirShaderEncodeMaterialPass( pass, pBytes + 48, 32 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirShaderMaterialHeader_t header = {};
	header.nMagic = SIR_SHADER_MATERIAL_MAGIC;
	header.nSchema = SIR_SHADER_MATERIAL_SCHEMA;
	header.nHeaderBytes = 48;
	header.nTotalBytes = 80;
	header.nDependencyCount = iShaderDependency == SIR_SHADER_MATERIAL_NO_SHADER ? 0 : 2;
	header.nPassCount = 1;
	header.nAssetId = nAssetId;
	header.nSourceRevision = 9;
	if ( sirShaderBuildMaterialHash( header, pBytes + 48, 32, &header.nManifestHash ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return sirShaderEncodeMaterialHeader( header, pBytes, 48 ) == SIR_GRAPHICS_SUCCESS;
}

static void testSchemas( void ) noexcept
{
	uint8_t szMaterial[80] = {};
	REQUIRE( testCookMaterial( szMaterial, sizeof( szMaterial ), 66, 0 ) );
	REQUIRE( sirShaderValidateMaterial( szMaterial, sizeof( szMaterial ) ) == SIR_GRAPHICS_SUCCESS );
	sirShaderMaterialHeader_t header = {};
	REQUIRE( sirShaderDecodeMaterialHeader( szMaterial, sizeof( szMaterial ), &header ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( header.nSourceRevision == 9 && header.nAssetId == 66 && header.nDependencyCount == 2 );
	sirShaderMaterialPassRecord_t pass = {};
	REQUIRE( sirShaderDecodeMaterialPass( szMaterial + 48, 32, &pass ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( pass.iShaderDependency == 0 && pass.nRenderStateHash == 29 );
	szMaterial[78] ^= 1;
	REQUIRE( sirShaderValidateMaterial( szMaterial, sizeof( szMaterial ) ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	szMaterial[78] ^= 1;
	REQUIRE( sirShaderValidateMaterial( szMaterial, 79 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirShaderDecodeMaterialHeader( nullptr, 80, &header ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE(
	    sirShaderEncodeMaterialPass( { SIR_SHADER_PASS_COMPUTE, SIR_SHADER_MATERIAL_NO_SHADER, SIR_GRAPHICS_FEATURE_FIXED_FUNCTION, 0, 0 },
		                             szMaterial, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( testCookMaterial( szMaterial, sizeof( szMaterial ), 66, SIR_SHADER_MATERIAL_NO_SHADER ) );
	REQUIRE( sirShaderValidateMaterial( szMaterial, sizeof( szMaterial ) ) == SIR_GRAPHICS_SUCCESS );
}

static void testGRDGPublication( void ) noexcept
{
	sirShaderGRDGTestAllocator_t allocator = {};
	{
		SIRResourceGraphConfig config = {};
		config.m_Allocator = { &allocator, testAllocate, testFree };
		config.m_nMaxResources = 16;
		config.m_nMaxStaging = 8;
		config.m_nMaxProviderPins = 8;
		config.m_nMaxCommittedBytes = 1024 * 1024;
		config.m_nMaxDependencyEdges = 32;
		config.m_nMaxInvalidationEvents = 32;
		SIRResourceGraph graph( config );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		const sirShaderGRDGFormats_t formats = { 41, 42, 43, 44 };
		const SIRResourceKey shaderKey = { 18, 41, 0 };
		const SIRResourceKey materialKey = { 66, 43, 0 };
		const SIRResourceKey optionalImageKey = { 911, 32, 0 };
		const sirShaderPackageValidation_t validation = { SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS };

		uint8_t szShader[120] = {};
		uint8_t szMaterial[80] = {};
		REQUIRE( testCookShader( szShader, sizeof( szShader ), 18, 37 ) );
		REQUIRE( testCookMaterial( szMaterial, sizeof( szMaterial ), 66, 0 ) );
		sirShaderPublicationRequest_t shaderRequest = {};
		shaderRequest.key = shaderKey;
		shaderRequest.pBytes = szShader;
		shaderRequest.nBytes = sizeof( szShader );
		shaderRequest.eShaderFormat = SIR_GRAPHICS_SHADER_SPIRV;
		shaderRequest.nAvailableFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
		SIRResourceHandle hShader = {};
		REQUIRE( sirShaderPublishPackage( *pWriter, formats, shaderRequest, &hShader ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( hShader.m_nTypeId == formats.nShaderTypeId );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );

		SIRResourceDependency szDeps[2] = {};
		szDeps[0].m_Target = shaderKey;
		szDeps[0].m_eKind = SIR_RESOURCE_EDGE_KIND_REQUIRED;
		szDeps[1].m_Target = optionalImageKey;
		szDeps[1].m_eKind = SIR_RESOURCE_EDGE_KIND_OPTIONAL;
		sirShaderPublicationRequest_t materialRequest = {};
		materialRequest.key = materialKey;
		materialRequest.pBytes = szMaterial;
		materialRequest.nBytes = sizeof( szMaterial );
		materialRequest.pDependencies = szDeps;
		materialRequest.nDependencyCount = 2;
		SIRResourceHandle hMaterial = {};
		REQUIRE( sirShaderPublishMaterial( *pWriter, formats, materialRequest, &hMaterial ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nUniqueChunks == 2 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView shaderView;
			SIRResourceView materialView;
			REQUIRE( graph.resolve( scope, hShader, &shaderView ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( graph.resolve( scope, hMaterial, &materialView ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirShaderPackageProjection_t shader = {};
			sirShaderMaterialProjection_t material = {};
			REQUIRE( sirShaderProjectPackage( shaderView, formats, validation, &shader ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( shader.nGraphRevision == 1 && shader.header.nSourceRevision == 37 );
			REQUIRE( sirShaderProjectMaterial( materialView, formats, &material ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( material.nGraphRevision == 1 && material.header.nSourceRevision == 9 );
			sirShaderLibraryContext_t context = { &graph, &scope, formats };
			sirShaderResolveStageRequest_t stageRequest = {};
			stageRequest.ePass = SIR_SHADER_PASS_COLOR;
			stageRequest.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
			stageRequest.validation = validation;
			sirShaderResolvedStage_t resolved = {};
			REQUIRE( sirShaderResolveMaterialStage( context, material, stageRequest, &resolved ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( !resolved.bFixedFunction && resolved.hShader.m_nSlot == hShader.m_nSlot );
			REQUIRE( resolved.binary.m_nSize == 8 && resolved.binary.m_pData == shader.bytes.m_pData + 112 );
			REQUIRE( resolved.nShaderGraphRevision == 1 && resolved.nShaderSourceRevision == 37 );
			stageRequest.nStageMask = SIR_GRAPHICS_STAGE_FRAGMENT;
			REQUIRE( sirShaderResolveMaterialStage( context, material, stageRequest, &resolved ) ==
			         SIR_RESOURCE_RESULT_DEPENDENCY_MISSING );

			sirShaderStageProjection_t stage = {};
			REQUIRE( sirShaderProjectStage( shader, 0, &stage ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( stage.binary.m_nSize == 8 && stage.stage.nStageMask == SIR_GRAPHICS_STAGE_VERTEX );
			sirShaderMaterialPassRecord_t pass = {};
			REQUIRE( sirShaderProjectMaterialPass( material, SIR_SHADER_PASS_COLOR, &pass ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( pass.iShaderDependency == 0 );
			REQUIRE( sirShaderProjectMaterialPass( material, SIR_SHADER_PASS_SHADOW, &pass ) == SIR_RESOURCE_RESULT_DEPENDENCY_MISSING );

			sirShaderDependencyRevision_t szRevisions[2] = {};
			sirShaderMaterialSnapshot_t snapshot = { szRevisions, 1, 0, 0, 0 };
			REQUIRE( sirShaderSnapshotMaterial( graph, scope, material, &snapshot ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
			REQUIRE( snapshot.nRequiredCount == 2 && szRevisions[0].key.m_nAssetId == 0 );
			snapshot.nCapacity = 2;
			REQUIRE( sirShaderSnapshotMaterial( graph, scope, material, &snapshot ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( szRevisions[0].bResident && szRevisions[0].nGraphRevision == 1 );
			REQUIRE( !szRevisions[1].bResident && szRevisions[1].key.m_nAssetId == 911 );
			const uint64_t nBefore = snapshot.nFingerprint;
			REQUIRE( sirShaderStampMaterialInvalidation( *pWriter, materialView, &snapshot ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( snapshot.nInvalidationSequence == 0 );
			REQUIRE( sirShaderPublishPackage( *pWriter, formats, shaderRequest, &hShader ) == SIR_RESOURCE_RESULT_STALE_REVISION );
			shaderRequest.nExpectedRevision = 1;
			REQUIRE( sirShaderPublishPackage( *pWriter, formats, shaderRequest, &hShader ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( graph.getStats().m_nUniqueChunks == 2 );
			REQUIRE( shader.nGraphRevision == 1 && shader.bytes.m_pData[112] == szShader[112] );
			REQUIRE( pWriter->collectRetired( 8 ) == 0 );

			SIRResourceView currentMaterialView;
			REQUIRE( graph.resolve( scope, hMaterial, &currentMaterialView ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirShaderMaterialProjection_t currentMaterial = {};
			REQUIRE( sirShaderProjectMaterial( currentMaterialView, formats, &currentMaterial ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirShaderMaterialSnapshot_t updated = { szRevisions, 2, 0, 0, 0 };
			REQUIRE( sirShaderSnapshotMaterial( graph, scope, currentMaterial, &updated ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( updated.nFingerprint != nBefore && szRevisions[0].nGraphRevision == 2 );
			REQUIRE( sirShaderStampMaterialInvalidation( *pWriter, currentMaterialView, &updated ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( updated.nInvalidationSequence != 0 );
		}
		REQUIRE( pWriter->collectRetired( 8 ) >= 1 );
		// An arriving optional image changes dependency fingerprints without
		// republishing the material's canonical composition bytes.
		SIRResourcePublication image = {};
		image.m_Key = optionalImageKey;
		image.m_eKind = SIR_RESOURCE_KIND_IMAGE;
		image.m_nSchemaVersion = 1;
		image.m_nFormatId = 12;
		SIRResourceHandle hImage = {};
		REQUIRE( pWriter->publish( image, &hImage ) == SIR_RESOURCE_RESULT_SUCCESS );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView materialView;
			REQUIRE( graph.resolve( scope, hMaterial, &materialView ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirShaderMaterialProjection_t material = {};
			REQUIRE( sirShaderProjectMaterial( materialView, formats, &material ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirShaderDependencyRevision_t szRevisions[2] = {};
			sirShaderMaterialSnapshot_t snapshot = { szRevisions, 2, 0, 0, 0 };
			REQUIRE( sirShaderSnapshotMaterial( graph, scope, material, &snapshot ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( szRevisions[1].bResident && szRevisions[1].eKind == SIR_RESOURCE_KIND_IMAGE );
			REQUIRE( szRevisions[0].nGraphRevision == 2 && material.nGraphRevision == 1 );
		}
		materialRequest.nExpectedRevision = 0;
		REQUIRE( sirShaderPublishMaterial( *pWriter, formats, materialRequest, &hMaterial ) == SIR_RESOURCE_RESULT_STALE_REVISION );
		szMaterial[79] ^= 0x77;
		materialRequest.nExpectedRevision = 1;
		REQUIRE( sirShaderPublishMaterial( *pWriter, formats, materialRequest, &hMaterial ) == SIR_RESOURCE_RESULT_INVALID_FORMAT );
		szMaterial[79] ^= 0x77;
		REQUIRE( sirShaderPublishMaterial( *pWriter, formats, materialRequest, &hMaterial ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nUniqueChunks == 3 || graph.getStats().m_nUniqueChunks == 2 );
		REQUIRE( pWriter->retire( hMaterial ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->retire( hShader ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->retire( hImage ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 32 ) >= 1 );
	}
	REQUIRE( allocator.nOutstanding.load() == 0 );
}

int main( void )
{
	testSchemas();
	testGRDGPublication();
	return 0;
}
