/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://www.mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/Compression/SIRZstdCodec.h>
#include <zstd.h>
#include <zstd_errors.h>

SIRCompressionType SIRZstdCodec::getType( void ) const noexcept
{
	return SIRCompressionType::SIR_COMPRESSION_TYPE_ZSTD;
}

uint64_t SIRZstdCodec::getMaximumEncodedSize( uint64_t nDecodedBytes ) const noexcept
{
	const size_t nBound = ZSTD_compressBound( static_cast<size_t>( nDecodedBytes ) );
	if ( nBound == ZSTD_CONTENTSIZE_ERROR || nBound == ZSTD_CONTENTSIZE_UNKNOWN ) {
		return 0;
	}
	return static_cast<uint64_t>( nBound );
}

uint64_t SIRZstdCodec::getScratchSize( void ) const noexcept
{
	return 0;
}

SIRCompressionResult SIRZstdCodec::encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	const uint64_t nBound = getMaximumEncodedSize( rInput.getSize() );
	if ( nBound == 0 || rOutput.getSize() < nBound ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	uint8_t nEmpty = 0;
	const void *pInput = rInput.getData() == nullptr ? &nEmpty : rInput.getData();
	const size_t nEncoded = ZSTD_compress( rOutput.getData(), rOutput.getSize(), pInput, rInput.getSize(), 3 );
	if ( ZSTD_isError( nEncoded ) ) {
		if ( ZSTD_getErrorCode( nEncoded ) == ZSTD_error_memory_allocation ) {
			return SIRCompressionResult::SIR_COMPRESSION_OUT_OF_MEMORY;
		}
		if ( ZSTD_getErrorCode( nEncoded ) == ZSTD_error_dstSize_tooSmall ) {
			return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
		}
		return SIRCompressionResult::SIR_COMPRESSION_ENCODING_FAILED;
	}
	nBytesWritten = nEncoded;
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult SIRZstdCodec::decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	if ( rInput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
	}
	if ( rOutput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	const size_t nDecoded = ZSTD_decompress( rOutput.getData(), rOutput.getSize(), rInput.getData(), rInput.getSize() );
	if ( ZSTD_isError( nDecoded ) ) {
		const ZSTD_ErrorCode eError = ZSTD_getErrorCode( nDecoded );
		if ( eError == ZSTD_error_dstSize_tooSmall ) {
			return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
		}
		if ( eError == ZSTD_error_srcSize_wrong ) {
			return SIRCompressionResult::SIR_COMPRESSION_TRUNCATED_DATA;
		}
		if ( eError == ZSTD_error_memory_allocation ) {
			return SIRCompressionResult::SIR_COMPRESSION_OUT_OF_MEMORY;
		}
		return SIRCompressionResult::SIR_COMPRESSION_CORRUPTED_DATA;
	}
	nBytesWritten = nDecoded;
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult SIRZstdCodec::encode( const SIRSpan<uint8_t>& rInput,
                                           SIRSpan<uint8_t>& rOutput,
                                           SIRSpan<uint8_t>& rScratch,
                                           size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return encode( rInput, rOutput, nBytesWritten );
}

SIRCompressionResult SIRZstdCodec::decode( const SIRSpan<uint8_t>& rInput,
                                           SIRSpan<uint8_t>& rOutput,
                                           SIRSpan<uint8_t>& rScratch,
                                           size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return decode( rInput, rOutput, nBytesWritten );
}
