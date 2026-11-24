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

#ifndef __SIR_SHADERPIPELINE_H__
#define __SIR_SHADERPIPELINE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

// Offline artifacts are borrowed; the cooker or resource owner retains the bytes.
// One variant describes one stage of one backend-specific permutation.
typedef struct sirShaderVariant_s
{
	SIRGraphicsBackend eBackend;
	SIRGraphicsShaderFormat eFormat;
	uint32_t nStage;
	uint64_t nRequiredFeatures;
	uint64_t nPermutationKey;
	uint32_t nVendorId;
	uint32_t nDeviceId;
	uint64_t nContentHash;
	const void *pBytes;
	uint64_t nByteCount;
	const char *pszEntryPoint;
	bool bUber;
} sirShaderVariant_t;

typedef struct sirShaderSelection_s
{
	SIRGraphicsBackend eBackend;
	uint32_t nStage;
	uint64_t nAvailableFeatures;
	uint64_t nPermutationKey;
	uint32_t nVendorId;
	uint32_t nDeviceId;
	bool bAllowUber;
} sirShaderSelection_t;

// Every field participates in a versioned, byte-order-independent key.
// The key is an index, not an authentication digest: a cache hit must also
// verify the cooked artifact metadata and content hash.
typedef struct sirShaderCacheDescriptor_s
{
	uint32_t nSchemaVersion;
	uint32_t nCompilerVersion;
	uint32_t nOptimizationFlags;
	uint32_t nStage;
	SIRGraphicsBackend eBackend;
	SIRGraphicsShaderFormat eFormat;
	uint64_t nSourceHash;
	uint64_t nIncludesHash;
	uint64_t nEntryPointHash;
	uint64_t nDefinitionsHash;
	uint64_t nPermutationKey;
	uint64_t nRequiredFeatures;
	uint64_t nTargetFingerprint;
} sirShaderCacheDescriptor_t;

typedef struct sirShaderCacheKey_s
{
	uint64_t nLow;
	uint64_t nHigh;
} sirShaderCacheKey_t;

typedef struct sirShaderWarmupItem_s
{
	sirShaderCacheKey_t key;
	uint32_t nPriority;
} sirShaderWarmupItem_t;

// The queue holds keys only. Cache storage, native pipeline creation and
// GPU fence retirement remain owned by their respective systems.
class SIRShaderWarmupQueue
{
public:
	SIRShaderWarmupQueue( void ) noexcept = default;
	~SIRShaderWarmupQueue( void ) noexcept = default;

	uint32_t getCount( void ) const noexcept;
	uint32_t getCapacity( void ) const noexcept;

	SIRGraphicsResult push( const sirShaderCacheKey_t& key, uint32_t nPriority ) noexcept;
	bool tryPop( sirShaderWarmupItem_t *pItem ) noexcept;
	void clear( void ) noexcept;

protected:
private:
	static constexpr uint32_t s_nCapacity = 128;
	sirShaderWarmupItem_t m_szItems[s_nCapacity] = {};
	uint32_t m_nCount = 0;
};

bool sirShaderIsStageValid( uint32_t nStage ) noexcept;
SIRGraphicsResult sirShaderGetBackendFormat( SIRGraphicsBackend eBackend, SIRGraphicsShaderFormat *pFormat ) noexcept;
SIRGraphicsResult sirShaderBuildCacheKey( const sirShaderCacheDescriptor_t& desc, sirShaderCacheKey_t *pKey ) noexcept;
SIRGraphicsResult sirShaderValidateVariant( const sirShaderVariant_t& variant ) noexcept;
SIRGraphicsResult sirShaderSelectVariant( const sirShaderVariant_t *pVariants, uint32_t nVariantCount,
                                            const sirShaderSelection_t& selection, const sirShaderVariant_t **ppVariant ) noexcept;
SIRGraphicsResult sirShaderToGraphicsCode( const sirShaderVariant_t& variant, sirGraphicsShaderCode_t *pCode ) noexcept;

#endif // __SIR_SHADERPIPELINE_H__
