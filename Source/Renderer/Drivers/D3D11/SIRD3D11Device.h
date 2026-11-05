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

#ifndef __SIRENGINE_SIRD3D11DEVICE_H__
#define __SIRENGINE_SIRD3D11DEVICE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/SIRGraphicsABIOperations.h>
#include <Renderer/Drivers/D3D11/SIRD3D11FeatureLevels.h>

// The public class holds opaque, singly owned COM references. No Windows SDK
// types cross this header. Device creation occurs in the driver implementation.
typedef struct sirD3D11NativeDeviceInit_s
{
	void *pNativeDevice;
	void *pImmediateContext;
	sirD3D11FeatureProfile_t profile;
	uint64_t nResidentBudgetBytes;
	uint32_t nFramesInFlight;
	uint32_t nSoftwareAdapter;
} sirD3D11NativeDeviceInit_t;

class SIRD3D11Device final : public SIRRenderingDevice
{
public:
	explicit SIRD3D11Device( const sirD3D11NativeDeviceInit_t& init ) noexcept;
	~SIRD3D11Device( void ) noexcept override;

	sirGraphicsBackend_t getBackend( void ) const noexcept override;
	uint32_t getDeviceGeneration( void ) const noexcept override;

	sirGraphicsResult_t queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept override;
	sirGraphicsResult_t queryPipelineModelCaps( sirGraphicsPipelineModel_t eModel,
	                                            sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept override;
	sirGraphicsResult_t queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept override;
	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex,
	                                      sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
	                                    uint32_t nWriteCount ) noexcept override;
	sirGraphicsResult_t mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes,
	                               sirGraphicsMappedBuffer_t *pMapped ) noexcept override;
	sirGraphicsResult_t unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t acquireCommands( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept override;
	sirGraphicsResult_t submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t& submission,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept override;
	sirGraphicsResult_t poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept override;
	sirGraphicsResult_t wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNanoseconds ) noexcept override;
	sirGraphicsResult_t retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept override;
	sirGraphicsResult_t createSurface( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& present,
	                                   sirGraphicsHandle_t *pSurface ) noexcept override;
	sirGraphicsResult_t resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t& native,
	                                   const sirGraphicsPresentDesc_t& present ) noexcept override;
	sirGraphicsResult_t acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept override;
	sirGraphicsResult_t present( const sirGraphicsSurfaceImage_t& image, const sirGraphicsCompletion_t& renderComplete ) noexcept override;
	sirGraphicsResult_t shutdown( void ) noexcept override;

protected:
private:
	void *m_pNativeDevice = nullptr;
	void *m_pImmediateContext = nullptr;
	sirD3D11FeatureProfile_t m_profile = {};
	uint64_t m_nResidentBudgetBytes = 0;
	uint32_t m_nFramesInFlight = 0;
	bool m_bSoftwareAdapter = false;
};

#endif // __SIRENGINE_SIRD3D11DEVICE_H__
