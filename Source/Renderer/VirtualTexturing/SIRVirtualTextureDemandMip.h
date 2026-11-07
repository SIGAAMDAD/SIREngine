/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREDEMANDMIP_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREDEMANDMIP_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Terrain3D baked-visibility histogram selection; this is not a texture
// package ABI. Only the 16 histogram bins are read from the donor record.
typedef struct sirVirtualTextureDemandMipHistogram_s
{
	uint16_t szMipWeight[16];
} sirVirtualTextureDemandMipHistogram_t;

typedef struct sirVirtualTextureDemandMipReference_s
{
	uint32_t nNominalTextureSize;
	uint32_t nNominalRenderSize;
	uint32_t nFovMilliDegrees;
} sirVirtualTextureDemandMipReference_t;

typedef struct sirVirtualTextureDemandMipProjection_s
{
	uint32_t nTextureSize;
	uint32_t nViewHeight;
	float fFovDegrees;
	uint32_t nMipCount;
} sirVirtualTextureDemandMipProjection_t;

// Terrain3D's conservative 10th-percentile visible mip choice, then projection
// adjustment. UINT16_MAX means that the source or runtime inputs are invalid.
uint16_t sirVTSelectDemandMip( const sirVirtualTextureDemandMipHistogram_t& histogram,
                               const sirVirtualTextureDemandMipReference_t& reference,
                               const sirVirtualTextureDemandMipProjection_t& projection ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREDEMANDMIP_H__
