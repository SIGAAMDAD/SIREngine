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

#ifndef __SIR_COOKED_STRING_INDEX_H__
#define __SIR_COOKED_STRING_INDEX_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Strings/SIRStringID.h>

// Static displacement index: one ordinal per key, no empty probe slots.
// All arrays are independently sized, non-overlapping, caller-owned spans.
// Entries are 32-bit host-endian values, not a persisted wire format.
typedef struct sirCookedStringIndexStorage_s
{
	uint32_t *pDisplacements;
	uint32_t *pOrdinals;
	uint32_t nBuckets;
	uint32_t nKeys;
} sirCookedStringIndexStorage_t;

typedef struct sirCookedStringIndexScratch_s
{
	uint32_t *pHeads; // nBuckets
	uint32_t *pCounts; // nBuckets
	uint32_t *pNext; // nKeys
} sirCookedStringIndexScratch_t;

// Offline index build and exact membership verification for an immutable key table.
// Build failure invalidates the supplied index; caller may retry with another seed.
// Returned ordinal is the input key ordinal, not a process-global string atom ID.
class SIRCookedStringIndex final
{
public:
	explicit SIRCookedStringIndex( void ) = delete;
	explicit SIRCookedStringIndex( const SIRCookedStringIndex& ) = delete;
	explicit SIRCookedStringIndex( SIRCookedStringIndex&& ) = delete;
	~SIRCookedStringIndex() = delete;

	SIRCookedStringIndex& operator=( const SIRCookedStringIndex& ) = delete;
	SIRCookedStringIndex& operator=( SIRCookedStringIndex&& ) = delete;

	[[nodiscard]] static SIRContainerResult tryBuild( const sirStringView_t *pKeys,
	                                                  const sirCookedStringIndexStorage_t& storage,
	                                                  const sirCookedStringIndexScratch_t& scratch,
	                                                  uint64_t nSeed ) noexcept;
	[[nodiscard]] static uint32_t
	find( sirStringView_t key, const sirStringView_t *pKeys, const sirCookedStringIndexStorage_t& storage, uint64_t nSeed ) noexcept;

	// Optional SIRS frozen region: SMP1 magic, key count, bucket count, seed,
	// then little-endian uint32 displacement and ordinal arrays. No native casts.
	[[nodiscard]] static uint64_t getWireBytes( uint32_t nKeys, uint32_t nBuckets ) noexcept;
	[[nodiscard]] static SIRContainerResult
	trySerialize( const sirCookedStringIndexStorage_t& storage, uint64_t nSeed, uint8_t *pBytes, uint32_t nCapacity ) noexcept;
	[[nodiscard]] static bool isWireValid( sirStringView_t wire, uint32_t nKeys ) noexcept;
	// Candidate only: callers must verify exact text membership.
	[[nodiscard]] static uint32_t findCandidate( sirStringView_t key, sirStringView_t wire ) noexcept;

	static constexpr uint32_t INVALID_ORDINAL = UINT32_MAX;

private:
	[[nodiscard]] static uint32_t read32( const char *pBytes ) noexcept;

	static void write32( uint8_t *pBytes, uint32_t nValue ) noexcept;
	static constexpr uint32_t HEADER_BYTES = 20;

	[[nodiscard]] static uint64_t mix( uint64_t nValue ) noexcept;
	[[nodiscard]] static uint32_t bucket( sirStringView_t key, uint32_t nBuckets, uint64_t nSeed ) noexcept;
	[[nodiscard]] static uint32_t slot( sirStringView_t key, uint32_t nKeys, uint32_t nDisplacement, uint64_t nSeed ) noexcept;
	[[nodiscard]] static bool tryPlace( const sirStringView_t *pKeys,
	                                    const sirCookedStringIndexStorage_t& storage,
	                                    const sirCookedStringIndexScratch_t& scratch,
	                                    uint64_t nSeed ) noexcept;

	static constexpr uint32_t DIRECT = 0x80000000u;
	static constexpr uint32_t MAX_DISPLACEMENT = 65535;
};
#endif // __SIR_COOKED_STRING_INDEX_H__
