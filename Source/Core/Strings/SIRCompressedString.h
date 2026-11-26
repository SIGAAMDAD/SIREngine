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

#ifndef __SIR_COMPRESSED_STRING_H__
#define __SIR_COMPRESSED_STRING_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRString.h>

// Fixed static dictionary: up to 255 symbols of 1-8 bytes. Code 255 escapes a
// literal byte. This is an engine-local format inspired by FSST, not FSST wire format.
// Dictionary initialization is exclusive; it must remain immutable while encoded data lives.
class SIRStringDictionary final
{
public:
	explicit SIRStringDictionary( void ) noexcept = default;
	explicit SIRStringDictionary( const SIRStringDictionary& ) = delete;
	explicit SIRStringDictionary( SIRStringDictionary&& ) = delete;
	~SIRStringDictionary() noexcept = default;

	SIRStringDictionary& operator=( const SIRStringDictionary& ) = delete;
	SIRStringDictionary& operator=( SIRStringDictionary&& ) = delete;

	[[nodiscard]] SIRContainerResult tryInitialize( const sirStringView_t *pSymbols, uint32_t nCount ) noexcept;
	[[nodiscard]] uint32_t getMatch( sirStringView_t input ) const noexcept;
	[[nodiscard]] uint32_t getOptimalMatch( sirStringView_t input, const uint32_t *pCosts ) const noexcept;
	[[nodiscard]] sirStringView_t getSymbol( uint32_t iCode ) const noexcept;

private:
	static constexpr uint32_t MAX_SYMBOLS = 255;
	char m_szSymbols[MAX_SYMBOLS][8]{};
	uint8_t m_nLengths[MAX_SYMBOLS]{};
	uint16_t m_iHeads[256]{};
	uint16_t m_iNext[MAX_SYMBOLS]{};
	uint32_t m_nCount{};
};

// Caller-owned DP workspace; one entry per input byte plus one sentinel.
typedef struct sirStringEncodeWorkspace_s
{
	uint32_t *pCosts;
	uint32_t nCount;
} sirStringEncodeWorkspace_t;

typedef struct sirStringEncodeOutput_s
{
	char *pBytes;
	uint32_t nCapacity;
} sirStringEncodeOutput_t;

// Non-owning encoded byte span and decoded length. Caller owns encoded storage and dictionary.
// Encoding/decoding perform a complete preflight before changing the destination.
class SIRCompressedString final
{
public:
	explicit SIRCompressedString( void ) noexcept;
	explicit SIRCompressedString( const SIRCompressedString& ) noexcept = default;
	explicit SIRCompressedString( SIRCompressedString&& ) noexcept = default;
	~SIRCompressedString() noexcept = default;

	SIRCompressedString& operator=( const SIRCompressedString& ) noexcept = default;
	SIRCompressedString& operator=( SIRCompressedString&& ) noexcept = default;

	[[nodiscard]] uint32_t getDecodedBytes( void ) const noexcept;
	[[nodiscard]] sirStringView_t getEncodedView( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryEncode( const SIRStringDictionary& dictionary, sirStringView_t input, char *pOutput, uint32_t nCapacity ) noexcept;
	[[nodiscard]] SIRContainerResult
	tryEncodeOptimal( const SIRStringDictionary& dictionary, sirStringView_t input, const sirStringEncodeOutput_t& output, const sirStringEncodeWorkspace_t& workspace ) noexcept;
	[[nodiscard]] SIRContainerResult tryBindRaw( sirStringView_t raw ) noexcept;
	[[nodiscard]] bool isRaw( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryBind( const SIRStringDictionary& dictionary, sirStringView_t encoded, uint32_t nDecodedBytes ) noexcept;
	[[nodiscard]] SIRContainerResult tryDecode( char *pszOutput, uint32_t nCapacity ) const noexcept;

private:
	[[nodiscard]] static SIRContainerResult measureEncoded( const SIRStringDictionary& dictionary, sirStringView_t input, uint32_t& nBytes ) noexcept;
	[[nodiscard]] static SIRContainerResult measureDecoded( const SIRStringDictionary& dictionary, sirStringView_t encoded, uint32_t& nBytes ) noexcept;

	const SIRStringDictionary *m_pDictionary{};
	sirStringView_t m_Encoded{};
	uint32_t m_nDecodedBytes{};
	bool m_bRaw : 1;
};

#endif // __SIR_COMPRESSED_STRING_H__
