/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Compression/SIRBZip2Codec.h>
#include <bzlib.h>
#include <limits.h>

SIRCompressionType SIRBZip2Codec::getType( void ) const noexcept
{
	return SIRCompressionType::SIR_COMPRESSION_TYPE_BZIP2;
}

uint64_t SIRBZip2Codec::getMaximumEncodedSize( uint64_t nDecodedBytes ) const noexcept
{
	const uint64_t nOverhead = nDecodedBytes / 100 + 601;
	if ( nDecodedBytes > UINT64_MAX - nOverhead || nDecodedBytes + nOverhead > UINT_MAX ) {
		return 0;
	}
	return nDecodedBytes + nOverhead;
}

uint64_t SIRBZip2Codec::getScratchSize( void ) const noexcept
{
	return 0;
}

SIRCompressionResult SIRBZip2Codec::encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	if ( rInput.getSize() > UINT_MAX || rOutput.getSize() > UINT_MAX ) {
		return SIRCompressionResult::SIR_COMPRESSION_SIZE_MISMATCH;
	}
	const uint64_t nBound = getMaximumEncodedSize( rInput.getSize() );
	if ( nBound == 0 || rOutput.getSize() < nBound ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	uint8_t nEmpty = 0;
	uint8_t *pInput = rInput.getData() == nullptr ? &nEmpty : rInput.getData();
	unsigned int nEncoded = static_cast<unsigned int>( rOutput.getSize() );
	const int nResult = BZ2_bzBuffToBuffCompress(
	    reinterpret_cast<char *>( rOutput.getData() ), &nEncoded, reinterpret_cast<char *>( pInput ), static_cast<unsigned int>( rInput.getSize() ), 9, 0, 30 );
	if ( nResult != BZ_OK ) {
		if ( nResult == BZ_OUTBUFF_FULL ) {
			return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
		}
		if ( nResult == BZ_MEM_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_OUT_OF_MEMORY;
		}
		return SIRCompressionResult::SIR_COMPRESSION_ENCODING_FAILED;
	}
	nBytesWritten = nEncoded;
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult SIRBZip2Codec::decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	if ( rInput.getSize() > UINT_MAX || rOutput.getSize() > UINT_MAX ) {
		return SIRCompressionResult::SIR_COMPRESSION_SIZE_MISMATCH;
	}
	if ( rInput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
	}
	if ( rOutput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	unsigned int nDecoded = static_cast<unsigned int>( rOutput.getSize() );
	const int nResult = BZ2_bzBuffToBuffDecompress(
	    reinterpret_cast<char *>( rOutput.getData() ), &nDecoded, reinterpret_cast<char *>( rInput.getData() ), static_cast<unsigned int>( rInput.getSize() ), 0, 0 );
	if ( nResult != BZ_OK ) {
		if ( nResult == BZ_OUTBUFF_FULL ) {
			return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
		}
		if ( nResult == BZ_MEM_ERROR ) {
			return SIRCompressionResult::SIR_COMPRESSION_OUT_OF_MEMORY;
		}
		if ( nResult == BZ_UNEXPECTED_EOF ) {
			return SIRCompressionResult::SIR_COMPRESSION_TRUNCATED_DATA;
		}
		if ( nResult == BZ_DATA_ERROR || nResult == BZ_DATA_ERROR_MAGIC ) {
			return SIRCompressionResult::SIR_COMPRESSION_CORRUPTED_DATA;
		}
		return SIRCompressionResult::SIR_COMPRESSION_DECODING_FAILED;
	}
	nBytesWritten = nDecoded;
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult SIRBZip2Codec::encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, SIRSpan<uint8_t>& rScratch, size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return encode( rInput, rOutput, nBytesWritten );
}

SIRCompressionResult SIRBZip2Codec::decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, SIRSpan<uint8_t>& rScratch, size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return decode( rInput, rOutput, nBytesWritten );
}
