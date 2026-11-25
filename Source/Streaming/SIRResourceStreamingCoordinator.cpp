#include <Streaming/SIRResourceStreamingCoordinator.h>

#include <string.h>

enum class SIRMaterializationState : uint8_t { EMPTY = 0, READING, PUBLISHED, FAILED };

static sirIOResult_t sirReadPackLease( void *pSource, uint64_t nOffset, sirIOBytes_t destination, uint64_t *pTransferred ) noexcept
{
	SIRPackSourceLease *pLease = static_cast<SIRPackSourceLease *>( pSource );
	if ( pLease == nullptr || !pLease->hasChunk() ) return SIR_IO_INVALID_ARGUMENT;
	const sirPackChunkSource_t& source = pLease->getSource();
	const SIRPackResult result = pLease->getPack()->readAt( source.chunk.nShardIndex, nOffset, { destination.pData, destination.nSize }, pTransferred );
	return result == SIRPackResult::SIR_PACK_OK ? SIR_IO_OK : ( result == SIRPackResult::SIR_PACK_TRUNCATED ? SIR_IO_TRUNCATED : SIR_IO_FAILED );
}

struct sirResourceStreamingOperation_s
{
	sirStreamingTicket_t ticket;
	sirStreamingDemand_t demand;
	SIRPackSourceLease source;
	sirIOHandle_t io;
	sirIOBufferLease_t encoded;
	SIRResourceStagingHandle staging;
	SIRResourceMutableBytes stagingBytes;
	SIRMaterializationState state;
	bool hasEncoded;
	bool hasStaging;
	uint32_t nPlanIndex;

	sirResourceStreamingOperation_s( void ) noexcept : ticket{}, demand{}, source{}, io{}, encoded{}, staging{}, stagingBytes{}, state( SIRMaterializationState::EMPTY ), hasEncoded( false ), hasStaging( false ), nPlanIndex( 0 ) {}
	void clear( void ) noexcept
	{
		source.reset();
		ticket = {};
		demand = {};
		io = {};
		encoded = {};
		staging = {};
		stagingBytes = {};
		state = SIRMaterializationState::EMPTY;
		hasEncoded = false;
		hasStaging = false;
		nPlanIndex = 0;
	}
};

static uint64_t readLe64( const uint8_t *pBytes ) noexcept
{
	uint64_t value = 0;
	for ( uint32_t i = 0; i < 8; ++i ) value |= static_cast<uint64_t>( pBytes[i] ) << ( i * 8 );
	return value;
}

static SIRResourceContentKey contentKey( const uint8_t *pDigest, uint32_t nFormat ) noexcept
{
	return { readLe64( pDigest ), readLe64( pDigest + 8 ), nFormat, 0 };
}

SIRResourceStreamingCoordinator::SIRResourceStreamingCoordinator( void ) noexcept
    : m_pOperations( nullptr ), m_nCapacity( 0 ), m_pConfig( nullptr ), m_bInitialized( false ), m_bStopping( false )
{
}

SIRResourceStreamingCoordinator::~SIRResourceStreamingCoordinator( void ) noexcept { shutdown(); }

sirStreamingResult_t SIRResourceStreamingCoordinator::initialize( const sirResourceStreamingConfig_t& config ) noexcept
{
	if ( m_bInitialized || config.pGraph == nullptr || config.pMount == nullptr || config.pResolver == nullptr || config.pIo == nullptr ||
	     config.pCodecs == nullptr || config.pCoordinator == nullptr || config.pOperations == nullptr || config.nOperationCapacity == 0 ||
	     config.pPlanChunks == nullptr || config.nPlanChunkCapacity == 0 || config.pPublicationChunks == nullptr ||
	     config.nPublicationChunkCapacity == 0 || config.pPublicationDependencies == nullptr || config.nPublicationDependencyCapacity == 0 ) {
		return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	}
	m_pConfig = const_cast<sirResourceStreamingConfig_t *>( &config );
	m_pOperations = config.pOperations;
	m_nCapacity = config.nOperationCapacity;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) m_pOperations[i].clear();
	m_bStopping = false;
	m_bInitialized = true;
	return SIRStreamingResult::SIR_STREAMING_OK;
}

