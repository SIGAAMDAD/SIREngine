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

#ifndef __SIRENGINE_SIRWORLDHLODPROJECTION_H__
#define __SIRENGINE_SIRWORLDHLODPROJECTION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <World/HLOD/SIRWorldHLODManifest.h>

typedef enum sirWorldHLODProjectionMode_e : uint8_t
{
	SIR_WORLD_HLOD_PROJECTION_PERSPECTIVE = 0,
	SIR_WORLD_HLOD_PROJECTION_ORTHOGRAPHIC = 1
} sirWorldHLODProjectionMode_t;

// Manifest bounds and geometric error are world-space measurements, with no
// transform or nonuniform scale applied at runtime. Camera coordinates are
// doubles to preserve precision after world-origin shifts.
typedef struct sirWorldHLODProjectionView_s
{
	double fCameraPosition[3];
	double fCameraForward[3];
	double fVerticalParameter; // radians of vertical FOV, or orthographic world-space height
	double fNearPlane;
	uint32_t nViewportHeightPixels;
	sirWorldHLODProjectionMode_t eMode;
} sirWorldHLODProjectionView_t;

typedef struct sirWorldHLODProjectionInput_s
{
	const sirWorldHLODManifestBounds_t *pBounds;
	const float *pGeometricErrors;
	uint32_t nNodeCount;
	sirWorldHLODProjectionView_t view;
} sirWorldHLODProjectionInput_t;

typedef struct sirWorldHLODProjectionOutput_s
{
	float *pProjectedErrors;
	uint32_t nCapacity;
} sirWorldHLODProjectionOutput_t;

// Conservative screen-space geometric error for the complete node AABB.
// The minimum depth of the box along the camera's forward axis is clamped to
// the near plane; off-frustum/behind-camera rejection belongs to visibility.
// The caller owns all buffers, and invalid input/capacity leaves output intact.
// Input/output buffers must be disjoint and immutable during evaluation.
sirWorldHLODResult_t sirWorldHLODProjectErrors( const sirWorldHLODProjectionInput_t& input,
                                                const sirWorldHLODProjectionOutput_t& output ) noexcept;

#endif // __SIRENGINE_SIRWORLDHLODPROJECTION_H__
