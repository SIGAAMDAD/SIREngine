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

#ifndef __SIR_PLATFORMFILE_H__
#define __SIR_PLATFORMFILE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class sirFileResult : uint8_t {
	SIR_FILE_OK = 0,
	SIR_FILE_END_OF_FILE = 1,
	SIR_FILE_CLOSED = 2,
	SIR_FILE_INVALID_ARGUMENT = 3,
	SIR_FILE_OUT_OF_RANGE = 4,
	SIR_FILE_NOT_FOUND = 5,
	SIR_FILE_ALREADY_EXISTS = 6,
	SIR_FILE_ACCESS_DENIED = 7,
	SIR_FILE_NO_SPACE = 8,
	SIR_FILE_IO_ERROR = 9
};

enum class sirFileOpenFlag : uint32_t {
	SIR_FILE_OPEN_READ = 1u << 0,
	SIR_FILE_OPEN_WRITE = 1u << 1,
	SIR_FILE_OPEN_CREATE = 1u << 2,
	SIR_FILE_OPEN_TRUNCATE = 1u << 3,
	SIR_FILE_OPEN_APPEND = 1u << 4,
	SIR_FILE_OPEN_EXCLUSIVE = 1u << 5
};

enum class sirFileSeekOrigin : uint8_t {
	SIR_FILE_SEEK_BEGIN = 0,
	SIR_FILE_SEEK_CURRENT = 1,
	SIR_FILE_SEEK_END = 2
};

typedef struct sirFileInfo_s
{
	uint64_t nSizeBytes;
	uint64_t nModifiedUnixNanoseconds;
	bool bRegularFile;
	bool bDirectory;
} sirFileInfo_t;

// Unbuffered replacement for stdio file operations. UTF-8 paths on all targets.
// Ownership: one object, one OS handle, no internal payload allocations.
// A sequential cursor is local to this object. Only positional readAt/writeAt
// may run concurrently on a stable file; cursor ops and close require exclusion.
// A read reports SIR_FILE_END_OF_FILE and the transferred count on short input.
// Append writes use the OS atomic-append mode; writeAt is rejected in that mode.
class SIRPlatformFile final
{
public:
	explicit SIRPlatformFile( void ) noexcept;
	explicit SIRPlatformFile( const SIRPlatformFile& ) = delete;
	explicit SIRPlatformFile( SIRPlatformFile&& other ) noexcept;
	~SIRPlatformFile() noexcept;

	SIRPlatformFile& operator=( const SIRPlatformFile& ) = delete;
	SIRPlatformFile& operator=( SIRPlatformFile&& ) = delete;


	[[nodiscard]] sirFileResult open( const char *pszUtf8Path, uint32_t nFlags ) noexcept;
	[[nodiscard]] sirFileResult close( void ) noexcept;
	bool isOpen( void ) const noexcept;
	[[nodiscard]] sirFileResult read( void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	[[nodiscard]] sirFileResult write( const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	[[nodiscard]] sirFileResult readAt( uint64_t nOffset, void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) const noexcept;
	[[nodiscard]] sirFileResult writeAt( uint64_t nOffset, const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) const noexcept;
	[[nodiscard]] sirFileResult seek( int64_t nOffset, sirFileSeekOrigin eOrigin, uint64_t *pPosition ) noexcept;
	[[nodiscard]] sirFileResult tell( uint64_t *pPosition ) const noexcept;
	[[nodiscard]] sirFileResult getSize( uint64_t *pBytes ) const noexcept;
	[[nodiscard]] sirFileResult flush( void ) const noexcept;
	[[nodiscard]] sirFileResult truncate( uint64_t nBytes ) noexcept;

private:
	friend class SIRPlatformFileMapping;
	friend class SIRPlatformRootDirectory;
	uintptr_t m_nNativeHandle;
	uint64_t m_nCursor;
	uint32_t m_nFlags;
};

#endif // __SIR_PLATFORMFILE_H__
