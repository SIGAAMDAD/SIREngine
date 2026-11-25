#include <Streaming/Pack/SIRCompressionRegistry.h>
#include <string.h>

static SIRCompressionType sirPackToCoreCodec( sirPackCodec_t codec ) noexcept
{
	return codec == SIR_PACK_CODEC_ZSTD ? SIRCompressionType::SIR_COMPRESSION_TYPE_ZSTD : SIRCompressionType::SIR_COMPRESSION_TYPE_LZ4;
}

SIRCompressionRegistry::SIRCompressionRegistry( void ) noexcept : m_pCodecs{}, m_nCount( 0 ), m_bFrozen( false ) {}

SIRPackResult SIRCompressionRegistry::registerCodec( SIRCompressionCodec& codec ) noexcept
{
	if ( m_bFrozen ) return SIRPackResult::SIR_PACK_BUSY;
	if ( m_nCount >= SIR_PACK_MAX_CODECS ) return SIRPackResult::SIR_PACK_OUT_OF_CAPACITY;
	if ( codec.getType() != SIRCompressionType::SIR_COMPRESSION_TYPE_ZSTD && codec.getType() != SIRCompressionType::SIR_COMPRESSION_TYPE_LZ4 ) return SIRPackResult::SIR_PACK_UNSUPPORTED_CODEC;
	if ( find( codec.getType() == SIRCompressionType::SIR_COMPRESSION_TYPE_ZSTD ? SIR_PACK_CODEC_ZSTD : SIR_PACK_CODEC_LZ4 ) != nullptr ) return SIRPackResult::SIR_PACK_BUSY;
	m_pCodecs[m_nCount++] = &codec;
	return SIRPackResult::SIR_PACK_OK;
}

const SIRCompressionCodec *SIRCompressionRegistry::find( sirPackCodec_t codec ) const noexcept
{
	const SIRCompressionType type = sirPackToCoreCodec( codec );
	for ( uint32_t i = 0; i < m_nCount; ++i ) if ( m_pCodecs[i]->getType() == type ) return m_pCodecs[i];
	return nullptr;
}

SIRPackResult SIRCompressionRegistry::decompress( sirPackCodec_t codec, const sirCompressionDesc_t& desc ) const noexcept
{
	if ( desc.nCodecVersion != 1 || desc.nDictionaryId != 0 || desc.source.m_pData == nullptr || desc.destination.m_pData == nullptr ) return SIRPackResult::SIR_PACK_INVALID_ARGUMENT;
	if ( codec == SIR_PACK_CODEC_NONE ) {
		if ( desc.source.m_nSize != desc.destination.m_nSize ) return SIRPackResult::SIR_PACK_INVALID_FORMAT;
		memcpy( desc.destination.m_pData, desc.source.m_pData, static_cast<size_t>( desc.source.m_nSize ) );
		return SIRPackResult::SIR_PACK_OK;
	}
	const SIRCompressionCodec *pCodec = find( codec );
	if ( pCodec == nullptr ) return SIRPackResult::SIR_PACK_UNSUPPORTED_CODEC;
	SIRSpan<uint8_t> input( const_cast<uint8_t *>( desc.source.m_pData ), static_cast<size_t>( desc.source.m_nSize ) );
	SIRSpan<uint8_t> output( desc.destination.m_pData, static_cast<size_t>( desc.destination.m_nSize ) );
	SIRSpan<uint8_t> scratch( desc.scratch.m_pData, static_cast<size_t>( desc.scratch.m_nSize ) );
	size_t nDecoded = 0;
	const SIRCompressionResult result = const_cast<SIRCompressionCodec *>( pCodec )->decode( input, output, scratch, nDecoded );
	if ( result != SIRCompressionResult::SIR_COMPRESSION_SUCCESS ) return SIRPackResult::SIR_PACK_INVALID_FORMAT;
	return nDecoded == desc.destination.m_nSize ? SIRPackResult::SIR_PACK_OK : SIRPackResult::SIR_PACK_INVALID_FORMAT;
}

void SIRCompressionRegistry::freeze( void ) noexcept { m_bFrozen = true; }
bool SIRCompressionRegistry::isFrozen( void ) const noexcept { return m_bFrozen; }
