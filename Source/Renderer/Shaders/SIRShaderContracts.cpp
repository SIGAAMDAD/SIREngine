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

static void sirShaderWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 2; ++iByte ) {
		pBytes[iByte] = static_cast<uint8_t>(  nValue >> ( iByte * 8 )  );
	}
}

static void sirShaderWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = static_cast<uint8_t>(  nValue >> ( iByte * 8 )  );
	}
}

static void sirShaderWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		pBytes[iByte] = static_cast<uint8_t>(  nValue >> ( iByte * 8 )  );
	}
}

static uint16_t sirShaderRead16( const uint8_t *pBytes ) noexcept
{
	return static_cast<uint16_t>(pBytes[0]) | ( static_cast<uint16_t>(pBytes[1]) << 8 );
}

static uint32_t sirShaderRead32( const uint8_t *pBytes ) noexcept
{
	uint32_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		nValue |= static_cast<uint32_t>(pBytes[iByte]) << ( iByte * 8 );
	}
	return nValue;
}

static uint64_t sirShaderRead64( const uint8_t *pBytes ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nValue |= static_cast<uint64_t>(pBytes[iByte]) << ( iByte * 8 );
	}
	return nValue;
}

static uint64_t sirShaderHashBytes( uint64_t nHash, const void *pBytes, uint64_t nCount ) noexcept
{
	const uint8_t *pData = static_cast<const uint8_t *>(pBytes);
	constexpr uint64_t HASH_PRIME = 1099511628211ULL;
	for ( uint64_t iByte = 0; iByte < nCount; ++iByte ) {
		nHash ^= pData[iByte];
		nHash *= HASH_PRIME;
	}
	return nHash;
}

static uint64_t sirShaderHashScalar( uint64_t nHash, uint64_t nValue ) noexcept
{
	uint8_t szBytes[8];
	memset( szBytes, 0, sizeof( szBytes ) );
	sirShaderWrite64( szBytes, nValue );
	return sirShaderHashBytes( nHash, szBytes, sizeof( szBytes ) );
}

static bool sirShaderStageMaskValid( uint32_t nStageMask ) noexcept
{
	constexpr uint32_t STAGE_MASK = ( static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK) << 1 ) - 1;
	return nStageMask != 0 && ( nStageMask & ~STAGE_MASK ) == 0 && ( nStageMask & ( nStageMask - 1 ) ) == 0;
}

static bool sirShaderHeaderValid( const sirShaderPackageHeader_t& header ) noexcept
{
	if ( header.nMagic != SIR_SHADER_PACKAGE_MAGIC || header.nSchema != SIR_SHADER_PACKAGE_SCHEMA ||
	     header.nHeaderBytes != SIR_SHADER_PACKAGE_HEADER_BYTES || header.nTotalBytes > SIR_SHADER_PACKAGE_MAX_BYTES ) {
		return false;
	}
	if ( header.nAssetId == 0 || header.nSourceRevision == 0 || header.nManifestHash == 0 || header.nTargetFingerprint == 0 ||
	     header.nStageCount == 0 || header.nStageCount > SIR_SHADER_PACKAGE_MAX_STAGES ||
	     header.nBindingCount > SIR_SHADER_PACKAGE_MAX_BINDINGS ) {
		return false;
	}
	const uint64_t nBindingStart =
	    static_cast<uint64_t>(SIR_SHADER_PACKAGE_HEADER_BYTES) + static_cast<uint64_t>(header.nStageCount) * SIR_SHADER_PACKAGE_STAGE_BYTES;
	const uint64_t nPayloadStart = nBindingStart + static_cast<uint64_t>(header.nBindingCount) * SIR_SHADER_PACKAGE_BINDING_BYTES;
	return header.nStageOffset == SIR_SHADER_PACKAGE_HEADER_BYTES && header.nBindingOffset == nBindingStart &&
	       header.nPayloadOffset == nPayloadStart && static_cast<uint64_t>(header.nPayloadOffset) + header.nPayloadBytes == header.nTotalBytes &&
	       header.nPayloadBytes >= 4;
}