sirStreamingResult_t SIRResourceStreamingCoordinator::prepare( sirStreamingTicket_t ticket, const sirStreamingDemand_t& demand ) noexcept
{
	if ( !m_bInitialized || m_bStopping ) return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) if ( m_pOperations[i].state != SIRMaterializationState::EMPTY && m_pOperations[i].ticket.nSlot == ticket.nSlot && m_pOperations[i].ticket.nGeneration == ticket.nGeneration ) return SIRStreamingResult::SIR_STREAMING_DUPLICATE;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) {
		if ( m_pOperations[i].state != SIRMaterializationState::EMPTY ) continue;
		m_pOperations[i].ticket = ticket;
		m_pOperations[i].demand = demand;
		m_pOperations[i].state = SIRMaterializationState::READING;
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	return SIRStreamingResult::SIR_STREAMING_OUT_OF_CAPACITY;
}

void SIRResourceStreamingCoordinator::fail( sirResourceStreamingOperation_t& operation ) noexcept
{
	if ( operation.hasEncoded && m_pConfig->pBufferPool != nullptr ) m_pConfig->pBufferPool->release( operation.encoded.handle );
	if ( operation.hasStaging ) {
		m_pConfig->pGraph->getWriter()->cancelStaging( operation.staging );
		operation.hasStaging = false;
	}
	operation.state = SIRMaterializationState::FAILED;
}

