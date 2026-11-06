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

#ifndef __SIRENGINE_SIRWORLDHLODMANIFEST_H__
#define __SIRENGINE_SIRWORLDHLODMANIFEST_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <World/HLOD/SIRWorldHLOD.h>

// Byte-oriented, little-endian HLOD manifest version 1. Never cast wire
// buffers to C++ records. Manifest identity is supplied by the trusted world
// catalog: the source fingerprint is not derived from the untrusted manifest.
static constexpr uint32_t SIR_WORLD_HLOD_MANIFEST_MAGIC = UINT32_C( 0x444f4c48 ); // HLOD
static constexpr uint16_t SIR_WORLD_HLOD_MANIFEST_VERSION = 1;
static constexpr uint32_t SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES = 48;
static constexpr uint32_t SIR_WORLD_HLOD_MANIFEST_NODE_BYTES = 64;

typedef enum sirWorldHLODManifestResult_e : uint8_t
{
	SIR_WORLD_HLOD_MANIFEST_OK = 0,
	SIR_WORLD_HLOD_MANIFEST_INVALID_ARGUMENT = 1,
	SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT = 2,
	SIR_WORLD_HLOD_MANIFEST_STALE_SOURCE = 3,
	SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY = 4,
	SIR_WORLD_HLOD_MANIFEST_CAPACITY = 5
} sirWorldHLODManifestResult_t;

typedef struct sirWorldHLODManifestIdentity_s
{
	uint64_t nWorldId;
	uint64_t nSourceHashLo;
	uint64_t nSourceHashHi;
} sirWorldHLODManifestIdentity_t;

typedef struct sirWorldHLODManifestNode_s
{
	sirWorldHLODNode_t node;
	float fMinimum[3];
	float fMaximum[3];
	float fGeometricError;
} sirWorldHLODManifestNode_t;

typedef struct sirWorldHLODManifestBounds_s
{
	float fMinimum[3];
	float fMaximum[3];
} sirWorldHLODManifestBounds_t;

typedef struct sirWorldHLODManifestRead_s
{
	const uint8_t *pBytes;
	uint64_t nBytes;
	sirWorldHLODManifestIdentity_t expected;
	uint32_t nMaxNodes;
} sirWorldHLODManifestRead_t;

typedef struct sirWorldHLODManifestWrite_s
{
	const sirWorldHLODManifestNode_t *pNodes;
	uint32_t nNodeCount;
	sirWorldHLODManifestIdentity_t identity;
} sirWorldHLODManifestWrite_t;

typedef struct sirWorldHLODManifestDecode_s
{
	sirWorldHLODNode_t *pNodes;
	sirWorldHLODManifestBounds_t *pBounds;
	float *pGeometricErrors;
	uint32_t nNodeCapacity;
	sirWorldHLODScratch_t *pScratch;
	uint32_t nScratchCapacity;
	uint32_t *pnDecoded;
} sirWorldHLODManifestDecode_t;

typedef struct sirWorldHLODManifestEncode_s
{
	uint8_t *pBytes;
	uint64_t nCapacity;
	sirWorldHLODScratch_t *pScratch;
	uint32_t nScratchCapacity;
	uint64_t *pnWritten;
} sirWorldHLODManifestEncode_t;

// Source is immutable during either call. Both operations use caller-owned
// scratch and commit output only after full validation. Input/output storage
// must not alias. Parent bounds conservatively enclose every direct child.
sirWorldHLODManifestResult_t sirWorldHLODReadManifest( const sirWorldHLODManifestRead_t& input,
                                                       sirWorldHLODManifestDecode_t& output ) noexcept;
sirWorldHLODManifestResult_t sirWorldHLODWriteManifest( const sirWorldHLODManifestWrite_t& input,
                                                        sirWorldHLODManifestEncode_t& output ) noexcept;

#endif // __SIRENGINE_SIRWORLDHLODMANIFEST_H__
