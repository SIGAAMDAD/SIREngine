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

#include <Renderer/SIRShaderPipeline.h>

static uint64_t sirShaderHashScalar( uint64_t nHash, uint64_t nValue ) noexcept
{
	constexpr uint64_t FNV_PRIME = 1099511628211ULL;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nHash ^= nValue & 0xffU;
		nHash *= FNV_PRIME;
		nValue >>= 8;
	}
	return nHash;
}

static uint32_t sirShaderCountBits( uint64_t nValue ) noexcept
{
	uint32_t nCount = 0;
	while ( nValue != 0 ) {
		nValue &= nValue - 1;
		++nCount;
	}
	return nCount;
}

static uint64_t sirShaderStageRequirements( uint32_t nStage ) noexcept
{
	if ( nStage == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE ) {
		return SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE;
	}
	uint64_t nRequired = SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( nStage == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_GEOMETRY ) {
		nRequired |= SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
	}
	if ( nStage == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TESS_CONTROL || nStage == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TESS_EVALUATION ) {
		nRequired |= SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_TESSELLATION;
	}
	if ( nStage == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_MESH || nStage == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK ) {
		nRequired |= SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_MESH_SHADER;
	}
	return nRequired;
}

bool sirShaderIsStageValid( uint32_t nStage ) noexcept
{
	constexpr uint32_t VALID_STAGES = ( SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK << 1 ) - 1U;
	return nStage != 0 && ( nStage & ( nStage - 1U ) ) == 0 && ( nStage & ~VALID_STAGES ) == 0;
}

