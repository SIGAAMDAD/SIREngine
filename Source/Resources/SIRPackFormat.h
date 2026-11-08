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

#ifndef __SIRENGINE_SIRPACKFORMAT_H__
#define __SIRENGINE_SIRPACKFORMAT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceTypes.h>

constexpr uint16_t SIR_PACK_VERSION = 1;
constexpr uint32_t SIR_PACK_ENDIAN_MARKER = 0x01020304U;
constexpr uint64_t SIR_PACK_HEADER_BYTES = 128;
constexpr uint64_t SIR_PACK_SHARD_HEADER_BYTES = 64;
constexpr uint64_t SIR_PACK_SHARD_RECORD_BYTES = 64;
constexpr uint64_t SIR_PACK_RESOURCE_RECORD_BYTES = 56;
constexpr uint64_t SIR_PACK_CHUNK_RECORD_BYTES = 80;
constexpr uint64_t SIR_PACK_DEPENDENCY_RECORD_BYTES = 24;
constexpr uint32_t SIR_PACK_MAX_SHARDS = 4096;
constexpr uint32_t SIR_PACK_MAX_RESOURCES = 1048576;
constexpr uint32_t SIR_PACK_MAX_CHUNKS = 4194304;
constexpr uint64_t SIR_PACK_MAX_DECODED_CHUNK_BYTES = 1ULL << 30;

typedef enum sirPackResult_e : uint8_t
{
	SIR_PACK_OK = 0,
	SIR_PACK_INVALID_ARGUMENT,
	SIR_PACK_INVALID_FORMAT,
	SIR_PACK_UNSUPPORTED_VERSION,
	SIR_PACK_TRUNCATED,
	SIR_PACK_BAD_DIGEST,
	SIR_PACK_OUT_OF_CAPACITY,
	SIR_PACK_NOT_FOUND,
	SIR_PACK_UNSUPPORTED_CODEC,
	SIR_PACK_CYCLIC_DEPENDENCY,
	SIR_PACK_STALE_SOURCE
} sirPackResult_t;

typedef enum sirPackCodec_e : uint16_t
{
	SIR_PACK_CODEC_NONE = 0,
	SIR_PACK_CODEC_ZSTD = 1,
	SIR_PACK_CODEC_LZ4 = 2
} sirPackCodec_t;

typedef struct sirPackResource_s
{
	SIRResourceKey key;
	uint64_t nRevision;
	uint64_t nLogicalBytes;
	uint32_t nFormatId;
	uint32_t nFirstChunk;
	uint32_t nChunkCount;
	uint32_t nFirstDependency;
	uint32_t nDependencyCount;
	uint16_t nSchemaVersion;
	uint8_t nKind;
} sirPackResource_t;

typedef struct sirPackChunk_s
{
	uint32_t nChunkId;
	uint32_t nShardIndex;
	uint64_t nOffset;
	uint64_t nEncodedBytes;
	uint64_t nDecodedBytes;
	uint8_t szDigest[32];
	sirPackCodec_t eCodec;
	uint16_t nCodecVersion;
	uint32_t nAlignment;
	uint32_t nDictionaryId;
} sirPackChunk_t;

typedef struct sirPackDependency_s
{
	SIRResourceKey key;
	sirResourceEdgeKind_t eKind;
} sirPackDependency_t;

typedef struct sirPackShard_s
{
	uint32_t nIndex;
	uint64_t nBytes;
	uint8_t szDigest[32];
} sirPackShard_t;

// Wire bytes are encoded and decoded field-by-field. No compiler record layout
// or raw memory address is part of the persistent format.
uint16_t sirPackRead16( const uint8_t *pBytes ) noexcept;
uint32_t sirPackRead32( const uint8_t *pBytes ) noexcept;
uint64_t sirPackRead64( const uint8_t *pBytes ) noexcept;
void sirPackWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept;
void sirPackWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept;
void sirPackWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept;
void sirPackSha256( const uint8_t *pBytes, uint64_t nBytes, uint8_t *pDigest ) noexcept;
void sirPackCatalogDigest( const uint8_t *pBytes, uint64_t nBytes, uint8_t *pDigest ) noexcept;

#endif // __SIRENGINE_SIRPACKFORMAT_H__
