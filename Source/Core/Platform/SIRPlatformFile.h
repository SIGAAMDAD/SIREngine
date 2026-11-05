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

#ifndef __SIRENGINE_SIRPLATFORMFILE_H__
#define __SIRENGINE_SIRPLATFORMFILE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirFileResult_e : uint8_t
{
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
} sirFileResult_t;

typedef enum sirFileOpenFlag_e : uint32_t
{
	SIR_FILE_OPEN_READ = 1u << 0,
	SIR_FILE_OPEN_WRITE = 1u << 1,
	SIR_FILE_OPEN_CREATE = 1u << 2,
	SIR_FILE_OPEN_TRUNCATE = 1u << 3,
	SIR_FILE_OPEN_APPEND = 1u << 4,
	SIR_FILE_OPEN_EXCLUSIVE = 1u << 5
} sirFileOpenFlag_t;

typedef enum sirFileSeekOrigin_e : uint8_t
{
	SIR_FILE_SEEK_BEGIN = 0,
	SIR_FILE_SEEK_CURRENT = 1,
	SIR_FILE_SEEK_END = 2
} sirFileSeekOrigin_t;

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
private:
	friend class SIRPlatformFileMapping;
	friend class SIRPlatformRootDirectory;
	uintptr_t m_nNativeHandle;
	uint64_t m_nCursor;
	uint32_t m_nFlags;

public:
	SIRPlatformFile( void ) noexcept;
	~SIRPlatformFile( void ) noexcept;
	SIRPlatformFile( const SIRPlatformFile& ) = delete;
	SIRPlatformFile& operator=( const SIRPlatformFile& ) = delete;
	SIRPlatformFile( SIRPlatformFile&& ) = delete;
	SIRPlatformFile& operator=( SIRPlatformFile&& ) = delete;

	sirFileResult_t open( const char *pszUtf8Path, uint32_t nFlags ) noexcept;
	sirFileResult_t close( void ) noexcept;
	bool isOpen( void ) const noexcept;
	sirFileResult_t read( void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	sirFileResult_t write( const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) noexcept;
	sirFileResult_t readAt( uint64_t nOffset, void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) const noexcept;
	sirFileResult_t writeAt( uint64_t nOffset, const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) const noexcept;
	sirFileResult_t seek( int64_t nOffset, sirFileSeekOrigin_t eOrigin, uint64_t *pPosition ) noexcept;
	sirFileResult_t tell( uint64_t *pPosition ) const noexcept;
	sirFileResult_t getSize( uint64_t *pBytes ) const noexcept;
	sirFileResult_t flush( void ) const noexcept;
	sirFileResult_t truncate( uint64_t nBytes ) noexcept;

	static sirFileResult_t getInfo( const char *pszUtf8Path, sirFileInfo_t *pInfo ) noexcept;
	static sirFileResult_t remove( const char *pszUtf8Path ) noexcept;
	static sirFileResult_t renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMFILE_H__
