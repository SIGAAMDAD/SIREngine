#ifndef __SIR_COMPRESSIONREGISTRY_H__
#define __SIR_COMPRESSIONREGISTRY_H__

#include <Streaming/Pack/SIRCompressionTypes.h>
#include <Core/Compression/SIRCompressionCodec.h>

constexpr uint32_t SIR_PACK_MAX_CODECS = 8;

class SIRCompressionRegistry final
{
public:
	SIRCompressionRegistry( void ) noexcept;
	SIRPackResult registerCodec( SIRCompressionCodec& codec ) noexcept;
	const SIRCompressionCodec *find( sirPackCodec_t codec ) const noexcept;
	SIRPackResult decompress( sirPackCodec_t codec, const sirCompressionDesc_t& desc ) const noexcept;
	void freeze( void ) noexcept;
	bool isFrozen( void ) const noexcept;

private:
	SIRCompressionCodec *m_pCodecs[SIR_PACK_MAX_CODECS];
	uint32_t m_nCount;
	bool m_bFrozen;
};

#endif // __SIR_COMPRESSIONREGISTRY_H__
