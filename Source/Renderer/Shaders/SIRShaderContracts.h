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

#ifndef __SIRENGINE_SIRSHADERCONTRACTS_H__
#define __SIRENGINE_SIRSHADERCONTRACTS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

// Serialized records are explicitly little-endian; never memcpy a C++ record
// directly into a package. Limits apply at the untrusted import boundary.
constexpr uint32_t SIR_SHADER_PACKAGE_MAGIC = 0x50485353U;
constexpr uint16_t SIR_SHADER_PACKAGE_SCHEMA = 1;
constexpr uint32_t SIR_SHADER_PACKAGE_HEADER_BYTES = 64;
constexpr uint32_t SIR_SHADER_PACKAGE_STAGE_BYTES = 48;
constexpr uint32_t SIR_SHADER_PACKAGE_BINDING_BYTES = 24;
constexpr uint32_t SIR_SHADER_PACKAGE_MAX_BYTES = 64U * 1024U * 1024U;
constexpr uint32_t SIR_SHADER_PACKAGE_MAX_STAGES = 32;
constexpr uint32_t SIR_SHADER_PACKAGE_MAX_BINDINGS = 1024;

// A material pass signature is part of variant and PSO compatibility.
typedef enum sirShaderPass_e : uint16_t
{
	SIR_SHADER_PASS_COLOR = 1,
	SIR_SHADER_PASS_DEPTH = 2,
	SIR_SHADER_PASS_SHADOW = 3,
	SIR_SHADER_PASS_VELOCITY = 4,
	SIR_SHADER_PASS_GBUFFER = 5,
	SIR_SHADER_PASS_COMPUTE = 6
} sirShaderPass_t;

typedef enum sirShaderBindingFrequency_e : uint8_t
{
	SIR_SHADER_BINDING_FRAME = 0,
	SIR_SHADER_BINDING_VIEW = 1,
	SIR_SHADER_BINDING_MATERIAL = 2,
	SIR_SHADER_BINDING_DRAW = 3
} sirShaderBindingFrequency_t;

typedef enum sirShaderRate_e : uint8_t
{
	SIR_SHADER_RATE_FULL = 1,
	SIR_SHADER_RATE_HALF = 2,
	SIR_SHADER_RATE_QUARTER = 4
} sirShaderRate_t;

typedef struct sirShaderPackageHeader_s
{
	uint32_t nMagic;
	uint16_t nSchema;
	uint16_t nHeaderBytes;
	uint32_t nTotalBytes;
	uint16_t nStageCount;
	uint16_t nBindingCount;
	uint32_t nStageOffset;
	uint32_t nBindingOffset;
	uint32_t nPayloadOffset;
	uint32_t nPayloadBytes;
	uint64_t nAssetId;
	uint64_t nSourceRevision;
	uint64_t nManifestHash;
	uint64_t nTargetFingerprint;
} sirShaderPackageHeader_t;

typedef struct sirShaderStageRecord_s
{
	uint32_t nStageMask;
	sirShaderPass_t ePass;
	sirGraphicsShaderFormat_t eFormat;
	uint64_t nRequiredFeatures;
	uint64_t nBinaryHash;
	uint64_t nEntryPointHash;
	uint32_t nPayloadOffset;
	uint32_t nPayloadBytes;
	uint64_t nBindingSignature;
} sirShaderStageRecord_t;

typedef struct sirShaderReflectionBinding_s
{
	uint16_t nSet;
	uint16_t nBinding;
	sirGraphicsBindingType_t eType;
	sirShaderBindingFrequency_t eFrequency;
	uint8_t nStageMask;
	uint16_t nSpace;
	uint16_t nArrayCount;
	uint16_t nFlags;
	uint64_t nNameHash;
} sirShaderReflectionBinding_t;

typedef struct sirShaderMaterialPassSignature_s
{
	sirShaderPass_t ePass;
	sirGraphicsFormat_t eDepthFormat;
	uint64_t nColorFormatsHash;
	uint64_t nBindingSignature;
	uint32_t nStageMask;
	uint32_t nSampleCount;
	bool bAlphaMasked;
	bool bDoubleSided;
	bool bWritesVelocity;
} sirShaderMaterialPassSignature_t;

typedef struct sirShaderPipelineRecipe_s
{
	sirShaderMaterialPassSignature_t pass;
	uint64_t nAssetId;
	uint64_t nSourceRevision;
	uint64_t nPackageManifestHash;
	uint32_t nPackageSchema;
	uint64_t nPermutationKey;
	uint64_t nRenderStateHash;
	uint64_t nRequiredFeatures;
} sirShaderPipelineRecipe_t;

typedef struct sirShaderNativeIdentity_s
{
	sirGraphicsBackend_t eBackend;
	uint32_t nVendorId;
	uint32_t nDeviceId;
	uint32_t nDriverVersion;
	uint32_t nDeviceGeneration;
	uint64_t nCompilerFingerprint;
	uint64_t nCacheUuidLow;
	uint64_t nCacheUuidHigh;
} sirShaderNativeIdentity_t;

typedef struct sirShaderPipelineKey_s
{
	uint64_t nLow;
	uint64_t nHigh;
} sirShaderPipelineKey_t;

