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

#ifndef __SIRENGINE_SIRFILESYSTEM_H__
#define __SIRENGINE_SIRFILESYSTEM_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

constexpr size_t SIR_FILE_SYSTEM_MAX_PATH_BYTES = 4096;

typedef enum sirFileSystemResult_e : uint8_t
{
	SIR_FILE_SYSTEM_OK = 0,
	SIR_FILE_SYSTEM_INVALID_ARGUMENT = 1,
	SIR_FILE_SYSTEM_INVALID_PATH = 2,
	SIR_FILE_SYSTEM_TEMP_EXISTS = 3,
	SIR_FILE_SYSTEM_IO_ERROR = 4,
	SIR_FILE_SYSTEM_REPLACE_FAILED = 5,
	SIR_FILE_SYSTEM_COMMITTED_NOT_DURABLE = 6,
	SIR_FILE_SYSTEM_CONFLICT = 7
} sirFileSystemResult_t;

typedef struct sirFileSystemAtomicWriteRequest_s
{
	const char *pszDestination;
	const char *pszTemporary;
	const char *pBytes;
	size_t nBytes;
	const char *pExpectedBytes = nullptr;
	size_t nExpectedBytes = 0;
} sirFileSystemAtomicWriteRequest_t;

// Platform-owned filesystem operations. The caller owns buffers and temporary
// pathname. Optional expected bytes protect against stale saves; a writer
// racing the final comparison still requires external file coordination.
class SIRFileSystem
{
public:
	SIRFileSystem( void ) = delete;
	~SIRFileSystem( void ) = delete;

	static sirFileSystemResult_t writeAtomically( const sirFileSystemAtomicWriteRequest_t &request ) noexcept;

protected:
private:
	static const char *lastSeparator( const char *pszPath ) noexcept;
	static bool validPaths( const char *pszDestination, const char *pszTemporary ) noexcept;
	static sirFileSystemResult_t writeAtomicallyPlatform( const sirFileSystemAtomicWriteRequest_t &request ) noexcept;
};

#endif // __SIRENGINE_SIRFILESYSTEM_H__
