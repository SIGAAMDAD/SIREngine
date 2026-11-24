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

#ifndef __SIR_GRAPHICSVALIDATION_H__
#define __SIR_GRAPHICSVALIDATION_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>

bool sirGraphicsIsBackendValid( SIRGraphicsBackend eBackend ) noexcept;
bool sirGraphicsHasFeatures( uint64_t nAvailable, uint64_t nRequired ) noexcept;
bool sirGraphicsIsHandleValid( const sirGraphicsHandle_t& handle, SIRGraphicsHandleType eExpected, uint32_t nDeviceGeneration ) noexcept;
bool sirGraphicsIsCompletionSetValid( const sirGraphicsCompletionSet_t& completion, uint32_t nDeviceGeneration ) noexcept;
SIRGraphicsResult sirGraphicsValidateBuffer( const sirGraphicsBufferDesc_t& desc ) noexcept;
SIRGraphicsResult sirGraphicsValidateTexture( const sirGraphicsTextureDesc_t& desc ) noexcept;
SIRGraphicsResult sirGraphicsValidateSampler( const sirGraphicsSamplerDesc_t& desc ) noexcept;
SIRGraphicsResult sirGraphicsValidatePipeline( const sirGraphicsPipelineDesc_t& desc ) noexcept;
SIRGraphicsResult sirGraphicsValidateSubmission( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission,
                                                   uint32_t nDeviceGeneration ) noexcept;

#endif // __SIR_GRAPHICSVALIDATION_H__
