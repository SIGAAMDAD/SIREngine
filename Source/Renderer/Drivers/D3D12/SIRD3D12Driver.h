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

#ifndef __SIRENGINE_SIRD3D12DRIVER_H__
#define __SIRENGINE_SIRD3D12DRIVER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Device.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Resources.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Pipelines.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Commands.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Bindings.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Surfaces.h>

// Direct native implementation of the shared rendering-device contract.
// The existing headless kernels are backend-private test facilities.
class SIRD3D12RenderingDevice final : public SIRRenderingDevice
{
private:
	SIRD3D12Device m_Native;
	SIRD3D12Resources m_Resources;
	SIRD3D12Pipelines m_Pipelines;
	SIRD3D12Bindings m_Bindings;
	SIRD3D12Commands m_Commands;
	SIRD3D12Surfaces m_Surfaces;
	bool m_bSoftware = false;

public:
	SIRD3D12RenderingDevice( void ) noexcept = default;
	~SIRD3D12RenderingDevice( void ) noexcept override = default;
	SIRD3D12RenderingDevice( const SIRD3D12RenderingDevice & ) = delete;
	SIRD3D12RenderingDevice &operator=( const SIRD3D12RenderingDevice & ) = delete;

	sirGraphicsResult_t initialize( bool bSoftware, const sirGraphicsDeviceCreateDesc_t &desc ) noexcept;
	// Native-only snapshot for debug tools and hardware test harnesses.
	sirGraphicsResult_t queryNativeDiagnostics( sirD3D12DiagnosticSnapshot_t *pSnapshot ) noexcept;

	sirGraphicsBackend_t getBackend( void ) const noexcept override;
	uint32_t getDeviceGeneration( void ) const noexcept override;
	sirGraphicsResult_t queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept override;
	sirGraphicsResult_t queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept override;

	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createView( const sirGraphicsViewDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createSampler( const sirGraphicsSamplerDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t createPipeline( const sirGraphicsPipelineDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept override;
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
	sirGraphicsResult_t submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t &submission,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept override;
	sirGraphicsResult_t poll( const sirGraphicsCompletion_t &completion, bool *pCompleted ) noexcept override;
	sirGraphicsResult_t wait( const sirGraphicsCompletion_t &completion, uint64_t nTimeoutNanoseconds ) noexcept override;
	sirGraphicsResult_t retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t &completion ) noexcept override;

	sirGraphicsResult_t createSurface( const sirGraphicsNativeSurface_t &native, const sirGraphicsPresentDesc_t &present,
	                                   sirGraphicsHandle_t *pSurface ) noexcept override;
	sirGraphicsResult_t resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t &native,
	                                   const sirGraphicsPresentDesc_t &present ) noexcept override;
	sirGraphicsResult_t acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept override;
	sirGraphicsResult_t present( const sirGraphicsSurfaceImage_t &image, const sirGraphicsCompletion_t &renderComplete ) noexcept override;
	sirGraphicsResult_t shutdown( void ) noexcept override;
};

// Fixed-capacity driver factory. One native device per registered factory;
// no global object, implicit allocator, or legacy frontend entrypoint.
class SIRD3D12Driver final : public SIRGraphicsDriver
{
private:
	SIRD3D12RenderingDevice m_Device;
	bool m_bDeviceActive = false;

public:
	SIRD3D12Driver( void ) noexcept = default;
	SIRD3D12Driver( const SIRD3D12Driver & ) = delete;
	SIRD3D12Driver &operator=( const SIRD3D12Driver & ) = delete;

	sirGraphicsBackend_t getBackend( void ) const noexcept override;
	sirGraphicsResult_t getAdapterCount( uint32_t *pCount ) const noexcept override;
	sirGraphicsResult_t queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept override;
	sirGraphicsResult_t createDevice( const sirGraphicsDeviceCreateDesc_t &desc, SIRRenderingDevice **ppDevice ) noexcept override;
	sirGraphicsResult_t destroyDevice( SIRRenderingDevice *pDevice ) noexcept override;
};

#endif // __SIRENGINE_SIRD3D12DRIVER_H__
