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

#ifndef __SIRENGINE_SIRSHADERMATERIALMANIFEST_H__
#define __SIRENGINE_SIRSHADERMATERIALMANIFEST_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Shaders/SIRShaderMaterial.h>

// Offline-only request: pass data is copied into the canonical, sorted wire
// representation. No resource graph, driver, or platform state is required.
typedef struct sirShaderMaterialManifestRequest_s
{
	uint64_t nAssetId;
	uint64_t nSourceRevision;
	const sirShaderMaterialPassRecord_t *pPasses;
	uint32_t nPassCount;
	uint32_t nDependencyCount;
} sirShaderMaterialManifestRequest_t;

// Uses the shared material encoder, hasher and validator. On invalid input or
// insufficient capacity the output bytes are unchanged. A valid size query
// reports the exact byte count through pRequiredBytes, including when the
// destination is null or undersized.
sirGraphicsResult_t sirShaderBuildMaterialManifest( const sirShaderMaterialManifestRequest_t& request, void *pBytes, uint32_t nCapacity,
                                                    uint32_t *pRequiredBytes ) noexcept;

#endif // __SIRENGINE_SIRSHADERMATERIALMANIFEST_H__