typedef struct sirShaderVisibilityId_s
{
	uint32_t nInstance;
	uint32_t nPrimitive;
	uint16_t nMaterial;
	uint8_t nFlags;
} sirShaderVisibilityId_t;

typedef struct sirShaderGeometryLease_s
{
	uint64_t nAssetId;
	uint64_t nSourceRevision;
	uint32_t nProviderGeneration;
	sirGraphicsCompletionSet_t completion;
} sirShaderGeometryLease_t;

typedef struct sirShaderWorkBudget_s
{
	uint32_t nVisibleCapacity;
	uint32_t nMaterialBinCapacity;
	uint32_t nIndirectCapacity;
	uint32_t nOptionalDispatchCapacity;
	uint64_t nScratchCapacityBytes;
} sirShaderWorkBudget_t;

typedef struct sirShaderWorkRequest_s
{
	uint32_t nVisibleCount;
	uint32_t nMaterialBinCount;
	uint32_t nIndirectCount;
	uint32_t nOptionalDispatchCount;
	uint64_t nScratchBytes;
} sirShaderWorkRequest_t;

typedef struct sirShaderRateEligibility_s
{
	sirShaderRate_t eRequested;
	bool bDerivativesValid;
	bool bHistoryValid;
	bool bMotionCoherent;
	bool bDiscontinuity;
	bool bAlphaMasked;
} sirShaderRateEligibility_t;

static_assert( sizeof( sirShaderPackageHeader_t ) == 64, "shader package header ABI must remain 64 bytes" );
static_assert( sizeof( sirShaderStageRecord_t ) == 56, "in-memory shader stage record ABI must remain 56 bytes" );
static_assert( sizeof( sirShaderReflectionBinding_t ) == 24, "shader binding record ABI must remain 24 bytes" );

sirGraphicsResult_t sirShaderEncodePackageHeader( const sirShaderPackageHeader_t& header, void *pBytes, uint64_t nCapacity ) noexcept;
sirGraphicsResult_t sirShaderDecodePackageHeader( const void *pBytes, uint64_t nLength, sirShaderPackageHeader_t *pHeader ) noexcept;
sirGraphicsResult_t sirShaderEncodeStage( const sirShaderStageRecord_t& stage, void *pBytes, uint64_t nCapacity ) noexcept;
sirGraphicsResult_t sirShaderDecodeStage( const void *pBytes, uint64_t nLength, sirShaderStageRecord_t *pStage ) noexcept;
sirGraphicsResult_t sirShaderEncodeBinding( const sirShaderReflectionBinding_t& binding, void *pBytes, uint64_t nCapacity ) noexcept;
sirGraphicsResult_t sirShaderDecodeBinding( const void *pBytes, uint64_t nLength, sirShaderReflectionBinding_t *pBinding ) noexcept;
sirGraphicsResult_t sirShaderBuildBindingSignature( const void *pRecords, uint32_t nCount, uint64_t *pSignature ) noexcept;
sirGraphicsResult_t sirShaderBuildManifestHash( const sirShaderPackageHeader_t& header, const void *pRecords, uint64_t nRecordBytes,
                                                uint64_t *pHash ) noexcept;
sirGraphicsResult_t sirShaderValidatePackage( const void *pBytes, uint64_t nLength, sirGraphicsShaderFormat_t eFormat,
                                              uint64_t nAvailableFeatures ) noexcept;
bool sirShaderPassSignaturesEqual( const sirShaderMaterialPassSignature_t& left, const sirShaderMaterialPassSignature_t& right ) noexcept;
sirGraphicsResult_t sirShaderBuildPortableKey( const sirShaderPipelineRecipe_t& recipe, sirShaderPipelineKey_t *pKey ) noexcept;
sirGraphicsResult_t sirShaderBuildNativeKey( const sirShaderPipelineRecipe_t& recipe, const sirShaderNativeIdentity_t& native,
                                             sirShaderPipelineKey_t *pKey ) noexcept;
bool sirShaderRecipesEqual( const sirShaderPipelineRecipe_t& left, const sirShaderPipelineRecipe_t& right ) noexcept;
bool sirShaderNativeIdentitiesEqual( const sirShaderNativeIdentity_t& left, const sirShaderNativeIdentity_t& right ) noexcept;
sirGraphicsResult_t sirShaderPackVisibilityId( const sirShaderVisibilityId_t& id, uint64_t *pPacked ) noexcept;
sirGraphicsResult_t sirShaderUnpackVisibilityId( uint64_t nPacked, sirShaderVisibilityId_t *pId ) noexcept;
sirGraphicsResult_t sirShaderValidateGeometryLease( const sirShaderGeometryLease_t& lease, uint32_t nDeviceGeneration ) noexcept;
bool sirShaderCanReuseGeometry( const sirShaderGeometryLease_t& existing, const sirShaderGeometryLease_t& requested ) noexcept;
sirGraphicsResult_t sirShaderAdmitWork( const sirShaderWorkRequest_t& request, const sirShaderWorkBudget_t& budget ) noexcept;
sirGraphicsResult_t sirShaderResolveRate( const sirShaderRateEligibility_t& eligibility, sirShaderRate_t *pRate ) noexcept;

#endif // __SIRENGINE_SIRSHADERCONTRACTS_H__
