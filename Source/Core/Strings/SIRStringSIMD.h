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

#ifndef __SIR_STRINGSIMD_H__
#define __SIR_STRINGSIMD_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// ISA selection is confined to the platform implementation. No reads past bounds.
class SIRStringSIMD final
{
public:
	explicit SIRStringSIMD() = delete;
	explicit SIRStringSIMD( const SIRStringSIMD& ) = delete;
	explicit SIRStringSIMD( SIRStringSIMD&& ) = delete;
	~SIRStringSIMD() = delete;
	SIRStringSIMD& operator=( const SIRStringSIMD& ) = delete;
	SIRStringSIMD& operator=( SIRStringSIMD&& ) = delete;
	[[nodiscard]] static bool equals( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static uint32_t findByte( sirStringView_t input, char nByte ) noexcept;
	// Input is 0..63 in each byte, count <= 8. Output packs low six bits of each byte.
	[[nodiscard]] static uint64_t packCodes( uint64_t nCodes ) noexcept;
	[[nodiscard]] static uint64_t unpackCodes( uint64_t nPacked ) noexcept;

	[[nodiscard]] static bool isValidUtf8( sirStringView_t input ) noexcept;
	// Four independent FNV-1a lanes, common prefix only. Caller validates ranges.
	static void hashFour( const sirStringView_t *pInputs, uint32_t nBytes, uint64_t *pHashes ) noexcept;

private:
	[[nodiscard]] static uint32_t asciiPrefix( sirStringView_t input ) noexcept;
	[[nodiscard]] static bool equalsScalar( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static uint32_t findScalar( sirStringView_t input, char nByte ) noexcept;
#if ( defined( __x86_64__ ) || defined( __i386__ ) ) && ( defined( __GNUC__ ) || defined( __clang__ ) )
	static void hashFourAVX2( const sirStringView_t *pInputs, uint32_t nBytes, uint64_t *pHashes ) noexcept;
	[[nodiscard]] static uint32_t asciiPrefixSSE2( sirStringView_t input ) noexcept;
	[[nodiscard]] static bool equalsSSE2( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static bool equalsAVX2( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static bool equalsAVX512( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static uint32_t findSSE2( sirStringView_t input, char nByte ) noexcept;
	[[nodiscard]] static uint32_t findAVX2( sirStringView_t input, char nByte ) noexcept;
	[[nodiscard]] static uint32_t findAVX512( sirStringView_t input, char nByte ) noexcept;
#if defined( __x86_64__ )
	[[nodiscard]] static uint64_t packBMI2( uint64_t nCodes ) noexcept;
	[[nodiscard]] static uint64_t unpackBMI2( uint64_t nPacked ) noexcept;
#endif
#endif
#if defined( __aarch64__ ) && defined( __ARM_NEON )
	[[nodiscard]] static bool equalsNEON( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static uint32_t findNEON( sirStringView_t input, char nByte ) noexcept;
#endif
};
#endif // __SIR_STRINGSIMD_H__
