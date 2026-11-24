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

#ifndef __SIR_RENDER_DEVICE_H__
#define __SIR_RENDER_DEVICE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderCommandList.h>
#include <Renderer/SIRGraphicsTypes.h>

class SIRRenderDevice
{
public:
	constexpr explicit SIRRenderDevice( void ) noexcept = default;
	constexpr explicit SIRRenderDevice( const SIRRenderDevice& ) noexcept = delete;
	constexpr explicit SIRRenderDevice( SIRRenderDevice&& ) noexcept = delete;

	const SIRRenderDevice& operator=( const SIRRenderDevice& ) noexcept = delete;
	const SIRRenderDevice& operator=( SIRRenderDevice&& ) noexcept = delete;

	virtual SIRGraphicsBackend getBackend( void ) const noexcept = 0;
	virtual uint32_t getDeviceGeneration( void ) const noexcept = 0;
	virtual SIRGraphicsResult queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept = 0;

	// Device-advertised pipeline-era support; no inference from backend or API version.
	virtual SIRGraphicsResult queryPipelineModelCaps( SIRGraphicsPipelineModel eModel,
	                                                    sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept = 0;
	virtual SIRGraphicsResult queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept = 0;

	virtual SIRGraphicsResult createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual SIRGraphicsResult createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual SIRGraphicsResult createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual SIRGraphicsResult createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual SIRGraphicsResult createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual SIRGraphicsResult createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex,
	                                              sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual SIRGraphicsResult updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
	                                            uint32_t nWriteCount ) noexcept = 0;
	virtual SIRGraphicsResult mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes,
	                                       sirGraphicsMappedBuffer_t *pMapped ) noexcept = 0;
	virtual SIRGraphicsResult unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept = 0;
	virtual SIRGraphicsResult flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept = 0;
	virtual SIRGraphicsResult invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept = 0;

	virtual SIRGraphicsResult acquireCommands( SIRGraphicsQueue eQueue, SIRRenderCommandList **ppCommands ) noexcept = 0;
	virtual SIRGraphicsResult submit( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission,
	                                    sirGraphicsCompletion_t *pCompletion ) noexcept = 0;
	virtual SIRGraphicsResult poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept = 0;
	virtual SIRGraphicsResult wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNanoseconds ) noexcept = 0;

	/// @brief Invalidates the public handle immediately. Defers native destruction
	/// until every queue fence named in the completion set has retired.
	virtual SIRGraphicsResult retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept = 0;

	// Window creation and events remain owned by the platform layer.
	virtual SIRGraphicsResult createSurface( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& present,
	                                           sirGraphicsHandle_t *pSurface ) noexcept = 0;
	virtual SIRGraphicsResult resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t& native,
	                                           const sirGraphicsPresentDesc_t& present ) noexcept = 0;
	virtual SIRGraphicsResult acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept = 0;

	virtual SIRGraphicsResult present( const sirGraphicsSurfaceImage_t& image, const sirGraphicsCompletion_t& renderComplete ) noexcept = 0;

	/// @brief Must return BUSY rather than waiting behind the caller's back.
	virtual SIRGraphicsResult shutdown( void ) noexcept = 0;

protected:
	virtual ~SIRRenderDevice() noexcept = default;
};

#endif // __SIR_RENDER_DEVICE_H__
