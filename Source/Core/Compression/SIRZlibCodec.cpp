/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Compression/SIRZlibCodec.h>
#include <limits.h>
#include <zlib.h>

SIRCompressionType SIRZlibCodec::getType( void ) const noexcept
{
	return SIRCompressionType::SIR_COMPRESSION_TYPE_ZLIB;
}

uint64_t SIRZlibCodec::getMaximumEncodedSize( uint64_t nDecodedBytes ) const noexcept
{
	if ( nDecodedBytes > static_cast<uint64_t>( ULONG_MAX ) ) {
		return 0;
	}
	return static_cast<uint64_t>( compressBound( static_cast<uLong>( nDecodedBytes ) ) );
}

uint64_t SIRZlibCodec::getScratchSize( void ) const noexcept
{
	return 0;
}

SIRCompressionResult SIRZlibCodec::encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	if ( rInput.getSize() > static_cast<size_t>( ULONG_MAX ) || rOutput.getSize() > static_cast<size_t>( ULONG_MAX ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_SIZE_MISMATCH;
	}
	const uint64_t nBound = getMaximumEncodedSize( rInput.getSize() );
	if ( nBound == 0 || rOutput.getSize() < nBound ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	uint8_t nEmpty = 0;
	const Bytef *pInput = rInput.getData() == nullptr ? &nEmpty : rInput.getData();
	uLongf nEncoded = static_cast<uLongf>( rOutput.getSize() );
	const int nResult = compress2(
	    reinterpret_cast<Bytef *>( rOutput.getData() ), &nEncoded, pInput, static_cast<uLong>( rInput.getSize() ), Z_DEFAULT_COMPRESSION );
	if ( nResult != Z_OK ) {
		if ( nResult == Z_MEM_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_OUT_OF_MEMORY;
		}
		if ( nResult == Z_BUF_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
		}
		return SIRCompressionResult::SIR_COMPRESSION_ENCODING_FAILED;
	}
	nBytesWritten = static_cast<size_t>( nEncoded );
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult SIRZlibCodec::decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	if ( rInput.getSize() > static_cast<size_t>( ULONG_MAX ) || rOutput.getSize() > static_cast<size_t>( ULONG_MAX ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_SIZE_MISMATCH;
	}
	if ( rInput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
	}
	if ( rOutput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	uLongf nDecoded = static_cast<uLongf>( rOutput.getSize() );
	const int nResult =
	    uncompress( reinterpret_cast<Bytef *>( rOutput.getData() ), &nDecoded, rInput.getData(), static_cast<uLong>( rInput.getSize() ) );
	if ( nResult != Z_OK ) {
		if ( nResult == Z_BUF_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
		}
		if ( nResult == Z_MEM_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_OUT_OF_MEMORY;
		}
		if ( nResult == Z_VERSION_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_UNSUPPORTED_VERSION;
		}
		if ( nResult == Z_DATA_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_CORRUPTED_DATA;
		}
		return SIRCompressionResult::SIR_COMPRESSION_DECODING_FAILED;
	}
	nBytesWritten = static_cast<size_t>( nDecoded );
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult SIRZlibCodec::encode( const SIRSpan<uint8_t>& rInput,
                                           SIRSpan<uint8_t>& rOutput,
                                           SIRSpan<uint8_t>& rScratch,
                                           size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return encode( rInput, rOutput, nBytesWritten );
}

SIRCompressionResult SIRZlibCodec::decode( const SIRSpan<uint8_t>& rInput,
                                           SIRSpan<uint8_t>& rOutput,
                                           SIRSpan<uint8_t>& rScratch,
                                           size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return decode( rInput, rOutput, nBytesWritten );
}
