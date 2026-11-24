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

#ifndef __SIR_D3_D11_DEVICE_H__
#define __SIR_D3_D11_DEVICE_H__

#include <Core/SIRPCH.h>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/SIRGraphicsABIOperations.h>
#include <Renderer/Drivers/D3D11/SIRD3D11FeatureLevels.h>
#include <Renderer/Drivers/D3D11/SIRD3D11Formats.h>
#include <Renderer/Drivers/D3D11/SIRD3D11Handles.h>

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
	uint32_t nDeviceGeneration;
} sirD3D11NativeDeviceInit_t;

class SIRD3D11Device final : public SIRRenderingDevice
{
public:
	explicit SIRD3D11Device( const sirD3D11NativeDeviceInit_t& init ) noexcept;
	~SIRD3D11Device( void ) noexcept override;

	SIRGraphicsBackend getBackend( void ) const noexcept override;
	uint32_t getDeviceGeneration( void ) const noexcept override;

	SIRGraphicsResult queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept override;
	SIRGraphicsResult queryPipelineModelCaps( SIRGraphicsPipelineModel eModel,
	                                            sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept override;
	SIRGraphicsResult queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept override;
	SIRGraphicsResult queryNativeFormatSupport( SIRGraphicsFormat eFormat, sirD3D11FormatSupport_t *pSupport ) const noexcept;
	SIRGraphicsResult createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex,
	                                      sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
	                                    uint32_t nWriteCount ) noexcept override;
	SIRGraphicsResult mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes,
	                               sirGraphicsMappedBuffer_t *pMapped ) noexcept override;
	SIRGraphicsResult unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept override;
	SIRGraphicsResult flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept override;
	SIRGraphicsResult invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept override;
	SIRGraphicsResult acquireCommands( SIRGraphicsQueue eQueue, SIRGraphicsCommandList **ppCommands ) noexcept override;
	SIRGraphicsResult submit( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept override;
	SIRGraphicsResult poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept override;
	SIRGraphicsResult wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNanoseconds ) noexcept override;
	SIRGraphicsResult retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept override;
	SIRGraphicsResult createSurface( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& present,
	                                   sirGraphicsHandle_t *pSurface ) noexcept override;
	SIRGraphicsResult resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t& native,
	                                   const sirGraphicsPresentDesc_t& present ) noexcept override;
	SIRGraphicsResult acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept override;
	SIRGraphicsResult present( const sirGraphicsSurfaceImage_t& image, const sirGraphicsCompletion_t& renderComplete ) noexcept override;
	SIRGraphicsResult shutdown( void ) noexcept override;

protected:
private:
	void *m_pNativeDevice = nullptr;
	void *m_pImmediateContext = nullptr;
	sirD3D11FeatureProfile_t m_profile {};
	SIRD3D11Handles m_handles;
	uint64_t m_nPeakCommittedBytes = 0;
	uint64_t m_nUploadMappedBytes = 0;
	uint64_t m_nReadbackMappedBytes = 0;
	uint64_t m_nResidentBudgetBytes = 0;
	uint32_t m_nFramesInFlight = 0;
	uint32_t m_nDeviceGeneration = 0;
	bool m_bSoftwareAdapter = false;
};

#endif // __SIR_D3_D11_DEVICE_H__
