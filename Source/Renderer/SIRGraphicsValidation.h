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

#ifndef __SIRENGINE_SIRGRAPHICSVALIDATION_H__
#define __SIRENGINE_SIRGRAPHICSVALIDATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>

bool sirGraphicsIsBackendValid( sirGraphicsBackend_t eBackend ) noexcept;
bool sirGraphicsHasFeatures( uint64_t nAvailable, uint64_t nRequired ) noexcept;
bool sirGraphicsIsHandleValid( const sirGraphicsHandle_t& handle, sirGraphicsHandleType_t eExpected, uint32_t nDeviceGeneration ) noexcept;
bool sirGraphicsIsCompletionSetValid( const sirGraphicsCompletionSet_t& completion, uint32_t nDeviceGeneration ) noexcept;
sirGraphicsResult_t sirGraphicsValidateBuffer( const sirGraphicsBufferDesc_t& desc ) noexcept;
sirGraphicsResult_t sirGraphicsValidateTexture( const sirGraphicsTextureDesc_t& desc ) noexcept;
sirGraphicsResult_t sirGraphicsValidateSampler( const sirGraphicsSamplerDesc_t& desc ) noexcept;
sirGraphicsResult_t sirGraphicsValidatePipeline( const sirGraphicsPipelineDesc_t& desc ) noexcept;
sirGraphicsResult_t sirGraphicsValidateSubmission( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t& submission,
                                                   uint32_t nDeviceGeneration ) noexcept;

#endif // __SIRENGINE_SIRGRAPHICSVALIDATION_H__
