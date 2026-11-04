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
#ifndef __SIRENGINE_SIRSTRINGPOOL_H__
#define __SIRENGINE_SIRSTRINGPOOL_H__

#include <SIRPCH.hpp>
#include <Core/Containers/SIRContainerResult.h>
#include <Core/Platform/SIRSync.h>
#include <atomic>
#include <stddef.h>
#include <stdint.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

constexpr uint32_t SIR_STRING_TABLE_DEFAULT_SLOT_COUNT = 4096;
constexpr uint32_t SIR_STRING_TABLE_DEFAULT_BLOCK_SIZE = 1U << 20;
constexpr uint16_t SIR_STRING_CATALOG_VERSION = 1;

/* Compile-time A/B switches for isolated hash experiments. */
#ifndef SIR_STRING_GROUPED_INDEX
#define SIR_STRING_GROUPED_INDEX 0
#endif
#ifndef SIR_STRING_ADAPTIVE_INDEX
#define SIR_STRING_ADAPTIVE_INDEX 0
#endif
#ifndef SIR_STRING_XXH3_HASH
// Enable only after the pinned xxHash submodule is wired into this target.
#define SIR_STRING_XXH3_HASH 0
#endif
#ifndef SIR_STRING_GROUP_SIMD
#define SIR_STRING_GROUP_SIMD 1
#endif
/* Optional index occupancy experiment. Production default preserves 50%. */
#ifndef SIR_STRING_INDEX_TARGET_LOAD_PERCENT
#define SIR_STRING_INDEX_TARGET_LOAD_PERCENT 50
#endif

/* Identity copies never acquire or release decoded character storage. The
   IDs are allocated from a never-reused process-global range. Each pool
   maps its local immutable indices into that range. Zero is invalid. */
class SIRStringAtom
{
public:
	SIRStringAtom( void ) noexcept;
	explicit SIRStringAtom( uint32_t nId ) noexcept;

	operator bool( void ) const noexcept;
	bool operator==( const SIRStringAtom& other ) const noexcept;
	bool operator!=( const SIRStringAtom& other ) const noexcept;
	uintptr_t operator()( const SIRStringAtom& key ) const noexcept;

	uint32_t getId( void ) const noexcept;
	uint64_t hash( void ) const noexcept;
	bool isValid( void ) const noexcept;

	void reset( void ) noexcept;

protected:
private:
	uint32_t m_nId;
};

/* Out-of-class inline definitions follow the idTech-era header convention. */
inline SIRStringAtom::SIRStringAtom( void ) noexcept : m_nId( 0 )
{
}

inline SIRStringAtom::SIRStringAtom( uint32_t nId ) noexcept : m_nId( nId )
{
}

inline void SIRStringAtom::reset( void ) noexcept
{
	m_nId = 0;
}

inline uint32_t SIRStringAtom::getId( void ) const noexcept
{
	return m_nId;
}

inline uint64_t SIRStringAtom::hash( void ) const noexcept
{
	return ( uint64_t )m_nId;
}

inline bool SIRStringAtom::isValid( void ) const noexcept
{
	return m_nId != 0;
}

inline SIRStringAtom::operator bool( void ) const noexcept
{
	return m_nId != 0;
}

inline bool SIRStringAtom::operator==( const SIRStringAtom& other ) const noexcept
{
	return m_nId == other.m_nId;
}

inline bool SIRStringAtom::operator!=( const SIRStringAtom& other ) const noexcept
{
	return m_nId != other.m_nId;
}

inline uintptr_t SIRStringAtom::operator()( const SIRStringAtom& key ) const noexcept
{
	return key.m_nId;
}

typedef char sirStringAtomSizeCheck_t[sizeof( SIRStringAtom ) == 4 ? 1 : -1];
typedef char sirStringAtomCopyCheck_t[__is_trivially_copyable( SIRStringAtom ) ? 1 : -1];

class SIRStringPool;

