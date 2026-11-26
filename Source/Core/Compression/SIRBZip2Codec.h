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

#ifndef __SIR_BZIP2CODEC_H__
#define __SIR_BZIP2CODEC_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Compression/SIRCompressionCodec.h>

class SIRBZip2Codec final : public SIRCompressionCodec
{
public:
	explicit SIRBZip2Codec( void ) noexcept = default;
	explicit SIRBZip2Codec( const SIRBZip2Codec& ) noexcept = default;
	explicit SIRBZip2Codec( SIRBZip2Codec&& ) noexcept = default;
	~SIRBZip2Codec() noexcept override = default;

	SIRBZip2Codec& operator=( const SIRBZip2Codec& ) noexcept = default;
	SIRBZip2Codec& operator=( SIRBZip2Codec&& ) noexcept = default;

	SIRCompressionType getType( void ) const noexcept override;
	uint64_t getMaximumEncodedSize( uint64_t nDecodedBytes ) const noexcept override;
	uint64_t getScratchSize( void ) const noexcept override;
	SIRCompressionResult encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept override;
	SIRCompressionResult decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, size_t& nBytesWritten ) noexcept override;
	SIRCompressionResult encode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, SIRSpan<uint8_t>& rScratch, size_t& nBytesWritten ) noexcept override;
	SIRCompressionResult decode( const SIRSpan<uint8_t>& rInput, SIRSpan<uint8_t>& rOutput, SIRSpan<uint8_t>& rScratch, size_t& nBytesWritten ) noexcept override;
};

#endif // __SIR_BZIP2CODEC_H__