sirStreamingResult_t SIRResourceStreamingCoordinator::materialize( sirResourceStreamingOperation_t& operation, const sirStreamingBudget_t& budget ) noexcept
{
	if ( operation.demand.nDeadlineTick != 0 && operation.demand.nDeadlineTick < budget.nTick ) {
		fail( operation );
		return SIRStreamingResult::SIR_STREAMING_STALE_TICKET;
	}
	if ( operation.io.nGeneration == 0 ) {
		sirStreamingUnitPlan_t plan { m_pConfig->pPlanChunks, m_pConfig->nPlanChunkCapacity, 0 };
		if ( m_pConfig->pResolver->resolve( operation.demand.keyUnit, &plan ) != SIRStreamingResult::SIR_STREAMING_OK || plan.nRequired == 0 ) return SIRStreamingResult::SIR_STREAMING_NOT_READY;
		if ( operation.nPlanIndex >= plan.nRequired ) return SIRStreamingResult::SIR_STREAMING_NOT_READY;
		const sirStreamingChunkDemand_t& selected = plan.pChunks[operation.nPlanIndex];
		if ( m_pConfig->pMount->resolveChunk( selected.lookup, selected.nChunkId, &operation.source ) != SIR_PACK_RUNTIME_OK ) {
			fail( operation );
			return SIRStreamingResult::SIR_STREAMING_NOT_READY;
		}
		const sirPackChunkSource_t& source = operation.source.getSource();
		SIRResourceGraphWriter *pWriter = m_pConfig->pGraph->getWriter();
		if ( pWriter->allocateStaging( source.chunk.nDecodedBytes, source.chunk.nAlignment, m_pConfig->nResidentDomain, &operation.staging, &operation.stagingBytes ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) return SIRStreamingResult::SIR_STREAMING_OUT_OF_CAPACITY;
		operation.hasStaging = true;
		SIRResourceMutableBytes destination = operation.stagingBytes;
		if ( source.chunk.eCodec != SIR_PACK_CODEC_NONE ) {
			if ( m_pConfig->pBufferPool == nullptr || m_pConfig->pBufferPool->acquire( source.chunk.nEncodedBytes, 16, &operation.encoded ) != SIR_IO_OK ) return SIRStreamingResult::SIR_STREAMING_OUT_OF_CAPACITY;
			operation.hasEncoded = true;
			destination = { operation.encoded.bytes.pData, operation.encoded.bytes.nSize };
		}
		const sirIORequest_t request { &operation.source, &sirReadPackLease, { destination.m_pData, destination.m_nSize }, source.chunk.nOffset, source.chunk.nEncodedBytes, operation.demand.nDeadlineTick, operation.demand.nPriority };
		if ( m_pConfig->pIo->submit( request, &operation.io ) != SIR_IO_OK ) {
			fail( operation );
			return SIRStreamingResult::SIR_STREAMING_OUT_OF_CAPACITY;
		}
		return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	}
	sirIOCompletion_t completion[4] = {};
	const uint32_t nCompletions = m_pConfig->pIo->collect( completion, 4 );
	for ( uint32_t i = 0; i < nCompletions; ++i ) {
		if ( completion[i].handle.nSlot != operation.io.nSlot || completion[i].handle.nGeneration != operation.io.nGeneration ) continue;
		if ( completion[i].eResult != SIR_IO_OK || completion[i].nTransferred != operation.source.getSource().chunk.nEncodedBytes ) {
			fail( operation );
			return SIRStreamingResult::SIR_STREAMING_BUSY;
		}
		const sirPackChunkSource_t& source = operation.source.getSource();
		if ( source.chunk.eCodec != SIR_PACK_CODEC_NONE ) {
			sirCompressionDesc_t decode { { operation.encoded.bytes.pData, operation.encoded.bytes.nSize }, operation.stagingBytes, {}, source.chunk.nCodecVersion, source.chunk.nDictionaryId };
			if ( m_pConfig->pCodecs->decompress( source.chunk.eCodec, decode ) != SIRPackResult::SIR_PACK_OK ) {
				fail( operation );
				return SIRStreamingResult::SIR_STREAMING_BUSY;
			}
			m_pConfig->pBufferPool->release( operation.encoded.handle );
			operation.hasEncoded = false;
		}
		uint8_t digest[32] = {};
		sirPackSha256( operation.stagingBytes.m_pData, operation.stagingBytes.m_nSize, digest );
		if ( memcmp( digest, source.chunk.szDigest, 32 ) != 0 ) {
			fail( operation );
			return SIRStreamingResult::SIR_STREAMING_BUSY;
		}
		SIRResourceVerifiedChunk *pChunks = m_pConfig->pPublicationChunks;
		uint32_t nChunks = 0;
		uint64_t nExpectedRevision = 0;
		SIRResourceReadScope scope( *m_pConfig->pGraph );
		SIRResourceHandle currentHandle {};
		SIRResourceView currentView;
		if ( m_pConfig->pGraph->find( scope, source.key, &currentHandle ) == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS &&
		     m_pConfig->pGraph->resolve( scope, currentHandle, &currentView ) == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			nExpectedRevision = currentView.getRevision();
			sirResourceSourceProvenance_t provenance {};
			if ( currentView.getSourceProvenance( &provenance ) == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS &&
				 provenance.nSourceId == source.source.nPackageId && provenance.nSourceGeneration == source.source.nPackageGeneration &&
				 provenance.nCookedRevision == source.nCookedRevision ) {
				SIRResourceInfo info {};
				currentView.getInfo( &info );
				for ( uint32_t i = 0; i < info.m_nChunkCount && nChunks < m_pConfig->nPublicationChunkCapacity; ++i ) {
					SIRResourceChunkInfo chunkInfo {};
					uint8_t oldDigest[32] = {};
					if ( currentView.getChunkInfo( i, &chunkInfo ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS || currentView.getChunkDigest( chunkInfo.m_Id, oldDigest ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) break;
					pChunks[nChunks++] = { chunkInfo.m_Id, chunkInfo.m_ContentKey, {}, chunkInfo.m_nBytes, chunkInfo.m_nAlignment, {}, SIR_RESOURCE_VERIFIED_CHUNK_ATTACH_CURRENT };
					memcpy( pChunks[nChunks - 1].szDigest, oldDigest, 32 );
				}
			}
		}
		if ( nChunks >= m_pConfig->nPublicationChunkCapacity ) {
			fail( operation );
			return SIRStreamingResult::SIR_STREAMING_OUT_OF_CAPACITY;
		}
		pChunks[nChunks++] = { { source.chunk.nChunkId }, contentKey( digest, source.nFormatId ), operation.staging, source.chunk.nDecodedBytes, source.chunk.nAlignment, {}, SIR_RESOURCE_VERIFIED_CHUNK_STAGING };
		memcpy( pChunks[nChunks - 1].szDigest, digest, 32 );
		SIRResourceVerifiedPublication publication {};
		publication.key = source.key;
		publication.provenance = { source.source.nPackageId, source.source.nPackageGeneration, source.source.nSourceRevision, source.nCookedRevision,
		                          source.source.mount.nSlot, source.source.mount.nGeneration };
		publication.eKind = static_cast<SIRResourceKind>( source.nKind );
		publication.nSchemaVersion = source.nSchemaVersion;
		publication.nFormatId = source.nFormatId;
		publication.pChunks = pChunks;
		publication.nChunkCount = nChunks;
		const SIRResourceResult publishResult = m_pConfig->pGraph->getWriter()->publishVerifiedExpected( publication, nExpectedRevision, &currentHandle );
		if ( publishResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			fail( operation );
			return SIRStreamingResult::SIR_STREAMING_BUSY;
		}
		operation.hasStaging = false;
		sirStreamingUnitPlan_t plan { m_pConfig->pPlanChunks, m_pConfig->nPlanChunkCapacity, 0 };
		if ( m_pConfig->pResolver->resolve( operation.demand.keyUnit, &plan ) != SIRStreamingResult::SIR_STREAMING_OK ) {
			fail( operation );
			return SIRStreamingResult::SIR_STREAMING_NOT_READY;
		}
		++operation.nPlanIndex;
		operation.source.reset();
		operation.io = {};
		operation.state = operation.nPlanIndex < plan.nRequired ? SIRMaterializationState::READING : SIRMaterializationState::PUBLISHED;
		if ( operation.state == SIRMaterializationState::PUBLISHED ) m_pConfig->pCoordinator->advance( operation.ticket, SIRStreamingStage::SIR_STREAMING_STAGE_PREPARED );
		return SIRStreamingResult::SIR_STREAMING_OK;
	}
	return SIRStreamingResult::SIR_STREAMING_NOT_READY;
}

sirStreamingResult_t SIRResourceStreamingCoordinator::update( const sirStreamingBudget_t& budget ) noexcept
{
	if ( !m_bInitialized ) return SIRStreamingResult::SIR_STREAMING_NOT_READY;
	for ( uint32_t i = 0; i < m_nCapacity && budget.nPublicationOperations != 0; ++i ) {
		if ( m_pOperations[i].state == SIRMaterializationState::READING ) materialize( m_pOperations[i], budget );
	}
	return SIRStreamingResult::SIR_STREAMING_OK;
}

sirStreamingResult_t SIRResourceStreamingCoordinator::activate( sirStreamingTicket_t ticket ) noexcept { return m_pConfig->pCoordinator->advance( ticket, SIRStreamingStage::SIR_STREAMING_STAGE_ACTIVE ); }
sirStreamingResult_t SIRResourceStreamingCoordinator::retire( sirStreamingTicket_t ticket ) noexcept { return m_pConfig->pCoordinator->advance( ticket, SIRStreamingStage::SIR_STREAMING_STAGE_RETIRING ); }
sirStreamingResult_t SIRResourceStreamingCoordinator::cancel( sirStreamingTicket_t ticket ) noexcept { return m_pConfig->pCoordinator->cancel( ticket ); }

sirStreamingResult_t SIRResourceStreamingCoordinator::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) return SIRStreamingResult::SIR_STREAMING_OK;
	m_bStopping = true;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) if ( m_pOperations[i].state == SIRMaterializationState::READING ) return SIRStreamingResult::SIR_STREAMING_BUSY;
	m_pOperations = nullptr;
	m_pConfig = nullptr;
	m_nCapacity = 0;
	m_bInitialized = false;
	return SIRStreamingResult::SIR_STREAMING_OK;
}