typedef struct sirStringLeaseBinding_s
{
	uint32_t nSlot;
	uint32_t nAtomId;
	uint32_t nGeneration;
	const char *pData;
	uint32_t nBytes;
} sirStringLeaseBinding_t;

/* A lease owns either a decoded cache pin or stable dynamic-text storage.
   Release invalidates this lease and decrements one ownership count.
   Last release performs no cleanup; cached bytes remain until reuse. */
class SIRStringLease
{
public:
	SIRStringLease( void ) noexcept;
	SIRStringLease( const SIRStringLease& ) = delete;
	~SIRStringLease() noexcept;

	SIRStringLease& operator=( const SIRStringLease& ) = delete;

	const char *data( void ) const noexcept;
	uint32_t bytes( void ) const noexcept;
	bool isValid( void ) const noexcept;

	void release( void ) noexcept;

protected:
private:
	friend class SIRStringPool;
	void bind( SIRStringPool *pPool, const sirStringLeaseBinding_t& binding ) noexcept;

	SIRStringPool *m_pPool;
	const char *m_pData;
	uint32_t m_nSlot;
	uint32_t m_nAtomId;
	uint32_t m_nGeneration;
	uint32_t m_nBytes;
};

typedef enum sirStringStatus_e : uint8_t
{
	SIR_STRING_OK = 0,
	SIR_STRING_NOT_INITIALIZED,
	SIR_STRING_INVALID_ATOM,
	SIR_STRING_INVALID_ARGUMENT,
	SIR_STRING_CORRUPT_CATALOG,
	SIR_STRING_OUTPUT_TOO_SMALL,
	SIR_STRING_ALL_SLOTS_PINNED,
	SIR_STRING_CAPACITY_EXHAUSTED
} sirStringStatus_t;

/* The whole registry, immutable catalog, mutable text arena and decoded cache
   are carved from one caller-owned contiguous memory span. No hot-path heap. */
typedef struct sirStringPoolConfig_s
{
	uint32_t nHashBuckets;
	uint32_t nMaxNames;
	uint32_t nDynamicTextBytes;
	uint32_t nShortSlots;
	uint32_t nLongSlots;
	uint32_t nShortBytes;
	uint32_t nLongBytes;
} sirStringPoolConfig_t;

/* On-disk SIRS v1 records are serialized as little-endian fields; do not
   reinterpret_cast packed input bytes to this structure. */
typedef struct sirCookedStringEntry_s
{
	uint32_t nOffset;
	uint16_t nDecodedBytes;
	uint16_t nEncodedBytesAndFlags;
} sirCookedStringEntry_t;
typedef char sirCookedRecordSizeCheck_t[sizeof( sirCookedStringEntry_t ) == 8 ? 1 : -1];

/* Persistent, lazily resolved text owner. set() does not touch characters.
   First data()/get() acquires ownership (decoding compressed text on a miss).
   Subsequent data()/get() calls read the same pointer without new atomics.
   Keep the owner alive for as long as its returned pointer is in use. */
class SIRStringTextRef
{
public:
	SIRStringTextRef( void ) noexcept;
	SIRStringTextRef( const SIRStringTextRef& ) = delete;
	~SIRStringTextRef() noexcept;

	SIRStringTextRef& operator=( const SIRStringTextRef& ) = delete;

	/* First call resolves/decompresses; later calls return the same pointer. */
	const char *data( void ) noexcept;
	uint32_t bytes( void ) noexcept;
	bool isPinned( void ) const noexcept;
	sirStringStatus_t get( const char *& pText, uint32_t& nBytes ) noexcept;

	void set( SIRStringPool *pPool, SIRStringAtom atom ) noexcept;
	void release( void ) noexcept;

protected:
private:
	SIRStringPool *m_pPool;
	SIRStringAtom m_Atom;
	SIRStringLease m_Lease;
};

typedef struct sirStringPoolSpan_s
{
	void *pMemory;
	size_t nMemoryBytes;
	const void *pCooked;
	size_t nCookedBytes;
} sirStringPoolSpan_t;

