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

#ifndef __SIR_PACK_SHARD_H__
#define __SIR_PACK_SHARD_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/File/SIRPlatformFile.h>
#include <Streaming/Pack/SIRPackFormat.h>

class SIRPlatformRootDirectory;

typedef struct sirPackShardOpenDesc_s
{
	SIRPlatformRootDirectory *pRoot;
	const char *pszPath;
	sirPackShard_t expected;
	uint64_t nPackageId;
	uint64_t nGeneration;
	void *pValidationScratch;
	uint64_t nValidationScratchBytes;
} sirPackShardOpenDesc_t;

class SIRPackShard final
{
public:
	SIRPackShard( void ) noexcept;
	SIRPackShard( const SIRPackShard& ) noexcept = delete;
	SIRPackShard( SIRPackShard&& ) noexcept = delete;
	~SIRPackShard() noexcept;

	SIRPackShard& operator=( const SIRPackShard& ) noexcept = delete;
	SIRPackShard& operator=( sirPackShard_t&& ) noexcept = delete;

	bool isOpen( void ) const noexcept;
	uint32_t getIndex( void ) const noexcept;
	uint64_t getSize( void ) const noexcept;

	SIRPackResult open( const sirPackShardOpenDesc_t& desc ) noexcept;
	SIRPackResult close( void ) noexcept;
	SIRPackResult readAt( uint64_t nOffset, void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) const noexcept;
	SIRPackResult validateRange( uint64_t nOffset, uint64_t nBytes ) const noexcept;

private:
	SIRPlatformFile m_File;
	uint64_t m_nSize;
	uint32_t m_iShard;
	bool m_bOpen;
};

#endif // __SIR_PACK_SHARD_H__
