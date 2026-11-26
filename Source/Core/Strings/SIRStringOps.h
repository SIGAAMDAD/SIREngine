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

#ifndef __SIR_STRINGOPS_H__
#define __SIR_STRINGOPS_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Shared checked byte-range operations. A null pointer is valid only for an empty view.
class SIRStringOps final
{
public:
	explicit SIRStringOps() = delete;
	explicit SIRStringOps( const SIRStringOps& ) = delete;
	explicit SIRStringOps( SIRStringOps&& ) = delete;
	~SIRStringOps() = delete;
	SIRStringOps& operator=( const SIRStringOps& ) = delete;
	SIRStringOps& operator=( SIRStringOps&& ) = delete;

	[[nodiscard]] static bool isValid( sirStringView_t view ) noexcept;
	// SIRS v1 codec 0 comparison: query doubles as verified LZ history.
	[[nodiscard]] static bool matchesCookedLZ( sirStringView_t query, const uint8_t *pEncoded, uint32_t nEncoded ) noexcept;
	[[nodiscard]] static bool equals( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static uint32_t findByte( sirStringView_t input, char nByte ) noexcept;
	[[nodiscard]] static uint32_t find( sirStringView_t input, sirStringView_t pattern ) noexcept;
	[[nodiscard]] static bool overlaps( sirStringView_t left, sirStringView_t right ) noexcept;
	[[nodiscard]] static SIRContainerResult copyTerminated( sirStringView_t view, char *pszOutput, uint32_t nCapacity ) noexcept;

private:
	[[nodiscard]] static uint32_t maximalSuffix( sirStringView_t pattern, bool bReverse, uint32_t& nPeriod ) noexcept;
};

#endif // __SIR_STRINGOPS_H__
