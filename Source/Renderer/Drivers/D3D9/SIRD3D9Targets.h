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
#ifndef __SIRENGINE_SIRD3D9TARGETS_H__
#define __SIRENGINE_SIRD3D9TARGETS_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#ifdef _WIN32
#include <Renderer/Drivers/D3D9/SIRD3D9Native.h>

typedef struct sirD3D9TargetDesc_s
{
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nSamples;
	sirGraphicsFormat_t eFormat;
	bool bDepth;
} sirD3D9TargetDesc_t;

typedef struct sirD3D9TargetSet_s
{
	IDirect3DSurface9 *szColor[4];
	IDirect3DSurface9 *pDepth;
	uint32_t nColorCount;
} sirD3D9TargetSet_t;

typedef struct sirD3D9SurfaceCopy_s
{
	IDirect3DSurface9 *pSource;
	IDirect3DSurface9 *pDestination;
	bool bStretch;
} sirD3D9SurfaceCopy_t;

sirGraphicsResult_t sirD3D9NativeCreateTarget( sirD3D9Native_t *pNative, const sirD3D9TargetDesc_t *pDesc,
                                               IDirect3DSurface9 **ppSurface ) noexcept;
sirGraphicsResult_t sirD3D9NativeSetTargets( sirD3D9Native_t *pNative, const sirD3D9TargetSet_t *pTargets ) noexcept;
sirGraphicsResult_t sirD3D9NativeCopySurface( sirD3D9Native_t *pNative, const sirD3D9SurfaceCopy_t *pCopy ) noexcept;
sirGraphicsResult_t sirD3D9NativeUpdateTexture( sirD3D9Native_t *pNative, IDirect3DBaseTexture9 *pSource,
                                                IDirect3DBaseTexture9 *pDestination ) noexcept;
#endif
#endif // __SIRENGINE_SIRD3D9TARGETS_H__
