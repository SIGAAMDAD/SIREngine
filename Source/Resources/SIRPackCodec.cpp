/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Resources/SIRPackCodec.h>

#include <limits.h>
#include <string.h>

#if defined( SIR_PACK_WITH_ZSTD )
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#endif

#if defined( SIR_PACK_WITH_LZ4 )
#include <lz4.h>
#endif

uint64_t sirPackDecodeScratchBytes( sirPackCodec_t eCodec ) noexcept
{
	if ( eCodec == SIR_PACK_CODEC_NONE || eCodec == SIR_PACK_CODEC_LZ4 ) {
		return 0;
	}
#if defined( SIR_PACK_WITH_ZSTD )
	if ( eCodec == SIR_PACK_CODEC_ZSTD ) {
		return ( uint64_t )ZSTD_estimateDCtxSize() + 8;
	}
#endif
	return UINT64_MAX;
}

sirPackResult_t sirPackDecode( const sirPackChunk_t& chunk, const void *pEncoded, uint64_t nEncodedBytes, void *pDecoded,
                               uint64_t nDecodedCapacity, void *pScratch, uint64_t nScratchBytes ) noexcept
{
	if ( pEncoded == nullptr || pDecoded == nullptr || nEncodedBytes != chunk.nEncodedBytes || nDecodedCapacity < chunk.nDecodedBytes ||
	     chunk.nDecodedBytes == 0 || chunk.nDecodedBytes > SIR_PACK_MAX_DECODED_CHUNK_BYTES || chunk.nDictionaryId != 0 ||
	     chunk.nCodecVersion != 1 || nEncodedBytes > SIZE_MAX || chunk.nDecodedBytes > SIZE_MAX ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}

	if ( chunk.eCodec == SIR_PACK_CODEC_NONE ) {
		if ( nEncodedBytes != chunk.nDecodedBytes ) {
			return SIR_PACK_INVALID_FORMAT;
		}
		if ( pDecoded != pEncoded ) {
			memmove( pDecoded, pEncoded, ( size_t )nEncodedBytes );
		}
		return SIR_PACK_OK;
	}

#if defined( SIR_PACK_WITH_LZ4 )
	if ( chunk.eCodec == SIR_PACK_CODEC_LZ4 ) {
		if ( nEncodedBytes > INT_MAX || chunk.nDecodedBytes > INT_MAX ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		const int nResult =
		    LZ4_decompress_safe( ( const char * )pEncoded, ( char * )pDecoded, ( int )nEncodedBytes, ( int )chunk.nDecodedBytes );
		return nResult >= 0 && ( uint64_t )nResult == chunk.nDecodedBytes ? SIR_PACK_OK : SIR_PACK_INVALID_FORMAT;
	}
#endif

#if defined( SIR_PACK_WITH_ZSTD )
	if ( chunk.eCodec == SIR_PACK_CODEC_ZSTD ) {
		const uint64_t nScratchRequired = sirPackDecodeScratchBytes( chunk.eCodec );
		if ( pScratch == nullptr || nScratchBytes < nScratchRequired || nScratchBytes > UINTPTR_MAX - ( uintptr_t )pScratch ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		const uintptr_t nAligned = ( ( uintptr_t )pScratch + 7 ) & ~( uintptr_t )7;
		ZSTD_DCtx *pContext = ZSTD_initStaticDCtx( ( void * )nAligned, ( size_t )( nScratchBytes - ( nAligned - ( uintptr_t )pScratch ) ) );
		if ( pContext == nullptr ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		const size_t nResult = ZSTD_decompressDCtx( pContext, pDecoded, ( size_t )chunk.nDecodedBytes, pEncoded, ( size_t )nEncodedBytes );
		return !ZSTD_isError( nResult ) && nResult == chunk.nDecodedBytes ? SIR_PACK_OK : SIR_PACK_INVALID_FORMAT;
	}
#endif
	return SIR_PACK_UNSUPPORTED_CODEC;
}

sirPackResult_t sirPackEncodeOffline( sirPackCodec_t eCodec, const void *pDecoded, uint64_t nDecodedBytes, void *pEncoded,
                                      uint64_t nEncodedCapacity, uint64_t *pEncodedBytes ) noexcept
{
	if ( pDecoded == nullptr || pEncoded == nullptr || pEncodedBytes == nullptr || nDecodedBytes == 0 ||
	     nDecodedBytes > SIR_PACK_MAX_DECODED_CHUNK_BYTES || nEncodedCapacity > SIZE_MAX ) {
		return SIR_PACK_INVALID_ARGUMENT;
	}
	if ( eCodec == SIR_PACK_CODEC_NONE ) {
		if ( nEncodedCapacity < nDecodedBytes ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		memmove( pEncoded, pDecoded, ( size_t )nDecodedBytes );
		*pEncodedBytes = nDecodedBytes;
		return SIR_PACK_OK;
	}
#if defined( SIR_PACK_WITH_LZ4 )
	if ( eCodec == SIR_PACK_CODEC_LZ4 ) {
		if ( nDecodedBytes > INT_MAX || nEncodedCapacity > INT_MAX ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		const int nWritten =
		    LZ4_compress_default( ( const char * )pDecoded, ( char * )pEncoded, ( int )nDecodedBytes, ( int )nEncodedCapacity );
		if ( nWritten <= 0 ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		*pEncodedBytes = ( uint64_t )nWritten;
		return SIR_PACK_OK;
	}
#endif
#if defined( SIR_PACK_WITH_ZSTD )
	if ( eCodec == SIR_PACK_CODEC_ZSTD ) {
		const size_t nWritten = ZSTD_compress( pEncoded, ( size_t )nEncodedCapacity, pDecoded, ( size_t )nDecodedBytes, 3 );
		if ( ZSTD_isError( nWritten ) ) {
			return SIR_PACK_OUT_OF_CAPACITY;
		}
		*pEncodedBytes = nWritten;
		return SIR_PACK_OK;
	}
#endif
	return SIR_PACK_UNSUPPORTED_CODEC;
}
