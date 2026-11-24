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

#ifndef __SIR_PLATFORMFILESYSTEM_H__
#define __SIR_PLATFORMFILESYSTEM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/File/SIRPlatformFile.h>
#include <stdint.h>
#include <stddef.h>

enum class sirFileSystemResult : uint8_t {
	SIR_FILESYSTEM_OK = 0,
	SIR_FILESYSTEM_INVALID_ARGUMENT = 1,
	SIR_FILESYSTEM_NOT_FOUND = 2,
	SIR_FILESYSTEM_ALREADY_EXISTS = 3,
	SIR_FILESYSTEM_ACCESS_DENIED = 4,
	SIR_FILESYSTEM_BUFFER_TOO_SMALL = 5,
	SIR_FILESYSTEM_STOPPED = 6,
	SIR_FILESYSTEM_IO_ERROR = 7,
	SIR_FILESYSTEM_INVALID_PATH = 8,
	SIR_FILESYSTEM_OUT_OF_CAPACITY = 9
};

enum class sirFileSystemEntryType : uint8_t {
	SIR_FILESYSTEM_REGULAR_FILE = 0,
	SIR_FILESYSTEM_DIRECTORY = 1,
	SIR_FILESYSTEM_SYMLINK = 2,
	SIR_FILESYSTEM_OTHER = 3
};

typedef struct sirFileSystemInfo_s
{
	uint64_t nSizeBytes;
	uint64_t nModifiedNanoseconds;
	sirFileSystemEntryType eType;
} sirFileSystemInfo_t;

// Save operation results preserve precommit/committed states distinctly
// from generic filesystem query and read outcomes.
enum class sirPlatformAtomicResult : uint8_t {
	SIR_ATOMIC_SAVE_OK = 0,
	SIR_ATOMIC_SAVE_INVALID_ARGUMENT = 1,
	SIR_ATOMIC_SAVE_INVALID_PATH = 2,
	SIR_ATOMIC_SAVE_TEMP_EXISTS = 3,
	SIR_ATOMIC_SAVE_IO_ERROR = 4,
	SIR_ATOMIC_SAVE_REPLACE_FAILED = 5,
	SIR_ATOMIC_SAVE_COMMITTED_NOT_DURABLE = 6,
	SIR_ATOMIC_SAVE_CONFLICT = 7,
	SIR_ATOMIC_SAVE_OUT_OF_CAPACITY = 8
};

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
	explicit SIRPlatformFileSystem( void ) = delete;
	explicit SIRPlatformFileSystem( const SIRPlatformFileSystem& ) = delete;
	explicit SIRPlatformFileSystem( SIRPlatformFileSystem&& ) = delete;
	~SIRPlatformFileSystem() = delete;

	SIRPlatformFileSystem& operator=( const SIRPlatformFileSystem& ) = delete;
	SIRPlatformFileSystem& operator=( SIRPlatformFileSystem&& ) = delete;

	[[nodiscard]] static sirFileResult getFileInfo( const char *pszUtf8Path, sirFileInfo_t *pInfo ) noexcept;
	[[nodiscard]] static sirFileResult removeFile( const char *pszUtf8Path ) noexcept;
	[[nodiscard]] static sirFileResult renameFileReplace( const char *pszOldPath, const char *pszNewPath ) noexcept;
	[[nodiscard]] static sirFileSystemResult query( const char *pszPath, sirFileSystemInfo_t *pInfo ) noexcept;
	[[nodiscard]] static sirFileSystemResult createDirectory( const char *pszPath, bool bRecursive ) noexcept;
	[[nodiscard]] static sirFileSystemResult removeDirectory( const char *pszPath ) noexcept;
	[[nodiscard]] static sirFileSystemResult renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept;
	[[nodiscard]] static sirFileSystemResult enumerate( const char *pszDirectory, sirFileSystemVisitor_t pVisitor, void *pContext ) noexcept;
	[[nodiscard]] static sirFileSystemResult readFile( const char *pszPath, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept;
	[[nodiscard]] static sirPlatformAtomicResult writeAtomically( const sirFileSystemAtomicWriteRequest_t& request ) noexcept;

private:
	static const char *lastSeparator( const char *pszPath ) noexcept;
	static bool validPaths( const char *pszDestination, const char *pszTemporary ) noexcept;
	[[nodiscard]] static sirFileSystemResult readFilePlatform( const char *pszPath, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept;
	[[nodiscard]] static sirPlatformAtomicResult writeAtomicallyPlatform( const sirFileSystemAtomicWriteRequest_t& request ) noexcept;
};

#endif // __SIR_PLATFORMFILESYSTEM_H__
