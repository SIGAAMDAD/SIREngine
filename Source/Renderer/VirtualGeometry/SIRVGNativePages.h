/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRVGNATIVEPAGES_H__
#define __SIRENGINE_SIRVGNATIVEPAGES_H__

#include <SIRPCH.hpp>
#include <Mesh/SIRVGFramePlan.h>
#include <Mesh/SIRVGPageDirectory.h>
#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Graph/SIRFrameGraph.h>
#include <Resources/SIRResourceViews.h>
#include <Streaming/SIRStreamingDemandReconciler.h>

// The renderer passes the single GRDG-registered native buffer and one
// transient upload reservation. The native device remains the sole allocator.
// Upload reservation reuse is gated by the recorded real queue completion.
typedef struct sirVGNativePagesConfig_s
{
	SIRRenderingDevice *pDevice;
	SIRVGPageDirectory *pDirectory;
	const SIRMeshPackSchema *pSchema;
	sirGraphicsHandle_t hBacking;
	sirGraphicsHandle_t hUpload;
	uint64_t nPhysicalBackingId;
	uint64_t nBackingBytes;
	uint64_t nPageArenaBytes;
	uint64_t nUploadBytes;
} sirVGNativePagesConfig_t;

typedef struct sirVGNativePageUpload_s
{
	const void *pCookedPage;
	uint64_t nCookedBytes;
	uint64_t nNativeOffset;
	uint64_t nUploadOffset;
	uint32_t nPage;
	uint32_t nRequestEpoch;
} sirVGNativePageUpload_t;

// Resolve page bytes only through the canonical GRDG source and the existing
// Streaming coordinator; no separate provider bridge or copy of the payload.
typedef struct sirVGNativePageSource_s
{
	const SIRResourceView *pResource;
	SIRStreamingCoordinator *pStreaming;
	sirStreamingTicket_t ticket;
	uint64_t nNativeOffset;
	uint64_t nUploadOffset;
	uint32_t nPage;
	uint32_t nRequestEpoch;
} sirVGNativePageSource_t;

typedef struct sirVGNativeBVHUpload_s
{
	const SIRResourceView *pResource;
	uint64_t nNativeOffset;
	uint64_t nUploadOffset;
	uint32_t nCollisionProfileIndex;
} sirVGNativeBVHUpload_t;

typedef struct sirVGNativeBVHView_s
{
	sirGraphicsHandle_t hBacking;
	uint64_t nPhysicalBackingId;
	uint64_t nOffsetBytes;
	uint64_t nBytes;
	uint64_t nSourceRevision;
	uint32_t nDeviceGeneration;
} sirVGNativeBVHView_t;

typedef struct sirVGDrawRecording_s
{
	const sirVGFramePlan_t *pPlan;
	SIRGraphicsCommandList *pCommands;
	const uint32_t *pSortedRangeIndices;
	uint32_t nRangeCount;
	uint32_t nMaterialId;
} sirVGDrawRecording_t;

// Frame submission consumes a compiled logical cut, never reselects LOD.
// Material binding sets and pipelines are owned by the renderer/material owner.
typedef struct sirVGMaterialPipeline_s
{
	uint32_t nMaterialId;
	sirGraphicsHandle_t hPipeline;
	sirGraphicsHandle_t hBindingSet;
	bool bHasBindingSet;
} sirVGMaterialPipeline_t;

typedef struct sirVGDrawPassSubmission_s
{
	const sirVGFramePlan_t *pPlan;
	const uint32_t *pSortedRangeIndices;
	const sirVGMaterialPipeline_t *pMaterials;
	const sirGraphicsRenderPass_t *pRenderPass;
	sirGraphicsViewport_t viewport;
	sirGraphicsRect_t scissor;
	uint32_t nSortedRangeCount;
	uint32_t nMaterialCount;
} sirVGDrawPassSubmission_t;

// The existing StreamingDemandReconciler owns tickets, deduplication and
// cancellation. Geometry only translates its immutable logical page demands.
typedef struct sirVGStreamingBatch_s
{
	const sirVGFramePlan_t *pPlan;
	SIRStreamingDemandReconciler *pReconciler;
	sirStreamingDemand_t *pScratch;
	uint32_t nScratchCapacity;
	const SIRMeshPackSchema *pSchema;
	sirMeshKey_t keySource;
	uint32_t nManifestGeneration;
	uint32_t nConsumerGeneration;
	uint64_t nConsumerId;
	uint32_t nRequestEpoch;
} sirVGStreamingBatch_t;

sirStreamingResult_t sirVGReconcileStreaming( const sirVGStreamingBatch_t& batch, sirStreamingReconcileStats_t *pStats ) noexcept;

class SIRVGNativePages
{
public:
	SIRVGNativePages( void ) noexcept;
	SIRVGNativePages( const SIRVGNativePages& ) = delete;
	SIRVGNativePages& operator=( const SIRVGNativePages& ) = delete;

	sirGraphicsResult_t initialize( const sirVGNativePagesConfig_t& config ) noexcept;
	sirGraphicsResult_t submitUpload( const sirVGNativePageUpload_t& upload ) noexcept;
	sirGraphicsResult_t submitGRDGPage( const sirVGNativePageSource_t& source ) noexcept;
	sirGraphicsResult_t submitSharedBVH( const sirVGNativeBVHUpload_t& upload ) noexcept;
	sirGraphicsResult_t getSharedBVH( sirVGNativeBVHView_t *pView ) const noexcept;
	sirGraphicsResult_t pollUpload( bool *pReady ) noexcept;
	sirGraphicsResult_t recordMaterialDraws( const sirVGDrawRecording_t& recording, uint32_t *pDrawCount ) noexcept;
	// Declare the exact persistent native buffer as a FrameGraph read, sharing
	// its identity with visibility, ray queries and other renderer consumers.
	sirGraphicsResult_t declareFrameGraphDrawRead( SIRFrameGraph& graph, uint32_t nPass, uint32_t *pResourceIndex ) const noexcept;
	sirGraphicsResult_t submitDrawPass( const sirVGDrawPassSubmission_t& submission, sirGraphicsCompletion_t *pCompletion ) noexcept;
	sirGraphicsResult_t markDrawsSubmitted( const sirVGFramePlan_t& plan, uint64_t nGraphicsFence ) noexcept;
	sirGraphicsResult_t shutdown( void ) noexcept;

protected:
private:
	bool validDevice( void ) const noexcept;
	sirGraphicsResult_t getDecodedBytes( uint32_t nPage, uint64_t *pVertices, uint64_t *pTotal ) const noexcept;
	sirGraphicsResult_t recordPageUpload( const sirVGNativePageUpload_t& upload, uint64_t nDecodedBytes ) noexcept;

	sirVGNativePagesConfig_t m_Config;
	sirGraphicsCompletion_t m_UploadCompletion;
	uint32_t m_nUploadPage;
	uint32_t m_nUploadEpoch;
	bool m_bUploadPending;
	bool m_bBackingInVertexState;
	SIRStreamingCoordinator *m_pStreaming;
	sirStreamingTicket_t m_StreamingTicket;
	bool m_bStreamingTicket;
	bool m_bBVHPending;
	bool m_bBVHResident;
	uint64_t m_nBVHOffset;
	uint64_t m_nBVHBytes;
};

#endif // __SIRENGINE_SIRVGNATIVEPAGES_H__
