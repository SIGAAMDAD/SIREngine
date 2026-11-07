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

typedef struct sirD3D9ShaderProfile_s
{
	uint8_t nMajor;
	uint8_t nMinor;
	uint8_t nStage;
	uint8_t nReserved;
} sirD3D9ShaderProfile_t;

// Verifies a D3D9-native shader token stream's outer envelope and SM2/SM3
// profile ceiling; the SDK performs full instruction validation on creation.
sirGraphicsResult_t sirD3D9ValidateShaderTokens( const void *pBytes, uint64_t nBytes, uint32_t nStage, uint32_t nMaximumProfile ) noexcept;
sirGraphicsResult_t sirD3D9ValidateTexture( const sirGraphicsTextureDesc_t *pTexture, uint32_t nMaximumDimension,
                                            uint32_t nMaximumSamples ) noexcept;
sirGraphicsResult_t sirD3D9ValidateBuffer( const sirGraphicsBufferDesc_t *pBuffer ) noexcept;

#endif // __SIRENGINE_SIRD3D9VALIDATION_H__
