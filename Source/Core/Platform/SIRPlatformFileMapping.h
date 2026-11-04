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

#ifndef __SIRENGINE_SIRPLATFORMFILEMAPPING_H__
#define __SIRENGINE_SIRPLATFORMFILEMAPPING_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRPlatformFile.h>

typedef enum sirFileMapMode_e : uint8_t
{
	SIR_FILE_MAP_READ_ONLY = 0,
	SIR_FILE_MAP_READ_WRITE = 1,
	SIR_FILE_MAP_COPY_ON_WRITE = 2
} sirFileMapMode_t;

// A single OS mapping. No buffer copies or internal heap allocations.
// Source file must remain open while map() runs, but may close afterward.
// The file must not be truncated while any mapping accesses its pages.
// Requested offsets are arbitrary; the OS view is aligned internally.
// nBytes == 0 maps the remainder of the current file; empty views are rejected.
// A read-write map changes the file; copy-on-write changes remain private.
// Reclamation is by unmap() or destruction, never through the GRDG allocator.
class SIRPlatformFileMapping final
{
private:
	void *m_pBase;
	uint8_t *m_pData;
	uint64_t m_nMappedBytes;
	uint64_t m_nViewOffset;
	uint64_t m_nViewBytes;
#if defined( _WIN32 )
	uintptr_t m_nNativeMapping;
#endif
	sirFileMapMode_t m_eMode;

public:
	SIRPlatformFileMapping( void ) noexcept;
	~SIRPlatformFileMapping( void ) noexcept;
	SIRPlatformFileMapping( const SIRPlatformFileMapping & ) = delete;
	SIRPlatformFileMapping &operator=( const SIRPlatformFileMapping & ) = delete;
	SIRPlatformFileMapping( SIRPlatformFileMapping && ) = delete;
	SIRPlatformFileMapping &operator=( SIRPlatformFileMapping && ) = delete;

	sirFileResult_t map( const SIRPlatformFile &rFile, uint64_t nOffset, uint64_t nBytes, sirFileMapMode_t eMode ) noexcept;
	sirFileResult_t unmap( void ) noexcept;
	sirFileResult_t flush( void ) const noexcept;
	bool isMapped( void ) const noexcept;
	const uint8_t *getData( void ) const noexcept;
	uint8_t *getWritableData( void ) noexcept;
	uint64_t getSize( void ) const noexcept;
	uint64_t getOffset( void ) const noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMFILEMAPPING_H__
