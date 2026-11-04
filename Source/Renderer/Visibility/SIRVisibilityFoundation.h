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

#ifndef __SIRENGINE_SIRVISIBILITYFOUNDATION_H__
#define __SIRENGINE_SIRVISIBILITYFOUNDATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

// The reference implementation is reverse-Z and conservative. GPU pipelines
// consume these same contracts; FrameGraph owns scheduling in a later phase.
typedef enum sirVisibilityMode_e : uint8_t
{
	SIR_VISIBILITY_DISABLED = 0,
	SIR_VISIBILITY_HIZ_TEMPORAL = 1,
	SIR_VISIBILITY_HIZ_TWO_PHASE = 2,
	SIR_VISIBILITY_HIERARCHICAL = 3,
	SIR_VISIBILITY_AUTO = 4
} sirVisibilityMode_t;

typedef enum sirVisibilityDepthConvention_e : uint8_t
{
	SIR_VISIBILITY_REVERSE_Z = 0,
	SIR_VISIBILITY_FORWARD_Z = 1
} sirVisibilityDepthConvention_t;

typedef struct sirVisibilityMethodDesc_s
{
	sirVisibilityMode_t eMode;
	sirVisibilityDepthConvention_t eDepthConvention;
	sirGraphicsFormat_t eDepthFormat;
	uint32_t nMaximumCandidates;
	uint32_t nMaximumDraws;
} sirVisibilityMethodDesc_t;

typedef struct sirVisibilityView_s
{
	uint64_t nViewId;
	uint64_t nFrameIndex;
	uint64_t nProjectionRevision;
	uint64_t nWorldOriginRevision;
	uint64_t nDepthProducerRevision;
	uint64_t nSceneResetRevision;
	uint32_t nDeviceGeneration;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nSampleCount;
	sirGraphicsFormat_t eDepthFormat;
	sirVisibilityDepthConvention_t eDepthConvention;
	bool bCameraCut;
} sirVisibilityView_t;

typedef struct sirHiZMip_s
{
	uint64_t nOffset;
	uint32_t nWidth;
	uint32_t nHeight;
} sirHiZMip_t;

typedef struct sirHiZLayout_s
{
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nMipCount;
	uint64_t nTexelCount;
	sirHiZMip_t szMips[32];
} sirHiZLayout_t;

typedef struct sirHiZSource_s
{
	const float *pDepth;
	uint64_t nTexelCapacity;
	uint32_t nRowStride;
} sirHiZSource_t;

typedef struct sirHiZTest_s
{
	uint32_t nMinX;
	uint32_t nMinY;
	uint32_t nMaxX;
	uint32_t nMaxY;
	float fNearestDepth;
	float fBias;
	bool bOccluded;
	uint32_t nSelectedMip;
} sirHiZTest_t;

typedef struct sirVisibilityCandidate_s
{
	uint32_t nInstanceId;
	sirHiZTest_t bounds;
	bool bForceVisible;
} sirVisibilityCandidate_t;

typedef struct sirVisibilityCullDesc_s
{
	const sirHiZLayout_t *pLayout;
	const float *pPyramid;
	uint64_t nPyramidTexels;
	const sirVisibilityCandidate_t *pCandidates;
	uint32_t nCandidateCount;
	bool bHistoryValid;
} sirVisibilityCullDesc_t;

typedef struct sirVisibilityOutput_s
{
	uint32_t *pVisibleIds;
	uint32_t nVisibleCapacity;
	uint32_t nVisibleCount;
	uint32_t *pRejectedIds;
	uint32_t nRejectedCapacity;
	uint32_t nRejectedCount;
} sirVisibilityOutput_t;

// Requires COMPUTE | SHADER_STORAGE | INDIRECT_DRAW for GPU modes.
// AUTO is resolved to an explicit mode before validation and execution.
sirGraphicsResult_t sirVisibilityValidateMethod( const sirVisibilityMethodDesc_t& method,
                                                 const sirGraphicsCapabilities_t& capabilities ) noexcept;
bool sirVisibilityCanReuseHistory( const sirVisibilityView_t& previous, const sirVisibilityView_t& current ) noexcept;

// Contiguous mip storage, including mip 0. The caller owns all memory.
sirGraphicsResult_t sirHiZCalculateLayout( uint32_t nWidth, uint32_t nHeight, sirHiZLayout_t *pLayout ) noexcept;
sirGraphicsResult_t sirHiZBuildReference( const sirHiZLayout_t& layout, const sirHiZSource_t& source, float *pOutput,
                                          uint64_t nOutputTexels ) noexcept;

// Rectangle uses half-open mip-0 pixel bounds. Any out-of-view rectangle
// fails open. A result is occluded only when every covered pyramid texel
// conclusively hides the candidate. This is a CPU correctness oracle.
sirGraphicsResult_t sirHiZTestRectangle( const sirHiZLayout_t& layout, const float *pPyramid, uint64_t nPyramidTexels,
                                         sirHiZTest_t *pTest ) noexcept;

// CPU reference for a complete culling/compaction cycle. Validates and counts
// before publishing results; does not allocate or partially update the output.
sirGraphicsResult_t sirVisibilityCullReference( const sirVisibilityCullDesc_t& desc, sirVisibilityOutput_t *pOutput ) noexcept;

// HIZ-00 v1: fixed-width modes, mip/index layout and shared CPU/GPU ABI.
// Device residency, synchronization and serialized resource identity are
// represented by later system contracts, not hidden in these reference PODs.
static_assert( sizeof( sirVisibilityMode_t ) == 1, "visibility mode must be 8-bit" );
static_assert( sizeof( sirVisibilityDepthConvention_t ) == 1, "visibility depth convention must be 8-bit" );
static_assert( sizeof( sirVisibilityMethodDesc_t ) == 16, "visibility method ABI changed" );
static_assert( sizeof( sirHiZMip_t ) == 16, "Hi-Z mip ABI changed" );
static_assert( offsetof( sirHiZMip_t, nOffset ) == 0, "Hi-Z mip offset ABI changed" );
static_assert( offsetof( sirHiZMip_t, nWidth ) == 8, "Hi-Z mip width ABI changed" );
static_assert( sizeof( sirHiZLayout_t ) == 536, "Hi-Z layout ABI changed" );
static_assert( offsetof( sirHiZLayout_t, szMips ) == 24, "Hi-Z mip array ABI changed" );
static_assert( sizeof( sirHiZTest_t ) == 32, "Hi-Z rectangle query ABI changed" );
static_assert( offsetof( sirHiZTest_t, nSelectedMip ) == 28, "Hi-Z mip selection ABI changed" );
#if UINTPTR_MAX == UINT64_MAX
static_assert( sizeof( sirVisibilityView_t ) == 72, "64-bit visibility view ABI changed" );
static_assert( sizeof( sirVisibilityCandidate_t ) == 40, "64-bit visibility candidate ABI changed" );
static_assert( sizeof( sirVisibilityCullDesc_t ) == 40, "64-bit visibility cull ABI changed" );
static_assert( sizeof( sirVisibilityOutput_t ) == 32, "64-bit visibility output ABI changed" );
#endif

#endif // __SIRENGINE_SIRVISIBILITYFOUNDATION_H__
