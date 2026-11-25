#include <Streaming/Pack/SIRPack.h>
#include <Streaming/Pack/SIRPackShard.h>

SIRPack::SIRPack( void ) noexcept
    : m_pShards( nullptr ),
      m_nShardCount( 0 ),
      m_nPackageId( 0 ),
      m_nGeneration( 0 ),
      m_bOpen( false )
{
}

SIRPack::~SIRPack( void ) noexcept
{
	close();
}

bool SIRPack::isOpen( void ) const noexcept
{
	return m_bOpen;
}

uint64_t SIRPack::getPackageId( void ) const noexcept
{
	return m_nPackageId;
}

uint64_t SIRPack::getGeneration( void ) const noexcept
{
	return m_nGeneration;
}

const SIRPackCatalog& SIRPack::getCatalog( void ) const noexcept
{
	return m_Catalog;
}

SIRPackResult SIRPack::open( const sirPackOpenDesc_t& desc ) noexcept
{
	if ( m_bOpen ) {
		return SIRPackResult::SIR_PACK_BUSY;
	}
	if ( desc.pRoot == nullptr || desc.pszCatalogPath == nullptr || desc.pShardStorage == nullptr || desc.pCatalogStorage == nullptr ||
	     desc.nShardCount == 0 || desc.nShardCount > desc.nShardCapacity || desc.ppszShardPaths == nullptr ) {
		return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	}
	if ( desc.pRoot->openRead( desc.pszCatalogPath, &m_CatalogFile ) != sirFileResult::SIR_FILE_OK ) {
		return SIRPackResult::SIR_PACK_NOT_FOUND;
	}
	uint64_t nCatalogBytes = 0;
	if ( m_CatalogFile.getSize( &nCatalogBytes ) != sirFileResult::SIR_FILE_OK || nCatalogBytes > desc.nCatalogStorageBytes ) {
		m_CatalogFile.close();
		return SIRPackResult::SIR_PACK_OUT_OF_CAPACITY;
	}
	uint64_t nRead = 0;
	if ( m_CatalogFile.readAt( 0, desc.pCatalogStorage, nCatalogBytes, &nRead ) != sirFileResult::SIR_FILE_OK || nRead != nCatalogBytes ) {
		m_CatalogFile.close();
		return SIRPackResult::SIR_PACK_TRUNCATED;
	}
	SIRPackResult result = m_Catalog.open( desc.pCatalogStorage, nCatalogBytes, desc.pValidationScratch, desc.nValidationScratchBytes );
	if ( result != SIRPackResult::SIR_PACK_OK ) {
		m_CatalogFile.close();
		m_Catalog.reset();
		return result;
	}
	if ( m_Catalog.getShardCount() != desc.nShardCount ) {
		m_CatalogFile.close();
		m_Catalog.reset();
		return SIRPackResult::SIR_PACK_INVALID_FORMAT;
	}
	m_pShards = desc.pShardStorage;
	m_nShardCount = desc.nShardCount;
	m_nPackageId = m_Catalog.getPackageId();
	m_nGeneration = m_Catalog.getGeneration();
	for ( uint32_t iShard = 0; iShard < m_nShardCount; ++iShard ) {
		sirPackShard_t expected{};
		m_Catalog.shardAt( iShard, &expected );
		sirPackShardOpenDesc_t shardDesc{};
		shardDesc.pRoot = desc.pRoot;
		shardDesc.pszPath = desc.ppszShardPaths[iShard];
		shardDesc.expected = expected;
		shardDesc.nPackageId = m_nPackageId;
		shardDesc.nGeneration = m_nGeneration;
		if ( m_pShards[iShard].open( shardDesc ) != SIRPackResult::SIR_PACK_OK ) {
			close();
			return SIRPackResult::SIR_PACK_IO_ERROR;
		}
	}
	m_bOpen = true;
	return SIRPackResult::SIR_PACK_OK;
}

SIRPackResult SIRPack::close( void ) noexcept
{
	if ( m_pShards != nullptr ) {
		for ( uint32_t i = 0; i < m_nShardCount; ++i ) {
			m_pShards[i].close();
		}
	}

	m_pShards = nullptr;
	m_nShardCount = 0;
	m_Catalog.reset();
	m_CatalogFile.close();
	m_nPackageId = 0;
	m_nGeneration = 0;
	m_bOpen = false;

	return SIRPackResult::SIR_PACK_OK;
}

SIRPackResult SIRPack::findResource( const SIRResourceKey& key, sirPackResource_t *pResource ) const noexcept
{
	return m_bOpen ? m_Catalog.findResource( key, pResource ) : SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
}

SIRPackResult SIRPack::resolveChunk( const SIRResourceKey& key, uint32_t nChunkId, sirPackChunkSource_t *pSource ) const noexcept
{
	if ( !m_bOpen || pSource == nullptr ) {
		return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	}

	sirPackResource_t resource{};
	SIRPackResult result = m_Catalog.findResource( key, &resource );
	if ( result != SIRPackResult::SIR_PACK_OK ) {
		return result;
	}

	for ( uint32_t i = 0; i < resource.nChunkCount; ++i ) {
		const uint32_t iCatalogChunk = resource.nFirstChunk + i;
		sirPackChunk_t chunk{};
		m_Catalog.chunkAt( iCatalogChunk, &chunk );
		if ( chunk.nChunkId == nChunkId ) {
			sirPackChunkSource_t source{};
			source.source.nPackageId = m_nPackageId;
			source.source.nPackageGeneration = m_nGeneration;
			source.source.nSourceRevision = resource.nRevision;
			source.key = key;
			source.nCookedRevision = resource.nRevision;
			source.iCatalogChunk = iCatalogChunk;
			source.chunk = chunk;
			source.nFormatId = resource.nFormatId;
			source.nSchemaVersion = resource.nSchemaVersion;
			source.nKind = resource.nKind;
			*pSource = source;
			return SIRPackResult::SIR_PACK_OK;
		}
	}

	return SIRPackResult::SIR_PACK_NOT_FOUND;
}

SIRPackResult SIRPack::readAt( uint32_t iShard, uint64_t nOffset, SIRResourceMutableBytes bytes, uint64_t *pTransferred ) const noexcept
{
	if ( !m_bOpen || m_pShards == nullptr || iShard >= m_nShardCount || bytes.m_pData == nullptr || pTransferred == nullptr ) {
		return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	}
	return m_pShards[iShard].readAt( nOffset, bytes.m_pData, bytes.m_nSize, pTransferred );
}
