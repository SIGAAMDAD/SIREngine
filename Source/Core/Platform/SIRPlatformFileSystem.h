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

#ifndef __SIRENGINE_SIRPLATFORMFILESYSTEM_H__
#define __SIRENGINE_SIRPLATFORMFILESYSTEM_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <stddef.h>

typedef enum sirFileSystemResult_e : uint8_t
{
	SIR_FILESYSTEM_OK = 0,
	SIR_FILESYSTEM_INVALID_ARGUMENT = 1,
	SIR_FILESYSTEM_NOT_FOUND = 2,
	SIR_FILESYSTEM_ALREADY_EXISTS = 3,
	SIR_FILESYSTEM_ACCESS_DENIED = 4,
	SIR_FILESYSTEM_BUFFER_TOO_SMALL = 5,
	SIR_FILESYSTEM_STOPPED = 6,
	SIR_FILESYSTEM_IO_ERROR = 7,
	SIR_FILESYSTEM_INVALID_PATH = 8,
	SIR_FILESYSTEM_TEMP_EXISTS = 9,
	SIR_FILESYSTEM_REPLACE_FAILED = 10,
	SIR_FILESYSTEM_COMMITTED_NOT_DURABLE = 11,
	SIR_FILESYSTEM_CONFLICT = 12,
	SIR_FILESYSTEM_OUT_OF_CAPACITY = 13
} sirFileSystemResult_t;

typedef enum sirFileSystemEntryType_e : uint8_t
{
	SIR_FILESYSTEM_REGULAR_FILE = 0,
	SIR_FILESYSTEM_DIRECTORY = 1,
	SIR_FILESYSTEM_SYMLINK = 2,
	SIR_FILESYSTEM_OTHER = 3
} sirFileSystemEntryType_t;

typedef struct sirFileSystemInfo_s
{
	uint64_t nSizeBytes;
	uint64_t nModifiedNanoseconds;
	sirFileSystemEntryType_t eType;
} sirFileSystemInfo_t;

// A sibling staging file is exclusively created, synced, then atomically
// published with an optional optimistic comparison of expected prior bytes.
constexpr size_t SIR_FILESYSTEM_MAX_PATH_BYTES = 4096;

typedef struct sirFileSystemAtomicWriteRequest_s
{
	const char *pszDestination;
	const char *pszTemporary;
	const char *pBytes;
	size_t nBytes;
	const char *pExpectedBytes = nullptr;
	size_t nExpectedBytes = 0;
} sirFileSystemAtomicWriteRequest_t;

// Callback runs synchronously. Name points at temporary storage valid only
// during this invocation. Return false to stop enumeration intentionally.
typedef bool ( *sirFileSystemVisitor_t )( const char *pszName, const sirFileSystemInfo_t *pInfo, void *pContext ) noexcept;

class SIRPlatformFileSystem
{
public:
	SIRPlatformFileSystem( void ) = delete;
	~SIRPlatformFileSystem( void ) = delete;

	static sirFileSystemResult_t query( const char *pszPath, sirFileSystemInfo_t *pInfo ) noexcept;
	static sirFileSystemResult_t createDirectory( const char *pszPath, bool bRecursive ) noexcept;
	static sirFileSystemResult_t removeDirectory( const char *pszPath ) noexcept;
	static sirFileSystemResult_t renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept;
	static sirFileSystemResult_t enumerate( const char *pszDirectory, sirFileSystemVisitor_t pVisitor, void *pContext ) noexcept;
	static sirFileSystemResult_t readFile( const char *pszPath, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept;
	static sirFileSystemResult_t writeAtomically( const sirFileSystemAtomicWriteRequest_t& request ) noexcept;

protected:
private:
	static const char *lastSeparator( const char *pszPath ) noexcept;
	static bool validPaths( const char *pszDestination, const char *pszTemporary ) noexcept;
	static sirFileSystemResult_t readFilePlatform( const char *pszPath, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept;
	static sirFileSystemResult_t writeAtomicallyPlatform( const sirFileSystemAtomicWriteRequest_t& request ) noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMFILESYSTEM_H__