typedef struct sirStringPoolStats_s
{
	uint32_t nStringCount;
	uint32_t nSlotCount;
	uint32_t nBlockCount;
	uint64_t nReservedBytes;
	uint32_t nCachedStrings;
	uint32_t nPinnedLeases;
	uint32_t nDynamicTextBytes;
	uint32_t nDecodedSlotCount;
} sirStringPoolStats_t;

typedef struct sirStringPoolCookedEntry_s sirStringPoolCookedEntry_t;
typedef struct sirStringPoolDynamicEntry_s sirStringPoolDynamicEntry_t;
typedef struct sirStringPoolCacheSlot_s sirStringPoolCacheSlot_t;

class SIRStringPool
{
public:
	SIRStringPool( void ) noexcept;
	SIRStringPool( const SIRStringPool& ) = delete;
	~SIRStringPool() noexcept;

	SIRStringPool& operator=( const SIRStringPool& ) = delete;

	bool isInitialized( void ) const noexcept;
	uint32_t getDecodedByteLength( SIRStringAtom atom ) const noexcept;
	uint64_t getContentHash( SIRStringAtom atom ) const noexcept;
	uint32_t getStringCount( void ) const noexcept;
	uint32_t getSlotCount( void ) const noexcept;
	uint32_t getBlockCount( void ) const noexcept;
	uint64_t getReservedBytes( void ) const noexcept;
	sirStringPoolStats_t getStats( void ) const noexcept;

	static sirStringPoolConfig_t defaultConfig( void ) noexcept;
	static size_t requiredBytes( const sirStringPoolConfig_t& config, size_t nCookedBytes ) noexcept;
	static size_t requiredBytes( const sirStringPoolConfig_t& config, const void *pCooked, size_t nCookedBytes ) noexcept;

	/* Preferred span comes from SIRMemoryManager; the caller owns it. */
	bool initialize( const sirStringPoolSpan_t& span, const sirStringPoolConfig_t& config ) noexcept;
	/* Explicit allocator path owns one SIRMemory domain allocation. */
	bool initialize( const sirContainerAllocator_t *pAllocator, const sirStringPoolConfig_t& config ) noexcept;
	/* Caller establishes exclusive shutdown/retirement. Live leases reject. */
	bool shutdown( void ) noexcept;
	bool retireScope( void ) noexcept;

	SIRStringAtom intern( const char *pszString ) noexcept;
	SIRStringAtom intern( const char *pszString, size_t nLength ) noexcept;
	SIRStringAtom find( const char *pszString ) const noexcept;
	SIRStringAtom find( const char *pszString, size_t nLength ) const noexcept;

	template <size_t N>
	inline SIRStringAtom internLiteral( const char ( &szString )[N] ) noexcept
	{
		return intern( szString, N - 1 );
	}

	template <size_t N>
	inline SIRStringAtom findLiteral( const char ( &szString )[N] ) const noexcept
	{
		return find( szString, N - 1 );
	}

	/* Reacquiring the same atom into the same live lease is idempotent. */
	sirStringStatus_t acquire( SIRStringAtom atom, SIRStringLease& lease ) noexcept;

	/* Direct raw pointer remains valid only until pool retirement/shutdown.
	   Compressed cooked names return nullptr; use acquire() for those. */
	const char *peekRaw( SIRStringAtom atom, uint32_t& nBytes ) const noexcept;
	sirStringStatus_t decodeInto( SIRStringAtom atom, char *pszOutput, size_t nCapacity, uint32_t& nWritten ) const noexcept;
	static uint64_t hashString( const char *pszString, size_t nLength ) noexcept;
	SIRStringAtom atomFromCookedOrdinal( uint32_t nOrdinal, uint64_t nExpectedCatalogHash ) const noexcept;

protected:
private:
	friend class SIRStringLease;

