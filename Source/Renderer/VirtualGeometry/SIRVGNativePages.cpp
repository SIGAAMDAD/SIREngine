/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/VirtualGeometry/SIRVGNativePages.h>

#include <stdint.h>

sirStreamingResult_t sirVGReconcileStreaming( const sirVGStreamingBatch_t& batch, sirStreamingReconcileStats_t *pStats ) noexcept
{
	if ( batch.pPlan == nullptr || batch.pReconciler == nullptr || batch.pSchema == nullptr || pStats == nullptr ||
	     !batch.pSchema->isOpen() || batch.nManifestGeneration == 0 || batch.keySource.nAssetId == 0 || batch.keySource.nRevision == 0 ||
	     batch.keySource.nVariant > UINT16_MAX || batch.nConsumerId == 0 || batch.nConsumerGeneration == 0 || batch.nRequestEpoch == 0 ||
	     sirVGValidateFramePlan( *batch.pPlan ) != SIR_VG_PLAN_OK )
		return SIR_STREAMING_INVALID_ARGUMENT;
	const sirMeshKey_t schemaKey = batch.pSchema->getKey();
	if ( schemaKey.nAssetId != batch.keySource.nAssetId || schemaKey.nRevision != batch.keySource.nRevision ||
	     schemaKey.nVariant != batch.keySource.nVariant )
		return SIR_STREAMING_INVALID_ARGUMENT;
	if ( batch.pPlan->nDemandCount > batch.nScratchCapacity || ( batch.pPlan->nDemandCount != 0 && batch.pScratch == nullptr ) )
		return SIR_STREAMING_OUT_OF_CAPACITY;
	// The manifest generation is the source/package revision. A page-directory
	// snapshot changes after every upload and must not invalidate tickets.
	for ( uint32_t i = 0; i < batch.pPlan->nDemandCount; ++i ) {
		const sirVGPageDemand_t& demand = batch.pPlan->pDemands[i];
		sirStreamingDemand_t& out = batch.pScratch[i];
		out = {};
		out.keyUnit.nContainerId = batch.keySource.nAssetId;
		out.keyUnit.nVariant = ( uint16_t )batch.keySource.nVariant;
		sirMeshPackPage_t page = {};
		if ( demand.nPageId == 0 || demand.nPageId > UINT32_MAX ||
		     batch.pSchema->pageAt( ( uint32_t )( demand.nPageId - 1 ), &page ) != SIR_MESH_PACK_OK || page.nChunkId == 0 )
			return SIR_STREAMING_INVALID_ARGUMENT;
		out.keyUnit.nUnitId = page.nChunkId;
		out.keyUnit.nManifestGeneration = batch.nManifestGeneration;
		out.keyUnit.eKind = SIR_STREAMING_UNIT_RESOURCE_BUNDLE;
		out.nConsumerId = batch.nConsumerId;
		out.nConsumerGeneration = batch.nConsumerGeneration;
		out.nRequestEpoch = batch.nRequestEpoch;
		out.nDeadlineTick = demand.nDeadlineTick;
		out.nPriority = demand.nPriority;
		out.eTarget = SIR_STREAMING_TARGET_ACTIVE;
		if ( out.keyUnit.nContainerId == 0 || demand.nRequestEpoch != batch.nRequestEpoch )
			return SIR_STREAMING_INVALID_ARGUMENT;
	}
	return batch.pReconciler->reconcile( batch.pScratch, batch.pPlan->nDemandCount, batch.nRequestEpoch, pStats );
}

static bool sirVGNativeBufferValid( sirGraphicsHandle_t handle, uint32_t nGeneration ) noexcept
{
	return handle.eType == SIR_GRAPHICS_HANDLE_BUFFER && handle.nGeneration != 0 && handle.nDeviceGeneration == nGeneration;
}