static bool sirShaderStageRecordValid( const sirShaderStageRecord_t& stage ) noexcept
{
	if ( !sirShaderStageMaskValid( stage.nStageMask ) || stage.ePass < SIRShaderPass::SIR_SHADER_PASS_COLOR || stage.ePass > SIRShaderPass::SIR_SHADER_PASS_COMPUTE ||
	     stage.eFormat < SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL ||
	     ( stage.eFormat > SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS || stage.eFormat == SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_FIXED_FUNCTION ) ) {
		return false;
	}
	if ( stage.nBinaryHash == 0 || stage.nEntryPointHash == 0 || stage.nBindingSignature == 0 || stage.nPayloadBytes < 4 ) {
		return false;
	}
	return ( stage.ePass == SIRShaderPass::SIR_SHADER_PASS_COMPUTE ) == ( stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE) );
}

static bool sirShaderBindingRecordValid( const sirShaderReflectionBinding_t& binding ) noexcept
{
	constexpr uint32_t VALID_BINDING_MASK = ( static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK) << 1 ) - 1;
	return binding.eType <= SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_SAMPLER && binding.eFrequency <= SIRShaderBindingFrequency::SIR_SHADER_BINDING_DRAW && binding.nArrayCount != 0 &&
	       binding.nStageMask != 0 && ( binding.nStageMask & ~VALID_BINDING_MASK ) == 0 && binding.nNameHash != 0 && binding.nFlags == 0;
}

