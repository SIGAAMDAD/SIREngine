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

#include <Renderer/Shaders/SIRShaderContracts.h>

#include <stdio.h>

static uint64_t testHashBytes( const uint8_t *pBytes, uint64_t nCount ) noexcept
{
	uint64_t nHash = 14695981039346656037ULL;
	for ( uint64_t iByte = 0; iByte < nCount; ++iByte ) {
		nHash ^= pBytes[iByte];
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

static bool testBuildPackage( uint8_t *pPackage, uint64_t nCapacity, sirShaderPackageHeader_t *pHeader ) noexcept
{
	if ( nCapacity < 144 ) {
		return false;
	}
	memset( pPackage, 0, ( size_t )nCapacity );
	pPackage[136] = 0x03;
	pPackage[137] = 0x02;
	pPackage[138] = 0x23;
	pPackage[139] = 0x07;
	pPackage[140] = 0x11;
	pPackage[141] = 0x55;
	pPackage[142] = 0x89;
	pPackage[143] = 0xfe;

	sirShaderStageRecord_t stage = {};
	stage.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	stage.ePass = SIR_SHADER_PASS_COLOR;
	stage.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	stage.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	stage.nBinaryHash = testHashBytes( pPackage + 136, 8 );
	stage.nEntryPointHash = 0x12345678ULL;
	stage.nPayloadOffset = 136;
	stage.nPayloadBytes = 8;
	sirShaderReflectionBinding_t binding = {};
	binding.nSet = 0;
	binding.nBinding = 0;
	binding.eType = SIR_GRAPHICS_BINDING_CONSTANT_BUFFER;
	binding.eFrequency = SIR_SHADER_BINDING_FRAME;
	binding.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	binding.nArrayCount = 1;
	binding.nNameHash = 0x7733ULL;
	if ( sirShaderEncodeBinding( binding, pPackage + 112, 24 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( sirShaderBuildBindingSignature( pPackage + 112, 1, &stage.nBindingSignature ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderEncodeStage( stage, pPackage + 64, 48 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirShaderPackageHeader_t header = {};
	header.nMagic = SIR_SHADER_PACKAGE_MAGIC;
	header.nSchema = SIR_SHADER_PACKAGE_SCHEMA;
	header.nHeaderBytes = 64;
	header.nTotalBytes = 144;
	header.nStageCount = 1;
	header.nBindingCount = 1;
	header.nStageOffset = 64;
	header.nBindingOffset = 112;
	header.nPayloadOffset = 136;
	header.nPayloadBytes = 8;
	header.nAssetId = 13;
	header.nSourceRevision = 27;
	header.nTargetFingerprint = 33;
	if ( sirShaderBuildManifestHash( header, pPackage + 64, 72, &header.nManifestHash ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderEncodePackageHeader( header, pPackage, 64 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	*pHeader = header;
	return true;
}

static bool testWireRoundTrips( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	sirShaderPackageHeader_t decoded = {};
	if ( sirShaderDecodePackageHeader( szPackage, sizeof( szPackage ), &decoded ) != SIR_GRAPHICS_SUCCESS ||
	     decoded.nSourceRevision != header.nSourceRevision || decoded.nManifestHash != header.nManifestHash ||
	     decoded.nTotalBytes != header.nTotalBytes ) {
		return false;
	}
	sirShaderStageRecord_t stage = {};
	if ( sirShaderDecodeStage( szPackage + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS || stage.nStageMask != SIR_GRAPHICS_STAGE_VERTEX ||
	     stage.nPayloadOffset != 136 ) {
		return false;
	}
	sirShaderReflectionBinding_t binding = {};
	if ( sirShaderDecodeBinding( szPackage + 112, 24, &binding ) != SIR_GRAPHICS_SUCCESS ||
	     binding.nStageMask != SIR_GRAPHICS_STAGE_VERTEX || binding.nNameHash != 0x7733ULL ) {
		return false;
	}
	if ( sirShaderValidatePackage( szPackage, sizeof( szPackage ), SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	     SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return szPackage[0] == 0x53 && szPackage[1] == 0x53 && szPackage[2] == 0x48 && szPackage[3] == 0x50;
}

static bool testCorruptPackage( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	if ( sirShaderValidatePackage( szPackage, 143, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	     SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	if ( sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_DXIL, SIR_GRAPHICS_FEATURE_GRAPHICS ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	if ( sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, 0 ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	szPackage[143] ^= 0x11;
	if ( sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	     SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	szPackage[143] ^= 0x11;
	szPackage[40] ^= 0x11;
	if ( sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	     SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	szPackage[40] ^= 0x11;
	szPackage[136 - 1] ^= 0x1;
	if ( sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	     SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	szPackage[135] ^= 0x1;
	szPackage[4]++;
	if ( sirShaderDecodePackageHeader( szPackage, 144, &header ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	szPackage[4]--;
	szPackage[24] = 0xff;
	return sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) ==
	       SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testInvalidRecordEncodings( void ) noexcept
{
	uint8_t szBytes[64] = {};
	sirShaderPackageHeader_t header = {};
	if ( sirShaderDecodePackageHeader( nullptr, 64, &header ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodePackageHeader( szBytes, 63, &header ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodePackageHeader( szBytes, 64, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderStageRecord_t stage = {};
	if ( sirShaderEncodeStage( stage, szBytes, 64 ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodeStage( szBytes, 47, &stage ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodeStage( nullptr, 48, &stage ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderReflectionBinding_t binding = {};
	if ( sirShaderEncodeBinding( binding, szBytes, 64 ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodeBinding( szBytes, 23, &binding ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodeBinding( szBytes, 24, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	return sirShaderEncodePackageHeader( header, szBytes, 64 ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirShaderBuildManifestHash( header, szBytes, 0, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testPipelineKeys( void ) noexcept
{
	sirShaderPipelineRecipe_t recipe = {};
	recipe.pass.ePass = SIR_SHADER_PASS_COLOR;
	recipe.pass.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	recipe.pass.nSampleCount = 1;
	recipe.pass.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	recipe.pass.nBindingSignature = 90;
	recipe.pass.bAlphaMasked = true;
	recipe.nAssetId = 13;
	recipe.nSourceRevision = 4;
	recipe.nPackageManifestHash = 0xabb1;
	recipe.nPackageSchema = SIR_SHADER_PACKAGE_SCHEMA;
	sirShaderPipelineKey_t key = {};
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirShaderPipelineKey_t baseline = key;
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_SUCCESS || key.nLow != baseline.nLow || key.nHigh != baseline.nHigh ||
	     !sirShaderRecipesEqual( recipe, recipe ) ) {
		return false;
	}
	sirShaderPipelineRecipe_t changed = recipe;
	changed.pass.bAlphaMasked = false;
	if ( sirShaderPassSignaturesEqual( recipe.pass, changed.pass ) || sirShaderRecipesEqual( recipe, changed ) ||
	     sirShaderBuildPortableKey( changed, &key ) != SIR_GRAPHICS_SUCCESS ||
	     ( key.nLow == baseline.nLow && key.nHigh == baseline.nHigh ) ) {
		return false;
	}
	changed = recipe;
	changed.nSourceRevision++;
	if ( sirShaderRecipesEqual( recipe, changed ) ) {
		return false;
	}
	sirShaderNativeIdentity_t native = {};
	native.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	native.nVendorId = 0x8086;
	native.nDeviceId = 1;
	native.nDriverVersion = 2;
	native.nCompilerFingerprint = 33;
	native.nDeviceGeneration = 7;
	native.nCacheUuidLow = 0xABCD;
	if ( sirShaderBuildNativeKey( recipe, native, &key ) != SIR_GRAPHICS_SUCCESS || !sirShaderNativeIdentitiesEqual( native, native ) ) {
		return false;
	}
	sirShaderPipelineKey_t driverKey = key;
	native.nDeviceGeneration++;
	if ( sirShaderBuildNativeKey( recipe, native, &key ) != SIR_GRAPHICS_SUCCESS ||
	     ( key.nLow == driverKey.nLow && key.nHigh == driverKey.nHigh ) ) {
		return false;
	}
	sirShaderNativeIdentity_t changedNative = native;
	changedNative.nDriverVersion++;
	return !sirShaderNativeIdentitiesEqual( native, changedNative ) &&
	       sirShaderBuildPortableKey( recipe, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirShaderBuildNativeKey( recipe, native, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testVisibilityAndLease( void ) noexcept
{
	sirShaderVisibilityId_t id = { 0xffffff, 0xffffe, 0xfff, 0xfe };
	uint64_t nPacked = 0;
	if ( sirShaderPackVisibilityId( id, &nPacked ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirShaderVisibilityId_t unpacked = {};
	if ( sirShaderUnpackVisibilityId( nPacked, &unpacked ) != SIR_GRAPHICS_SUCCESS || unpacked.nInstance != id.nInstance ||
	     unpacked.nPrimitive != id.nPrimitive || unpacked.nMaterial != id.nMaterial || unpacked.nFlags != id.nFlags ) {
		return false;
	}
	id.nPrimitive = 0x100000;
	if ( sirShaderPackVisibilityId( id, &nPacked ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderPackVisibilityId( id, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderUnpackVisibilityId( UINT64_MAX, &unpacked ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderGeometryLease_t lease = {};
	lease.nAssetId = 100;
	lease.nSourceRevision = 7;
	lease.nProviderGeneration = 19;
	lease.completion.nDeviceGeneration = 4;
	lease.completion.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COMPUTE );
	lease.completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 10;
	lease.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] = 11;
	if ( sirShaderValidateGeometryLease( lease, 4 ) != SIR_GRAPHICS_SUCCESS || !sirShaderCanReuseGeometry( lease, lease ) ) {
		return false;
	}
	sirShaderGeometryLease_t newer = lease;
	newer.nSourceRevision++;
	if ( sirShaderCanReuseGeometry( lease, newer ) ) {
		return false;
	}
	newer = lease;
	newer.completion.nDeviceGeneration++;
	if ( sirShaderCanReuseGeometry( lease, newer ) ) {
		return false;
	}
	newer = lease;
	newer.completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT]++;
	if ( sirShaderCanReuseGeometry( lease, newer ) ) {
		return false;
	}
	newer = lease;
	newer.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 99;
	return sirShaderValidateGeometryLease( newer, 4 ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirShaderValidateGeometryLease( lease, 5 ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testBudgetsAndRate( void ) noexcept
{
	sirShaderWorkBudget_t budget = { 128, 64, 32, 8, 4096 };
	sirShaderWorkRequest_t request = { 128, 64, 32, 8, 4096 };
	if ( sirShaderAdmitWork( request, budget ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	request.nIndirectCount++;
	if ( sirShaderAdmitWork( request, budget ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ) {
		return false;
	}
	sirShaderRateEligibility_t eligibility = {};
	eligibility.eRequested = SIR_SHADER_RATE_QUARTER;
	eligibility.bDerivativesValid = true;
	eligibility.bHistoryValid = true;
	eligibility.bMotionCoherent = true;
	sirShaderRate_t eRate = SIR_SHADER_RATE_FULL;
	if ( sirShaderResolveRate( eligibility, &eRate ) != SIR_GRAPHICS_SUCCESS || eRate != SIR_SHADER_RATE_QUARTER ) {
		return false;
	}
	eligibility.bAlphaMasked = true;
	if ( sirShaderResolveRate( eligibility, &eRate ) != SIR_GRAPHICS_SUCCESS || eRate != SIR_SHADER_RATE_FULL ) {
		return false;
	}
	eligibility.eRequested = ( sirShaderRate_t )3;
	return sirShaderResolveRate( eligibility, &eRate ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirShaderResolveRate( eligibility, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testResignPackage( uint8_t *pPackage, sirShaderPackageHeader_t& header ) noexcept
{
	const uint64_t nRecords = header.nPayloadOffset - header.nStageOffset;
	if ( sirShaderBuildManifestHash( header, pPackage + header.nStageOffset, nRecords, &header.nManifestHash ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return sirShaderEncodePackageHeader( header, pPackage, SIR_SHADER_PACKAGE_HEADER_BYTES ) == SIR_GRAPHICS_SUCCESS;
}

static bool testRebuildPackage( uint8_t *pPackage, sirShaderPackageHeader_t& header, sirShaderStageRecord_t& stage,
                                sirShaderReflectionBinding_t& binding ) noexcept
{
	if ( sirShaderEncodeBinding( binding, pPackage + header.nBindingOffset, SIR_SHADER_PACKAGE_BINDING_BYTES ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( sirShaderBuildBindingSignature( pPackage + header.nBindingOffset, header.nBindingCount, &stage.nBindingSignature ) !=
	     SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( sirShaderEncodeStage( stage, pPackage + header.nStageOffset, SIR_SHADER_PACKAGE_STAGE_BYTES ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return testResignPackage( pPackage, header );
}

static bool testAllStageCapabilities( void ) noexcept
{
	const uint32_t szStages[] = { SIR_GRAPHICS_STAGE_VERTEX,   SIR_GRAPHICS_STAGE_FRAGMENT,     SIR_GRAPHICS_STAGE_COMPUTE,
	                              SIR_GRAPHICS_STAGE_GEOMETRY, SIR_GRAPHICS_STAGE_TESS_CONTROL, SIR_GRAPHICS_STAGE_TESS_EVALUATION,
	                              SIR_GRAPHICS_STAGE_MESH,     SIR_GRAPHICS_STAGE_TASK };
	for ( uint32_t iStage = 0; iStage < sizeof( szStages ) / sizeof( szStages[0] ); ++iStage ) {
		uint8_t szPackage[144] = {};
		sirShaderPackageHeader_t header = {};
		if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
			return false;
		}
		sirShaderStageRecord_t stage = {};
		sirShaderReflectionBinding_t binding = {};
		if ( sirShaderDecodeStage( szPackage + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS ||
		     sirShaderDecodeBinding( szPackage + 112, 24, &binding ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		stage.nStageMask = szStages[iStage];
		stage.ePass = szStages[iStage] == SIR_GRAPHICS_STAGE_COMPUTE ? SIR_SHADER_PASS_COMPUTE : SIR_SHADER_PASS_COLOR;
		binding.nStageMask = ( uint8_t )szStages[iStage];
		uint64_t nCapabilities = SIR_GRAPHICS_FEATURE_GRAPHICS;
		if ( szStages[iStage] == SIR_GRAPHICS_STAGE_COMPUTE ) {
			nCapabilities = SIR_GRAPHICS_FEATURE_COMPUTE;
		} else if ( szStages[iStage] == SIR_GRAPHICS_STAGE_MESH || szStages[iStage] == SIR_GRAPHICS_STAGE_TASK ) {
			nCapabilities |= SIR_GRAPHICS_FEATURE_MESH_SHADER;
		} else if ( szStages[iStage] == SIR_GRAPHICS_STAGE_GEOMETRY ) {
			nCapabilities |= SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
		} else if ( szStages[iStage] == SIR_GRAPHICS_STAGE_TESS_CONTROL || szStages[iStage] == SIR_GRAPHICS_STAGE_TESS_EVALUATION ) {
			nCapabilities |= SIR_GRAPHICS_FEATURE_TESSELLATION;
		}
		stage.nRequiredFeatures = nCapabilities;
		if ( !testRebuildPackage( szPackage, header, stage, binding ) ||
		     sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, nCapabilities ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		if ( sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, 0 ) != SIR_GRAPHICS_UNSUPPORTED ) {
			return false;
		}
		stage.nRequiredFeatures = 0;
		if ( !testRebuildPackage( szPackage, header, stage, binding ) ||
		     sirShaderValidatePackage( szPackage, 144, SIR_GRAPHICS_SHADER_SPIRV, 0 ) != SIR_GRAPHICS_UNSUPPORTED ) {
			return false;
		}
	}
	return true;
}

static bool testHeaderBoundaryFailures( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	for ( uint32_t iField = 0; iField < 13; ++iField ) {
		sirShaderPackageHeader_t altered = header;
		switch ( iField ) {
		case 0:
			altered.nMagic = 0;
			break;
		case 1:
			altered.nSchema = 8;
			break;
		case 2:
			altered.nHeaderBytes = 0;
			break;
		case 3:
			altered.nTotalBytes = SIR_SHADER_PACKAGE_MAX_BYTES + 1;
			break;
		case 4:
			altered.nStageCount = 0;
			break;
		case 5:
			altered.nStageCount = 33;
			break;
		case 6:
			altered.nBindingCount = 1025;
			break;
		case 7:
			altered.nStageOffset = 65;
			break;
		case 8:
			altered.nBindingOffset = 111;
			break;
		case 9:
			altered.nPayloadOffset = 135;
			break;
		case 10:
			altered.nPayloadBytes = 3;
			break;
		case 11:
			altered.nAssetId = 0;
			break;
		case 12:
			altered.nTargetFingerprint = 0;
			break;
		}
		if ( sirShaderEncodePackageHeader( altered, szPackage, 64 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	sirShaderPackageHeader_t altered = header;
	altered.nSourceRevision = 0;
	if ( sirShaderEncodePackageHeader( altered, szPackage, 64 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = header;
	altered.nManifestHash = 0;
	return sirShaderEncodePackageHeader( altered, szPackage, 64 ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirShaderEncodePackageHeader( header, nullptr, 64 ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirShaderEncodePackageHeader( header, szPackage, 63 ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testReflectionIntegrity( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	const uint32_t szOffsets[] = { 4, 8, 9, 12, 14, 16 };
	const uint8_t szValues[] = { 0xff, 0xff, 0, 0, 1, 0 };
	for ( uint32_t iField = 0; iField < sizeof( szOffsets ) / sizeof( szOffsets[0] ); ++iField ) {
		uint8_t szMutation[144] = {};
		memcpy( szMutation, szPackage, sizeof( szMutation ) );
		szMutation[header.nBindingOffset + szOffsets[iField]] = szValues[iField];
		if ( !testResignPackage( szMutation, header ) ) {
			return false;
		}
		if ( sirShaderValidatePackage( szMutation, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
		     SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	uint8_t szMutation[144] = {};
	memcpy( szMutation, szPackage, sizeof( szMutation ) );
	szMutation[121] = SIR_GRAPHICS_STAGE_FRAGMENT;
	if ( !testResignPackage( szMutation, header ) ||
	     sirShaderValidatePackage( szMutation, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	         SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	return sirShaderBuildBindingSignature( nullptr, 1, &header.nManifestHash ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirShaderBuildBindingSignature( nullptr, 0, &header.nManifestHash ) == SIR_GRAPHICS_SUCCESS &&
	       sirShaderBuildBindingSignature( szPackage + 112, 1025, &header.nManifestHash ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testStageAndKeyFailures( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	sirShaderStageRecord_t stage = {};
	if ( sirShaderDecodeStage( szPackage + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	stage.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_COMPUTE;
	if ( sirShaderEncodeStage( stage, szPackage + 64, 48 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	stage.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	if ( sirShaderEncodeStage( stage, szPackage + 64, 48 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	stage.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	stage.ePass = SIR_SHADER_PASS_COMPUTE;
	if ( sirShaderEncodeStage( stage, szPackage + 64, 48 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	stage.ePass = SIR_SHADER_PASS_COLOR;
	stage.nBinaryHash = 0;
	if ( sirShaderEncodeStage( stage, szPackage + 64, 48 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderPipelineRecipe_t recipe = {};
	recipe.pass.ePass = SIR_SHADER_PASS_COMPUTE;
	recipe.pass.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	recipe.pass.nSampleCount = 1;
	recipe.pass.nBindingSignature = 1;
	recipe.nAssetId = 1;
	recipe.nSourceRevision = 1;
	recipe.nPackageManifestHash = 0x10;
	recipe.nPackageSchema = SIR_SHADER_PACKAGE_SCHEMA;
	sirShaderPipelineKey_t key = {};
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	recipe.pass.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	recipe.pass.nSampleCount = 0;
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	recipe.pass.nSampleCount = 2;
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	recipe.pass.nSampleCount = 1;
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirShaderNativeIdentity_t native = {};
	native.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	native.nDeviceGeneration = 1;
	native.nCompilerFingerprint = 1;
	if ( sirShaderBuildNativeKey( recipe, native, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	native.eBackend = SIR_GRAPHICS_BACKEND_D3D12;
	native.nDeviceGeneration = 0;
	return sirShaderBuildNativeKey( recipe, native, &key ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testMultiBindingOrdering( void ) noexcept
{
	uint8_t szPackage[168] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, 144, &header ) ) {
		return false;
	}
	memmove( szPackage + 160, szPackage + 136, 8 );
	memset( szPackage + 136, 0, 24 );
	header.nBindingCount = 2;
	header.nPayloadOffset = 160;
	header.nTotalBytes = 168;
	sirShaderStageRecord_t stage = {};
	sirShaderReflectionBinding_t binding = {};
	if ( sirShaderDecodeStage( szPackage + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderDecodeBinding( szPackage + 112, 24, &binding ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	stage.nPayloadOffset = 160;
	binding.nBinding = 1;
	binding.nNameHash = 0x9910;
	if ( sirShaderEncodeBinding( binding, szPackage + 136, 24 ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderBuildBindingSignature( szPackage + 112, 2, &stage.nBindingSignature ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderEncodeStage( stage, szPackage + 64, 48 ) != SIR_GRAPHICS_SUCCESS || !testResignPackage( szPackage, header ) ||
	     sirShaderValidatePackage( szPackage, 168, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	// Reordering one binding changes both the canonical key and its validation.
	szPackage[138] = 0;
	if ( sirShaderBuildBindingSignature( szPackage + 112, 2, &stage.nBindingSignature ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderEncodeStage( stage, szPackage + 64, 48 ) != SIR_GRAPHICS_SUCCESS || !testResignPackage( szPackage, header ) ||
	     sirShaderValidatePackage( szPackage, 168, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	         SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	return true;
}

static bool testDeepPackageValidation( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	const uint32_t szStageMutationOffsets[] = { 6, 32, 40, 3 };
	const uint8_t szStageMutationValues[] = { 0xff, 0, 0, 0x80 };
	for ( uint32_t iMutation = 0; iMutation < sizeof( szStageMutationOffsets ) / sizeof( szStageMutationOffsets[0] ); ++iMutation ) {
		uint8_t szMutated[144] = {};
		memcpy( szMutated, szPackage, sizeof( szMutated ) );
		szMutated[header.nStageOffset + szStageMutationOffsets[iMutation]] = szStageMutationValues[iMutation];
		if ( !testResignPackage( szMutated, header ) ||
		     sirShaderValidatePackage( szMutated, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
		         SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	// Tamper with valid reflection, then recompute both binding signature and manifest
	// so validation reaches the typed reflection and cross-stage checks.
	const uint32_t szBindingOffsets[] = { 4, 8, 9, 12, 14, 16 };
	const uint8_t szBindingValues[] = { 0xff, 0xff, 0, 0, 1, 0 };
	for ( uint32_t iMutation = 0; iMutation < sizeof( szBindingOffsets ) / sizeof( szBindingOffsets[0] ); ++iMutation ) {
		uint8_t szMutated[144] = {};
		memcpy( szMutated, szPackage, sizeof( szMutated ) );
		szMutated[header.nBindingOffset + szBindingOffsets[iMutation]] = szBindingValues[iMutation];
		if ( szBindingOffsets[iMutation] == 16 ) {
			szMutated[header.nBindingOffset + 17] = 0;
		}
		sirShaderStageRecord_t stage = {};
		if ( sirShaderDecodeStage( szMutated + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS ||
		     sirShaderBuildBindingSignature( szMutated + 112, 1, &stage.nBindingSignature ) != SIR_GRAPHICS_SUCCESS ||
		     sirShaderEncodeStage( stage, szMutated + 64, 48 ) != SIR_GRAPHICS_SUCCESS || !testResignPackage( szMutated, header ) ||
		     sirShaderValidatePackage( szMutated, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
		         SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	return sirShaderValidatePackage( szPackage, 144, ( sirGraphicsShaderFormat_t )99, SIR_GRAPHICS_FEATURE_GRAPHICS ) ==
	       SIR_GRAPHICS_UNSUPPORTED;
}

static bool testLeaseAndIdsBoundary( void ) noexcept
{
	sirShaderVisibilityId_t id = { 0xffffffU, 0xfffffU, 0xfffU, 0xffU };
	uint64_t nPacked = 0;
	if ( sirShaderPackVisibilityId( id, &nPacked ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderUnpackVisibilityId( 1, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	id.nInstance++;
	if ( sirShaderPackVisibilityId( id, &nPacked ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	id = { 0, 0, 0, 0 };
	if ( sirShaderPackVisibilityId( id, &nPacked ) != SIR_GRAPHICS_SUCCESS || nPacked != 0 ) {
		return false;
	}
	sirShaderGeometryLease_t lease = {};
	lease.nAssetId = 1;
	lease.nSourceRevision = 1;
	lease.nProviderGeneration = 1;
	lease.completion.nDeviceGeneration = 2;
	if ( sirShaderValidateGeometryLease( lease, 2 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	lease.completion.nQueueMask = ( 1U << 12 );
	if ( sirShaderValidateGeometryLease( lease, 2 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	lease.completion.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	if ( sirShaderValidateGeometryLease( lease, 2 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	lease.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 9;
	if ( sirShaderValidateGeometryLease( lease, 2 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirShaderGeometryLease_t newer = lease;
	newer.nProviderGeneration++;
	if ( sirShaderCanReuseGeometry( lease, newer ) ) {
		return false;
	}
	newer = lease;
	newer.nAssetId++;
	if ( sirShaderCanReuseGeometry( lease, newer ) ) {
		return false;
	}
	lease.nAssetId = 0;
	return sirShaderValidateGeometryLease( lease, 2 ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testOtherNegativePaths( void ) noexcept
{
	uint8_t szBytes[64] = {};
	sirShaderStageRecord_t stage = {};
	sirShaderReflectionBinding_t binding = {};
	if ( sirShaderEncodeStage( stage, nullptr, 48 ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodeStage( szBytes, 48, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderEncodeBinding( binding, nullptr, 24 ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderDecodeBinding( nullptr, 24, &binding ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderPackageHeader_t header = {};
	if ( sirShaderBuildManifestHash( header, nullptr, 0, &header.nManifestHash ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderBuildBindingSignature( szBytes, 1, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderPipelineRecipe_t recipe = {};
	recipe.pass.ePass = SIR_SHADER_PASS_COMPUTE;
	recipe.pass.nBindingSignature = 1;
	recipe.pass.nStageMask = 0xffffffff;
	recipe.pass.nSampleCount = 1;
	recipe.nAssetId = 1;
	recipe.nSourceRevision = 1;
	recipe.nPackageManifestHash = 0x10;
	recipe.nPackageSchema = SIR_SHADER_PACKAGE_SCHEMA;
	sirShaderPipelineKey_t key = {};
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderNativeIdentity_t native = {};
	native.eBackend = SIR_GRAPHICS_BACKEND_D3D12;
	native.nCompilerFingerprint = 1;
	native.nDeviceGeneration = 1;
	if ( sirShaderBuildNativeKey( recipe, native, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderRateEligibility_t eligibility = {};
	eligibility.eRequested = SIR_SHADER_RATE_HALF;
	eligibility.bDerivativesValid = false;
	eligibility.bHistoryValid = true;
	eligibility.bMotionCoherent = true;
	sirShaderRate_t eRate = SIR_SHADER_RATE_QUARTER;
	if ( sirShaderResolveRate( eligibility, &eRate ) != SIR_GRAPHICS_SUCCESS || eRate != SIR_SHADER_RATE_FULL ) {
		return false;
	}
	eligibility.bDerivativesValid = true;
	eligibility.bHistoryValid = false;
	if ( sirShaderResolveRate( eligibility, &eRate ) != SIR_GRAPHICS_SUCCESS || eRate != SIR_SHADER_RATE_FULL ) {
		return false;
	}
	eligibility.bHistoryValid = true;
	eligibility.bMotionCoherent = false;
	if ( sirShaderResolveRate( eligibility, &eRate ) != SIR_GRAPHICS_SUCCESS || eRate != SIR_SHADER_RATE_FULL ) {
		return false;
	}
	eligibility.bMotionCoherent = true;
	eligibility.bDiscontinuity = true;
	return sirShaderResolveRate( eligibility, &eRate ) == SIR_GRAPHICS_SUCCESS && eRate == SIR_SHADER_RATE_FULL;
}

static bool testEqualityFieldMatrix( void ) noexcept
{
	sirShaderMaterialPassSignature_t pass = {};
	pass.ePass = SIR_SHADER_PASS_COLOR;
	pass.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	pass.nColorFormatsHash = 1;
	pass.nBindingSignature = 2;
	pass.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	pass.nSampleCount = 1;
	if ( !sirShaderPassSignaturesEqual( pass, pass ) ) {
		return false;
	}
	for ( uint32_t iField = 0; iField < 9; ++iField ) {
		sirShaderMaterialPassSignature_t altered = pass;
		switch ( iField ) {
		case 0:
			altered.ePass = SIR_SHADER_PASS_DEPTH;
			break;
		case 1:
			altered.eDepthFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
			break;
		case 2:
			altered.nColorFormatsHash++;
			break;
		case 3:
			altered.nBindingSignature++;
			break;
		case 4:
			altered.nStageMask++;
			break;
		case 5:
			altered.nSampleCount++;
			break;
		case 6:
			altered.bAlphaMasked = true;
			break;
		case 7:
			altered.bDoubleSided = true;
			break;
		case 8:
			altered.bWritesVelocity = true;
			break;
		}
		if ( sirShaderPassSignaturesEqual( pass, altered ) ) {
			return false;
		}
	}
	sirShaderPipelineRecipe_t recipe = {};
	recipe.pass = pass;
	recipe.nAssetId = 1;
	recipe.nSourceRevision = 2;
	recipe.nPackageManifestHash = 3;
	recipe.nPackageSchema = SIR_SHADER_PACKAGE_SCHEMA;
	recipe.nPermutationKey = 4;
	recipe.nRenderStateHash = 5;
	recipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( !sirShaderRecipesEqual( recipe, recipe ) ) {
		return false;
	}
	for ( uint32_t iField = 0; iField < 8; ++iField ) {
		sirShaderPipelineRecipe_t altered = recipe;
		switch ( iField ) {
		case 0:
			altered.pass.bAlphaMasked = true;
			break;
		case 1:
			altered.nAssetId++;
			break;
		case 2:
			altered.nSourceRevision++;
			break;
		case 3:
			altered.nPackageManifestHash++;
			break;
		case 4:
			altered.nPackageSchema++;
			break;
		case 5:
			altered.nPermutationKey++;
			break;
		case 6:
			altered.nRenderStateHash++;
			break;
		case 7:
			altered.nRequiredFeatures++;
			break;
		}
		if ( sirShaderRecipesEqual( recipe, altered ) ) {
			return false;
		}
	}
	sirShaderNativeIdentity_t native = {};
	native.eBackend = SIR_GRAPHICS_BACKEND_D3D12;
	native.nVendorId = 1;
	native.nDeviceId = 2;
	native.nDriverVersion = 3;
	native.nDeviceGeneration = 4;
	native.nCompilerFingerprint = 5;
	native.nCacheUuidLow = 6;
	native.nCacheUuidHigh = 7;
	if ( !sirShaderNativeIdentitiesEqual( native, native ) ) {
		return false;
	}
	for ( uint32_t iField = 0; iField < 8; ++iField ) {
		sirShaderNativeIdentity_t altered = native;
		switch ( iField ) {
		case 0:
			altered.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
			break;
		case 1:
			altered.nVendorId++;
			break;
		case 2:
			altered.nDeviceId++;
			break;
		case 3:
			altered.nDriverVersion++;
			break;
		case 4:
			altered.nDeviceGeneration++;
			break;
		case 5:
			altered.nCompilerFingerprint++;
			break;
		case 6:
			altered.nCacheUuidLow++;
			break;
		case 7:
			altered.nCacheUuidHigh++;
			break;
		}
		if ( sirShaderNativeIdentitiesEqual( native, altered ) ) {
			return false;
		}
	}
	return true;
}

static bool testInvalidRecipeMatrix( void ) noexcept
{
	sirShaderPipelineRecipe_t recipe = {};
	recipe.pass.ePass = SIR_SHADER_PASS_COLOR;
	recipe.pass.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	recipe.pass.nSampleCount = 1;
	recipe.pass.nBindingSignature = 1;
	recipe.nAssetId = 1;
	recipe.nSourceRevision = 1;
	recipe.nPackageManifestHash = 1;
	recipe.nPackageSchema = SIR_SHADER_PACKAGE_SCHEMA;
	sirShaderPipelineKey_t key = {};
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	for ( uint32_t iField = 0; iField < 10; ++iField ) {
		sirShaderPipelineRecipe_t altered = recipe;
		switch ( iField ) {
		case 0:
			altered.pass.ePass = ( sirShaderPass_t )0;
			break;
		case 1:
			altered.pass.ePass = ( sirShaderPass_t )7;
			break;
		case 2:
			altered.nAssetId = 0;
			break;
		case 3:
			altered.nSourceRevision = 0;
			break;
		case 4:
			altered.nPackageManifestHash = 0;
			break;
		case 5:
			altered.nPackageSchema++;
			break;
		case 6:
			altered.pass.nBindingSignature = 0;
			break;
		case 7:
			altered.pass.nStageMask = 0;
			break;
		case 8:
			altered.pass.nSampleCount = 0;
			break;
		case 9:
			altered.pass.nSampleCount = 3;
			break;
		}
		if ( sirShaderBuildPortableKey( altered, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	sirShaderNativeIdentity_t native = {};
	native.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	native.nDeviceGeneration = 1;
	native.nCompilerFingerprint = 1;
	for ( uint32_t iField = 0; iField < 3; ++iField ) {
		sirShaderNativeIdentity_t altered = native;
		switch ( iField ) {
		case 0:
			altered.eBackend = ( sirGraphicsBackend_t )99;
			break;
		case 1:
			altered.nDeviceGeneration = 0;
			break;
		case 2:
			altered.nCompilerFingerprint = 0;
			break;
		}
		if ( sirShaderBuildNativeKey( recipe, altered, &key ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	return true;
}

static bool testBudgetAndLeaseMatrix( void ) noexcept
{
	sirShaderWorkBudget_t budget = { 8, 8, 8, 8, 8 };
	sirShaderWorkRequest_t request = { 8, 8, 8, 8, 8 };
	for ( uint32_t iField = 0; iField < 5; ++iField ) {
		sirShaderWorkRequest_t altered = request;
		switch ( iField ) {
		case 0:
			altered.nVisibleCount++;
			break;
		case 1:
			altered.nMaterialBinCount++;
			break;
		case 2:
			altered.nIndirectCount++;
			break;
		case 3:
			altered.nOptionalDispatchCount++;
			break;
		case 4:
			altered.nScratchBytes++;
			break;
		}
		if ( sirShaderAdmitWork( altered, budget ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ) {
			return false;
		}
	}
	sirShaderGeometryLease_t lease = {};
	lease.nAssetId = 10;
	lease.nSourceRevision = 20;
	lease.nProviderGeneration = 30;
	lease.completion.nDeviceGeneration = 2;
	for ( uint32_t iField = 0; iField < 5; ++iField ) {
		sirShaderGeometryLease_t altered = lease;
		uint32_t nDeviceGeneration = 2;
		switch ( iField ) {
		case 0:
			nDeviceGeneration = 0;
			break;
		case 1:
			altered.nAssetId = 0;
			break;
		case 2:
			altered.nSourceRevision = 0;
			break;
		case 3:
			altered.nProviderGeneration = 0;
			break;
		case 4:
			altered.completion.nDeviceGeneration = 3;
			break;
		}
		if ( sirShaderValidateGeometryLease( altered, nDeviceGeneration ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	return true;
}

static bool testInvalidRecordMatrix( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	sirShaderStageRecord_t stage = {};
	sirShaderReflectionBinding_t binding = {};
	if ( sirShaderDecodeStage( szPackage + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderDecodeBinding( szPackage + 112, 24, &binding ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	for ( uint32_t iField = 0; iField < 12; ++iField ) {
		sirShaderStageRecord_t altered = stage;
		switch ( iField ) {
		case 0:
			altered.nStageMask = 0;
			break;
		case 1:
			altered.nStageMask = 0x100;
			break;
		case 2:
			altered.ePass = ( sirShaderPass_t )0;
			break;
		case 3:
			altered.ePass = ( sirShaderPass_t )7;
			break;
		case 4:
			altered.eFormat = ( sirGraphicsShaderFormat_t )0;
			break;
		case 5:
			altered.eFormat = ( sirGraphicsShaderFormat_t )99;
			break;
		case 6:
			altered.nBinaryHash = 0;
			break;
		case 7:
			altered.nEntryPointHash = 0;
			break;
		case 8:
			altered.nBindingSignature = 0;
			break;
		case 9:
			altered.nPayloadBytes = 3;
			break;
		case 10:
			altered.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
			break;
		case 11:
			altered.ePass = SIR_SHADER_PASS_COMPUTE;
			break;
		}
		if ( sirShaderEncodeStage( altered, szPackage + 64, 48 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	for ( uint32_t iField = 0; iField < 6; ++iField ) {
		sirShaderReflectionBinding_t altered = binding;
		switch ( iField ) {
		case 0:
			altered.eType = ( sirGraphicsBindingType_t )99;
			break;
		case 1:
			altered.eFrequency = ( sirShaderBindingFrequency_t )99;
			break;
		case 2:
			altered.nArrayCount = 0;
			break;
		case 3:
			altered.nStageMask = 0;
			break;
		case 4:
			altered.nNameHash = 0;
			break;
		case 5:
			altered.nFlags = 2;
			break;
		}
		if ( sirShaderEncodeBinding( altered, szPackage + 112, 24 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	sirShaderPackageHeader_t altered = header;
	altered.nPayloadBytes = 3;
	altered.nTotalBytes = altered.nPayloadOffset + altered.nPayloadBytes;
	if ( sirShaderEncodePackageHeader( altered, szPackage, 64 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	if ( sirShaderEncodeStage( stage, szPackage + 64, 47 ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderEncodeBinding( binding, szPackage + 112, 23 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	return true;
}

static bool testManifestInputMatrix( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	for ( uint32_t iField = 0; iField < 7; ++iField ) {
		sirShaderPackageHeader_t altered = header;
		uint64_t nRecords = 72;
		switch ( iField ) {
		case 0:
			altered.nStageCount = 0;
			break;
		case 1:
			altered.nStageCount = 33;
			break;
		case 2:
			altered.nBindingCount = 1025;
			break;
		case 3:
			nRecords = 71;
			break;
		case 4:
			altered.nAssetId = 0;
			break;
		case 5:
			altered.nSourceRevision = 0;
			break;
		case 6:
			altered.nTargetFingerprint = 0;
			break;
		}
		uint64_t nHash = 0;
		if ( sirShaderBuildManifestHash( altered, szPackage + 64, nRecords, &nHash ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			return false;
		}
	}
	return true;
}

static bool testBindingLexicographicOrders( void ) noexcept
{
	uint8_t szBase[168] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szBase, 144, &header ) ) {
		return false;
	}
	memmove( szBase + 160, szBase + 136, 8 );
	header.nBindingCount = 2;
	header.nPayloadOffset = 160;
	header.nTotalBytes = 168;
	sirShaderStageRecord_t stage = {};
	sirShaderReflectionBinding_t first = {};
	if ( sirShaderDecodeStage( szBase + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderDecodeBinding( szBase + 112, 24, &first ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	stage.nPayloadOffset = 160;
	for ( uint32_t iOrder = 0; iOrder < 7; ++iOrder ) {
		uint8_t szPackage[168] = {};
		memcpy( szPackage, szBase, sizeof( szPackage ) );
		sirShaderReflectionBinding_t firstCase = first;
		sirShaderReflectionBinding_t second = first;
		second.nNameHash++;
		switch ( iOrder ) {
		case 0:
			firstCase.nSet = 2;
			second.nSet = 1;
			break;
		case 1:
			firstCase.nSet = 1;
			second.nSet = 2;
			break;
		case 2:
			firstCase.nSpace = 2;
			second.nSpace = 1;
			break;
		case 3:
			firstCase.nSpace = 1;
			second.nSpace = 2;
			break;
		case 4:
			firstCase.nBinding = 2;
			second.nBinding = 1;
			break;
		case 5:
			firstCase.nBinding = 1;
			second.nBinding = 2;
			break;
		case 6:
			firstCase.nBinding = 0;
			second.nBinding = 0;
			break;
		}
		if ( sirShaderEncodeBinding( firstCase, szPackage + 112, 24 ) != SIR_GRAPHICS_SUCCESS ||
		     sirShaderEncodeBinding( second, szPackage + 136, 24 ) != SIR_GRAPHICS_SUCCESS ||
		     sirShaderBuildBindingSignature( szPackage + 112, 2, &stage.nBindingSignature ) != SIR_GRAPHICS_SUCCESS ||
		     sirShaderEncodeStage( stage, szPackage + 64, 48 ) != SIR_GRAPHICS_SUCCESS || !testResignPackage( szPackage, header ) ) {
			return false;
		}
		const bool bExpectedValid = iOrder == 1 || iOrder == 3 || iOrder == 5;
		const sirGraphicsResult_t eResult =
		    sirShaderValidatePackage( szPackage, 168, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS );
		if ( ( eResult == SIR_GRAPHICS_SUCCESS ) != bExpectedValid ) {
			return false;
		}
	}
	return true;
}

static bool testRemainingNegativeEdges( void ) noexcept
{
	uint8_t szPackage[144] = {};
	sirShaderPackageHeader_t header = {};
	if ( !testBuildPackage( szPackage, sizeof( szPackage ), &header ) ) {
		return false;
	}
	if ( sirShaderValidatePackage( szPackage, 144, ( sirGraphicsShaderFormat_t )0, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	     SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	// Valid record and signed manifest, but its bytecode view extends beyond
	// the package payload.
	uint8_t szMutated[144] = {};
	memcpy( szMutated, szPackage, 144 );
	szMutated[header.nStageOffset + 32] = 143;
	if ( !testResignPackage( szMutated, header ) ||
	     sirShaderValidatePackage( szMutated, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	         SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	memcpy( szMutated, szPackage, 144 );
	szMutated[header.nBindingOffset + 9] = SIR_GRAPHICS_STAGE_FRAGMENT;
	sirShaderStageRecord_t stage = {};
	if ( sirShaderDecodeStage( szMutated + 64, 48, &stage ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderBuildBindingSignature( szMutated + 112, 1, &stage.nBindingSignature ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderEncodeStage( stage, szMutated + 64, 48 ) != SIR_GRAPHICS_SUCCESS || !testResignPackage( szMutated, header ) ||
	     sirShaderValidatePackage( szMutated, 144, SIR_GRAPHICS_SHADER_SPIRV, SIR_GRAPHICS_FEATURE_GRAPHICS ) !=
	         SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderVisibilityId_t id = { 0, 0, 0x1000, 0 };
	uint64_t nPacked = 0;
	if ( sirShaderPackVisibilityId( id, &nPacked ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirShaderGeometryLease_t lease = {};
	lease.nAssetId = 1;
	lease.nSourceRevision = 1;
	lease.nProviderGeneration = 1;
	lease.completion.nDeviceGeneration = 4;
	sirShaderGeometryLease_t changed = lease;
	changed.nSourceRevision++;
	if ( sirShaderCanReuseGeometry( lease, changed ) ) {
		return false;
	}
	changed = lease;
	changed.nAssetId = 0;
	if ( sirShaderCanReuseGeometry( changed, lease ) ) {
		return false;
	}
	sirShaderRateEligibility_t eligibility = {};
	eligibility.eRequested = SIR_SHADER_RATE_FULL;
	sirShaderRate_t eRate = SIR_SHADER_RATE_QUARTER;
	return sirShaderResolveRate( eligibility, &eRate ) == SIR_GRAPHICS_SUCCESS && eRate == SIR_SHADER_RATE_FULL;
}

typedef struct sirShaderContractTestCase_s
{
	const char *pszName;
	bool ( *pFunction )( void ) noexcept;
} sirShaderContractTestCase_t;

int main( void )
{
	const sirShaderContractTestCase_t szCases[] = { { "wire round trips", testWireRoundTrips },
	                                                { "corrupt package", testCorruptPackage },
	                                                { "record negatives", testInvalidRecordEncodings },
	                                                { "pipeline keys", testPipelineKeys },
	                                                { "visibility and leases", testVisibilityAndLease },
	                                                { "work and rate", testBudgetsAndRate },
	                                                { "stage capabilities", testAllStageCapabilities },
	                                                { "header failures", testHeaderBoundaryFailures },
	                                                { "reflection integrity", testReflectionIntegrity },
	                                                { "stage and key failures", testStageAndKeyFailures },
	                                                { "multiple binding ordering", testMultiBindingOrdering },
	                                                { "deep validation", testDeepPackageValidation },
	                                                { "lease and ids boundary", testLeaseAndIdsBoundary },
	                                                { "other negatives", testOtherNegativePaths },
	                                                { "equality field matrix", testEqualityFieldMatrix },
	                                                { "invalid recipe matrix", testInvalidRecipeMatrix },
	                                                { "budgets and leases matrix", testBudgetAndLeaseMatrix },
	                                                { "invalid records matrix", testInvalidRecordMatrix },
	                                                { "manifest input matrix", testManifestInputMatrix },
	                                                { "binding order matrix", testBindingLexicographicOrders },
	                                                { "last negative edges", testRemainingNegativeEdges } };
	for ( uint32_t iCase = 0; iCase < sizeof( szCases ) / sizeof( szCases[0] ); ++iCase ) {
		if ( !szCases[iCase].pFunction() ) {
			printf( "FAIL: %s\n", szCases[iCase].pszName );
			return 1;
		}
	}
	puts( "SIRShaderContractsTests passed" );
	return 0;
}