SIRGraphicsResult sirShaderGetBackendFormat( SIRGraphicsBackend eBackend, SIRGraphicsShaderFormat *pFormat ) noexcept
{
	if ( pFormat == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	switch ( eBackend ) {
	case SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_D3D12:
		*pFormat = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL;
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	case SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_VULKAN:
		*pFormat = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_SPIRV;
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	case SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_OPENGL:
		*pFormat = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_GLSL;
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	default:
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
}

SIRGraphicsResult sirShaderBuildCacheKey( const sirShaderCacheDescriptor_t& desc, sirShaderCacheKey_t *pKey ) noexcept
{
	if ( pKey == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pKey = {};
	SIRGraphicsShaderFormat eExpected = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL;
	if ( sirShaderGetBackendFormat( desc.eBackend, &eExpected ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( desc.eFormat != eExpected || !sirShaderIsStageValid( desc.nStage ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint64_t VALID_FEATURES = ( SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_PRESENT << 1 ) - 1ULL;
	if ( ( desc.nRequiredFeatures & ~VALID_FEATURES ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nSchemaVersion == 0 || desc.nCompilerVersion == 0 || desc.nSourceHash == 0 || desc.nEntryPointHash == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	// Hash explicit scalar values in fixed little-endian order, never padding.
	const uint64_t szValues[] = { desc.nSchemaVersion,       desc.nCompilerVersion,    desc.nOptimizationFlags, desc.nStage,
	                              static_cast<uint64_t>(desc.eBackend), static_cast<uint64_t>(desc.eFormat), desc.nSourceHash,        desc.nIncludesHash,
	                              desc.nEntryPointHash,      desc.nDefinitionsHash,    desc.nPermutationKey,    desc.nRequiredFeatures,
	                              desc.nTargetFingerprint };
	uint64_t nLow = 14695981039346656037ULL;
	uint64_t nHigh = 7809847782465536322ULL;
	for ( uint32_t iValue = 0; iValue < sizeof( szValues ) / sizeof( szValues[0] ); ++iValue ) {
		nLow = sirShaderHashScalar( nLow, szValues[iValue] );
		nHigh = sirShaderHashScalar( nHigh, szValues[iValue] ^ 0x9e3779b97f4a7c15ULL );
	}
	pKey->nLow = nLow;
	pKey->nHigh = nHigh;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderValidateVariant( const sirShaderVariant_t& variant ) noexcept
{
	SIRGraphicsShaderFormat eExpected = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL;
	if ( sirShaderGetBackendFormat( variant.eBackend, &eExpected ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( variant.eFormat != eExpected || !sirShaderIsStageValid( variant.nStage ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( variant.pBytes == nullptr || variant.nByteCount < 4 || variant.nContentHash == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( variant.pszEntryPoint == nullptr || variant.pszEntryPoint[0] == '\0' ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( variant.nDeviceId != 0 && variant.nVendorId == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint64_t VALID_FEATURES = ( SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_PRESENT << 1 ) - 1ULL;
	if ( ( variant.nRequiredFeatures & ~VALID_FEATURES ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( variant.bUber && variant.nPermutationKey != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirShaderSelectVariant( const sirShaderVariant_t *pVariants, uint32_t nVariantCount,
                                            const sirShaderSelection_t& selection, const sirShaderVariant_t **ppVariant ) noexcept
{
	if ( ppVariant == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppVariant = nullptr;
	if ( pVariants == nullptr || nVariantCount == 0 || nVariantCount > 4096 || !sirShaderIsStageValid( selection.nStage ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	SIRGraphicsShaderFormat eExpected = SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL;
	if ( sirShaderGetBackendFormat( selection.eBackend, &eExpected ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ( selection.nAvailableFeatures & sirShaderStageRequirements( selection.nStage ) ) !=
	     sirShaderStageRequirements( selection.nStage ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}

	uint32_t nBestScore = 0;
	for ( uint32_t iVariant = 0; iVariant < nVariantCount; ++iVariant ) {
		const sirShaderVariant_t& variant = pVariants[iVariant];
		const SIRGraphicsResult eValidation = sirShaderValidateVariant( variant );
		if ( eValidation != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eValidation;
		}
		if ( variant.eBackend != selection.eBackend || variant.nStage != selection.nStage ) {
			continue;
		}
		if ( ( variant.nRequiredFeatures & selection.nAvailableFeatures ) != variant.nRequiredFeatures ) {
			continue;
		}
		if ( variant.nVendorId != 0 && variant.nVendorId != selection.nVendorId ) {
			continue;
		}
		if ( variant.nDeviceId != 0 && variant.nDeviceId != selection.nDeviceId ) {
			continue;
		}
		if ( variant.bUber ) {
			if ( !selection.bAllowUber ) {
				continue;
			}
		} else if ( variant.nPermutationKey != selection.nPermutationKey ) {
			continue;
		}

		const uint32_t nScore = ( variant.bUber ? 0U : 256U ) + ( variant.nVendorId != 0 ? 64U : 0U ) +
		                        ( variant.nDeviceId != 0 ? 128U : 0U ) + sirShaderCountBits( variant.nRequiredFeatures ) + 1U;
		if ( nScore > nBestScore ) {
			nBestScore = nScore;
			*ppVariant = &variant;
		}
	}
	return *ppVariant != nullptr ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
}

SIRGraphicsResult sirShaderToGraphicsCode( const sirShaderVariant_t& variant, sirGraphicsShaderCode_t *pCode ) noexcept
{
	if ( pCode == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCode = {};
	const SIRGraphicsResult eValidation = sirShaderValidateVariant( variant );
	if ( eValidation != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	pCode->eFormat = variant.eFormat;
	pCode->pBytes = variant.pBytes;
	pCode->nByteCount = variant.nByteCount;
	pCode->nContentHash = variant.nContentHash;
	pCode->pszEntryPoint = variant.pszEntryPoint;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

uint32_t SIRShaderWarmupQueue::getCount( void ) const noexcept
{
	return m_nCount;
}

uint32_t SIRShaderWarmupQueue::getCapacity( void ) const noexcept
{
	return s_nCapacity;
}

SIRGraphicsResult SIRShaderWarmupQueue::push( const sirShaderCacheKey_t& key, uint32_t nPriority ) noexcept
{
	if ( key.nLow == 0 && key.nHigh == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iItem = 0; iItem < m_nCount; ++iItem ) {
		sirShaderWarmupItem_t& item = m_szItems[iItem];
		if ( item.key.nLow == key.nLow && item.key.nHigh == key.nHigh ) {
			if ( nPriority > item.nPriority ) {
				item.nPriority = nPriority;
			}
			return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
		}
	}
	if ( m_nCount == s_nCapacity ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	m_szItems[m_nCount].key = key;
	m_szItems[m_nCount].nPriority = nPriority;
	++m_nCount;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

bool SIRShaderWarmupQueue::tryPop( sirShaderWarmupItem_t *pItem ) noexcept
{
	if ( pItem == nullptr || m_nCount == 0 ) {
		return false;
	}
	uint32_t iBest = 0;
	for ( uint32_t iItem = 1; iItem < m_nCount; ++iItem ) {
		if ( m_szItems[iItem].nPriority > m_szItems[iBest].nPriority ) {
			iBest = iItem;
		}
	}
	*pItem = m_szItems[iBest];
	for ( uint32_t iItem = iBest + 1; iItem < m_nCount; ++iItem ) {
		m_szItems[iItem - 1] = m_szItems[iItem];
	}
	--m_nCount;
	m_szItems[m_nCount] = {};
	return true;
}

void SIRShaderWarmupQueue::clear( void ) noexcept
{
	for ( uint32_t iItem = 0; iItem < m_nCount; ++iItem ) {
		m_szItems[iItem] = {};
	}
	m_nCount = 0;
}
