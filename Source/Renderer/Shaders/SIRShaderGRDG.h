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

#ifndef __SIRENGINE_SIRSHADERGRDG_H__
#define __SIRENGINE_SIRSHADERGRDG_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Shaders/SIRShaderMaterial.h>
#include <Resources/SIRResourceGraph.h>

// Type/format IDs come from the engine's resource-type registry. They are
// supplied by the caller, not allocated or duplicated by this adapter.
typedef struct sirShaderGRDGFormats_s
{
	uint32_t nShaderTypeId;
	uint32_t nShaderFormatId;
	uint32_t nMaterialTypeId;
	uint32_t nMaterialFormatId;
} sirShaderGRDGFormats_t;

typedef struct sirShaderPackageValidation_s
{
	sirGraphicsShaderFormat_t eFormat;
	uint64_t nAvailableFeatures;
} sirShaderPackageValidation_t;

// Everything borrowed here expires with the original GRDG read scope.
typedef struct sirShaderPackageProjection_s
{
	SIRResourceView view;
	SIRResourceConstBytes bytes;
	sirShaderPackageHeader_t header;
	uint64_t nGraphRevision;
} sirShaderPackageProjection_t;

typedef struct sirShaderStageProjection_s
{
	sirShaderStageRecord_t stage;
	SIRResourceConstBytes binary;
} sirShaderStageProjection_t;

typedef struct sirShaderMaterialProjection_s
{
	SIRResourceView view;
	SIRResourceConstBytes bytes;
	sirShaderMaterialHeader_t header;
	uint64_t nGraphRevision;
} sirShaderMaterialProjection_t;

typedef struct sirShaderDependencyRevision_s
{
	SIRResourceKey key;
	uint64_t nGraphRevision;
	SIRResourceKind eKind;
	SIRResourceEdgeKind eEdgeKind;
	bool bResident;
} sirShaderDependencyRevision_t;

// Caller-owned bounded storage. On OUT_OF_BUDGET, nRequiredCount is filled
// and no dependency array is touched. The fingerprint is for change detection,
// not authenticity or a globally collision-free content identifier.
typedef struct sirShaderMaterialSnapshot_s
{
	sirShaderDependencyRevision_t *pDependencies;
	uint32_t nCapacity;
	uint32_t nRequiredCount;
	uint64_t nFingerprint;
	uint64_t nInvalidationSequence;
} sirShaderMaterialSnapshot_t;

typedef struct sirShaderPublicationRequest_s
{
	SIRResourceKey key;
	const void *pBytes;
	uint64_t nBytes;
	uint64_t nExpectedRevision;
	uint32_t nStagingDomain;
	sirGraphicsShaderFormat_t eShaderFormat;
	uint64_t nAvailableFeatures;
	const SIRResourceDependency *pDependencies;
	uint32_t nDependencyCount;
} sirShaderPublicationRequest_t;

// Reads never allocate, copy canonical data or retain read scopes.
SIRResourceResult sirShaderProjectPackage( const SIRResourceView& view, const sirShaderGRDGFormats_t& formats,
                                           const sirShaderPackageValidation_t& validation, sirShaderPackageProjection_t *pOut ) noexcept;
SIRResourceResult sirShaderProjectStage( const sirShaderPackageProjection_t& package, uint32_t iStage,
                                         sirShaderStageProjection_t *pOut ) noexcept;
SIRResourceResult sirShaderProjectMaterial( const SIRResourceView& view, const sirShaderGRDGFormats_t& formats,
                                            sirShaderMaterialProjection_t *pOut ) noexcept;
SIRResourceResult sirShaderProjectMaterialPass( const sirShaderMaterialProjection_t& material, uint32_t nPass,
                                                sirShaderMaterialPassRecord_t *pOut ) noexcept;
SIRResourceResult sirShaderSnapshotMaterial( const SIRResourceGraph& graph, const SIRResourceReadScope& scope,
                                             const sirShaderMaterialProjection_t& material, sirShaderMaterialSnapshot_t *pOut ) noexcept;
// Owner-thread writer sequence catches transitive invalidations even when
// the material's own publication revision does not change.
SIRResourceResult sirShaderStampMaterialInvalidation( SIRResourceGraphWriter& writer, const SIRResourceView& view,
                                                      sirShaderMaterialSnapshot_t *pSnapshot ) noexcept;

// The GRDG writer is the only canonical publisher. All failure paths cancel
// their own staging; expected-revision rejection cannot clobber newer bytes.
SIRResourceResult sirShaderPublishPackage( SIRResourceGraphWriter& writer, const sirShaderGRDGFormats_t& formats,
                                           const sirShaderPublicationRequest_t& request, SIRResourceHandle *pHandle ) noexcept;
SIRResourceResult sirShaderPublishMaterial( SIRResourceGraphWriter& writer, const sirShaderGRDGFormats_t& formats,
                                            const sirShaderPublicationRequest_t& request, SIRResourceHandle *pHandle ) noexcept;

#endif // __SIRENGINE_SIRSHADERGRDG_H__

