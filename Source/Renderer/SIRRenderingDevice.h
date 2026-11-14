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

#ifndef __SIRENGINE_SIRRENDERINGDEVICE_H__
#define __SIRENGINE_SIRRENDERINGDEVICE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

// Driver-owned command recording context. Reuse is allowed only after the
// submission fence that last referenced the context has completed.
class SIRGraphicsCommandList
{
protected:
	virtual ~SIRGraphicsCommandList( void ) noexcept = default;

public:
	virtual sirGraphicsQueue_t getQueue( void ) const noexcept = 0;
	virtual sirGraphicsResult_t reset( void ) noexcept = 0;
	virtual sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept = 0;
	virtual sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource,
	                                        uint64_t nSrcOffset, uint64_t nBytes ) noexcept = 0;
	virtual sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t& copy ) noexcept = 0;
	virtual sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t& pass ) noexcept = 0;
	virtual sirGraphicsResult_t endRenderPass( void ) noexcept = 0;
	virtual sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept = 0;
	virtual sirGraphicsResult_t bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept = 0;
	virtual sirGraphicsResult_t bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes,
	                                              uint32_t nStrideBytes ) noexcept = 0;
	virtual sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, bool bIndex32 ) noexcept = 0;
	virtual sirGraphicsResult_t setViewport( const sirGraphicsViewport_t& viewport, const sirGraphicsRect_t& scissor ) noexcept = 0;
	virtual sirGraphicsResult_t draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex,
	                                  uint32_t nFirstInstance ) noexcept = 0;
	virtual sirGraphicsResult_t drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset,
	                                         uint32_t nFirstInstance ) noexcept = 0;
	virtual sirGraphicsResult_t dispatch( uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept = 0;
	virtual sirGraphicsResult_t close( void ) noexcept = 0;
};

typedef struct sirGraphicsSubmission_s
{
	SIRGraphicsCommandList *const *ppCommandLists;
	const sirGraphicsCompletion_t *pWaits;
	const sirGraphicsSurfaceImage_t *pAcquisitions;
	uint32_t nCommandListCount;
	uint32_t nWaitCount;
	uint32_t nAcquisitionCount;
} sirGraphicsSubmission_t;

// One backend-independent device ownership contract. The backend owns all
// native handles, queue objects, descriptor heaps and physical GPU backing.
// GRDG can later register canonical identities without replacing this ABI.
class SIRRenderingDevice
{
protected:
	virtual ~SIRRenderingDevice( void ) noexcept = default;

public:
	virtual sirGraphicsBackend_t getBackend( void ) const noexcept = 0;
	virtual uint32_t getDeviceGeneration( void ) const noexcept = 0;
	virtual sirGraphicsResult_t queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept = 0;
	// Device-advertised pipeline-era support; no inference from backend or API version.
	virtual sirGraphicsResult_t queryPipelineModelCaps( sirGraphicsPipelineModel_t eModel,
	                                                    sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept = 0;
	virtual sirGraphicsResult_t queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept = 0;

	virtual sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual sirGraphicsResult_t createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual sirGraphicsResult_t createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual sirGraphicsResult_t createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual sirGraphicsResult_t createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex, sirGraphicsHandle_t *pHandle ) noexcept = 0;
	virtual sirGraphicsResult_t updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
	                                            uint32_t nWriteCount ) noexcept = 0;
	virtual sirGraphicsResult_t mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes,
	                                       sirGraphicsMappedBuffer_t *pMapped ) noexcept = 0;
	virtual sirGraphicsResult_t unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept = 0;
	virtual sirGraphicsResult_t flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept = 0;
	virtual sirGraphicsResult_t invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept = 0;

	virtual sirGraphicsResult_t acquireCommands( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept = 0;
	virtual sirGraphicsResult_t submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t& submission,
	                                    sirGraphicsCompletion_t *pCompletion ) noexcept = 0;
	virtual sirGraphicsResult_t poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept = 0;
	virtual sirGraphicsResult_t wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNanoseconds ) noexcept = 0;
	// Invalidates the public handle immediately. Defers native destruction
	// until every queue fence named in the completion set has retired.
	virtual sirGraphicsResult_t retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept = 0;

	// Window creation and events remain owned by the platform layer.
	virtual sirGraphicsResult_t createSurface( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& present,
	                                           sirGraphicsHandle_t *pSurface ) noexcept = 0;
	virtual sirGraphicsResult_t resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t& native,
	                                           const sirGraphicsPresentDesc_t& present ) noexcept = 0;
	virtual sirGraphicsResult_t acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept = 0;
	virtual sirGraphicsResult_t present( const sirGraphicsSurfaceImage_t& image,
	                                     const sirGraphicsCompletion_t& renderComplete ) noexcept = 0;
	// Must return BUSY rather than waiting behind the caller's back.
	virtual sirGraphicsResult_t shutdown( void ) noexcept = 0;
};

// One implementation per explicit graphics API. Backend factories own and
// destroy their devices using the engine's configured CPU allocator.
class SIRGraphicsDriver
{
protected:
	virtual ~SIRGraphicsDriver( void ) noexcept = default;

public:
	virtual sirGraphicsBackend_t getBackend( void ) const noexcept = 0;
	virtual sirGraphicsResult_t getAdapterCount( uint32_t *pCount ) const noexcept = 0;
	virtual sirGraphicsResult_t queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept = 0;
	virtual sirGraphicsResult_t createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept = 0;
	virtual sirGraphicsResult_t destroyDevice( SIRRenderingDevice *pDevice ) noexcept = 0;
};

#endif // __SIRENGINE_SIRRENDERINGDEVICE_H__
