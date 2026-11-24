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

#ifndef __SIR_D3_D12_DRIVER_H__
#define __SIR_D3_D12_DRIVER_H__

#include <Core/SIRPCH.h>

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

// D3D12-specific startup settings must not change SIRRenderingDevice ABI.
typedef struct sirD3D12NativeDeviceConfig_s
{
	// Borrowed Core allocator; must outlive native device shutdown.
	SIRGeneralHeap *pCpuHeap;
	sirD3D12DiagnosticConfig_t diagnostics;
} sirD3D12NativeDeviceConfig_t;

// Direct native implementation of the shared rendering-device contract.
// The existing headless kernels are backend-private test facilities.
class SIRD3D12RenderingDevice final : public SIRRenderingDevice
{
public:
	SIRD3D12RenderingDevice( void ) noexcept = default;
	SIRD3D12RenderingDevice( const SIRD3D12RenderingDevice& ) = delete;
	~SIRD3D12RenderingDevice( void ) noexcept override = default;

	SIRD3D12RenderingDevice& operator=( const SIRD3D12RenderingDevice& ) = delete;

	SIRGraphicsBackend getBackend( void ) const noexcept override;
	uint32_t getDeviceGeneration( void ) const noexcept override;

	SIRGraphicsResult initialize( bool bSoftware, const sirGraphicsDeviceCreateDesc_t& desc,
	                                const sirD3D12NativeDeviceConfig_t& nativeConfig,
	                                const sirD3D12SealedPoolConfig_t *pSealedPools ) noexcept;
	// Native-only cache persistence, keeping the shared rendering ABI unchanged.
	SIRGraphicsResult loadNativePipelineCache( const void *pData, uint64_t nBytes ) noexcept;
	SIRGraphicsResult serializeNativePipelineCache( void *pData, uint64_t *pBytes ) const noexcept;
	SIRGraphicsResult queryNativePipelineCacheStats( sirD3D12PipelineCacheStats_t *pStats ) const noexcept;
	// Native-only snapshot for debug tools and hardware test harnesses.
	SIRGraphicsResult queryNativeDiagnostics( sirD3D12DiagnosticSnapshot_t *pSnapshot ) noexcept;
	SIRGraphicsResult queryNativeAdapterInfo( sirD3D12AdapterInfo_t *pInfo ) const noexcept;

	SIRGraphicsResult queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept override;
	SIRGraphicsResult queryPipelineModelCaps( SIRGraphicsPipelineModel eModel,
	                                            sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept override;
	SIRGraphicsResult queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept override;

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
	SIRD3D12Device m_Native;
	SIRD3D12Resources m_Resources;
	SIRD3D12Pipelines m_Pipelines;
	SIRD3D12Bindings m_Bindings;
	SIRD3D12Commands m_Commands;
	SIRD3D12Surfaces m_Surfaces;
	bool m_bSoftware = false;
	bool m_bSealedPoolsActive = false;
};

// Fixed-capacity driver factory. One native device per registered factory;
// no global object, implicit allocator, or legacy frontend entrypoint.
class SIRD3D12Driver final : public SIRGraphicsDriver
{
public:
	SIRD3D12Driver( void ) noexcept = default;
	SIRD3D12Driver( const SIRD3D12Driver& ) = delete;

	SIRD3D12Driver& operator=( const SIRD3D12Driver& ) = delete;

	SIRGraphicsBackend getBackend( void ) const noexcept override;

	SIRGraphicsResult getAdapterCount( uint32_t *pCount ) const noexcept override;
	SIRGraphicsResult queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept override;
	SIRGraphicsResult createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept override;
	// Native explicit-memory path; the shared rendering-device ABI remains unchanged.
	SIRGraphicsResult createSealedDevice( const sirGraphicsDeviceCreateDesc_t& desc, const sirD3D12SealedPoolConfig_t& pools,
	                                        SIRRenderingDevice **ppDevice ) noexcept;
	SIRGraphicsResult createDeviceWithConfig( const sirGraphicsDeviceCreateDesc_t& desc, const sirD3D12NativeDeviceConfig_t& nativeConfig,
	                                            SIRRenderingDevice **ppDevice ) noexcept;
	SIRGraphicsResult createSealedDeviceWithConfig( const sirGraphicsDeviceCreateDesc_t& desc, const sirD3D12SealedPoolConfig_t& pools,
	                                                  const sirD3D12NativeDeviceConfig_t& nativeConfig,
	                                                  SIRRenderingDevice **ppDevice ) noexcept;
	SIRGraphicsResult destroyDevice( SIRRenderingDevice *pDevice ) noexcept override;

protected:
private:
	SIRD3D12RenderingDevice m_Device;
	bool m_bDeviceActive = false;

	SIRGraphicsResult createDeviceInternal( const sirGraphicsDeviceCreateDesc_t& desc, const sirD3D12NativeDeviceConfig_t& nativeConfig,
	                                          const sirD3D12SealedPoolConfig_t *pSealedPools, SIRRenderingDevice **ppDevice ) noexcept;
};

#endif // __SIR_D3_D12_DRIVER_H__
