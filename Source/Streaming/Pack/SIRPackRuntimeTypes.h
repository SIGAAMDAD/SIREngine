/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://www.mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIR_PACKRUNTIMETYPES_H__
#define __SIR_PACKRUNTIMETYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Streaming/Pack/SIRPackFormat.h>
#include <Core/VFS/SIRVFSMountTable.h>

typedef struct sirPackSourceId_s
{
	uint64_t nPackageId;
	uint64_t nPackageGeneration;
	uint64_t nSourceRevision;
	sirVFSMountHandle_t mount;
} sirPackSourceId_t;

typedef struct sirPackChunkSource_s
{
	sirPackSourceId_t source;
	SIRResourceKey key;
	uint64_t nCookedRevision;
	uint32_t iCatalogChunk;
	sirPackChunk_t chunk;
	uint32_t nFormatId;
	uint16_t nSchemaVersion;
	uint8_t nKind;
} sirPackChunkSource_t;

typedef enum sirPackRuntimeResult_e : uint8_t
{
	SIR_PACK_RUNTIME_OK = 0,
	SIR_PACK_RUNTIME_INVALID_ARGUMENT,
	SIR_PACK_RUNTIME_BUSY,
	SIR_PACK_RUNTIME_IO_ERROR,
	SIR_PACK_RUNTIME_CANCELLED,
	SIR_PACK_RUNTIME_NOT_READY,
	SIR_PACK_RUNTIME_FORBIDDEN,
	SIR_PACK_RUNTIME_STALE_SOURCE,
	SIR_PACK_RUNTIME_OUT_OF_CAPACITY,
	SIR_PACK_RUNTIME_NOT_FOUND
} sirPackRuntimeResult_t;

typedef struct sirPackLookup_s
{
	sirVFSPath_t nameSpace;
	SIRResourceKey key;
} sirPackLookup_t;

#endif // __SIR_PACKRUNTIMETYPES_H__
