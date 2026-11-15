/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRPACKASYNCRESOURCELOAD_H__
#define __SIRENGINE_SIRPACKASYNCRESOURCELOAD_H__

#include <Core/IO/SIRAsyncIOService.h>
#include <Resources/SIRPackCatalog.h>
#include <Resources/SIRResourceGraph.h>

// One exact cooked resource and revision. The owning streaming coordinator
// may share this transaction among independent consumer tickets. No duplicate
// decoded payload or second resource registry is created.
//
// All described scratch, chunk and dependency arrays remain caller-owned and
// stable. One physical read at a time permits a single fixed ingress buffer.
// The caller must feed the appropriate terminal IO completion on the graph
// writer thread and drain cancellation before destroying this object.
typedef struct sirPackAsyncWorkspace_s
{
	SIRResourceChunkPublication *pChunks;
	uint32_t nChunkCapacity;
	SIRResourceDependency *pDependencies;
	uint32_t nDependencyCapacity;
	uint8_t *pIngress;
	uint64_t nIngressCapacity;
	void *pCodecScratch;
	uint64_t nCodecScratchBytes;
} sirPackAsyncWorkspace_t;

class SIRPackAsyncResourceLoad final
{
private:
	SIRResourceGraph *m_pGraph;
	SIRAsyncIOService *m_pIO;
	const SIRPackCatalog *m_pCatalog;
	const sirIOSourceHandle_t *m_pSources;
	uint32_t m_nSourceCount;
	sirPackAsyncWorkspace_t m_Workspace;
	sirPackResource_t m_Resource;
	sirPackChunk_t m_Chunk;
	uint64_t m_nCatalogGeneration;
	uint64_t m_nExpectedRevision;
	uint32_t m_nNextChunk;
	uint32_t m_nSubscriberCount;
	sirIORequestHandle_t m_hRequest;
	sirIOBufferHandle_t m_hBuffer;
	SIRResourceStagingHandle m_hStaging;
	sirResourceStagingLease_t m_hLease;
	SIRResourceMutableBytes m_Span;
	bool m_bActive;
	bool m_bPending;
	bool m_bTransferPinned;
	bool m_bCancelled;

	sirResourceResult_t beginChunk( void ) noexcept;
	void cleanup( void ) noexcept;
	sirResourceResult_t publishResource( SIRResourceHandle *pResource ) noexcept;

public:
	SIRPackAsyncResourceLoad( void ) noexcept;
	~SIRPackAsyncResourceLoad( void ) noexcept;
	SIRPackAsyncResourceLoad( const SIRPackAsyncResourceLoad& ) = delete;
	SIRPackAsyncResourceLoad& operator=( const SIRPackAsyncResourceLoad& ) = delete;

	// Caller must retain catalog/shard mounts through the entire operation.
	sirResourceResult_t begin( SIRResourceGraph& graph, SIRAsyncIOService& io, const SIRPackCatalog& catalog,
	                           const sirIOSourceHandle_t *pSources, uint32_t nSourceCount, const SIRResourceKey& key,
	                           uint64_t nExpectedRevision, const sirPackAsyncWorkspace_t& workspace ) noexcept;
	// Fast, read-only resident reuse: verifies every immutable decoded chunk
	// against the cooked catalog before accepting a canonical GRDG backing.
	static sirResourceResult_t findReusable( const SIRResourceGraph& graph, const SIRPackCatalog& catalog, const SIRResourceKey& key,
	                                         uint64_t nExpectedRevision, SIRResourceHandle *pResource ) noexcept;
	bool accepts( const SIRResourceKey& key, uint64_t nExpectedRevision ) const noexcept;
	bool retainSubscriber( const SIRResourceKey& key, uint64_t nExpectedRevision ) noexcept;
	sirResourceResult_t withdrawSubscriber( void ) noexcept;
	bool ownsCompletion( const sirIOCompletion_t& completion ) const noexcept;
	// Returns BUSY when a subsequent chunk has been queued. On terminal failure
	// the transaction discards staging; on success publishes only complete data.
	sirResourceResult_t complete( const sirIOCompletion_t& completion, SIRResourceHandle *pResource ) noexcept;
	bool isActive( void ) const noexcept;
};

#endif // __SIRENGINE_SIRPACKASYNCRESOURCELOAD_H__