	static uint16_t readU16( const uint8_t *pBytes ) noexcept;
	static uint32_t readU32( const uint8_t *pBytes ) noexcept;
	static uint64_t readU64( const uint8_t *pBytes ) noexcept;
	static bool isValidUtf8( const char *pText, size_t nBytes ) noexcept;
	static bool isPowerOfTwo( uint32_t nValue ) noexcept;
	static uint64_t lookupHash( const char *pText, size_t nLength ) noexcept;
	void *carve( size_t nBytes, size_t nAlignment ) noexcept;
	bool loadCatalog( const uint8_t *pCooked, size_t nCookedBytes ) noexcept;
	bool decodeCooked( uint32_t nIndex, char *pOutput, size_t nCapacity, uint32_t& nWritten ) const noexcept;
	bool matchesCooked( uint32_t nIndex, const char *pText, size_t nBytes ) const noexcept;
	bool matchesDynamic( uint32_t nIndex, const char *pText, size_t nBytes ) const noexcept;
	uint32_t findLocal( const char *pText, size_t nLength, uint64_t nHash ) const noexcept;
	uint32_t findLocalFlat( const char *pText, size_t nLength, uint64_t nHash ) const noexcept;
	bool findInsertionSlot( uint64_t nHash, uint32_t& nSlot ) const noexcept;
	void publishIndex( uint32_t nSlot, uint64_t nHash, uint32_t nIndexPlus ) noexcept;
	SIRStringAtom makeAtom( uint32_t nLocalIndex ) const noexcept;
	bool resolveIndex( SIRStringAtom atom, uint32_t& nIndex ) const noexcept;
	bool tryAcquireHot( uint32_t nIndex, uint32_t nAtomId, SIRStringLease& lease ) const noexcept;
	void releaseLease( uint32_t nSlot, uint32_t nGeneration ) noexcept;
	void resetState( void ) noexcept;

	uint8_t *m_pMemory;
	size_t m_nMemoryBytes;
	size_t m_nCursor;
	bool m_bOwnsMemory;
	sirContainerAllocator_t m_Allocator;
	uint32_t m_nBaseId;
	uint64_t m_nCatalogHash;
	uint32_t m_nHashBuckets;
	uint32_t m_nMaxNames;
	uint32_t m_nCookedNames;
	uint32_t m_nDynamicTextCapacity;
	uint32_t m_nDynamicUsed;
	uint32_t m_nCacheCursor;
	uint32_t m_nCacheSlots;
	/* Monotonic, cache-lock-owned virgin cursors. A slot is counted as
	   initialized only after its first successful decoded publication. */
	uint32_t m_nNextUnusedShort;
	uint32_t m_nNextUnusedLong;
	uint32_t m_nShortBytes;
	uint32_t m_nLongBytes;
	uint32_t m_nShortSlots;
	uint32_t m_nLongSlots;
	std::atomic<uint32_t> m_nNames;
	std::atomic<uint32_t> m_nDirectLeases;
#if SIR_STRING_GROUPED_INDEX
	std::atomic<uint64_t> *m_pGroupControl;
	uint8_t *m_pGroupIndices;
#if SIR_STRING_ADAPTIVE_INDEX
	std::atomic<uint32_t> *m_pDynamicIndex;
	bool m_bGroupedIndex;
#endif
#else
	std::atomic<uint32_t> *m_pDynamicIndex;
#endif
	sirStringPoolCookedEntry_t *m_pCooked;
	uint64_t *m_pCookedHashes;
	std::atomic<uint32_t> *m_pCookedHot;
	sirStringPoolDynamicEntry_t *m_pDynamicEntries;
	sirStringPoolCacheSlot_t *m_pCache;
	uint8_t *m_pDynamic;
	const uint8_t *m_pBlob;
	uint32_t m_nBlobBytes;
	SIRSpinLock m_registryLock;
	mutable SIRSpinLock m_cacheLock;
};

#endif // __SIRENGINE_SIRSTRINGPOOL_H__
