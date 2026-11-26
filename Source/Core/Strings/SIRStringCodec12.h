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

#ifndef __SIR_STRINGCODEC12_H__
#define __SIR_STRINGCODEC12_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRString.h>

// Engine-local 12-bit static dictionary codec. Codes 0..255 are literal bytes;
// 256..4095 are symbols of 2..8 bytes. Two codes occupy three little-endian bytes.
// The dictionary is immutable while streams are in use. No hidden allocation.
typedef struct sirStringSymbol12_s
{
	char szBytes[8];
	uint32_t nBytes;
	uint32_t nFrequency;
} sirStringSymbol12_t;

typedef struct sirStringCodec12Workspace_s
{
	uint32_t *pCosts; // input length + 1
	uint16_t *pCodes; // input length
	uint32_t nCapacity;
} sirStringCodec12Workspace_t;

typedef struct sirStringCodec12Output_s
{
	uint8_t *pBytes;
	uint32_t nCapacity;
	uint32_t nWritten;
	uint32_t nCodes;
	bool bRaw : 1;
} sirStringCodec12Output_t;

class SIRStringCodec12 final
{
public:
	explicit SIRStringCodec12() noexcept = default;
	explicit SIRStringCodec12( const SIRStringCodec12& ) = delete;
	explicit SIRStringCodec12( SIRStringCodec12&& ) = delete;
	~SIRStringCodec12() noexcept = default;

	SIRStringCodec12& operator=( const SIRStringCodec12& ) = delete;
	SIRStringCodec12& operator=( SIRStringCodec12&& ) = delete;

	[[nodiscard]] uint32_t getSymbolCount( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInitialize( const sirStringSymbol12_t *pSymbols, uint32_t nCount ) noexcept;

	// Caller-owned open-addressed candidate table, cleared by train. Exhaustion
	// returns CAPACITY_EXCEEDED without changing this dictionary.
	[[nodiscard]] SIRContainerResult tryTrain( const sirStringView_t *pCorpus, uint32_t nCount, sirStringSymbol12_t *pCandidates, uint32_t nCapacity ) noexcept;
	[[nodiscard]] SIRContainerResult tryEncode( sirStringView_t input, const sirStringCodec12Workspace_t& workspace, sirStringCodec12Output_t& output ) const noexcept;
	[[nodiscard]] SIRContainerResult tryEncodeAdaptive( sirStringView_t input, const sirStringCodec12Workspace_t& workspace, sirStringCodec12Output_t& output ) const noexcept;
	[[nodiscard]] SIRContainerResult tryDecodeAdaptive( const sirStringCodec12Output_t& input, char *pOutput, uint32_t nCapacity ) const noexcept;
	[[nodiscard]] SIRContainerResult tryDecode( sirStringView_t encoded, uint32_t nCodes, char *pOutput, uint32_t nCapacity ) const noexcept;

private:
	[[nodiscard]] SIRContainerResult
	encodeImpl( sirStringView_t input, const sirStringCodec12Workspace_t& workspace, sirStringCodec12Output_t& output, bool bAdaptive ) const noexcept;
	static constexpr uint32_t MAX_SYMBOLS = 3840;
	[[nodiscard]] static uint64_t score( const sirStringSymbol12_t& symbol ) noexcept;
	[[nodiscard]] static uint16_t readCode( const char *pBytes, uint32_t iCode ) noexcept;
	[[nodiscard]] uint32_t symbolLength( uint16_t nCode ) const noexcept;
	void addSymbol( const sirStringSymbol12_t& symbol ) noexcept;

	sirStringSymbol12_t m_Symbols[MAX_SYMBOLS]{};
	uint16_t m_iHeads[256]{};
	uint16_t m_iNext[MAX_SYMBOLS]{};
	uint32_t m_nCount{};
};

#endif // __SIR_STRINGCODEC12_H__
#include <Core/Containers/SIRString.h>