SIRGraphicsResult sirShaderEncodePackageHeader( const sirShaderPackageHeader_t& header, void *pBytes, uint64_t nCapacity ) noexcept
{
	if ( pBytes == nullptr || nCapacity < SIR_SHADER_PACKAGE_HEADER_BYTES || !sirShaderHeaderValid( header ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint8_t *pData = static_cast<uint8_t *>(pBytes);
	sirShaderWrite32( pData, header.nMagic );
	sirShaderWrite16( pData + 4, header.nSchema );
	sirShaderWrite16( pData + 6, header.nHeaderBytes );
	sirShaderWrite32( pData + 8, header.nTotalBytes );
	sirShaderWrite16( pData + 12, header.nStageCount );
	sirShaderWrite16( pData + 14, header.nBindingCount );
	sirShaderWrite32( pData + 16, header.nStageOffset );
	sirShaderWrite32( pData + 20, header.nBindingOffset );
	sirShaderWrite32( pData + 24, header.nPayloadOffset );
	sirShaderWrite32( pData + 28, header.nPayloadBytes );
	sirShaderWrite64( pData + 32, header.nAssetId );
	sirShaderWrite64( pData + 40, header.nSourceRevision );
	sirShaderWrite64( pData + 48, header.nManifestHash );
	sirShaderWrite64( pData + 56, header.nTargetFingerprint );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderDecodePackageHeader( const void *pBytes, uint64_t nLength, sirShaderPackageHeader_t *pHeader ) noexcept
{
	if ( pBytes == nullptr || pHeader == nullptr || nLength < SIR_SHADER_PACKAGE_HEADER_BYTES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint8_t *pData = static_cast<const uint8_t *>(pBytes);
	sirShaderPackageHeader_t header {};
	header.nMagic = sirShaderRead32( pData );
	header.nSchema = sirShaderRead16( pData + 4 );
	header.nHeaderBytes = sirShaderRead16( pData + 6 );
	header.nTotalBytes = sirShaderRead32( pData + 8 );
	header.nStageCount = sirShaderRead16( pData + 12 );
	header.nBindingCount = sirShaderRead16( pData + 14 );
	header.nStageOffset = sirShaderRead32( pData + 16 );
	header.nBindingOffset = sirShaderRead32( pData + 20 );
	header.nPayloadOffset = sirShaderRead32( pData + 24 );
	header.nPayloadBytes = sirShaderRead32( pData + 28 );
	header.nAssetId = sirShaderRead64( pData + 32 );
	header.nSourceRevision = sirShaderRead64( pData + 40 );
	header.nManifestHash = sirShaderRead64( pData + 48 );
	header.nTargetFingerprint = sirShaderRead64( pData + 56 );
	if ( !sirShaderHeaderValid( header ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHeader = header;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderEncodeStage( const sirShaderStageRecord_t& stage, void *pBytes, uint64_t nCapacity ) noexcept
{
	if ( pBytes == nullptr || nCapacity < SIR_SHADER_PACKAGE_STAGE_BYTES || !sirShaderStageRecordValid( stage ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint8_t *pData = static_cast<uint8_t *>(pBytes);
	sirShaderWrite32( pData, stage.nStageMask );
	sirShaderWrite16( pData + 4, static_cast<uint16_t>(stage.ePass) );
	sirShaderWrite16( pData + 6, static_cast<uint16_t>(stage.eFormat) );
	sirShaderWrite64( pData + 8, stage.nRequiredFeatures );
	sirShaderWrite64( pData + 16, stage.nBinaryHash );
	sirShaderWrite64( pData + 24, stage.nEntryPointHash );
	sirShaderWrite32( pData + 32, stage.nPayloadOffset );
	sirShaderWrite32( pData + 36, stage.nPayloadBytes );
	sirShaderWrite64( pData + 40, stage.nBindingSignature );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderDecodeStage( const void *pBytes, uint64_t nLength, sirShaderStageRecord_t *pStage ) noexcept
{
	if ( pBytes == nullptr || pStage == nullptr || nLength < SIR_SHADER_PACKAGE_STAGE_BYTES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint8_t *pData = static_cast<const uint8_t *>(pBytes);
	sirShaderStageRecord_t stage {};
	stage.nStageMask = sirShaderRead32( pData );
	stage.ePass = static_cast<SIRShaderPass>(sirShaderRead16( pData + 4 ));
	stage.eFormat = static_cast<SIRGraphicsShaderFormat>(sirShaderRead16( pData + 6 ));
	stage.nRequiredFeatures = sirShaderRead64( pData + 8 );
	stage.nBinaryHash = sirShaderRead64( pData + 16 );
	stage.nEntryPointHash = sirShaderRead64( pData + 24 );
	stage.nPayloadOffset = sirShaderRead32( pData + 32 );
	stage.nPayloadBytes = sirShaderRead32( pData + 36 );
	stage.nBindingSignature = sirShaderRead64( pData + 40 );
	if ( !sirShaderStageRecordValid( stage ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pStage = stage;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderEncodeBinding( const sirShaderReflectionBinding_t& binding, void *pBytes, uint64_t nCapacity ) noexcept
{
	if ( pBytes == nullptr || nCapacity < SIR_SHADER_PACKAGE_BINDING_BYTES || !sirShaderBindingRecordValid( binding ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint8_t *pData = static_cast<uint8_t *>(pBytes);
	sirShaderWrite16( pData, binding.nSet );
	sirShaderWrite16( pData + 2, binding.nBinding );
	sirShaderWrite32( pData + 4, static_cast<uint32_t>(binding.eType) );
	pData[8] = static_cast<uint8_t>(binding.eFrequency);
	pData[9] = binding.nStageMask;
	sirShaderWrite16( pData + 10, binding.nSpace );
	sirShaderWrite16( pData + 12, binding.nArrayCount );
	sirShaderWrite16( pData + 14, binding.nFlags );
	sirShaderWrite64( pData + 16, binding.nNameHash );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderDecodeBinding( const void *pBytes, uint64_t nLength, sirShaderReflectionBinding_t *pBinding ) noexcept
{
	if ( pBytes == nullptr || pBinding == nullptr || nLength < SIR_SHADER_PACKAGE_BINDING_BYTES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint8_t *pData = static_cast<const uint8_t *>(pBytes);
	sirShaderReflectionBinding_t binding {};
	binding.nSet = sirShaderRead16( pData );
	binding.nBinding = sirShaderRead16( pData + 2 );
	binding.eType = static_cast<SIRGraphicsBindingType>(sirShaderRead32( pData + 4 ));
	binding.eFrequency = static_cast<SIRShaderBindingFrequency>(pData[8]);
	binding.nStageMask = pData[9];
	binding.nSpace = sirShaderRead16( pData + 10 );
	binding.nArrayCount = sirShaderRead16( pData + 12 );
	binding.nFlags = sirShaderRead16( pData + 14 );
	binding.nNameHash = sirShaderRead64( pData + 16 );
	if ( !sirShaderBindingRecordValid( binding ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBinding = binding;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderBuildBindingSignature( const void *pRecords, uint32_t nCount, uint64_t *pSignature ) noexcept
{
	if ( pSignature == nullptr || nCount > SIR_SHADER_PACKAGE_MAX_BINDINGS || ( nCount != 0 && pRecords == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nHash = sirShaderHashScalar( 14695981039346656037ULL, nCount );
	if ( nCount != 0 ) {
		nHash = sirShaderHashBytes( nHash, pRecords, static_cast<uint64_t>(nCount) * SIR_SHADER_PACKAGE_BINDING_BYTES );
	}
	*pSignature = nHash;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderBuildManifestHash( const sirShaderPackageHeader_t& header, const void *pRecords, uint64_t nRecordBytes,
                                                uint64_t *pHash ) noexcept
{
	const uint64_t nExpected = static_cast<uint64_t>(header.nStageCount) * SIR_SHADER_PACKAGE_STAGE_BYTES +
	                           static_cast<uint64_t>(header.nBindingCount) * SIR_SHADER_PACKAGE_BINDING_BYTES;
	if ( pRecords == nullptr || pHash == nullptr || header.nStageCount == 0 || header.nStageCount > SIR_SHADER_PACKAGE_MAX_STAGES ||
	     header.nBindingCount > SIR_SHADER_PACKAGE_MAX_BINDINGS || nRecordBytes != nExpected || header.nAssetId == 0 ||
	     header.nSourceRevision == 0 || header.nTargetFingerprint == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nHash = 14695981039346656037ULL;
	nHash = sirShaderHashScalar( nHash, header.nSchema );
	nHash = sirShaderHashScalar( nHash, header.nAssetId );
	nHash = sirShaderHashScalar( nHash, header.nSourceRevision );
	nHash = sirShaderHashScalar( nHash, header.nTargetFingerprint );
	nHash = sirShaderHashScalar( nHash, header.nStageCount );
	nHash = sirShaderHashScalar( nHash, header.nBindingCount );
	nHash = sirShaderHashBytes( nHash, pRecords, nRecordBytes );
	*pHash = nHash;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderValidatePackage( const void *pBytes, uint64_t nLength, SIRGraphicsShaderFormat eFormat,
                                              uint64_t nAvailableFeatures ) noexcept
{
	sirShaderPackageHeader_t header {};
	if ( sirShaderDecodePackageHeader( pBytes, nLength, &header ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS || nLength != header.nTotalBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( eFormat < SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL ||
	     ( eFormat > SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS || eFormat == SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_FIXED_FUNCTION ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint8_t *pData = static_cast<const uint8_t *>(pBytes);
	uint64_t nManifest = 0;
	// Header validation has already proven every argument accepted by this helper.
	static_cast<void>(sirShaderBuildManifestHash( header, pData + header.nStageOffset, header.nPayloadOffset - header.nStageOffset, &nManifest ));
	if ( nManifest != header.nManifestHash ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nBindingSignature = 0;
	// The validated header guarantees a non-null record span and a bounded count.
	static_cast<void>(sirShaderBuildBindingSignature( pData + header.nBindingOffset, header.nBindingCount, &nBindingSignature ));
	uint32_t nAvailableStages = 0;
	uint64_t nStagePassPairs = 0;
	for ( uint32_t iStage = 0; iStage < header.nStageCount; ++iStage ) {
		sirShaderStageRecord_t stage {};
		const uint8_t *pStage = pData + header.nStageOffset + iStage * SIR_SHADER_PACKAGE_STAGE_BYTES;
		if ( sirShaderDecodeStage( pStage, SIR_SHADER_PACKAGE_STAGE_BYTES, &stage ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( stage.nBindingSignature != nBindingSignature ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( stage.eFormat != eFormat || ( stage.nRequiredFeatures & nAvailableFeatures ) != stage.nRequiredFeatures ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		uint64_t nNeeded = static_cast<uint64_t>(SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS);
		if ( stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE) ) {
			nNeeded = static_cast<uint64_t>(SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE);
		} else if ( stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_MESH) || stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK) ) {
			nNeeded |= static_cast<uint64_t>(SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_MESH_SHADER);
		} else if ( stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_GEOMETRY) ) {
			nNeeded |= static_cast<uint64_t>(SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER);
		} else if ( stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TESS_CONTROL) || stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TESS_EVALUATION) ) {
			nNeeded |= static_cast<uint64_t>(SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_TESSELLATION);
		}
		if ( ( nNeeded & nAvailableFeatures ) != nNeeded ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( stage.nPayloadOffset < header.nPayloadOffset || static_cast<uint64_t>(stage.nPayloadOffset) + stage.nPayloadBytes > header.nTotalBytes ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( sirShaderHashBytes( 14695981039346656037ULL, pData + stage.nPayloadOffset, stage.nPayloadBytes ) != stage.nBinaryHash ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( stage.eFormat == SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS ) {
			const uint8_t *pTokens = pData + stage.nPayloadOffset;
			const uint32_t nKind = static_cast<uint32_t>(pTokens[2]) | ( static_cast<uint32_t>(pTokens[3]) << 8 );
			const uint32_t nExpected = stage.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX) ? 0xfffe : 0xffff;
			if ( ( stage.nStageMask != static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX) && stage.nStageMask != static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT) ) ||
			     stage.nPayloadBytes < 8 || stage.nPayloadBytes % 4 != 0 || nKind != nExpected || pTokens[1] == 0 || pTokens[1] > 3 ||
			     pTokens[stage.nPayloadBytes - 4] != 0xff || pTokens[stage.nPayloadBytes - 3] != 0xff ||
			     pTokens[stage.nPayloadBytes - 2] != 0 || pTokens[stage.nPayloadBytes - 1] != 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		// Every (pass, stage) pair is unambiguous. A duplicate must not
		// silently select one of two conflicting binaries at runtime.
		uint32_t nStageBit = 0;
		for ( uint32_t nMask = stage.nStageMask; nMask > 1; nMask >>= 1 ) {
			++nStageBit;
		}
		const uint32_t nPairIndex = ( static_cast<uint32_t>(stage.ePass) - static_cast<uint32_t>(SIRShaderPass::SIR_SHADER_PASS_COLOR) ) * 8 + nStageBit;
		const uint64_t nPair = 1ULL << nPairIndex;
		if ( ( nStagePassPairs & nPair ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		nStagePassPairs |= nPair;
		nAvailableStages |= stage.nStageMask;
	}
	for ( uint32_t iBinding = 0; iBinding < header.nBindingCount; ++iBinding ) {
		sirShaderReflectionBinding_t binding {};
		const uint8_t *pBinding = pData + header.nBindingOffset + iBinding * SIR_SHADER_PACKAGE_BINDING_BYTES;
		if ( sirShaderDecodeBinding( pBinding, SIR_SHADER_PACKAGE_BINDING_BYTES, &binding ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ||
		     ( binding.nStageMask & ~nAvailableStages ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( iBinding != 0 ) {
			const uint8_t *pPrior = pData + header.nBindingOffset + ( iBinding - 1 ) * SIR_SHADER_PACKAGE_BINDING_BYTES;
			const uint16_t nPriorSet = sirShaderRead16( pPrior );
			const uint16_t nPriorSpace = sirShaderRead16( pPrior + 10 );
			const uint16_t nPriorBinding = sirShaderRead16( pPrior + 2 );
			if ( binding.nSet < nPriorSet || ( binding.nSet == nPriorSet && binding.nSpace < nPriorSpace ) ||
			     ( binding.nSet == nPriorSet && binding.nSpace == nPriorSpace && binding.nBinding <= nPriorBinding ) ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

bool sirShaderPassSignaturesEqual( const sirShaderMaterialPassSignature_t& left, const sirShaderMaterialPassSignature_t& right ) noexcept
{
	return left.ePass == right.ePass && left.eDepthFormat == right.eDepthFormat && left.nColorFormatsHash == right.nColorFormatsHash &&
	       left.nBindingSignature == right.nBindingSignature && left.nStageMask == right.nStageMask &&
	       left.nSampleCount == right.nSampleCount && left.bAlphaMasked == right.bAlphaMasked && left.bDoubleSided == right.bDoubleSided &&
	       left.bWritesVelocity == right.bWritesVelocity;
}

bool sirShaderRecipesEqual( const sirShaderPipelineRecipe_t& left, const sirShaderPipelineRecipe_t& right ) noexcept
{
	return sirShaderPassSignaturesEqual( left.pass, right.pass ) && left.nAssetId == right.nAssetId &&
	       left.nSourceRevision == right.nSourceRevision && left.nPackageManifestHash == right.nPackageManifestHash &&
	       left.nPackageSchema == right.nPackageSchema && left.nPermutationKey == right.nPermutationKey &&
	       left.nRenderStateHash == right.nRenderStateHash && left.nRequiredFeatures == right.nRequiredFeatures;
}

bool sirShaderNativeIdentitiesEqual( const sirShaderNativeIdentity_t& left, const sirShaderNativeIdentity_t& right ) noexcept
{
	return left.eBackend == right.eBackend && left.nVendorId == right.nVendorId && left.nDeviceId == right.nDeviceId &&
	       left.nDriverVersion == right.nDriverVersion && left.nDeviceGeneration == right.nDeviceGeneration &&
	       left.nCompilerFingerprint == right.nCompilerFingerprint && left.nCacheUuidLow == right.nCacheUuidLow &&
	       left.nCacheUuidHigh == right.nCacheUuidHigh;
}

static bool sirShaderRecipeValid( const sirShaderPipelineRecipe_t& recipe ) noexcept
{
	if ( recipe.pass.ePass < SIRShaderPass::SIR_SHADER_PASS_COLOR || recipe.pass.ePass > SIRShaderPass::SIR_SHADER_PASS_COMPUTE || recipe.nAssetId == 0 ||
	     recipe.nSourceRevision == 0 || recipe.nPackageManifestHash == 0 || recipe.nPackageSchema != SIR_SHADER_PACKAGE_SCHEMA ||
	     recipe.pass.nBindingSignature == 0 || recipe.pass.nStageMask == 0 || recipe.pass.nSampleCount == 0 ||
	     ( recipe.pass.nSampleCount & ( recipe.pass.nSampleCount - 1 ) ) != 0 ) {
		return false;
	}
	constexpr uint32_t VALID_STAGE_MASK = ( static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK) << 1 ) - 1;
	if ( ( recipe.pass.nStageMask & ~VALID_STAGE_MASK ) != 0 ) {
		return false;
	}
	const bool bCompute = recipe.pass.ePass == SIRShaderPass::SIR_SHADER_PASS_COMPUTE;
	if ( bCompute ) {
		return recipe.pass.nStageMask == static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE) && recipe.pass.nSampleCount == 1;
	}
	return ( recipe.pass.nStageMask & static_cast<uint32_t>(SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE) ) == 0;
}

SIRGraphicsResult sirShaderBuildPortableKey( const sirShaderPipelineRecipe_t& recipe, sirShaderPipelineKey_t *pKey ) noexcept
{
	if ( pKey == nullptr || !sirShaderRecipeValid( recipe ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nLow = 14695981039346656037ULL;
	uint64_t nHigh = 7809847782465536322ULL;
	const uint64_t szParts[] = { static_cast<uint64_t>(recipe.pass.ePass),
	                             static_cast<uint64_t>(recipe.pass.eDepthFormat),
	                             recipe.pass.nColorFormatsHash,
	                             recipe.pass.nBindingSignature,
	                             recipe.pass.nStageMask,
	                             recipe.pass.nSampleCount,
	                             static_cast<uint64_t>(recipe.pass.bAlphaMasked),
	                             static_cast<uint64_t>(recipe.pass.bDoubleSided),
	                             static_cast<uint64_t>(recipe.pass.bWritesVelocity),
	                             recipe.nAssetId,
	                             recipe.nSourceRevision,
	                             recipe.nPackageManifestHash,
	                             recipe.nPackageSchema,
	                             recipe.nPermutationKey,
	                             recipe.nRenderStateHash,
	                             recipe.nRequiredFeatures };
	for ( uint32_t iPart = 0; iPart < sizeof( szParts ) / sizeof( szParts[0] ); ++iPart ) {
		nLow = sirShaderHashScalar( nLow, szParts[iPart] );
		nHigh = sirShaderHashScalar( nHigh, szParts[iPart] );
	}
	pKey->nLow = nLow;
	pKey->nHigh = nHigh;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderBuildNativeKey( const sirShaderPipelineRecipe_t& recipe, const sirShaderNativeIdentity_t& native,
                                             sirShaderPipelineKey_t *pKey ) noexcept
{
	if ( pKey == nullptr || native.eBackend == SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_AUTO || native.eBackend > SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_OPENGL ||
	     native.nDeviceGeneration == 0 || native.nCompilerFingerprint == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirShaderPipelineKey_t key {};
	if ( sirShaderBuildPortableKey( recipe, &key ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t szParts[] = { static_cast<uint64_t>(native.eBackend), native.nVendorId,         native.nDeviceId,
	                             native.nDriverVersion,       native.nDeviceGeneration, native.nCompilerFingerprint,
	                             native.nCacheUuidLow,        native.nCacheUuidHigh };
	for ( uint32_t iPart = 0; iPart < sizeof( szParts ) / sizeof( szParts[0] ); ++iPart ) {
		key.nLow = sirShaderHashScalar( key.nLow, szParts[iPart] );
		key.nHigh = sirShaderHashScalar( key.nHigh, szParts[iPart] );
	}
	*pKey = key;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderPackVisibilityId( const sirShaderVisibilityId_t& id, uint64_t *pPacked ) noexcept
{
	if ( pPacked == nullptr || id.nInstance > 0xffffffU || id.nPrimitive > 0xfffffU || id.nMaterial > 0xfffU ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t nValue = static_cast<uint64_t>(id.nInstance) | ( static_cast<uint64_t>(id.nPrimitive) << 24 ) | ( static_cast<uint64_t>(id.nMaterial) << 44 ) |
	                        ( static_cast<uint64_t>(id.nFlags) << 56 );
	if ( nValue == UINT64_MAX ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pPacked = nValue;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderUnpackVisibilityId( uint64_t nPacked, sirShaderVisibilityId_t *pId ) noexcept
{
	if ( pId == nullptr || nPacked == UINT64_MAX ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirShaderVisibilityId_t id {};
	id.nInstance = static_cast<uint32_t>( nPacked & 0xffffffU );
	id.nPrimitive = static_cast<uint32_t>(  ( nPacked >> 24 ) & 0xfffffU  );
	id.nMaterial = static_cast<uint16_t>(  ( nPacked >> 44 ) & 0xfffU  );
	id.nFlags = static_cast<uint8_t>( nPacked >> 56 );
	*pId = id;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderValidateGeometryLease( const sirShaderGeometryLease_t& lease, uint32_t nDeviceGeneration ) noexcept
{
	if ( nDeviceGeneration == 0 || lease.nAssetId == 0 || lease.nSourceRevision == 0 || lease.nProviderGeneration == 0 ||
	     lease.completion.nDeviceGeneration != nDeviceGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint32_t VALID_QUEUES = ( 1U << static_cast<uint32_t>(SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT) ) - 1;
	if ( ( lease.completion.nQueueMask & ~VALID_QUEUES ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iQueue = 0; iQueue < static_cast<uint32_t>(SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT); ++iQueue ) {
		const bool bTagged = ( lease.completion.nQueueMask & ( 1U << iQueue ) ) != 0;
		const bool bSignaled = lease.completion.szQueueValues[iQueue] != 0;
		if ( bTagged != bSignaled ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

bool sirShaderCanReuseGeometry( const sirShaderGeometryLease_t& existing, const sirShaderGeometryLease_t& requested ) noexcept
{
	if ( sirShaderValidateGeometryLease( existing, existing.completion.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ||
	     sirShaderValidateGeometryLease( requested, existing.completion.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ||
	     existing.nAssetId != requested.nAssetId || existing.nSourceRevision != requested.nSourceRevision ||
	     existing.nProviderGeneration != requested.nProviderGeneration ) {
		return false;
	}
	for ( uint32_t iQueue = 0; iQueue < static_cast<uint32_t>(SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT); ++iQueue ) {
		if ( existing.completion.szQueueValues[iQueue] < requested.completion.szQueueValues[iQueue] ) {
			return false;
		}
	}
	return true;
}

SIRGraphicsResult sirShaderAdmitWork( const sirShaderWorkRequest_t& request, const sirShaderWorkBudget_t& budget ) noexcept
{
	if ( request.nVisibleCount > budget.nVisibleCapacity || request.nMaterialBinCount > budget.nMaterialBinCapacity ||
	     request.nIndirectCount > budget.nIndirectCapacity || request.nOptionalDispatchCount > budget.nOptionalDispatchCapacity ||
	     request.nScratchBytes > budget.nScratchCapacityBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderResolveRate( const sirShaderRateEligibility_t& eligibility, SIRShaderRate *pRate ) noexcept
{
	if ( pRate == nullptr || ( eligibility.eRequested != SIRShaderRate::SIR_SHADER_RATE_FULL && eligibility.eRequested != SIRShaderRate::SIR_SHADER_RATE_HALF &&
	                           eligibility.eRequested != SIRShaderRate::SIR_SHADER_RATE_QUARTER ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pRate = SIRShaderRate::SIR_SHADER_RATE_FULL;
	if ( eligibility.bDerivativesValid && eligibility.bHistoryValid && eligibility.bMotionCoherent && !eligibility.bDiscontinuity &&
	     !eligibility.bAlphaMasked ) {
		*pRate = eligibility.eRequested;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
