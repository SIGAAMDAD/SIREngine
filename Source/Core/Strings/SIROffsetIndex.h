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

#ifndef __SIR_OFFSETINDEX_H__
#define __SIR_OFFSETINDEX_H__
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Strings/SIROffsetString.h>

// Host-endian Elias-Fano index over a monotonic array of uint32_t boundaries.
// Storage spans must be independent, exclusive during build and immutable thereafter.
typedef struct sirOffsetIndexLayout_s
{
	uint32_t nLowWords;
	uint32_t nHighWords;
	uint32_t nSamples;
	uint32_t nLowBits;
} sirOffsetIndexLayout_t;

typedef struct sirOffsetIndexStorage_s
{
	uint64_t *pLow;
	uint64_t *pHigh;
	uint32_t *pSamples;
	sirOffsetIndexLayout_t capacity;
} sirOffsetIndexStorage_t;

class SIROffsetIndex final
{
public:
	explicit SIROffsetIndex() noexcept = default;
	explicit SIROffsetIndex( const SIROffsetIndex& ) noexcept = default;
	explicit SIROffsetIndex( SIROffsetIndex&& ) noexcept = default;
	~SIROffsetIndex() noexcept = default;
	SIROffsetIndex& operator=( const SIROffsetIndex& ) noexcept = default;
	SIROffsetIndex& operator=( SIROffsetIndex&& ) noexcept = default;
	[[nodiscard]] static SIRContainerResult tryGetLayout( uint32_t nCount, uint32_t nMaximum, sirOffsetIndexLayout_t& layout ) noexcept;
	[[nodiscard]] SIRContainerResult tryBuild( const uint32_t *pOffsets, uint32_t nCount, const sirOffsetIndexStorage_t& storage ) noexcept;
	[[nodiscard]] SIRContainerResult tryGet( uint32_t iOffset, uint32_t& nOffset ) const noexcept;
	[[nodiscard]] SIRContainerResult tryResolve( uint32_t iString, sirStringView_t table, sirStringView_t& output ) const noexcept;

private:
	static constexpr uint32_t SAMPLE_STRIDE = 32;
	[[nodiscard]] static uint32_t countBits( uint64_t nWord ) noexcept;
	[[nodiscard]] static uint32_t firstBit( uint64_t nWord ) noexcept;
	[[nodiscard]] uint32_t select( uint32_t iOffset ) const noexcept;
	const uint64_t *m_pLow{};
	const uint64_t *m_pHigh{};
	const uint32_t *m_pSamples{};
	uint32_t m_nCount{};
	uint32_t m_nLowBits{};
	uint32_t m_nHighWords{};
};
#endif // __SIR_OFFSETINDEX_H__
