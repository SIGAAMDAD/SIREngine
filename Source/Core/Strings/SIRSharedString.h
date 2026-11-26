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

#ifndef __SIR_SHAREDSTRING_H__
#define __SIR_SHAREDSTRING_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef struct sirSharedStringBlock_s sirSharedStringBlock_t;
typedef struct sirSharedStringHeader_s sirSharedStringHeader_t;
typedef struct sirSharedStringSlabHeader_s sirSharedStringSlabHeader_t;

// One explicit allocator-domain slab. Retiring the owner stops allocations;
// live immutable strings keep the slab resident until the last release.
// Initialization/reset of the same owner require external exclusion.
class SIRSharedStringSlab final
{
public:
	explicit SIRSharedStringSlab() noexcept = default;
	explicit SIRSharedStringSlab( const SIRSharedStringSlab& ) = delete;
	explicit SIRSharedStringSlab( SIRSharedStringSlab&& ) = delete;
	~SIRSharedStringSlab() noexcept;
	SIRSharedStringSlab& operator=( const SIRSharedStringSlab& ) = delete;
	SIRSharedStringSlab& operator=( SIRSharedStringSlab&& ) = delete;
	[[nodiscard]] SIRContainerResult tryInitialize( const sirContainerAllocator_t *pAllocator, uint32_t nPayloadCapacity ) noexcept;
	[[nodiscard]] uint32_t getPayloadCapacity( void ) const noexcept;
	void reset( void ) noexcept;

private:
	friend class SIRSharedString;
	static constexpr uint32_t SLAB_BYTES = 65536;
	[[nodiscard]] SIRContainerResult allocate( sirStringView_t view, sirSharedStringHeader_t *& pHeader ) noexcept;
	[[nodiscard]] static sirSharedStringSlabHeader_t *ownerOf( const sirSharedStringHeader_t *pHeader ) noexcept;
	static void release( sirSharedStringHeader_t *pHeader ) noexcept;
	static void destroy( sirSharedStringSlabHeader_t *pSlab ) noexcept;
	sirSharedStringSlabHeader_t *m_pSlab{};
};

// Immutable, allocator-domain-owned storage. A share acquires one atomic reference.
// The allocator descriptor and its context must outlive all shares. Distinct handles
// may be used on different threads; mutation of the same handle requires synchronization.
class SIRSharedString final
{
public:
	explicit SIRSharedString() noexcept = default;
	explicit SIRSharedString( const SIRSharedString& ) = delete;
	explicit SIRSharedString( SIRSharedString&& other ) noexcept;
	~SIRSharedString() noexcept;
	SIRSharedString& operator=( const SIRSharedString& ) = delete;
	SIRSharedString& operator=( SIRSharedString&& other ) noexcept;

	[[nodiscard]] sirStringView_t getView( void ) const noexcept;
	[[nodiscard]] const char *cStr( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryAssign( const sirContainerAllocator_t *pAllocator, sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAssign( SIRSharedStringSlab& slab, sirStringView_t view ) noexcept;
	// Descriptor and its null-terminated payload must have static lifetime.
	[[nodiscard]] SIRContainerResult tryBindStatic( const sirStringView_t& staticView ) noexcept;
	SIRContainerResult tryBindStatic( const sirStringView_t&& ) = delete;
	[[nodiscard]] SIRContainerResult tryShare( const SIRSharedString& other ) noexcept;
	void reset( void ) noexcept;

private:
	[[nodiscard]] bool isStatic( void ) const noexcept;
	[[nodiscard]] bool isSlab( void ) const noexcept;
	[[nodiscard]] sirSharedStringHeader_t *getHeader( void ) const noexcept;
	[[nodiscard]] sirSharedStringBlock_t *getBlock( void ) const noexcept;
	// Low two bits: 0 heap, 1 static view, 2 slab slot.
	uintptr_t m_nStorage{};
};

#endif // __SIR_SHAREDSTRING_H__
