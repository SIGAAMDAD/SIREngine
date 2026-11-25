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

#include <Core/Compression/SIRLZ4Codec.h>
#include <limits.h>
#include <lz4.h>

SIRCompressionType SIRLZ4Codec::getType( void ) const noexcept
{
	return SIRCompressionType::SIR_COMPRESSION_TYPE_LZ4;
}

uint64_t SIRLZ4Codec::getMaximumEncodedSize( uint64_t nDecodedBytes ) const noexcept
{
	if ( nDecodedBytes > static_cast<uint64_t>( INT_MAX ) ) {
		return 0;
	}
	return static_cast<uint64_t>( LZ4_compressBound( static_cast<int>( nDecodedBytes ) ) );
}

uint64_t SIRLZ4Codec::getScratchSize( void ) const noexcept
{
	return 0;
}

SIRCompressionResult SIRLZ4Codec::encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	if ( rInput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
	}
	if ( rInput.getSize() > static_cast<size_t>( INT_MAX ) || rOutput.getSize() > static_cast<size_t>( INT_MAX ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_SIZE_MISMATCH;
	}
	const uint64_t nBound = getMaximumEncodedSize( rInput.getSize() );
	if ( rOutput.getSize() < nBound ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	const int nEncoded = LZ4_compress_default( reinterpret_cast<const char *>( rInput.getData() ),
	                                           reinterpret_cast<char *>( rOutput.getData() ),
	                                           static_cast<int>( rInput.getSize() ),
	                                           static_cast<int>( rOutput.getSize() ) );
	if ( nEncoded <= 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_ENCODING_FAILED;
	}
	nBytesWritten = static_cast<size_t>( nEncoded );
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult SIRLZ4Codec::decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept
{
	nBytesWritten = 0;
	if ( ( rInput.getSize() != 0 && rInput.getData() == nullptr ) || ( rOutput.getSize() != 0 && rOutput.getData() == nullptr ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT;
	}
	if ( rInput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
	}
	if ( rInput.getSize() > static_cast<size_t>( INT_MAX ) || rOutput.getSize() > static_cast<size_t>( INT_MAX ) ) {
		return SIRCompressionResult::SIR_COMPRESSION_SIZE_MISMATCH;
	}
	if ( rOutput.getSize() == 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY;
	}
	const int nDecoded = LZ4_decompress_safe( reinterpret_cast<const char *>( rInput.getData() ),
	                                          reinterpret_cast<char *>( rOutput.getData() ),
	                                          static_cast<int>( rInput.getSize() ),
	                                          static_cast<int>( rOutput.getSize() ) );
	if ( nDecoded < 0 ) {
		return SIRCompressionResult::SIR_COMPRESSION_CORRUPTED_DATA;
	}
	nBytesWritten = static_cast<size_t>( nDecoded );
	return SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
}

SIRCompressionResult
SIRLZ4Codec::encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, SIRSpan<uint8_t>& rScratch, size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return encode( rInput, rOutput, nBytesWritten );
}

SIRCompressionResult
SIRLZ4Codec::decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, SIRSpan<uint8_t>& rScratch, size_t& nBytesWritten ) noexcept
{
	static_cast<void>( rScratch );
	return decode( rInput, rOutput, nBytesWritten );
}
