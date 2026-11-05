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

#ifndef __SIRENGINE_SIRINI_H__
#define __SIRENGINE_SIRINI_H__

#include <SIRPCH.hpp>
#include <Core/Config/SIRConfigTypes.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

constexpr uint32_t SIR_INI_MAX_KEY_BYTES = 96;
constexpr uint32_t SIR_INI_MAX_SECTION_BYTES = 48;

typedef struct sirIniEntry_s
{
	char szSection[SIR_INI_MAX_SECTION_BYTES + 1];
	char szKey[SIR_INI_MAX_KEY_BYTES + 1];
	char *pszValue;
	uint32_t nValueBytes;
	uint32_t nLine;
	bool bQuoted;
} sirIniEntry_t;

typedef struct sirIniStorage_s
{
	sirIniEntry_t *pEntries;
	uint32_t nEntryCapacity;
	char *pszValues;
	uint32_t nValueStride;
	size_t nValueCapacity;
} sirIniStorage_t;

// Canonical INI format codec owned by Core/Config, never by the console parser.
// Every parsed value is copied into caller-owned storage. Parsing never executes an entry.
class SIRIniCodec
{
public:
	SIRIniCodec( void ) = delete;
	~SIRIniCodec( void ) = delete;

	static sirConfigResult_t parse( const sirConfigInput_t& input, const sirConfigLimits_t& limits, const sirIniStorage_t& storage,
	                                uint32_t *pCount, sirConfigDiagnostic_t *pDiagnostic ) noexcept;
	static sirConfigResult_t write( const sirIniEntry_t *pEntries, uint32_t nCount, const sirConfigSink_t& sink,
	                                uint64_t nMaxBytes ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRINI_H__