SIRVGNativePages::SIRVGNativePages( void ) noexcept
    : m_Config{}, m_UploadCompletion{}, m_nUploadPage( 0 ), m_nUploadEpoch( 0 ), m_bUploadPending( false ),
      m_bBackingInVertexState( false ), m_pStreaming( nullptr ), m_StreamingTicket{}, m_bStreamingTicket( false ), m_bBVHPending( false ),
      m_bBVHResident( false ), m_nBVHOffset( 0 ), m_nBVHBytes( 0 )
{
}

bool SIRVGNativePages::validDevice( void ) const noexcept
{
	return m_Config.pDevice != nullptr && m_Config.pDirectory != nullptr && m_Config.pSchema != nullptr &&
	       m_Config.pDirectory->getDeviceGeneration() == m_Config.pDevice->getDeviceGeneration() &&
	       m_Config.pDirectory->getBackingId() == m_Config.nPhysicalBackingId;
}

sirGraphicsResult_t SIRVGNativePages::initialize( const sirVGNativePagesConfig_t& config ) noexcept
{
	if ( m_Config.pDevice != nullptr || config.pDevice == nullptr || config.pDirectory == nullptr || config.pSchema == nullptr ||
	     !config.pSchema->isOpen() || config.nPhysicalBackingId == 0 || config.nBackingBytes == 0 || config.nPageArenaBytes == 0 ||
	     config.nUploadBytes == 0 || config.nPageArenaBytes > config.nBackingBytes ||
	     config.nPageArenaBytes != config.pDirectory->getBackingCapacityBytes() )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	const uint32_t nGeneration = config.pDevice->getDeviceGeneration();
	if ( nGeneration == 0 || !sirVGNativeBufferValid( config.hBacking, nGeneration ) ||
	     !sirVGNativeBufferValid( config.hUpload, nGeneration ) || config.pDirectory->getDeviceGeneration() != nGeneration ||
	     config.pDirectory->getBackingId() != config.nPhysicalBackingId )
		return SIR_GRAPHICS_INVALID_HANDLE;
	m_Config = config;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::getDecodedBytes( uint32_t nPage, uint64_t *pVertices, uint64_t *pTotal ) const noexcept
{
	if ( !validDevice() || pVertices == nullptr || pTotal == nullptr )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	if ( m_Config.pSchema->getDecodedPageCounts( nPage, &nVertices, &nIndices ) != SIR_MESH_PACK_OK || nVertices == 0 || nIndices == 0 )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	*pVertices = ( uint64_t )nVertices * sizeof( sirMeshPackVertex_t );
	*pTotal = *pVertices + ( uint64_t )nIndices * sizeof( uint32_t );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::recordPageUpload( const sirVGNativePageUpload_t& upload, uint64_t nDecodedBytes ) noexcept
{
	SIRGraphicsCommandList *pCommands = nullptr;
	sirGraphicsResult_t eResult = m_Config.pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCommands );
	if ( eResult != SIR_GRAPHICS_SUCCESS || pCommands == nullptr )
		return eResult == SIR_GRAPHICS_SUCCESS ? SIR_GRAPHICS_DRIVER_ERROR : eResult;
	eResult = pCommands->reset();
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	sirGraphicsBarrier_t toCopy = {};
	toCopy.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	toCopy.hResource = m_Config.hBacking;
	toCopy.eBefore = m_bBackingInVertexState ? SIR_GRAPHICS_STATE_VERTEX_INDEX : SIR_GRAPHICS_STATE_COMMON;
	toCopy.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	eResult = pCommands->barrier( &toCopy, 1 );
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	eResult = pCommands->copyBuffer( m_Config.hBacking, upload.nNativeOffset, m_Config.hUpload, upload.nUploadOffset, nDecodedBytes );
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	sirGraphicsBarrier_t toDraw = toCopy;
	toDraw.eBefore = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	toDraw.eAfter = SIR_GRAPHICS_STATE_VERTEX_INDEX;
	eResult = pCommands->barrier( &toDraw, 1 );
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	eResult = pCommands->close();
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	SIRGraphicsCommandList *pCommandLists[] = { pCommands };
	sirGraphicsSubmission_t submission = {};
	submission.ppCommandLists = pCommandLists;
	submission.nCommandListCount = 1;
	eResult = m_Config.pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &m_UploadCompletion );
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	m_bBackingInVertexState = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::submitUpload( const sirVGNativePageUpload_t& upload ) noexcept
{
	if ( !validDevice() || m_bUploadPending || upload.pCookedPage == nullptr || upload.nCookedBytes == 0 || upload.nRequestEpoch == 0 )
		return m_bUploadPending ? SIR_GRAPHICS_BUSY : SIR_GRAPHICS_INVALID_ARGUMENT;
	uint64_t nVertexBytes = 0;
	uint64_t nDecodedBytes = 0;
	if ( getDecodedBytes( upload.nPage, &nVertexBytes, &nDecodedBytes ) != SIR_GRAPHICS_SUCCESS )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	sirVGNativePageUpload_t accepted = upload;
	if ( accepted.nNativeOffset == UINT64_MAX &&
	     m_Config.pDirectory->findFreeSpan( nDecodedBytes, 16, &accepted.nNativeOffset ) != SIR_VG_RESIDENCY_OK )
		return SIR_GRAPHICS_BUSY;
	if ( accepted.nNativeOffset % 16 != 0 || accepted.nUploadOffset % 16 != 0 || accepted.nNativeOffset > m_Config.nPageArenaBytes ||
	     nDecodedBytes > m_Config.nPageArenaBytes - accepted.nNativeOffset || accepted.nUploadOffset > m_Config.nUploadBytes ||
	     nDecodedBytes > m_Config.nUploadBytes - accepted.nUploadOffset )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	const sirVGPageAdmission_t admission = { upload.pCookedPage,
	                                         upload.nCookedBytes,
	                                         nDecodedBytes,
	                                         m_Config.nPhysicalBackingId,
	                                         accepted.nNativeOffset,
	                                         UINT64_MAX,
	                                         0,
	                                         0,
	                                         upload.nPage,
	                                         m_Config.pDevice->getDeviceGeneration(),
	                                         upload.nRequestEpoch };
	if ( m_Config.pDirectory->admit( admission ) != SIR_VG_RESIDENCY_OK )
		return SIR_GRAPHICS_BUSY;
	sirGraphicsMappedBuffer_t mapped = {};
	sirGraphicsResult_t eResult = m_Config.pDevice->mapBuffer( m_Config.hUpload, accepted.nUploadOffset, nDecodedBytes, &mapped );
	if ( eResult != SIR_GRAPHICS_SUCCESS || mapped.pData == nullptr ) {
		( void )m_Config.pDirectory->abortAdmission( upload.nPage, upload.nRequestEpoch );
		return eResult == SIR_GRAPHICS_SUCCESS ? SIR_GRAPHICS_DRIVER_ERROR : eResult;
	}
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	( void )m_Config.pSchema->getDecodedPageCounts( upload.nPage, &nVertices, &nIndices );
	sirMeshPackPageDecode_t decode = { upload.pCookedPage,
	                                   upload.nCookedBytes,
	                                   upload.nPage,
	                                   ( sirMeshPackVertex_t * )mapped.pData,
	                                   ( uint32_t * )( ( uint8_t * )mapped.pData + nVertexBytes ),
	                                   nVertices,
	                                   nIndices };
	const sirMeshPackResult_t eDecode = m_Config.pSchema->decodePage( decode );
	if ( eDecode != SIR_MESH_PACK_OK ) {
		( void )m_Config.pDevice->unmapBuffer( m_Config.hUpload, accepted.nUploadOffset, nDecodedBytes );
		( void )m_Config.pDirectory->abortAdmission( upload.nPage, upload.nRequestEpoch );
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	eResult = m_Config.pDevice->flushBuffer( m_Config.hUpload, accepted.nUploadOffset, nDecodedBytes );
	const sirGraphicsResult_t eUnmap = m_Config.pDevice->unmapBuffer( m_Config.hUpload, accepted.nUploadOffset, nDecodedBytes );
	if ( eResult == SIR_GRAPHICS_SUCCESS )
		eResult = eUnmap;
	if ( eResult == SIR_GRAPHICS_SUCCESS )
		eResult = recordPageUpload( accepted, nDecodedBytes );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		( void )m_Config.pDirectory->abortAdmission( upload.nPage, upload.nRequestEpoch );
		return eResult;
	}
	m_nUploadPage = upload.nPage;
	m_nUploadEpoch = upload.nRequestEpoch;
	m_bUploadPending = true;
	if ( m_UploadCompletion.eQueue != SIR_GRAPHICS_QUEUE_DIRECT || m_UploadCompletion.nValue == 0 ||
	     m_Config.pDirectory->attachSubmission( upload.nPage, upload.nRequestEpoch, m_UploadCompletion.nValue ) != SIR_VG_RESIDENCY_OK )
		return SIR_GRAPHICS_DRIVER_ERROR;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::submitGRDGPage( const sirVGNativePageSource_t& source ) noexcept
{
	if ( !validDevice() || source.pResource == nullptr || source.nRequestEpoch == 0 || source.nPage >= m_Config.pSchema->getPageCount() ||
	     m_bUploadPending )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	SIRResourceInfo info = {};
	if ( source.pResource->getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS || info.m_eKind != SIR_RESOURCE_KIND_GEOMETRY ||
	     info.m_eState != SIR_RESOURCE_STATE_RESIDENT )
		return SIR_GRAPHICS_INVALID_HANDLE;
	const sirMeshKey_t key = m_Config.pSchema->getKey();
	if ( info.m_Key.m_nAssetId != key.nAssetId || info.m_nRevision != key.nRevision || info.m_Key.m_nVariantId != key.nVariant )
		return SIR_GRAPHICS_INVALID_HANDLE;
	sirMeshPackPage_t page = {};
	if ( m_Config.pSchema->pageAt( source.nPage, &page ) != SIR_MESH_PACK_OK )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	if ( source.pStreaming != nullptr ) {
		sirStreamingSnapshot_t snapshot = {};
		if ( source.pStreaming->query( source.ticket, &snapshot ) != SIR_STREAMING_OK || snapshot.eStage != SIR_STREAMING_STAGE_PREPARED ||
		     snapshot.bCancellationRequested || snapshot.demand.nRequestEpoch != source.nRequestEpoch ||
		     snapshot.demand.keyUnit.nContainerId != key.nAssetId || snapshot.demand.keyUnit.nUnitId != page.nChunkId )
			return SIR_GRAPHICS_BUSY;
	}
	SIRResourceConstBytes bytes = {};
	if ( source.pResource->getChunk( { page.nChunkId }, &bytes ) != SIR_RESOURCE_RESULT_SUCCESS || bytes.m_pData == nullptr ||
	     bytes.m_nSize != page.nByteCount )
		return SIR_GRAPHICS_INVALID_HANDLE;
	const sirVGNativePageUpload_t upload = { bytes.m_pData,        bytes.m_nSize, source.nNativeOffset,
	                                         source.nUploadOffset, source.nPage,  source.nRequestEpoch };
	const sirGraphicsResult_t eSubmit = submitUpload( upload );
	if ( eSubmit == SIR_GRAPHICS_SUCCESS && source.pStreaming != nullptr ) {
		m_pStreaming = source.pStreaming;
		m_StreamingTicket = source.ticket;
		m_bStreamingTicket = true;
	}
	return eSubmit;
}

sirGraphicsResult_t SIRVGNativePages::submitSharedBVH( const sirVGNativeBVHUpload_t& upload ) noexcept
{
	if ( !validDevice() || m_bUploadPending || m_bBVHResident || upload.pResource == nullptr )
		return SIR_GRAPHICS_BUSY;
	SIRResourceInfo info = {};
	if ( upload.pResource->getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS || info.m_eKind != SIR_RESOURCE_KIND_GEOMETRY ||
	     info.m_eState != SIR_RESOURCE_STATE_RESIDENT )
		return SIR_GRAPHICS_INVALID_HANDLE;
	const sirMeshKey_t key = m_Config.pSchema->getKey();
	if ( info.m_Key.m_nAssetId != key.nAssetId || info.m_nRevision != key.nRevision || info.m_Key.m_nVariantId != key.nVariant )
		return SIR_GRAPHICS_INVALID_HANDLE;
	sirMeshPackCollision_t collision = {};
	if ( m_Config.pSchema->collisionAt( upload.nCollisionProfileIndex, &collision ) != SIR_MESH_PACK_OK )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	SIRResourceConstBytes bytes = {};
	if ( upload.pResource->getChunk( { collision.nBVHChunkId }, &bytes ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     m_Config.pSchema->verifyBVH( upload.nCollisionProfileIndex, bytes.m_pData, bytes.m_nSize ) != SIR_MESH_PACK_OK )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	if ( upload.nNativeOffset % 16 || upload.nUploadOffset % 16 || upload.nNativeOffset < m_Config.nPageArenaBytes ||
	     upload.nNativeOffset > m_Config.nBackingBytes || bytes.m_nSize > m_Config.nBackingBytes - upload.nNativeOffset ||
	     upload.nUploadOffset > m_Config.nUploadBytes || bytes.m_nSize > m_Config.nUploadBytes - upload.nUploadOffset )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	sirGraphicsMappedBuffer_t mapped = {};
	sirGraphicsResult_t eResult = m_Config.pDevice->mapBuffer( m_Config.hUpload, upload.nUploadOffset, bytes.m_nSize, &mapped );
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	memcpy( mapped.pData, bytes.m_pData, ( size_t )bytes.m_nSize );
	eResult = m_Config.pDevice->flushBuffer( m_Config.hUpload, upload.nUploadOffset, bytes.m_nSize );
	const sirGraphicsResult_t eUnmap = m_Config.pDevice->unmapBuffer( m_Config.hUpload, upload.nUploadOffset, bytes.m_nSize );
	if ( eResult == SIR_GRAPHICS_SUCCESS )
		eResult = eUnmap;
	const sirVGNativePageUpload_t command = { bytes.m_pData, bytes.m_nSize, upload.nNativeOffset, upload.nUploadOffset, 0, 0 };
	if ( eResult == SIR_GRAPHICS_SUCCESS )
		eResult = recordPageUpload( command, bytes.m_nSize );
	if ( eResult != SIR_GRAPHICS_SUCCESS )
		return eResult;
	m_nBVHOffset = upload.nNativeOffset;
	m_nBVHBytes = bytes.m_nSize;
	m_bUploadPending = true;
	m_bBVHPending = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::getSharedBVH( sirVGNativeBVHView_t *pView ) const noexcept
{
	if ( !validDevice() || !m_bBVHResident || pView == nullptr )
		return SIR_GRAPHICS_INVALID_HANDLE;
	*pView = { m_Config.hBacking, m_Config.nPhysicalBackingId,          m_nBVHOffset,
	           m_nBVHBytes,       m_Config.pSchema->getKey().nRevision, m_Config.pDevice->getDeviceGeneration() };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::pollUpload( bool *pReady ) noexcept
{
	if ( pReady == nullptr || !validDevice() )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	*pReady = false;
	if ( !m_bUploadPending )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	bool bComplete = false;
	const sirGraphicsResult_t ePoll = m_Config.pDevice->poll( m_UploadCompletion, &bComplete );
	if ( ePoll != SIR_GRAPHICS_SUCCESS || !bComplete )
		return ePoll;
	if ( m_bBVHPending ) {
		m_bBVHPending = false;
		m_bBVHResident = true;
		m_bUploadPending = false;
		*pReady = true;
		return SIR_GRAPHICS_SUCCESS;
	}
	bool bPublish = true;
	if ( m_bStreamingTicket ) {
		sirStreamingSnapshot_t snapshot = {};
		if ( m_pStreaming->query( m_StreamingTicket, &snapshot ) != SIR_STREAMING_OK || snapshot.bCancellationRequested ||
		     snapshot.eStage != SIR_STREAMING_STAGE_PREPARED || snapshot.demand.nRequestEpoch != m_nUploadEpoch )
			bPublish = false;
	}
	const sirVGPageCompletion_t completion = {
	    m_UploadCompletion.nValue, 0, 0, m_nUploadPage, m_UploadCompletion.nDeviceGeneration, m_nUploadEpoch, bPublish };
	const sirVGResidencyResult_t ePublish = m_Config.pDirectory->complete( completion );
	if ( ePublish == SIR_VG_RESIDENCY_OK && m_bStreamingTicket )
		( void )m_pStreaming->advance( m_StreamingTicket, SIR_STREAMING_STAGE_ACTIVE );
	m_bStreamingTicket = false;
	m_pStreaming = nullptr;
	m_StreamingTicket = {};
	m_bUploadPending = false;
	*pReady = ePublish == SIR_VG_RESIDENCY_OK;
	return ePublish == SIR_VG_RESIDENCY_OK ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_BUSY;
}

sirGraphicsResult_t SIRVGNativePages::recordMaterialDraws( const sirVGDrawRecording_t& recording, uint32_t *pDrawCount ) noexcept
{
	if ( !validDevice() || pDrawCount == nullptr || recording.pPlan == nullptr || recording.pCommands == nullptr ||
	     recording.pSortedRangeIndices == nullptr || recording.nRangeCount > recording.pPlan->nRangeCount ||
	     sirVGValidateFramePlan( *recording.pPlan ) != SIR_VG_PLAN_OK ||
	     !m_Config.pDirectory->isCurrentSnapshot( recording.pPlan->nPageSnapshotGeneration ) )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	*pDrawCount = 0;
	uint32_t nLastPage = UINT32_MAX;
	for ( uint32_t i = 0; i < recording.nRangeCount; ++i ) {
		const uint32_t nRangeIndex = recording.pSortedRangeIndices[i];
		if ( nRangeIndex >= recording.pPlan->nRangeCount )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		const sirVGDrawRange_t& range = recording.pPlan->pRanges[nRangeIndex];
		if ( range.nMaterialId != recording.nMaterialId )
			continue;
		if ( range.nPageId == 0 || range.nPageId > UINT32_MAX )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		const uint32_t nPage = ( uint32_t )( range.nPageId - 1 );
		sirVGPageResidentView_t view = {};
		if ( m_Config.pDirectory->getResidentView( nPage, &view ) != SIR_VG_RESIDENCY_OK ||
		     view.nSnapshotGeneration != recording.pPlan->nPageSnapshotGeneration ||
		     view.nPhysicalBackingId != m_Config.nPhysicalBackingId )
			return SIR_GRAPHICS_BUSY;
		if ( nPage != nLastPage ) {
			uint64_t nVertexBytes = 0;
			uint64_t nTotalBytes = 0;
			if ( getDecodedBytes( nPage, &nVertexBytes, &nTotalBytes ) != SIR_GRAPHICS_SUCCESS || nTotalBytes > view.nNativeBytes )
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			sirGraphicsResult_t eResult =
			    recording.pCommands->bindVertexBuffer( 0, m_Config.hBacking, view.nNativeOffset, sizeof( sirMeshPackVertex_t ) );
			if ( eResult != SIR_GRAPHICS_SUCCESS )
				return eResult;
			eResult = recording.pCommands->bindIndexBuffer( m_Config.hBacking, view.nNativeOffset + nVertexBytes, true );
			if ( eResult != SIR_GRAPHICS_SUCCESS )
				return eResult;
			nLastPage = nPage;
		}
		const sirGraphicsResult_t eDraw = recording.pCommands->drawIndexed( range.nIndexCount, range.nInstanceCount, range.nFirstIndex,
		                                                                    range.nBaseVertex, range.nFirstInstance );
		if ( eDraw != SIR_GRAPHICS_SUCCESS )
			return eDraw;
		++*pDrawCount;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::declareFrameGraphDrawRead( SIRFrameGraph& graph, uint32_t nPass,
                                                                 uint32_t *pResourceIndex ) const noexcept
{
	if ( !validDevice() || pResourceIndex == nullptr || nPass >= graph.getPassCount() || m_bUploadPending ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirFrameGraphResource_t resource = {};
	resource.hImported = m_Config.hBacking;
	resource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	resource.eInitialState = m_bBackingInVertexState ? SIR_GRAPHICS_STATE_VERTEX_INDEX : SIR_GRAPHICS_STATE_COMMON;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	resource.nMipLevels = 1;
	resource.nArrayLayers = 1;
	resource.bImported = true;
	uint32_t nIndex = UINT32_MAX;
	sirGraphicsResult_t result = graph.addResource( resource, &nIndex );
	if ( result != SIR_GRAPHICS_SUCCESS ) {
		return result;
	}
	sirFrameGraphAccess_t access = {};
	access.nPass = nPass;
	access.nResource = nIndex;
	access.nMipCount = 1;
	access.nLayerCount = 1;
	access.eState = SIR_GRAPHICS_STATE_VERTEX_INDEX;
	access.bWrite = false;
	result = graph.addAccess( access );
	if ( result == SIR_GRAPHICS_SUCCESS ) {
		*pResourceIndex = nIndex;
	}
	return result;
}

sirGraphicsResult_t SIRVGNativePages::submitDrawPass( const sirVGDrawPassSubmission_t& submission,
                                                      sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( !validDevice() || pCompletion == nullptr || submission.pPlan == nullptr || submission.pRenderPass == nullptr ||
	     submission.pMaterials == nullptr || submission.nMaterialCount == 0 ||
	     submission.nSortedRangeCount != submission.pPlan->nRangeCount ||
	     ( submission.nSortedRangeCount != 0 && submission.pSortedRangeIndices == nullptr ) ||
	     sirVGValidateFramePlan( *submission.pPlan ) != SIR_VG_PLAN_OK ||
	     !m_Config.pDirectory->isCurrentSnapshot( submission.pPlan->nPageSnapshotGeneration ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Full preflight prevents partially recorded draws for missing pages and
	// unbound material variants. Native resources are never allocated here.
	for ( uint32_t i = 0; i < submission.nSortedRangeCount; ++i ) {
		const uint32_t index = submission.pSortedRangeIndices[i];
		if ( index >= submission.pPlan->nRangeCount ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirVGDrawRange_t& range = submission.pPlan->pRanges[index];
		if ( range.nPageId == 0 || range.nPageId > UINT32_MAX || range.nMaterialId > UINT32_MAX ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !m_Config.pDirectory->isResident( ( uint32_t )( range.nPageId - 1 ) ) ) {
			return SIR_GRAPHICS_BUSY;
		}
		bool bMaterialFound = false;
		for ( uint32_t j = 0; j < submission.nMaterialCount; ++j ) {
			if ( submission.pMaterials[j].nMaterialId == range.nMaterialId ) {
				bMaterialFound = true;
				break;
			}
		}
		if ( !bMaterialFound ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	sirGraphicsResult_t result = m_Config.pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCommands );
	if ( result != SIR_GRAPHICS_SUCCESS || pCommands == nullptr ) {
		return result == SIR_GRAPHICS_SUCCESS ? SIR_GRAPHICS_DRIVER_ERROR : result;
	}
	result = pCommands->reset();
	if ( result != SIR_GRAPHICS_SUCCESS ) {
		return result;
	}
	result = pCommands->beginRenderPass( *submission.pRenderPass );
	if ( result != SIR_GRAPHICS_SUCCESS ) {
		return result;
	}
	result = pCommands->setViewport( submission.viewport, submission.scissor );
	if ( result != SIR_GRAPHICS_SUCCESS ) {
		return result;
	}
	// Batch each cooked material once, using the common sorted logical plan.
	for ( uint32_t j = 0; j < submission.nMaterialCount; ++j ) {
		const sirVGMaterialPipeline_t& material = submission.pMaterials[j];
		uint32_t nDraws = 0;
		for ( uint32_t i = 0; i < submission.nSortedRangeCount; ++i ) {
			if ( submission.pPlan->pRanges[submission.pSortedRangeIndices[i]].nMaterialId == material.nMaterialId ) {
				++nDraws;
			}
		}
		if ( nDraws == 0 ) {
			continue;
		}
		result = pCommands->bindPipeline( material.hPipeline );
		if ( result != SIR_GRAPHICS_SUCCESS ) {
			return result;
		}
		if ( material.bHasBindingSet ) {
			result = pCommands->bindBindingSet( 0, material.hBindingSet );
			if ( result != SIR_GRAPHICS_SUCCESS ) {
				return result;
			}
		}
		const sirVGDrawRecording_t recording = { submission.pPlan, pCommands, submission.pSortedRangeIndices, submission.nSortedRangeCount,
		                                         material.nMaterialId };
		result = recordMaterialDraws( recording, &nDraws );
		if ( result != SIR_GRAPHICS_SUCCESS ) {
			return result;
		}
	}
	result = pCommands->endRenderPass();
	if ( result == SIR_GRAPHICS_SUCCESS ) {
		result = pCommands->close();
	}
	if ( result != SIR_GRAPHICS_SUCCESS ) {
		return result;
	}
	SIRGraphicsCommandList *pLists[] = { pCommands };
	sirGraphicsSubmission_t command = {};
	command.ppCommandLists = pLists;
	command.nCommandListCount = 1;
	sirGraphicsCompletion_t complete = {};
	result = m_Config.pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, command, &complete );
	if ( result != SIR_GRAPHICS_SUCCESS ) {
		return result;
	}
	// Even when the directory snapshot changes after recording, the pages must
	// stay resident until this submitted graphics fence retires.
	result = markDrawsSubmitted( *submission.pPlan, complete.nValue );
	if ( result != SIR_GRAPHICS_SUCCESS ) {
		return result;
	}
	*pCompletion = complete;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::markDrawsSubmitted( const sirVGFramePlan_t& plan, uint64_t nGraphicsFence ) noexcept
{
	if ( !validDevice() || nGraphicsFence == 0 || sirVGValidateFramePlan( plan ) != SIR_VG_PLAN_OK ||
	     !m_Config.pDirectory->isCurrentSnapshot( plan.nPageSnapshotGeneration ) )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	for ( uint32_t i = 0; i < plan.nGroupCount; ++i ) {
		if ( plan.pGroups[i].nPageId == 0 || plan.pGroups[i].nPageId > UINT32_MAX ||
		     m_Config.pDirectory->markSubmitted( ( uint32_t )( plan.pGroups[i].nPageId - 1 ), nGraphicsFence ) != SIR_VG_RESIDENCY_OK )
			return SIR_GRAPHICS_BUSY;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVGNativePages::shutdown( void ) noexcept
{
	if ( m_bUploadPending )
		return SIR_GRAPHICS_BUSY;
	m_Config = {};
	m_UploadCompletion = {};
	m_bBackingInVertexState = false;
	m_pStreaming = nullptr;
	m_StreamingTicket = {};
	m_bStreamingTicket = false;
	m_bBVHPending = false;
	m_bBVHResident = false;
	m_nBVHOffset = 0;
	m_nBVHBytes = 0;
	return SIR_GRAPHICS_SUCCESS;
}
