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
#ifndef __SIRENGINE_SIRD3D9VALIDATION_H__
#define __SIRENGINE_SIRD3D9VALIDATION_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Renderer/SIRGraphicsTypes.h>
#include <Renderer/SIRGraphicsABIOperations.h>

typedef struct sirD3D9ShaderProfile_s
{
	uint8_t nMajor;
	uint8_t nMinor;
	uint8_t nStage;
	uint8_t nReserved;
} sirD3D9ShaderProfile_t;

// D3D9 shader tokens are a distinct binary format from DXBC/DXIL.
// Constant registers are counted in float4/int4/bool units, not bytes.
typedef struct sirD3D9ShaderConstants_s
{
	uint32_t nStage;
	uint32_t nFirstRegister;
	uint32_t nRegisterCount;
	const float *pFloat4;
	const int32_t *pInt4;
	const int32_t *pBool;
} sirD3D9ShaderConstants_t;

sirGraphicsResult_t sirD3D9ValidateShaderConstants( const sirD3D9ShaderConstants_t *pConstants, uint32_t nShaderProfile,
                                                    uint32_t nMaximumVertexFloat4 ) noexcept;

// Verifies a D3D9-native shader token stream's outer envelope and SM2/SM3
// profile ceiling; the SDK performs full instruction validation on creation.
sirGraphicsResult_t sirD3D9ValidateShaderTokens( const void *pBytes, uint64_t nBytes, uint32_t nStage, uint32_t nMaximumProfile ) noexcept;
sirGraphicsResult_t sirD3D9ValidateSampler( const sirGraphicsSamplerDesc_t *pSampler, uint32_t nMaximumAnisotropy ) noexcept;
sirGraphicsResult_t sirD3D9ValidateTexture( const sirGraphicsTextureDesc_t *pTexture, uint32_t nMaximumDimension,
                                            uint32_t nMaximumSamples ) noexcept;
sirGraphicsResult_t sirD3D9ValidateBuffer( const sirGraphicsBufferDesc_t *pBuffer ) noexcept;
// A write must remain within its native allocation and respect the element alignment.
sirGraphicsResult_t sirD3D9ValidateBufferWrite( uint32_t nCapacity, uint32_t nOffset, uint32_t nBytes, uint32_t nAlignment ) noexcept;
typedef struct sirD3D9TextureUploadLayout_s
{
	sirGraphicsFormat_t eFormat;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nRowPitch;
	uint64_t nBytes;
} sirD3D9TextureUploadLayout_t;

sirGraphicsResult_t sirD3D9ValidateTextureUploadLayout( const sirD3D9TextureUploadLayout_t *pLayout, uint32_t *pRowBytes,
                                                        uint32_t *pRows ) noexcept;
sirGraphicsResult_t sirD3D9EstimateTextureBytes( const sirGraphicsTextureDesc_t *pTexture, uint64_t *pBytes ) noexcept;
sirGraphicsResult_t sirD3D9ValidateDrawRange( uint32_t nTopology, uint32_t nPrimitiveCount, uint32_t nFirstElement,
                                              uint32_t nAvailableElements ) noexcept;
sirGraphicsResult_t sirD3D9ValidateFixedRecipe( const sirGraphicsABIFixedRecipe_t *pRecipe, uint32_t nMaximumStages,
                                                uint32_t nMaximumTextures, uint32_t nMaximumLights ) noexcept;

#endif // __SIRENGINE_SIRD3D9VALIDATION_H__
