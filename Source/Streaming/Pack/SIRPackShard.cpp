#include <Streaming/Pack/SIRPack.h>
#include <Streaming/Pack/SIRPackShard.h>
#include <string.h>

SIRPackShard::SIRPackShard( void ) noexcept : m_nSize( 0 ), m_iShard( 0 ), m_bOpen( false )
{
}

SIRPackShard::~SIRPackShard( void ) noexcept
{
	close();
}

bool SIRPackShard::isOpen( void ) const noexcept { return m_bOpen && m_File.isOpen(); }
uint32_t SIRPackShard::getIndex( void ) const noexcept { return m_iShard; }
uint64_t SIRPackShard::getSize( void ) const noexcept { return m_nSize; }

SIRPackResult SIRPackShard::open( const sirPackShardOpenDesc_t& desc ) noexcept
{
	if ( isOpen() ) return SIRPackResult::SIR_PACK_BUSY;
	if ( desc.pRoot == nullptr || desc.pszPath == nullptr || desc.nPackageId == 0 || desc.nGeneration == 0 ) return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	if ( desc.pRoot->openRead( desc.pszPath, &m_File ) != sirFileResult::SIR_FILE_OK ) return SIRPackResult::SIR_PACK_NOT_FOUND;
	uint64_t nSize = 0;
	if ( m_File.getSize( &nSize ) != sirFileResult::SIR_FILE_OK || nSize != desc.expected.nBytes ) {
		m_File.close();
		return SIRPackResult::SIR_PACK_TRUNCATED;
	}
	m_nSize = nSize;
	m_iShard = desc.expected.nIndex;
	m_bOpen = true;
	uint8_t header[SIR_PACK_SHARD_HEADER_BYTES] = {};
	uint64_t nRead = 0;
	if ( m_File.readAt( 0, header, sizeof( header ), &nRead ) != sirFileResult::SIR_FILE_OK || nRead != sizeof( header ) ) {
		close();
		return SIRPackResult::SIR_PACK_TRUNCATED;
	}
	if ( header[0] != 'S' || header[1] != 'I' || header[2] != 'R' || header[3] != 'P' || sirPackRead16( header + 4 ) != SIR_PACK_VERSION ||
	     sirPackRead32( header + 12 ) != m_iShard || sirPackRead64( header + 16 ) != desc.nPackageId ||
	     sirPackRead64( header + 24 ) != desc.nGeneration || sirPackRead64( header + 32 ) != nSize ) {
		close();
		return SIRPackResult::SIR_PACK_STALE_SOURCE;
	}
	sirPackSha256State_t hash {};
	sirPackSha256Init( &hash );
	uint8_t block[64 * 1024];
	uint64_t nOffset = 0;
	while ( nOffset < nSize ) {
		const uint64_t nRequest = ( nSize - nOffset ) < sizeof( block ) ? ( nSize - nOffset ) : sizeof( block );
		nRead = 0;
		if ( m_File.readAt( nOffset, block, nRequest, &nRead ) != sirFileResult::SIR_FILE_OK || nRead != nRequest ) {
			close();
			return SIRPackResult::SIR_PACK_TRUNCATED;
		}
		sirPackSha256Update( &hash, block, nRead );
		nOffset += nRead;
	}
	uint8_t digest[32] = {};
	sirPackSha256Final( &hash, digest );
	if ( memcmp( digest, desc.expected.szDigest, sizeof( digest ) ) != 0 ) {
		close();
		return SIRPackResult::SIR_PACK_BAD_DIGEST;
	}
	return SIRPackResult::SIR_PACK_OK;
}

SIRPackResult SIRPackShard::close( void ) noexcept
{
	if ( !isOpen() ) return SIRPackResult::SIR_PACK_OK;
	if ( m_File.close() != sirFileResult::SIR_FILE_OK ) return SIRPackResult::SIR_PACK_BUSY;
	m_nSize = 0;
	m_bOpen = false;
	return SIRPackResult::SIR_PACK_OK;
}

SIRPackResult SIRPackShard::validateRange( uint64_t nOffset, uint64_t nBytes ) const noexcept
{
	if ( !isOpen() || nBytes == 0 || nOffset > m_nSize || nBytes > m_nSize - nOffset ) return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	return SIRPackResult::SIR_PACK_OK;
}

SIRPackResult SIRPackShard::readAt( uint64_t nOffset, void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) const noexcept
{
	if ( pTransferred == nullptr ) return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	*pTransferred = 0;
	if ( pDestination == nullptr || validateRange( nOffset, nBytes ) != SIRPackResult::SIR_PACK_OK ) return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	const sirFileResult eRead = m_File.readAt( nOffset, pDestination, nBytes, pTransferred );
	if ( eRead != sirFileResult::SIR_FILE_OK ) return eRead == sirFileResult::SIR_FILE_END_OF_FILE ? SIRPackResult::SIR_PACK_TRUNCATED : SIRPackResult::SIR_PACK_IO_ERROR;
	return *pTransferred == nBytes ? SIRPackResult::SIR_PACK_OK : SIRPackResult::SIR_PACK_TRUNCATED;
}
