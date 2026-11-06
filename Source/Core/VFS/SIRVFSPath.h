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

#ifndef __SIRENGINE_SIRVFSPATH_H__
#define __SIRENGINE_SIRVFSPATH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

constexpr uint32_t SIR_VFS_MAX_PATH_BYTES = 1024;
constexpr uint32_t SIR_VFS_MAX_NAMESPACE_BYTES = 32;

typedef enum sirVFSPathResult_e : uint8_t
{
	SIR_VFS_PATH_OK = 0,
	SIR_VFS_PATH_INVALID_ARGUMENT,
	SIR_VFS_PATH_INVALID_NAMESPACE,
	SIR_VFS_PATH_INVALID_ENCODING,
	SIR_VFS_PATH_FORBIDDEN,
	SIR_VFS_PATH_TOO_LONG,
	SIR_VFS_PATH_INVALID_SEGMENT
} sirVFSPathResult_t;

typedef struct sirVFSPath_s
{
	char szPath[SIR_VFS_MAX_PATH_BYTES];
	uint16_t nLength;
	uint8_t nNamespaceLength;
} sirVFSPath_t;

// Logical path syntax: namespace:/relative/path. No native path opens or
// implicit URI decoding occur here. The mount backend enforces OS root safety.
class SIRVFSPath final
{
public:
	SIRVFSPath( void ) = delete;
	~SIRVFSPath( void ) = delete;

	static sirVFSPathResult_t normalize( const char *pszInput, uint32_t nInputBytes, sirVFSPath_t *pOutput ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRVFSPATH_H__
