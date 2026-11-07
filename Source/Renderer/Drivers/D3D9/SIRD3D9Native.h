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
#ifndef __SIRENGINE_SIRD3D9NATIVE_H__
#define __SIRENGINE_SIRD3D9NATIVE_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#ifdef _WIN32
#include <windows.h>
#include <d3d9.h>
#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/Drivers/D3D9/SIRD3D9UploadRing.h>
#include <Renderer/Drivers/D3D9/SIRD3D9Resources.h>

typedef struct sirD3D9Native_s
{
	HMODULE hRuntime;
	IDirect3D9 *pRuntime;
	IDirect3D9Ex *pRuntimeEx;
	IDirect3DDevice9 *pDevice;
	IDirect3DDevice9Ex *pDeviceEx;
	IDirect3DQuery9 *pCompletionQuery;
	D3DPRESENT_PARAMETERS present;
	D3DCAPS9 caps;
	HWND hFocusWindow;
	uint32_t nAdapterIndex;
	uint32_t nDeviceGeneration;
	bool bEx;
	bool bSceneActive;
} sirD3D9Native_t;

sirGraphicsResult_t sirD3D9NativeInitialize( sirD3D9Native_t *pNative, HWND hFocusWindow, uint32_t nAdapterIndex, bool bPreferEx ) noexcept;
sirGraphicsResult_t sirD3D9NativeClear( sirD3D9Native_t *pNative, uint32_t nColor ) noexcept;
sirGraphicsResult_t sirD3D9NativeDrawTriangle( sirD3D9Native_t *pNative ) noexcept;
sirGraphicsResult_t sirD3D9NativePresent( sirD3D9Native_t *pNative ) noexcept;
sirGraphicsResult_t sirD3D9NativeResize( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight ) noexcept;
sirGraphicsResult_t sirD3D9NativePoll( sirD3D9Native_t *pNative, bool *pComplete ) noexcept;
sirGraphicsResult_t sirD3D9NativeConfigureResidentBudget( sirD3D9Native_t *pNative, uint64_t nBytes ) noexcept;
sirGraphicsResult_t sirD3D9NativeCreateTrackedBuffer( sirD3D9Native_t *pNative, uint32_t nBytes, bool bIndex,
                                                      sirGraphicsHandle_t *pHandle ) noexcept;
sirGraphicsResult_t sirD3D9NativeCreateTrackedTexture( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight,
                                                       sirGraphicsHandle_t *pHandle ) noexcept;
sirGraphicsResult_t sirD3D9NativeLookupResource( sirD3D9Native_t *pNative, sirGraphicsHandle_t hHandle, void **ppNative ) noexcept;
sirGraphicsResult_t sirD3D9NativeRetireResource( sirD3D9Native_t *pNative, sirGraphicsHandle_t hHandle,
                                                 uint64_t nCompletionValue ) noexcept;
void sirD3D9NativeCollectResources( sirD3D9Native_t *pNative, uint64_t nCompletedValue ) noexcept;
sirGraphicsResult_t sirD3D9NativeCreateBuffer( sirD3D9Native_t *pNative, uint32_t nBytes, bool bIndex,
                                               IDirect3DResource9 **ppResource ) noexcept;
sirGraphicsResult_t sirD3D9NativeCreateTexture( sirD3D9Native_t *pNative, uint32_t nWidth, uint32_t nHeight,
                                                IDirect3DTexture9 **ppTexture ) noexcept;
typedef struct sirD3D9BufferUpdate_s
{
	IDirect3DResource9 *pBuffer;
	const void *pData;
	uint32_t nOffset;
	uint32_t nBytes;
	uint32_t nLockFlags;
} sirD3D9BufferUpdate_t;

typedef struct sirD3D9Readback_s
{
	void *pPixels;
	uint32_t nCapacityBytes;
	uint32_t nRowStride;
	uint32_t nWidth;
	uint32_t nHeight;
} sirD3D9Readback_t;

sirGraphicsResult_t sirD3D9NativeUploadBuffer( sirD3D9Native_t *pNative, const sirD3D9BufferUpdate_t *pUpdate ) noexcept;
sirGraphicsResult_t sirD3D9NativeCreateShader( sirD3D9Native_t *pNative, const sirGraphicsABIShaderCode_t *pShader, uint32_t nStage,
                                               IUnknown **ppShader ) noexcept;
sirGraphicsResult_t sirD3D9NativeReadback( sirD3D9Native_t *pNative, sirD3D9Readback_t *pReadback ) noexcept;
typedef struct sirD3D9UploadRequest_s
{
	const void *pData;
	uint32_t nBytes;
	uint32_t nAlignment;
	bool bIndex;
} sirD3D9UploadRequest_t;

sirGraphicsResult_t sirD3D9NativeConfigureUploadRings( sirD3D9Native_t *pNative, uint32_t nVertexCapacity,
                                                       uint32_t nIndexCapacity ) noexcept;
sirGraphicsResult_t sirD3D9NativeUpload( sirD3D9Native_t *pNative, const sirD3D9UploadRequest_t *pRequest,
                                         sirD3D9UploadRange_t *pRange ) noexcept;

typedef struct sirD3D9DrawPacket_s
{
	IDirect3DVertexBuffer9 *pVertexBuffer;
	IDirect3DIndexBuffer9 *pIndexBuffer;
	IDirect3DVertexDeclaration9 *pVertexDeclaration;
	IDirect3DVertexShader9 *pVertexShader;
	IDirect3DPixelShader9 *pPixelShader;
	IDirect3DBaseTexture9 *pTexture;
	uint32_t nFVF;
	uint32_t nVertexStride;
	uint32_t nVertexOffset;
	uint32_t nFirstVertex;
	uint32_t nVertexCount;
	uint32_t nFirstIndex;
	uint32_t nPrimitiveCount;
	uint32_t nTopology;
	int32_t iBaseVertex;
} sirD3D9DrawPacket_t;

sirGraphicsResult_t sirD3D9NativeDraw( sirD3D9Native_t *pNative, const sirD3D9DrawPacket_t *pPacket ) noexcept;
sirGraphicsResult_t sirD3D9NativeApplyFixedRecipe( sirD3D9Native_t *pNative, const sirGraphicsABIFixedRecipe_t *pRecipe ) noexcept;
sirGraphicsResult_t sirD3D9NativeDeviceStatus( sirD3D9Native_t *pNative ) noexcept;
void sirD3D9NativeShutdown( sirD3D9Native_t *pNative ) noexcept;
#endif
#endif // __SIRENGINE_SIRD3D9NATIVE_H__
