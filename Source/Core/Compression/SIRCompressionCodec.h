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

#ifndef __SIR_COMPRESSION_CODEC_H__
#define __SIR_COMPRESSION_CODEC_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>

enum class SIRCompressionType : uint8_t
{
	SIR_COMPRESSION_TYPE_LZ4,
	SIR_COMPRESSION_TYPE_ZSTD,
	SIR_COMPRESSION_TYPE_ZLIB,
	SIR_COMPRESSION_TYPE_BZIP2
};

enum class SIRCompressionResult : uint8_t
{
	SIR_COMPRESSION_SUCCESS = 0,
	SIR_COMPRESSION_INVALID_ARGUMENT,
	SIR_COMPRESSION_UNSUPPORTED_CODEC,
	SIR_COMPRESSION_UNSUPPORTED_VERSION,
	SIR_COMPRESSION_UNSUPPORTED_LEVEL,
	SIR_COMPRESSION_INSUFFICIENT_CAPACITY,
	SIR_COMPRESSION_INSUFFICIENT_SCRATCH,
	SIR_COMPRESSION_OUT_OF_MEMORY,
	SIR_COMPRESSION_CORRUPTED_DATA,
	SIR_COMPRESSION_TRUNCATED_DATA,
	SIR_COMPRESSION_SIZE_MISMATCH,
	SIR_COMPRESSION_DICTIONARY_REQUIRED,
	SIR_COMPRESSION_DICTIONARY_MISMATCH,
	SIR_COMPRESSION_ENCODING_FAILED,
	SIR_COMPRESSION_DECODING_FAILED,
	SIR_COMPRESSION_INTERNAL_ERROR
};

class SIRCompressionCodec
{
public:
	virtual ~SIRCompressionCodec() noexcept = default;

	virtual SIRCompressionType getType( void ) const noexcept = 0;
	virtual uint64_t getMaximumEncodedSize( uint64_t nDecodedBytes ) const noexcept = 0;
	virtual uint64_t getScratchSize( void ) const noexcept = 0;
	virtual SIRCompressionResult encode( const SIRSpan<uint8_t>& input, SIRSpan<uint8_t>& output, size_t& nBytesWritten ) noexcept = 0;
	virtual SIRCompressionResult decode( const SIRSpan<uint8_t>& input, SIRSpan<uint8_t>& output, size_t& nBytesWritten ) noexcept = 0;
	virtual SIRCompressionResult
	encode( const SIRSpan<uint8_t>& input, SIRSpan<uint8_t>& output, SIRSpan<uint8_t>& scratch, size_t& nBytesWritten ) noexcept = 0;
	virtual SIRCompressionResult
	decode( const SIRSpan<uint8_t>& input, SIRSpan<uint8_t>& output, SIRSpan<uint8_t>& scratch, size_t& nBytesWritten ) noexcept = 0;
};

#endif // __SIR_COMPRESSION_CODEC_H__
