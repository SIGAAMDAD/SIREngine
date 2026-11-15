/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Resources/SIRPackAsyncResourceLoad.h>
#include <Resources/SIRPackCodec.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

SIRPackAsyncResourceLoad::SIRPackAsyncResourceLoad( void ) noexcept
    : m_pGraph( nullptr ), m_pIO( nullptr ), m_pCatalog( nullptr ), m_pSources( nullptr ), m_nSourceCount( 0 ), m_Workspace{}, m_Resource{},
      m_Chunk{}, m_nCatalogGeneration( 0 ), m_nExpectedRevision( 0 ), m_nNextChunk( 0 ), m_nSubscriberCount( 0 ), m_hRequest{}, m_hBuffer{},
      m_hStaging{}, m_hLease{}, m_Span{}, m_bActive( false ), m_bPending( false ), m_bTransferPinned( false ), m_bCancelled( false )
{
}

SIRPackAsyncResourceLoad::~SIRPackAsyncResourceLoad( void ) noexcept
{
	// Returning an in-flight native destination would be a use-after-free.
	if ( m_bPending ) {
		abort();
	}
	cleanup();
}

bool SIRPackAsyncResourceLoad::isActive( void ) const noexcept
{
	return m_bActive;
}

sirResourceResult_t SIRPackAsyncResourceLoad::findReusable( const SIRResourceGraph& graph, const SIRPackCatalog& catalog,
                                                            const SIRResourceKey& key, uint64_t nExpectedRevision,
                                                            SIRResourceHandle *pResource ) noexcept
{
	if ( pResource == nullptr || !catalog.isOpen() || nExpectedRevision == UINT64_MAX ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirPackResource_t resource = {};
	if ( catalog.findResource( key, &resource ) != SIR_PACK_OK ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	SIRResourceReadScope scope( graph );
	SIRResourceHandle hResource = {};
	SIRResourceView view;
	if ( !scope.isActive() || graph.find( scope, key, &hResource ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     graph.resolve( scope, hResource, &view ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_RESOURCE_RESULT_NOT_RESIDENT;
	}
	SIRResourceInfo info = {};
	if ( view.getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS || info.m_nRevision != nExpectedRevision + 1 ||
	     info.m_nFormatId != resource.nFormatId || info.m_nSchemaVersion != resource.nSchemaVersion ||
	     info.m_nChunkCount != resource.nChunkCount ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	for ( uint32_t iChunk = 0; iChunk < resource.nChunkCount; ++iChunk ) {
		sirPackChunk_t chunk = {};
		if ( catalog.chunkAt( resource.nFirstChunk + iChunk, &chunk ) != SIR_PACK_OK ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		SIRResourceConstBytes bytes = {};
		if ( view.getChunk( { chunk.nChunkId }, &bytes ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     catalog.verifyDecodedChunk( resource.nFirstChunk + iChunk, bytes.m_pData, bytes.m_nSize ) != SIR_PACK_OK ) {
			return SIR_RESOURCE_RESULT_NOT_RESIDENT;
		}
	}
	*pResource = hResource;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

bool SIRPackAsyncResourceLoad::accepts( const SIRResourceKey& key, uint64_t nExpectedRevision ) const noexcept
{
	return m_bActive && !m_bCancelled && m_nExpectedRevision == nExpectedRevision && key.m_nAssetId == m_Resource.key.m_nAssetId &&
	       key.m_nTypeId == m_Resource.key.m_nTypeId && key.m_nVariantId == m_Resource.key.m_nVariantId;
}

bool SIRPackAsyncResourceLoad::retainSubscriber( const SIRResourceKey& key, uint64_t nExpectedRevision ) noexcept
{
	if ( !accepts( key, nExpectedRevision ) || m_nSubscriberCount == UINT32_MAX ) {
		return false;
	}
	++m_nSubscriberCount;
	return true;
}

sirResourceResult_t SIRPackAsyncResourceLoad::withdrawSubscriber( void ) noexcept
{
	if ( !m_bActive || m_nSubscriberCount == 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	--m_nSubscriberCount;
	if ( m_nSubscriberCount != 0 ) {
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	m_bCancelled = true;
	if ( m_bPending ) {
		return m_pIO->cancel( m_hRequest ) == SIR_IO_OK ? SIR_RESOURCE_RESULT_BUSY : SIR_RESOURCE_RESULT_FAILED;
	}
	cleanup();
	return SIR_RESOURCE_RESULT_SUCCESS;
}

sirResourceResult_t SIRPackAsyncResourceLoad::begin( SIRResourceGraph& graph, SIRAsyncIOService& io, const SIRPackCatalog& catalog,
                                                     const sirIOSourceHandle_t *pSources, uint32_t nSourceCount, const SIRResourceKey& key,
                                                     uint64_t nExpectedRevision, const sirPackAsyncWorkspace_t& workspace ) noexcept
{
	if ( m_bActive || graph.getWriter() == nullptr || !catalog.isOpen() || pSources == nullptr || nSourceCount < catalog.getShardCount() ||
	     workspace.pChunks == nullptr || workspace.nChunkCapacity == 0 ||
	     ( workspace.nDependencyCapacity > 0 && workspace.pDependencies == nullptr ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirPackResource_t resource = {};
	if ( catalog.findResource( key, &resource ) != SIR_PACK_OK ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( resource.nChunkCount > workspace.nChunkCapacity || resource.nDependencyCount > workspace.nDependencyCapacity ||
	     ( resource.nDependencyCount != 0 && workspace.pDependencies == nullptr ) ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}

	m_pGraph = &graph;
	m_pIO = &io;
	m_pCatalog = &catalog;
	m_pSources = pSources;
	m_nSourceCount = nSourceCount;
	m_Workspace = workspace;
	m_Resource = resource;
	m_nCatalogGeneration = catalog.getGeneration();
	m_nExpectedRevision = nExpectedRevision;
	m_nNextChunk = 0;
	m_nSubscriberCount = 1;
	m_bActive = true;
	m_bCancelled = false;
	for ( uint32_t iDependency = 0; iDependency < resource.nDependencyCount; ++iDependency ) {
		sirPackDependency_t edge = {};
		if ( catalog.dependencyAt( resource.nFirstDependency + iDependency, &edge ) != SIR_PACK_OK ) {
			cleanup();
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		m_Workspace.pDependencies[iDependency] = { edge.key, edge.eKind, {} };
	}
	return beginChunk();
}

sirResourceResult_t SIRPackAsyncResourceLoad::beginChunk( void ) noexcept
{
	if ( m_pCatalog == nullptr || !m_pCatalog->isOpen() || m_pCatalog->getGeneration() != m_nCatalogGeneration ) {
		cleanup();
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	if ( m_nNextChunk >= m_Resource.nChunkCount || m_pCatalog->chunkAt( m_Resource.nFirstChunk + m_nNextChunk, &m_Chunk ) != SIR_PACK_OK ||
	     m_Chunk.nShardIndex >= m_nSourceCount ) {
		cleanup();
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}

	const uint32_t nAlignment = m_Chunk.nAlignment;
	SIRResourceGraphWriter *pWriter = m_pGraph->getWriter();
	SIRResourceMutableBytes staging = {};
	const sirResourceResult_t eStaging =
	    pWriter->allocateStaging( m_Chunk.nDecodedBytes, nAlignment, SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT, &m_hStaging, &staging );
	if ( eStaging != SIR_RESOURCE_RESULT_SUCCESS ) {
		cleanup();
		return eStaging;
	}
	const sirResourceResult_t eTransfer = pWriter->beginTransfer( m_hStaging, 0, m_Chunk.nDecodedBytes, &m_hLease, &m_Span );
	if ( eTransfer != SIR_RESOURCE_RESULT_SUCCESS ) {
		cleanup();
		return eTransfer;
	}
	m_bTransferPinned = true;

	uint8_t *pDestination = m_Span.m_pData;
	uint64_t nDestination = m_Span.m_nSize;
	if ( m_Chunk.eCodec != SIR_PACK_CODEC_NONE ) {
		if ( m_Workspace.pIngress == nullptr || m_Chunk.nEncodedBytes > m_Workspace.nIngressCapacity ||
		     sirPackDecodeScratchBytes( m_Chunk.eCodec ) > m_Workspace.nCodecScratchBytes ) {
			cleanup();
			return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
		}
		pDestination = m_Workspace.pIngress;
		nDestination = m_Workspace.nIngressCapacity;
	}
	if ( m_pIO->registerBuffer( pDestination, nDestination, &m_hBuffer ) != SIR_IO_OK ) {
		cleanup();
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}

	const sirIOReadRequest_t request = { m_pSources[m_Chunk.nShardIndex], m_hBuffer, m_Chunk.nOffset,      0,
	                                     m_Chunk.nEncodedBytes,           0,         m_nCatalogGeneration, 128 };
	const sirIOResult_t eSubmitted = m_pIO->submit( request, &m_hRequest );
	if ( eSubmitted != SIR_IO_OK ) {
		m_pIO->unregisterBuffer( m_hBuffer );
		m_hBuffer = {};
		cleanup();
		return eSubmitted == SIR_IO_OUT_OF_CAPACITY ? SIR_RESOURCE_RESULT_OUT_OF_BUDGET : SIR_RESOURCE_RESULT_FAILED;
	}
	m_bPending = true;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

bool SIRPackAsyncResourceLoad::ownsCompletion( const sirIOCompletion_t& completion ) const noexcept
{
	return m_bPending && m_hRequest.nSlot == completion.hRequest.nSlot && m_hRequest.nGeneration == completion.hRequest.nGeneration;
}

sirResourceResult_t SIRPackAsyncResourceLoad::complete( const sirIOCompletion_t& completion, SIRResourceHandle *pResource ) noexcept
{
	if ( !ownsCompletion( completion ) || m_pGraph->getWriter() == nullptr || pResource == nullptr ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( m_pIO->acknowledge( m_hRequest ) != SIR_IO_OK ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	m_bPending = false;
	if ( m_pIO->unregisterBuffer( m_hBuffer ) != SIR_IO_OK ) {
		abort();
	}
	m_hBuffer = {};
	if ( m_bCancelled || completion.eResult != SIR_IO_OK || completion.nTransferred != m_Chunk.nEncodedBytes ||
	     completion.nClientToken != m_nCatalogGeneration || m_pCatalog->getGeneration() != m_nCatalogGeneration ) {
		cleanup();
		return SIR_RESOURCE_RESULT_FAILED;
	}

	const void *pEncoded = m_Chunk.eCodec == SIR_PACK_CODEC_NONE ? m_Span.m_pData : m_Workspace.pIngress;
	const sirPackResult_t eDecoded = sirPackDecode( m_Chunk, pEncoded, m_Chunk.nEncodedBytes, m_Span.m_pData, m_Span.m_nSize,
	                                                m_Workspace.pCodecScratch, m_Workspace.nCodecScratchBytes );
	if ( eDecoded != SIR_PACK_OK ) {
		cleanup();
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	const sirPackResult_t eVerified =
	    m_pCatalog->verifyDecodedChunk( m_Resource.nFirstChunk + m_nNextChunk, m_Span.m_pData, m_Chunk.nDecodedBytes );
	if ( eVerified != SIR_PACK_OK ) {
		cleanup();
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	if ( m_pGraph->getWriter()->completeTransfer( m_hLease ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		abort();
	}
	m_bTransferPinned = false;

	SIRResourceContentKey keyContent = { sirPackRead64( m_Chunk.szDigest ), sirPackRead64( m_Chunk.szDigest + 8 ), m_Resource.nFormatId,
	                                     0 };
	m_Workspace.pChunks[m_nNextChunk] = { { m_Chunk.nChunkId }, keyContent, m_hStaging, false };
	m_hStaging = {};
	++m_nNextChunk;
	if ( m_nNextChunk < m_Resource.nChunkCount ) {
		const sirResourceResult_t eNext = beginChunk();
		return eNext == SIR_RESOURCE_RESULT_SUCCESS ? SIR_RESOURCE_RESULT_BUSY : eNext;
	}
	return publishResource( pResource );
}

sirResourceResult_t SIRPackAsyncResourceLoad::publishResource( SIRResourceHandle *pResource ) noexcept
{
	if ( m_pCatalog->getGeneration() != m_nCatalogGeneration || m_bCancelled ) {
		cleanup();
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	const SIRResourcePublication publication = {
	    m_Resource.key,         ( SIRResourceKind )m_Resource.nKind, m_Resource.nSchemaVersion,  m_Resource.nFormatId, m_Workspace.pChunks,
	    m_Resource.nChunkCount, m_Workspace.pDependencies,           m_Resource.nDependencyCount };
	const SIRResourceResult eResult = m_pGraph->getWriter()->publishExpected( publication, m_nExpectedRevision, pResource );
	if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
		m_nNextChunk = 0;
		m_bActive = false;
		m_nSubscriberCount = 0;
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	cleanup();
	return eResult;
}

void SIRPackAsyncResourceLoad::cleanup( void ) noexcept
{
	if ( !m_bActive || m_bPending ) {
		return;
	}
	SIRResourceGraphWriter *pWriter = m_pGraph != nullptr ? m_pGraph->getWriter() : nullptr;
	if ( pWriter == nullptr ) {
		abort();
	}
	if ( m_bTransferPinned ) {
		if ( pWriter->completeTransfer( m_hLease ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			abort();
		}
		m_bTransferPinned = false;
	}
	if ( m_hStaging.m_nGeneration != 0 ) {
		if ( pWriter->cancelStaging( m_hStaging ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			abort();
		}
		m_hStaging = {};
	}
	for ( uint32_t iChunk = 0; iChunk < m_nNextChunk; ++iChunk ) {
		if ( pWriter->cancelStaging( m_Workspace.pChunks[iChunk].m_Staging ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			abort();
		}
	}
	m_nNextChunk = 0;
	m_nSubscriberCount = 0;
	m_bActive = false;
}
