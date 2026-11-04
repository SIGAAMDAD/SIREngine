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
#include <Core/Strings/SIRStringPool.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <new>
#if SIR_STRING_GROUPED_INDEX && defined( _MSC_VER )
#include <intrin.h>
#endif
#if SIR_STRING_XXH3_HASH
#define XXH_INLINE_ALL
#include <xxhash.h>
#endif
#if SIR_STRING_GROUPED_INDEX && SIR_STRING_GROUP_SIMD &&                                                                                   \
    ( defined( __SSE2__ ) || defined( _M_X64 ) || ( defined( _M_IX86_FP ) && _M_IX86_FP >= 2 ) )
#include <emmintrin.h>
#define SIR_STRING_HAS_SSE2_GROUP 1
#else
#define SIR_STRING_HAS_SSE2_GROUP 0
#endif

static constexpr size_t SIR_COOKED_HEADER_BYTES = 48;
static constexpr uint32_t SIR_MAX_COOKED_TEXT_BYTES = 1024;
static constexpr uint32_t SIR_DIRECT_TEXT_SLOT = UINT32_MAX;
static constexpr uint32_t SIR_ENTRY_INDEX_MASK = 0x00ffffffU;
static constexpr uint32_t SIR_DEFAULT_CACHE_COUNTS[SIR_STRING_CACHE_TIER_COUNT] = { 512, 256, 128, 64, 24, 12 };
static constexpr uint32_t SIR_DEFAULT_CACHE_BYTES[SIR_STRING_CACHE_TIER_COUNT] = { 16, 64, 128, 256, 512, 1024 };
static constexpr uint32_t SIR_INITIAL_CACHE_COUNTS[SIR_STRING_CACHE_TIER_COUNT] = { 16, 8, 4, 2, 1, 1 };
static std::atomic<uint64_t> s_nNextAtomId( 1 );

static bool reserveAtomIds( uint32_t count, uint32_t& baseId ) noexcept
{
	uint64_t first = s_nNextAtomId.load( std::memory_order_relaxed );
	for ( ;; ) {
		if ( first > UINT32_MAX || count > ( uint64_t )UINT32_MAX + 1 - first ) {
			return false;
		}
		if ( s_nNextAtomId.compare_exchange_weak( first, first + count, std::memory_order_acq_rel, std::memory_order_relaxed ) ) {
			baseId = ( uint32_t )first;
			return true;
		}
	}
}
#if SIR_STRING_GROUPED_INDEX
static constexpr uint64_t SIR_GROUP_EMPTY = 0x8080808080808080ULL;
static inline uint16_t groupMask( uint64_t lo, uint64_t hi, uint8_t value ) noexcept
{
#if SIR_STRING_HAS_SSE2_GROUP
	const __m128i control = _mm_set_epi64x( ( long long )hi, ( long long )lo );
	const __m128i match = _mm_cmpeq_epi8( control, _mm_set1_epi8( ( char )value ) );
	return ( uint16_t )_mm_movemask_epi8( match );
#else
	uint16_t mask = 0;
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( ( uint8_t )( lo >> ( i * 8 ) ) == value ) {
			mask |= ( uint16_t )( 1U << i );
		}
		if ( ( uint8_t )( hi >> ( i * 8 ) ) == value ) {
			mask |= ( uint16_t )( 1U << ( i + 8 ) );
		}
	}
	return mask;
#endif
}
static inline uint32_t firstSetBit( uint16_t bits ) noexcept
{
#if defined( _MSC_VER )
	unsigned long index = 0;
	_BitScanForward( &index, bits );
	return ( uint32_t )index;
#else
	return ( uint32_t )__builtin_ctz( ( unsigned int )bits );
#endif
}
#endif

static bool checkedAdd( size_t& total, size_t amount ) noexcept
{
	if ( amount > SIZE_MAX - total ) {
		return false;
	}
	total += amount;
	return true;
}
static bool checkedMultiply( size_t x, size_t y, size_t& result ) noexcept
{
	if ( y != 0 && x > SIZE_MAX / y ) {
		return false;
	}
	result = x * y;
	return true;
}
/* Keep catalog verification separate from the runtime hot hash kernel.
   This cold routine also prevents the catalog checksum from being folded
   into the canonical-name hash during link-time optimization. */
static uint64_t hashCookedPayload( const char *pText, size_t nBytes ) noexcept
{
	uint64_t hash = 14695981039346656037ULL;
	for ( size_t i = 0; i < nBytes; ++i ) {
		hash = ( hash ^ ( uint8_t )pText[i] ) * 1099511628211ULL;
	}
	return hash;
}
/* Limits are admission budgets, not additional allocations. The caller
   reserves the hard maximum once so hot atom/lease addresses remain stable. */
static uint32_t initialBudget( uint32_t nMaximum, uint32_t nRequested, uint32_t nDefault ) noexcept
{
	const uint32_t nInitial = nRequested != 0 ? nRequested : nDefault;
	return nInitial < nMaximum ? nInitial : nMaximum;
}

static uint32_t grownBudget( uint32_t nActive, uint32_t nRequired, uint32_t nMaximum ) noexcept
{
	while ( nActive < nRequired && nActive < nMaximum ) {
		const uint64_t nDoubled = nActive == 0 ? 1u : ( uint64_t )nActive * 2u;
		nActive = nDoubled > nMaximum ? nMaximum : ( uint32_t )nDoubled;
	}
	return nActive;
}

static uint32_t nextPowerOfTwo( uint64_t value ) noexcept
{
	if ( value > 0x80000000ULL ) {
		return 0;
	}
	uint32_t n = 1;
	while ( n < value ) {
		n <<= 1;
	}
	return n;
}

typedef struct sirStringPoolCookedEntry_s
{
	uint32_t nOffset;
	uint16_t nLength;
	uint16_t nFlags;
} sirStringPoolCookedEntry_t;
typedef struct sirStringPoolDynamicEntry_s
{
	uint64_t nHash;
	uint32_t nOffset;
	uint32_t nLength;
} sirStringPoolDynamicEntry_t;
typedef struct sirStringPoolCacheSlot_s
{
	std::atomic<uint32_t> nPins;
	std::atomic<uint32_t> nAtomId;
	uint32_t nGeneration;
	uint32_t nCapacity;
	uint32_t nBytes;
	char *pszData;
	sirStringPoolCacheSlot_s( void ) noexcept : nPins( 0 ), nAtomId( 0 ), nGeneration( 0 ), nCapacity( 0 ), nBytes( 0 ), pszData( NULL )
	{
	}
} sirStringPoolCacheSlot_t;

SIRStringLease::SIRStringLease( void ) noexcept
    : m_pPool( NULL ), m_pData( NULL ), m_nSlot( 0 ), m_nAtomId( 0 ), m_nGeneration( 0 ), m_nBytes( 0 )
{
}

void SIRStringLease::bind( SIRStringPool *pPool, const sirStringLeaseBinding_t& binding ) noexcept
{
	m_pPool = pPool;
	m_pData = binding.pData;
	m_nSlot = binding.nSlot;
	m_nAtomId = binding.nAtomId;
	m_nGeneration = binding.nGeneration;
	m_nBytes = binding.nBytes;
}

SIRStringLease::~SIRStringLease() noexcept
{
	release();
}

void SIRStringLease::release( void ) noexcept
{
	SIRStringPool *pPool = m_pPool;
	if ( pPool == NULL ) {
		return;
	}
	const uint32_t slot = m_nSlot;
	const uint32_t generation = m_nGeneration;
	m_pPool = NULL;
	m_pData = NULL;
	m_nBytes = 0;
	m_nSlot = 0;
	m_nAtomId = 0;
	m_nGeneration = 0;
	pPool->releaseLease( slot, generation );
}

const char *SIRStringLease::data( void ) const noexcept
{
	return m_pData;
}

uint32_t SIRStringLease::bytes( void ) const noexcept
{
	return m_nBytes;
}

bool SIRStringLease::isValid( void ) const noexcept
{
	return m_pPool != NULL;
}

SIRStringTextRef::SIRStringTextRef( void ) noexcept : m_pPool( NULL ), m_Atom(), m_Lease()
{
}

SIRStringTextRef::~SIRStringTextRef() noexcept
{
	release();
}

void SIRStringTextRef::set( SIRStringPool *pPool, SIRStringAtom atom ) noexcept
{
	if ( pPool == m_pPool && atom == m_Atom ) {
		return;
	}
	release();
	m_pPool = pPool;
	m_Atom = atom;
}

sirStringStatus_t SIRStringTextRef::get( const char *& pText, uint32_t& nBytes ) noexcept
{
	pText = NULL;
	nBytes = 0;
	if ( m_Lease.isValid() ) {
		pText = m_Lease.data();
		nBytes = m_Lease.bytes();
		return SIR_STRING_OK;
	}
	if ( m_pPool == NULL ) {
		return SIR_STRING_NOT_INITIALIZED;
	}
	const sirStringStatus_t status = m_pPool->acquire( m_Atom, m_Lease );
	if ( status == SIR_STRING_OK ) {
		pText = m_Lease.data();
		nBytes = m_Lease.bytes();
	}
	return status;
}

const char *SIRStringTextRef::data( void ) noexcept
{
	if ( m_Lease.isValid() ) {
		return m_Lease.data();
	}
	const char *pText = NULL;
	uint32_t nBytes = 0;
	get( pText, nBytes );
	return pText;
}

uint32_t SIRStringTextRef::bytes( void ) noexcept
{
	if ( m_Lease.isValid() ) {
		return m_Lease.bytes();
	}
	const char *pText = NULL;
	uint32_t nBytes = 0;
	return get( pText, nBytes ) == SIR_STRING_OK ? nBytes : 0;
}

bool SIRStringTextRef::isPinned( void ) const noexcept
{
	return m_Lease.isValid();
}

void SIRStringTextRef::release( void ) noexcept
{
	m_pPool = NULL;
	m_Atom.reset();
	m_Lease.release();
}

SIRStringPool::SIRStringPool( void ) noexcept
    : m_pMemory( NULL ), m_nMemoryBytes( 0 ), m_nCursor( 0 ), m_bOwnsMemory( false ), m_Allocator(), m_nBaseId( 0 ), m_nCatalogHash( 0 ),
      m_nHashBuckets( 0 ), m_nMaxNames( 0 ), m_nCookedNames( 0 ), m_nDynamicTextCapacity( 0 ), m_nDynamicUsed( 0 ), m_nCacheCursor( 0 ),
      m_nCacheSlots( 0 ), m_nNextUnusedShort( 0 ), m_nNextUnusedLong( 0 ), m_nShortBytes( 0 ), m_nLongBytes( 0 ), m_nShortSlots( 0 ),
      m_nLongSlots( 0 ), m_bTieredCache( false ), m_bAdaptiveBudgets( false ), m_nInitialNameBudget( 0 ), m_nInitialDynamicTextBudget( 0 ),
      m_nInitialShortSlots( 0 ), m_nInitialLongSlots( 0 ), m_nActiveNameBudget( 0 ), m_nActiveDynamicTextBudget( 0 ),
      m_nActiveShortSlots( 0 ), m_nActiveLongSlots( 0 ), m_nNames( 0 ), m_nDirectLeases( 0 ),
#if SIR_STRING_GROUPED_INDEX
      m_pGroupControl( NULL ), m_pGroupIndices( NULL ),
#if SIR_STRING_ADAPTIVE_INDEX
      m_pDynamicIndex( NULL ), m_bGroupedIndex( false ),
#endif
#else
      m_pDynamicIndex( NULL ),
#endif
      m_pCooked( NULL ), m_pCookedHashes( NULL ), m_pCookedHot( NULL ), m_pDynamicEntries( NULL ), m_pCache( NULL ), m_pDynamic( NULL ),
      m_pBlob( NULL ), m_nBlobBytes( 0 )
{
}

SIRStringPool::~SIRStringPool() noexcept
{
	shutdown();
}

sirStringPoolConfig_t SIRStringPool::defaultConfig( void ) noexcept
{
	sirStringPoolConfig_t config;
	memset( &config, 0, sizeof( config ) );
	config.nHashBuckets = SIR_STRING_TABLE_DEFAULT_SLOT_COUNT;
	config.nMaxNames = 8192;
	config.nDynamicTextBytes = SIR_STRING_TABLE_DEFAULT_BLOCK_SIZE;
	config.nShortSlots = 16;
	config.nLongSlots = 4;
	config.nShortBytes = 128;
	config.nLongBytes = 1024;
	config.bAdaptiveBudgets = true;
	config.bTieredCache = true;
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		config.nTierSlots[iTier] = SIR_DEFAULT_CACHE_COUNTS[iTier];
		config.nTierBytes[iTier] = SIR_DEFAULT_CACHE_BYTES[iTier];
	}
	return config;
}

size_t SIRStringPool::requiredBytes( const sirStringPoolConfig_t& config, size_t cookedBytes ) noexcept
{
	if ( SIR_STRING_INDEX_TARGET_LOAD_PERCENT < 25 || SIR_STRING_INDEX_TARGET_LOAD_PERCENT > 85 || !isPowerOfTwo( config.nHashBuckets ) ||
	     config.nMaxNames == 0 || config.nMaxNames > SIR_ENTRY_INDEX_MASK ||
	     ( cookedBytes != 0 && cookedBytes < SIR_COOKED_HEADER_BYTES ) ||
	     ( config.bAdaptiveBudgets &&
	       ( config.nInitialNameBudget > config.nMaxNames || config.nInitialDynamicTextBytes > config.nDynamicTextBytes ) ) ) {
		return 0;
	}
	uint64_t nCacheSlots = 0;
	if ( config.bTieredCache ) {
		uint32_t nPreviousBytes = 0;
		for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
			if ( config.nTierBytes[iTier] <= nPreviousBytes ||
			     ( config.bAdaptiveBudgets && config.nInitialTierSlots[iTier] > config.nTierSlots[iTier] ) ) {
				return 0;
			}
			nPreviousBytes = config.nTierBytes[iTier];
			nCacheSlots += config.nTierSlots[iTier];
		}
		if ( nCacheSlots > UINT32_MAX || nPreviousBytes < SIR_MAX_COOKED_TEXT_BYTES ) {
			return 0;
		}
	} else {
		if ( config.nLongBytes < config.nShortBytes || config.nLongBytes < SIR_MAX_COOKED_TEXT_BYTES ||
		     ( config.bAdaptiveBudgets &&
		       ( config.nInitialShortSlots > config.nShortSlots || config.nInitialLongSlots > config.nLongSlots ) ) ) {
			return 0;
		}
		nCacheSlots = ( uint64_t )config.nShortSlots + config.nLongSlots;
		if ( nCacheSlots > UINT32_MAX ) {
			return 0;
		}
	}
	uint64_t requiredSlots =
	    ( ( uint64_t )config.nMaxNames * 100 + SIR_STRING_INDEX_TARGET_LOAD_PERCENT - 1 ) / SIR_STRING_INDEX_TARGET_LOAD_PERCENT;
#if SIR_STRING_GROUPED_INDEX && !SIR_STRING_ADAPTIVE_INDEX
	if ( requiredSlots < 16 ) {
		requiredSlots = 16;
	}
#endif
	const uint32_t slots = nextPowerOfTwo( requiredSlots );
	if ( slots == 0 ) {
		return 0;
	}
	const size_t cookedUpper = cookedBytes ? ( cookedBytes - 48 ) / 8 < config.nMaxNames ? ( cookedBytes - 48 ) / 8 : config.nMaxNames : 0;
	size_t total = 0, bytes = 0;
#if SIR_STRING_GROUPED_INDEX
	/* 1 control byte + 3 immutable index bytes per slot, equal to the
	   4-byte packed tag footprint of the original table. */
	if ( !checkedMultiply( slots, 4, bytes ) || !checkedAdd( total, bytes ) ) {
		return 0;
	}
#else
	if ( !checkedMultiply( slots, sizeof( std::atomic<uint32_t> ), bytes ) || !checkedAdd( total, bytes ) ) {
		return 0;
	}
#endif
	if ( !checkedMultiply( config.nMaxNames, sizeof( sirStringPoolDynamicEntry_t ), bytes ) || !checkedAdd( total, bytes ) ||
	     !checkedMultiply( cookedUpper, sizeof( sirStringPoolCookedEntry_t ) + sizeof( uint64_t ) + sizeof( std::atomic<uint32_t> ),
	                       bytes ) ||
	     !checkedAdd( total, bytes ) || !checkedMultiply( ( size_t )nCacheSlots, sizeof( sirStringPoolCacheSlot_t ), bytes ) ||
	     !checkedAdd( total, bytes ) || !checkedAdd( total, ( size_t )config.nDynamicTextBytes + 1 ) ||
	     !checkedAdd( total, cookedBytes ) ) {
		return 0;
	}
	if ( config.bTieredCache ) {
		for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
			if ( !checkedMultiply( config.nTierSlots[iTier], ( size_t )config.nTierBytes[iTier] + 1, bytes ) ||
			     !checkedAdd( total, bytes ) ) {
				return 0;
			}
		}
	} else {
		if ( !checkedMultiply( config.nShortSlots, ( size_t )config.nShortBytes + 1, bytes ) || !checkedAdd( total, bytes ) ||
		     !checkedMultiply( config.nLongSlots, ( size_t )config.nLongBytes + 1, bytes ) || !checkedAdd( total, bytes ) ) {
			return 0;
		}
	}
	return checkedAdd( total, 8 * 64 ) ? total : 0;
}

size_t SIRStringPool::requiredBytes( const sirStringPoolConfig_t& config, const void *pCooked, size_t cookedBytes ) noexcept
{
	if ( cookedBytes && ( !pCooked || cookedBytes < 48 || memcmp( pCooked, "SIRS", 4 ) != 0 ||
	                      readU16( ( const uint8_t * )pCooked + 4 ) != SIR_STRING_CATALOG_VERSION ) ) {
		return 0;
	}
	const size_t conservative = requiredBytes( config, cookedBytes );
	if ( !conservative || !cookedBytes ) {
		return conservative;
	}
	const size_t cookedUpper = ( cookedBytes - 48 ) / 8 < config.nMaxNames ? ( cookedBytes - 48 ) / 8 : config.nMaxNames;
	const uint32_t cooked = readU32( ( const uint8_t * )pCooked + 8 );
	if ( cooked > cookedUpper ) {
		return 0;
	}
	/* Every cooked record replaces a 16-byte dynamic descriptor and adds
	   an 8-byte descriptor, 8-byte hash and 4-byte hint: net +4 bytes. */
	return conservative - ( cookedUpper - cooked ) * 20 - ( size_t )cooked * 16;
}

void *SIRStringPool::carve( size_t nBytes, size_t nAlignment ) noexcept
{
	if ( m_pMemory == NULL || nAlignment == 0 || ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
		return NULL;
	}
	const uintptr_t base = ( uintptr_t )m_pMemory;
	if ( m_nCursor > UINTPTR_MAX - base || base + m_nCursor > UINTPTR_MAX - ( nAlignment - 1 ) ) {
		return NULL;
	}
	const uintptr_t start = ( base + m_nCursor + nAlignment - 1 ) & ~( ( uintptr_t )nAlignment - 1 );
	const size_t offset = ( size_t )( start - base );
	if ( offset > m_nMemoryBytes || nBytes > m_nMemoryBytes - offset ) {
		return NULL;
	}
	m_nCursor = offset + nBytes;
	return ( void * )start;
}

void SIRStringPool::resetState( void ) noexcept
{
	m_pMemory = NULL;
	m_nMemoryBytes = m_nCursor = 0;
	m_bOwnsMemory = false;
	memset( &m_Allocator, 0, sizeof( m_Allocator ) );
	m_nBaseId = 0;
	m_nCatalogHash = 0;
	m_nHashBuckets = m_nMaxNames = m_nCookedNames = 0;
	m_nDynamicTextCapacity = m_nDynamicUsed = m_nCacheCursor = m_nCacheSlots = 0;
	m_nNextUnusedShort = m_nNextUnusedLong = 0;
	m_nShortSlots = m_nLongSlots = m_nShortBytes = m_nLongBytes = 0;
	m_bTieredCache = false;
	memset( m_nTierStart, 0, sizeof( m_nTierStart ) );
	memset( m_nTierSlots, 0, sizeof( m_nTierSlots ) );
	memset( m_nTierBytes, 0, sizeof( m_nTierBytes ) );
	memset( m_nTierNext, 0, sizeof( m_nTierNext ) );
	memset( m_nTierCursor, 0, sizeof( m_nTierCursor ) );
	memset( m_nInitialTierSlots, 0, sizeof( m_nInitialTierSlots ) );
	memset( m_nActiveTierSlots, 0, sizeof( m_nActiveTierSlots ) );
	m_bAdaptiveBudgets = false;
	m_nInitialNameBudget = m_nInitialDynamicTextBudget = m_nInitialShortSlots = m_nInitialLongSlots = 0;
	m_nActiveNameBudget = m_nActiveDynamicTextBudget = m_nActiveShortSlots = m_nActiveLongSlots = 0;
	m_nBlobBytes = 0;
	m_nNames.store( 0, std::memory_order_relaxed );
	m_nDirectLeases.store( 0, std::memory_order_relaxed );
#if SIR_STRING_GROUPED_INDEX
	m_pGroupControl = NULL;
	m_pGroupIndices = NULL;
#if SIR_STRING_ADAPTIVE_INDEX
	m_pDynamicIndex = NULL;
	m_bGroupedIndex = false;
#endif
#else
	m_pDynamicIndex = NULL;
#endif
	m_pCooked = NULL;
	m_pCookedHashes = NULL;
	m_pCookedHot = NULL;
	m_pDynamicEntries = NULL;
	m_pCache = NULL;
	m_pDynamic = NULL;
	m_pBlob = NULL;
}

bool SIRStringPool::initialize( const sirStringPoolSpan_t& span, const sirStringPoolConfig_t& config ) noexcept
{
	void *pMemory = span.pMemory;
	size_t nMemoryBytes = span.nMemoryBytes;
	const void *pCooked = span.pCooked;
	size_t nCookedBytes = span.nCookedBytes;
	if ( m_pMemory != NULL || pMemory == NULL || ( pCooked == NULL && nCookedBytes != 0 ) ) {
		return false;
	}
	/* Preflight the cooked count before carving, without trusting offsets. */
	uint32_t cooked = 0;
	if ( nCookedBytes != 0 ) {
		if ( nCookedBytes < SIR_COOKED_HEADER_BYTES || memcmp( pCooked, "SIRS", 4 ) != 0 ||
		     readU16( ( const uint8_t * )pCooked + 4 ) != SIR_STRING_CATALOG_VERSION ) {
			return false;
		}
		cooked = readU32( ( const uint8_t * )pCooked + 8 );
	}
	const size_t required = requiredBytes( config, pCooked, nCookedBytes );
	if ( required == 0 || nMemoryBytes < required || cooked > config.nMaxNames ) {
		return false;
	}
	m_pMemory = ( uint8_t * )pMemory;
	m_nMemoryBytes = nMemoryBytes;
	m_nCursor = 0;
	uint64_t requiredSlots =
	    ( ( uint64_t )config.nMaxNames * 100 + SIR_STRING_INDEX_TARGET_LOAD_PERCENT - 1 ) / SIR_STRING_INDEX_TARGET_LOAD_PERCENT;
#if SIR_STRING_GROUPED_INDEX && !SIR_STRING_ADAPTIVE_INDEX
	if ( requiredSlots < 16 ) {
		requiredSlots = 16;
	}
#endif
	m_nHashBuckets = nextPowerOfTwo( requiredSlots );
	m_nMaxNames = config.nMaxNames;
	m_nDynamicTextCapacity = config.nDynamicTextBytes;
	m_nShortSlots = config.nShortSlots;
	m_nLongSlots = config.nLongSlots;
	m_nShortBytes = config.nShortBytes;
	m_nLongBytes = config.nLongBytes;
	m_bTieredCache = config.bTieredCache;
	m_nCacheSlots = m_bTieredCache ? 0 : m_nShortSlots + m_nLongSlots;
	m_bAdaptiveBudgets = config.bAdaptiveBudgets;
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		m_nTierStart[iTier] = m_nCacheSlots;
		m_nTierSlots[iTier] = 0;
		m_nTierBytes[iTier] = 0;
		m_nTierNext[iTier] = 0;
		m_nTierCursor[iTier] = 0;
		m_nInitialTierSlots[iTier] = 0;
		m_nActiveTierSlots[iTier] = 0;
		if ( m_bTieredCache ) {
			m_nTierSlots[iTier] = config.nTierSlots[iTier];
			m_nTierBytes[iTier] = config.nTierBytes[iTier];
			m_nInitialTierSlots[iTier] = m_bAdaptiveBudgets ? initialBudget( config.nTierSlots[iTier], config.nInitialTierSlots[iTier],
			                                                                 SIR_INITIAL_CACHE_COUNTS[iTier] )
			                                                : config.nTierSlots[iTier];
			m_nActiveTierSlots[iTier] = m_nInitialTierSlots[iTier];
			m_nCacheSlots += m_nTierSlots[iTier];
		}
	}
	m_nInitialNameBudget = m_bAdaptiveBudgets ? initialBudget( m_nMaxNames, config.nInitialNameBudget, 512 ) : m_nMaxNames;
	m_nInitialDynamicTextBudget =
	    m_bAdaptiveBudgets ? initialBudget( m_nDynamicTextCapacity, config.nInitialDynamicTextBytes, 65536 ) : m_nDynamicTextCapacity;
	m_nInitialShortSlots = m_bAdaptiveBudgets ? initialBudget( m_nShortSlots, config.nInitialShortSlots, 4 ) : m_nShortSlots;
	m_nInitialLongSlots = m_bAdaptiveBudgets ? initialBudget( m_nLongSlots, config.nInitialLongSlots, 1 ) : m_nLongSlots;
	m_nActiveNameBudget = m_nInitialNameBudget < cooked ? cooked : m_nInitialNameBudget;
	m_nActiveDynamicTextBudget = m_nInitialDynamicTextBudget;
	m_nActiveShortSlots = m_nInitialShortSlots;
	m_nActiveLongSlots = m_nInitialLongSlots;
	m_nNextUnusedShort = 0;
	m_nNextUnusedLong = m_nShortSlots;
	m_nCookedNames = cooked;
	m_nNames.store( 0, std::memory_order_relaxed );
	m_nDirectLeases.store( 0, std::memory_order_relaxed );
#if SIR_STRING_GROUPED_INDEX
#if SIR_STRING_ADAPTIVE_INDEX
	/* Small sets avoid the metadata-group setup; large random-access
	   sets use 16-way control groups. Both use 4 bytes per slot. */
	m_bGroupedIndex = m_nMaxNames >= 32768;
	if ( m_bGroupedIndex ) {
#endif
		m_pGroupControl = ( std::atomic<uint64_t> * )carve( ( size_t )m_nHashBuckets, alignof( std::atomic<uint64_t> ) );
		m_pGroupIndices = ( uint8_t * )carve( ( size_t )m_nHashBuckets * 3, 1 );
#if SIR_STRING_ADAPTIVE_INDEX
	} else {
		m_pDynamicIndex = ( std::atomic<uint32_t> * )carve( ( size_t )m_nHashBuckets * sizeof( std::atomic<uint32_t> ),
		                                                    alignof( std::atomic<uint32_t> ) );
	}
#endif
#else
	m_pDynamicIndex =
	    ( std::atomic<uint32_t> * )carve( ( size_t )m_nHashBuckets * sizeof( std::atomic<uint32_t> ), alignof( std::atomic<uint32_t> ) );
#endif
	m_pCooked = ( sirStringPoolCookedEntry_t * )carve( ( size_t )cooked * sizeof( sirStringPoolCookedEntry_t ),
	                                                   alignof( sirStringPoolCookedEntry_t ) );
	m_pCookedHashes = ( uint64_t * )carve( ( size_t )cooked * sizeof( uint64_t ), alignof( uint64_t ) );
	m_pCookedHot = ( std::atomic<uint32_t> * )carve( ( size_t )cooked * sizeof( std::atomic<uint32_t> ), alignof( std::atomic<uint32_t> ) );
	m_pDynamicEntries = ( sirStringPoolDynamicEntry_t * )carve( ( size_t )( m_nMaxNames - cooked ) * sizeof( sirStringPoolDynamicEntry_t ),
	                                                            alignof( sirStringPoolDynamicEntry_t ) );
	m_pCache = ( sirStringPoolCacheSlot_t * )carve( ( size_t )m_nCacheSlots * sizeof( sirStringPoolCacheSlot_t ),
	                                                alignof( sirStringPoolCacheSlot_t ) );
	m_pDynamic = ( uint8_t * )carve( ( size_t )m_nDynamicTextCapacity + 1, 1 );
	uint8_t *copy = ( uint8_t * )carve( nCookedBytes, 1 );
#if SIR_STRING_GROUPED_INDEX
	if (
#if SIR_STRING_ADAPTIVE_INDEX
	    ( m_bGroupedIndex ? ( m_pGroupControl == NULL || m_pGroupIndices == NULL ) : m_pDynamicIndex == NULL ) ||
#else
	    m_pGroupControl == NULL || m_pGroupIndices == NULL ||
#endif
	    m_pCooked == NULL ||
#else
	if ( m_pDynamicIndex == NULL || m_pCooked == NULL ||
#endif
	    m_pCookedHashes == NULL || m_pCookedHot == NULL || m_pDynamicEntries == NULL || m_pCache == NULL || m_pDynamic == NULL ||
	    ( nCookedBytes && copy == NULL ) ) {
		resetState();
		return false;
	}
#if SIR_STRING_GROUPED_INDEX
#if SIR_STRING_ADAPTIVE_INDEX
	if ( m_bGroupedIndex ) {
#endif
		for ( uint32_t i = 0; i < m_nHashBuckets / 8; ++i ) {
			new ( &m_pGroupControl[i] ) std::atomic<uint64_t>( SIR_GROUP_EMPTY );
		}
		memset( m_pGroupIndices, 0, ( size_t )m_nHashBuckets * 3 );
#if SIR_STRING_ADAPTIVE_INDEX
	} else {
		for ( uint32_t i = 0; i < m_nHashBuckets; ++i ) {
			new ( &m_pDynamicIndex[i] ) std::atomic<uint32_t>( 0 );
		}
	}
#endif
#else
	for ( uint32_t i = 0; i < m_nHashBuckets; ++i ) {
		new ( &m_pDynamicIndex[i] ) std::atomic<uint32_t>( 0 );
	}
#endif
	for ( uint32_t i = 0; i < cooked; ++i ) {
		new ( &m_pCooked[i] ) sirStringPoolCookedEntry_t();
		new ( &m_pCookedHashes[i] ) uint64_t( 0 );
		new ( &m_pCookedHot[i] ) std::atomic<uint32_t>( 0 );
	}
	for ( uint32_t i = 0; i < m_nCacheSlots; ++i ) {
		new ( &m_pCache[i] ) sirStringPoolCacheSlot_t();
		if ( m_bTieredCache ) {
			uint32_t iTier = 0;
			while ( iTier + 1 < SIR_STRING_CACHE_TIER_COUNT && i >= m_nTierStart[iTier + 1] ) {
				++iTier;
			}
			m_pCache[i].nCapacity = m_nTierBytes[iTier];
		} else {
			m_pCache[i].nCapacity = i < m_nShortSlots ? m_nShortBytes : m_nLongBytes;
		}
		m_pCache[i].pszData = ( char * )carve( ( size_t )m_pCache[i].nCapacity + 1, 1 );
		if ( m_pCache[i].pszData == NULL ) {
			resetState();
			return false;
		}
	}
	if ( nCookedBytes ) {
		memcpy( copy, pCooked, nCookedBytes );
		if ( !loadCatalog( copy, nCookedBytes ) ) {
			resetState();
			return false;
		}
	}
	uint32_t baseId = 0;
	if ( !reserveAtomIds( m_nMaxNames, baseId ) ) {
		resetState();
		return false;
	}
	m_nBaseId = baseId;
	m_nNames.store( cooked, std::memory_order_release );
	return true;
}

bool SIRStringPool::initialize( const sirContainerAllocator_t *pAllocator, const sirStringPoolConfig_t& config ) noexcept
{
	if ( pAllocator == nullptr || pAllocator->pfnAllocate == nullptr || pAllocator->pfnDeallocate == nullptr || m_pMemory != nullptr ) {
		return false;
	}
	const size_t nBytes = requiredBytes( config, 0 );
	if ( nBytes == 0 ) {
		return false;
	}
	void *pMemory = pAllocator->pfnAllocate( pAllocator->pContext, nBytes, alignof( max_align_t ) );
	if ( pMemory == nullptr ) {
		return false;
	}
	if ( !initialize( { pMemory, nBytes, nullptr, 0 }, config ) ) {
		pAllocator->pfnDeallocate( pAllocator->pContext, pMemory, nBytes, alignof( max_align_t ) );
		return false;
	}
	m_bOwnsMemory = true;
	m_Allocator = *pAllocator;
	return true;
}

bool SIRStringPool::shutdown( void ) noexcept
{
	if ( m_pMemory == NULL ) {
		return true;
	}
	if ( m_nDirectLeases.load( std::memory_order_acquire ) != 0 ) {
		return false;
	}
	m_cacheLock.lock();
	for ( uint32_t i = 0; i < m_nCacheSlots; ++i ) {
		if ( m_pCache[i].nPins.load( std::memory_order_acquire ) != 0 ) {
			m_cacheLock.unlock();
			return false;
		}
	}
	m_cacheLock.unlock();
	uint8_t *pMemory = m_pMemory;
	const size_t nBytes = m_nMemoryBytes;
	const bool owned = m_bOwnsMemory;
	const sirContainerAllocator_t allocator = m_Allocator;
	resetState();
	if ( owned ) {
		allocator.pfnDeallocate( allocator.pContext, pMemory, nBytes, alignof( max_align_t ) );
	}
	return true;
}

bool SIRStringPool::retireScope( void ) noexcept
{
	if ( !isInitialized() || m_nDirectLeases.load( std::memory_order_acquire ) != 0 ) {
		return false;
	}
	/* Caller establishes an exclusive retirement boundary. Locks guard
	   internal state while checking for retained compressed-text owners. */
	m_registryLock.lock();
	m_cacheLock.lock();
	for ( uint32_t i = 0; i < m_nCacheSlots; ++i ) {
		if ( m_pCache[i].nPins.load( std::memory_order_acquire ) != 0 ) {
			m_cacheLock.unlock();
			m_registryLock.unlock();
			return false;
		}
	}
	uint32_t freshBase = 0;
	if ( !reserveAtomIds( m_nMaxNames, freshBase ) ) {
		m_cacheLock.unlock();
		m_registryLock.unlock();
		return false;
	}
#if SIR_STRING_GROUPED_INDEX
#if SIR_STRING_ADAPTIVE_INDEX
	if ( m_bGroupedIndex ) {
#endif
		for ( uint32_t i = 0; i < m_nHashBuckets / 8; ++i ) {
			m_pGroupControl[i].store( SIR_GROUP_EMPTY, std::memory_order_release );
		}
		memset( m_pGroupIndices, 0, ( size_t )m_nHashBuckets * 3 );
#if SIR_STRING_ADAPTIVE_INDEX
	} else {
		for ( uint32_t i = 0; i < m_nHashBuckets; ++i ) {
			m_pDynamicIndex[i].store( 0, std::memory_order_release );
		}
	}
#endif
#else
	for ( uint32_t i = 0; i < m_nHashBuckets; ++i ) {
		m_pDynamicIndex[i].store( 0, std::memory_order_release );
	}
#endif
	for ( uint32_t i = 0; i < m_nCacheSlots; ++i ) {
		m_pCache[i].nAtomId.store( 0, std::memory_order_release );
	}
	for ( uint32_t i = 0; i < m_nCookedNames; ++i ) {
		m_pCookedHot[i].store( 0, std::memory_order_release );
	}
	m_nNextUnusedShort = 0;
	m_nNextUnusedLong = m_nShortSlots;
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		m_nTierNext[iTier] = 0;
		m_nTierCursor[iTier] = 0;
		m_nActiveTierSlots[iTier] = m_nInitialTierSlots[iTier];
	}
	m_nActiveShortSlots = m_nInitialShortSlots;
	m_nActiveLongSlots = m_nInitialLongSlots;
	m_nActiveNameBudget = m_nInitialNameBudget < m_nCookedNames ? m_nCookedNames : m_nInitialNameBudget;
	m_nActiveDynamicTextBudget = m_nInitialDynamicTextBudget;
	m_nCacheCursor = 0;
	m_nDynamicUsed = 0;
	m_nNames.store( m_nCookedNames, std::memory_order_release );
	m_nBaseId = freshBase;
	/* The immutable cooked data is already verified and never relocated.
	   Rebuild its index without allocating or reopening the catalog. */
	char text[SIR_MAX_COOKED_TEXT_BYTES + 1];
	for ( uint32_t i = 0; i < m_nCookedNames; ++i ) {
		uint32_t written = 0;
		uint32_t slot = 0;
		const bool decoded = decodeCooked( i, text, sizeof( text ), written );
		assert( decoded );
		const uint64_t hash = lookupHash( text, written );
		const bool inserted = decoded && findInsertionSlot( hash, slot );
		assert( inserted );
		if ( !inserted ) {
			m_cacheLock.unlock();
			m_registryLock.unlock();
			return false;
		}
		publishIndex( slot, hash, i + 1 );
	}
	m_cacheLock.unlock();
	m_registryLock.unlock();
	return true;
}

bool SIRStringPool::isInitialized( void ) const noexcept
{
	return m_nBaseId != 0;
}
uint16_t SIRStringPool::readU16( const uint8_t *p ) noexcept
{
	return ( uint16_t )( p[0] | ( ( uint16_t )p[1] << 8 ) );
}
uint32_t SIRStringPool::readU32( const uint8_t *p ) noexcept
{
	return ( uint32_t )p[0] | ( ( uint32_t )p[1] << 8 ) | ( ( uint32_t )p[2] << 16 ) | ( ( uint32_t )p[3] << 24 );
}
uint64_t SIRStringPool::readU64( const uint8_t *p ) noexcept
{
	return ( uint64_t )readU32( p ) | ( ( uint64_t )readU32( p + 4 ) << 32 );
}
bool SIRStringPool::isValidUtf8( const char *pText, size_t nBytes ) noexcept
{
	for ( size_t i = 0; i < nBytes; ) {
		const uint8_t c = ( uint8_t )pText[i];
		if ( c < 0x80 ) {
			++i;
			continue;
		}
		uint32_t cp = 0, more = 0;
		if ( c >= 0xc2 && c <= 0xdf ) {
			cp = c & 0x1f;
			more = 1;
		} else if ( c >= 0xe0 && c <= 0xef ) {
			cp = c & 0x0f;
			more = 2;
		} else if ( c >= 0xf0 && c <= 0xf4 ) {
			cp = c & 7;
			more = 3;
		} else {
			return false;
		}
		if ( more > nBytes - i - 1 ) {
			return false;
		}
		for ( uint32_t j = 0; j < more; ++j ) {
			const uint8_t d = ( uint8_t )pText[i + j + 1];
			if ( ( d & 0xc0 ) != 0x80 ) {
				return false;
			}
			cp = ( cp << 6 ) | ( d & 0x3f );
		}
		if ( ( more == 2 && cp < 0x800 ) || ( more == 3 && cp < 0x10000 ) || ( cp >= 0xd800 && cp <= 0xdfff ) || cp > 0x10ffff ) {
			return false;
		}
		i += more + 1;
	}
	return true;
}

bool SIRStringPool::loadCatalog( const uint8_t *data, size_t size ) noexcept
{
	if ( size < 48 || memcmp( data, "SIRS", 4 ) != 0 || readU16( data + 4 ) != SIR_STRING_CATALOG_VERSION || readU16( data + 6 ) != 0 ||
	     readU32( data + 44 ) != 48 ) {
		return false;
	}
	const uint32_t count = readU32( data + 8 );
	const uint32_t recBytes = readU32( data + 12 );
	const uint32_t frozenBytes = readU32( data + 16 );
	const uint32_t dictionaryBytes = readU32( data + 20 );
	const uint32_t blobBytes = readU32( data + 24 );
	if ( count != m_nCookedNames || ( uint64_t )count * 8 != recBytes || dictionaryBytes != 0 || frozenBytes != 0 ||
	     ( uint64_t )recBytes + frozenBytes + blobBytes != size - 48 ) {
		return false;
	}
	const uint8_t *payload = data + 48;
	if ( hashCookedPayload( ( const char * )payload, size - 48 ) != readU64( data + 28 ) ) {
		return false;
	}
	m_nCatalogHash = readU64( data + 36 );
	m_pBlob = payload + recBytes;
	m_nBlobBytes = blobBytes;
	uint64_t corpus = 14695981039346656037ULL;
	char text[SIR_MAX_COOKED_TEXT_BYTES + 1];
	char previous[SIR_MAX_COOKED_TEXT_BYTES + 1];
	uint32_t previousBytes = 0;
	for ( uint32_t i = 0; i < count; ++i ) {
		const uint8_t *record = payload + i * 8;
		sirStringPoolCookedEntry_t& entry = m_pCooked[i];
		entry.nOffset = readU32( record );
		entry.nLength = readU16( record + 4 );
		entry.nFlags = readU16( record + 6 );
		const uint32_t encoded = entry.nFlags & 0x3fffU;
		const uint16_t codec = entry.nFlags & 0xc000U;
		if ( entry.nLength > SIR_MAX_COOKED_TEXT_BYTES || codec == 0x4000U || codec == 0xc000U || entry.nOffset > m_nBlobBytes ||
		     encoded > m_nBlobBytes - entry.nOffset ||
		     ( codec == 0x8000U &&
		       ( encoded != entry.nLength || encoded >= m_nBlobBytes - entry.nOffset || m_pBlob[entry.nOffset + encoded] != 0 ) ) ) {
			return false;
		}
		uint32_t written = 0;
		if ( !decodeCooked( i, text, sizeof( text ), written ) || written != entry.nLength || !isValidUtf8( text, written ) ) {
			return false;
		}
		if ( i != 0 ) {
			const size_t n = written < previousBytes ? written : previousBytes;
			const int order = memcmp( previous, text, n );
			if ( order > 0 || ( order == 0 && previousBytes >= written ) ) {
				return false;
			}
		}
		memcpy( previous, text, written );
		previousBytes = written;
		m_pCookedHashes[i] = hashString( text, written );
		for ( uint32_t j = 0; j < written; ++j ) {
			corpus = ( corpus ^ ( uint8_t )text[j] ) * 1099511628211ULL;
		}
		corpus = ( corpus ^ 0xff ) * 1099511628211ULL;
	}
	if ( corpus != m_nCatalogHash ) {
		return false;
	}
	/* Publish a single resident flat index for both cooked and dynamic names.
	   Static ordinals are provided by the deterministic generated header. */
	for ( uint32_t i = 0; i < count; ++i ) {
		uint32_t slot = 0;
		uint32_t written = 0;
		if ( !decodeCooked( i, text, sizeof( text ), written ) || !findInsertionSlot( lookupHash( text, written ), slot ) ) {
			return false;
		}
		publishIndex( slot, lookupHash( text, written ), i + 1 );
	}
	return true;
}

bool SIRStringPool::decodeCooked( uint32_t index, char *out, size_t capacity, uint32_t& written ) const noexcept
{
	written = 0;
	if ( !out || index >= m_nCookedNames ) {
		return false;
	}
	const sirStringPoolCookedEntry_t& entry = m_pCooked[index];
	if ( capacity <= entry.nLength ) {
		return false;
	}
	const uint32_t encoded = entry.nFlags & 0x3fffU;
	const uint16_t codec = entry.nFlags & 0xc000U;
	if ( entry.nOffset > m_nBlobBytes || encoded > m_nBlobBytes - entry.nOffset ) {
		return false;
	}
	const uint8_t *source = m_pBlob + entry.nOffset;
	if ( codec == 0x8000U ) {
		memcpy( out, source, entry.nLength );
		out[entry.nLength] = 0;
		written = entry.nLength;
		return true;
	}
	if ( codec != 0 ) {
		return false;
	}
	uint32_t in = 0, pos = 0;
	while ( in < encoded ) {
		const uint8_t token = source[in++];
		if ( token < 128 ) {
			const uint32_t n = ( uint32_t )token + 1;
			if ( n > encoded - in || n > entry.nLength - pos ) {
				return false;
			}
			memcpy( out + pos, source + in, n );
			in += n;
			pos += n;
		} else {
			const uint32_t n = ( token & 127U ) + 3U;
			if ( in == encoded || n > entry.nLength - pos ) {
				return false;
			}
			const uint32_t dist = source[in++];
			if ( dist == 0 || dist > pos ) {
				return false;
			}
			for ( uint32_t k = 0; k < n; ++k ) {
				out[pos + k] = out[pos + k - dist];
			}
			pos += n;
		}
	}
	if ( pos != entry.nLength ) {
		return false;
	}
	out[pos] = 0;
	written = pos;
	return true;
}

uint64_t SIRStringPool::hashString( const char *pText, size_t bytes ) noexcept
{
	uint64_t hash = 14695981039346656037ULL;
	for ( size_t i = 0; i < bytes; ++i ) {
		hash = ( hash ^ ( uint8_t )pText[i] ) * 1099511628211ULL;
	}
	return hash;
}
uint64_t SIRStringPool::lookupHash( const char *pText, size_t bytes ) noexcept
{
#if SIR_STRING_XXH3_HASH
	return XXH3_64bits( pText, bytes );
#else
	return hashString( pText, bytes );
#endif
}
bool SIRStringPool::isPowerOfTwo( uint32_t value ) noexcept
{
	return value && ( value & ( value - 1 ) ) == 0;
}

SIRStringAtom SIRStringPool::makeAtom( uint32_t index ) const noexcept
{
	return SIRStringAtom( m_nBaseId != 0 && index < m_nMaxNames ? m_nBaseId + index : 0 );
}
bool SIRStringPool::resolveIndex( SIRStringAtom atom, uint32_t& index ) const noexcept
{
	const uint32_t id = atom.getId();
	if ( m_nBaseId == 0 || id < m_nBaseId ) {
		return false;
	}
	index = id - m_nBaseId;
	return index < m_nNames.load( std::memory_order_acquire );
}
SIRStringAtom SIRStringPool::atomFromCookedOrdinal( uint32_t ordinal, uint64_t expected ) const noexcept
{
	if ( !isInitialized() || expected != m_nCatalogHash || ordinal >= m_nCookedNames ) {
		return SIRStringAtom();
	}
	return makeAtom( ordinal );
}

bool SIRStringPool::matchesCooked( uint32_t index, const char *text, size_t bytes ) const noexcept
{
	const sirStringPoolCookedEntry_t& entry = m_pCooked[index];
	if ( entry.nLength != bytes ) {
		return false;
	}
	if ( ( entry.nFlags & 0xc000U ) == 0x8000U ) {
		return bytes == 0 || memcmp( m_pBlob + entry.nOffset, text, bytes ) == 0;
	}
	if ( m_nBaseId != 0 ) {
		SIRStringLease lease;
		if ( tryAcquireHot( index, makeAtom( index ).getId(), lease ) ||
		     const_cast<SIRStringPool *>( this )->acquire( makeAtom( index ), lease ) == SIR_STRING_OK ) {
			return bytes == 0 || memcmp( lease.data(), text, bytes ) == 0;
		}
	}
	char scratch[SIR_MAX_COOKED_TEXT_BYTES + 1];
	uint32_t written = 0;
	return decodeCooked( index, scratch, sizeof( scratch ), written ) && written == bytes &&
	       ( bytes == 0 || memcmp( scratch, text, bytes ) == 0 );
}
bool SIRStringPool::matchesDynamic( uint32_t index, const char *text, size_t bytes ) const noexcept
{
	const sirStringPoolDynamicEntry_t& entry = m_pDynamicEntries[index - m_nCookedNames];
	return entry.nLength == bytes && ( bytes == 0 || memcmp( m_pDynamic + entry.nOffset, text, bytes ) == 0 );
}

bool SIRStringPool::findInsertionSlot( uint64_t hash, uint32_t& slot ) const noexcept
{
#if SIR_STRING_GROUPED_INDEX
#if SIR_STRING_ADAPTIVE_INDEX
	if ( !m_bGroupedIndex ) {
		uint32_t at = ( uint32_t )hash & ( m_nHashBuckets - 1 );
		for ( uint32_t scanned = 0; scanned < m_nHashBuckets; ++scanned ) {
			if ( m_pDynamicIndex[at].load( std::memory_order_relaxed ) == 0 ) {
				slot = at;
				return true;
			}
			at = ( at + 1 ) & ( m_nHashBuckets - 1 );
		}
		return false;
	}
#endif
	const uint32_t groups = m_nHashBuckets / 16;
	uint32_t group = ( uint32_t )hash & ( groups - 1 );
	for ( uint32_t scanned = 0; scanned < groups; ++scanned ) {
		const uint64_t lo = m_pGroupControl[group * 2].load( std::memory_order_relaxed );
		const uint64_t hi = m_pGroupControl[group * 2 + 1].load( std::memory_order_relaxed );
		const uint16_t empty = groupMask( lo, hi, 0x80 );
		if ( empty ) {
			slot = group * 16 + firstSetBit( empty );
			return true;
		}
		group = ( group + 1 ) & ( groups - 1 );
	}
	return false;
#else
	uint32_t at = ( uint32_t )hash & ( m_nHashBuckets - 1 );
	for ( uint32_t scanned = 0; scanned < m_nHashBuckets; ++scanned ) {
		if ( m_pDynamicIndex[at].load( std::memory_order_relaxed ) == 0 ) {
			slot = at;
			return true;
		}
		at = ( at + 1 ) & ( m_nHashBuckets - 1 );
	}
	return false;
#endif
}

void SIRStringPool::publishIndex( uint32_t slot, uint64_t hash, uint32_t indexPlus ) noexcept
{
#if SIR_STRING_GROUPED_INDEX
#if SIR_STRING_ADAPTIVE_INDEX
	if ( !m_bGroupedIndex ) {
		m_pDynamicIndex[slot].store( ( ( uint32_t )( hash >> 56 ) << 24 ) | indexPlus, std::memory_order_release );
		return;
	}
#endif
	const size_t at = ( size_t )slot * 3;
	m_pGroupIndices[at] = ( uint8_t )indexPlus;
	m_pGroupIndices[at + 1] = ( uint8_t )( indexPlus >> 8 );
	m_pGroupIndices[at + 2] = ( uint8_t )( indexPlus >> 16 );
	const uint32_t word = slot / 8;
	const uint32_t shift = ( slot & 7 ) * 8;
	const uint64_t original = m_pGroupControl[word].load( std::memory_order_relaxed );
	const uint64_t published = ( original & ~( 0xffULL << shift ) ) | ( ( ( hash >> 57 ) & 0x7fULL ) << shift );
	m_pGroupControl[word].store( published, std::memory_order_release );
#else
	m_pDynamicIndex[slot].store( ( ( uint32_t )( hash >> 56 ) << 24 ) | indexPlus, std::memory_order_release );
#endif
}

uint32_t SIRStringPool::findLocal( const char *text, size_t bytes, uint64_t hash ) const noexcept
{
#if SIR_STRING_GROUPED_INDEX
#if SIR_STRING_ADAPTIVE_INDEX
	if ( !m_bGroupedIndex ) {
		return findLocalFlat( text, bytes, hash );
	}
#endif
	if ( !m_nHashBuckets ) {
		return 0;
	}
	const uint32_t groups = m_nHashBuckets / 16;
	uint32_t group = ( uint32_t )hash & ( groups - 1 );
	const uint8_t fingerprint = ( uint8_t )( hash >> 57 );
	for ( uint32_t scanned = 0; scanned < groups; ++scanned ) {
		const uint64_t lo = m_pGroupControl[group * 2].load( std::memory_order_acquire );
		const uint64_t hi = m_pGroupControl[group * 2 + 1].load( std::memory_order_acquire );
		uint16_t match = groupMask( lo, hi, fingerprint );
		while ( match ) {
			const uint32_t bit = firstSetBit( match );
			match &= ( uint16_t )( match - 1 );
			const size_t at = ( size_t )( group * 16 + bit ) * 3;
			const uint32_t indexPlus = ( uint32_t )m_pGroupIndices[at] | ( ( uint32_t )m_pGroupIndices[at + 1] << 8 ) |
			                           ( ( uint32_t )m_pGroupIndices[at + 2] << 16 );
			if ( indexPlus == 0 ) {
				continue;
			}
			const uint32_t index = indexPlus - 1;
			if ( index < m_nCookedNames ) {
				if ( matchesCooked( index, text, bytes ) ) {
					return indexPlus;
				}
			} else if ( index < m_nNames.load( std::memory_order_acquire ) ) {
				if ( matchesDynamic( index, text, bytes ) ) {
					return indexPlus;
				}
			}
		}
		if ( groupMask( lo, hi, 0x80 ) ) {
			return 0;
		}
		group = ( group + 1 ) & ( groups - 1 );
	}
	return 0;
#else
	return findLocalFlat( text, bytes, hash );
#endif
}

uint32_t SIRStringPool::findLocalFlat( const char *text, size_t bytes, uint64_t hash ) const noexcept
{
#if !SIR_STRING_GROUPED_INDEX || SIR_STRING_ADAPTIVE_INDEX
	if ( !m_nHashBuckets ) {
		return 0;
	}
	uint32_t at = ( uint32_t )hash & ( m_nHashBuckets - 1 );
	const uint32_t fingerprint = ( uint32_t )( hash >> 56 );
	for ( uint32_t step = 0; step < m_nHashBuckets; ++step ) {
		const uint32_t tag = m_pDynamicIndex[at].load( std::memory_order_acquire );
		if ( tag == 0 ) {
			break;
		}
		const uint32_t indexPlus = tag & SIR_ENTRY_INDEX_MASK;
		if ( ( tag >> 24 ) == fingerprint && indexPlus != 0 ) {
			const uint32_t index = indexPlus - 1;
			if ( index < m_nCookedNames ) {
#if SIR_STRING_XXH3_HASH
				if ( matchesCooked( index, text, bytes ) ) {
					return indexPlus;
				}
#else
				if ( m_pCookedHashes[index] == hash && matchesCooked( index, text, bytes ) ) {
					return indexPlus;
				}
#endif
			} else if ( index < m_nNames.load( std::memory_order_acquire ) ) {
#if !SIR_STRING_XXH3_HASH
				const sirStringPoolDynamicEntry_t& entry = m_pDynamicEntries[index - m_nCookedNames];
#endif
#if SIR_STRING_XXH3_HASH
				if ( matchesDynamic( index, text, bytes ) ) {
					return indexPlus;
				}
#else
				if ( entry.nHash == hash && matchesDynamic( index, text, bytes ) ) {
					return indexPlus;
				}
#endif
			}
		}
		at = ( at + 1 ) & ( m_nHashBuckets - 1 );
	}
	return 0;
#else
	( void )text;
	( void )bytes;
	( void )hash;
	return 0;
#endif
}

SIRStringAtom SIRStringPool::find( const char *text ) const noexcept
{
	return text ? find( text, strlen( text ) ) : SIRStringAtom();
}
SIRStringAtom SIRStringPool::find( const char *text, size_t bytes ) const noexcept
{
	if ( !isInitialized() || !text || bytes > UINT32_MAX ) {
		return SIRStringAtom();
	}
	const uint32_t ordinal = findLocal( text, bytes, lookupHash( text, bytes ) );
	return ordinal ? makeAtom( ordinal - 1 ) : SIRStringAtom();
}
SIRStringAtom SIRStringPool::intern( const char *text ) noexcept
{
	return text ? intern( text, strlen( text ) ) : SIRStringAtom();
}
#if defined( __GNUC__ ) || defined( __clang__ )
__attribute__( ( aligned( 64 ) ) )
#endif
SIRStringAtom SIRStringPool::intern( const char *text, size_t bytes ) noexcept
{
	if ( !isInitialized() || !text || bytes > UINT32_MAX ) {
		return SIRStringAtom();
	}
	const uint64_t hash = lookupHash( text, bytes );
	uint32_t found = findLocal( text, bytes, hash );
	if ( found ) {
		return makeAtom( found - 1 );
	}
	m_registryLock.lock();
	found = findLocal( text, bytes, hash );
	if ( found ) {
		m_registryLock.unlock();
		return makeAtom( found - 1 );
	}
	if ( !isValidUtf8( text, bytes ) ) {
		m_registryLock.unlock();
		return SIRStringAtom();
	}
	const uint32_t index = m_nNames.load( std::memory_order_relaxed );
	if ( index >= m_nMaxNames || bytes >= m_nDynamicTextCapacity - m_nDynamicUsed ) {
		m_registryLock.unlock();
		return SIRStringAtom();
	}
	const uint32_t nNameBudget = grownBudget( m_nActiveNameBudget, index + 1, m_nMaxNames );
	const uint32_t nTextBudget = grownBudget( m_nActiveDynamicTextBudget, m_nDynamicUsed + ( uint32_t )bytes + 1, m_nDynamicTextCapacity );
	if ( nNameBudget <= index || nTextBudget < m_nDynamicUsed + ( uint32_t )bytes + 1 ) {
		m_registryLock.unlock();
		return SIRStringAtom();
	}
	uint32_t position = 0;
	if ( !findInsertionSlot( hash, position ) ) {
		m_registryLock.unlock();
		return SIRStringAtom();
	}
	sirStringPoolDynamicEntry_t& entry = m_pDynamicEntries[index - m_nCookedNames];
	new ( &entry ) sirStringPoolDynamicEntry_t();
	entry.nHash = hashString( text, bytes );
	entry.nOffset = m_nDynamicUsed;
	entry.nLength = ( uint32_t )bytes;
	if ( bytes ) {
		memcpy( m_pDynamic + m_nDynamicUsed, text, bytes );
	}
	m_pDynamic[m_nDynamicUsed + bytes] = 0;
	m_nDynamicUsed += ( uint32_t )bytes + 1;
	m_nActiveNameBudget = nNameBudget;
	m_nActiveDynamicTextBudget = nTextBudget;
	m_nNames.store( index + 1, std::memory_order_release );
	publishIndex( position, hash, index + 1 );
	m_registryLock.unlock();
	return makeAtom( index );
}

bool SIRStringPool::tryAcquireHot( uint32_t index, uint32_t atomId, SIRStringLease& lease ) const noexcept
{
	if ( index >= m_nCookedNames ) {
		return false;
	}
	const uint32_t hot = m_pCookedHot[index].load( std::memory_order_acquire );
	if ( hot == 0 || hot > m_nCacheSlots ) {
		return false;
	}
	sirStringPoolCacheSlot_t& slot = m_pCache[hot - 1];
	if ( slot.nAtomId.load( std::memory_order_acquire ) != atomId ) {
		return false;
	}
	uint32_t pins = slot.nPins.load( std::memory_order_acquire );
	while ( pins < UINT32_MAX - 1 ) {
		if ( slot.nPins.compare_exchange_weak( pins, pins + 1, std::memory_order_acq_rel, std::memory_order_acquire ) ) {
			if ( slot.nAtomId.load( std::memory_order_acquire ) != atomId ) {
				slot.nPins.fetch_sub( 1, std::memory_order_acq_rel );
				return false;
			}
			lease.bind( const_cast<SIRStringPool *>( this ), { hot - 1, atomId, slot.nGeneration, slot.pszData, slot.nBytes } );
			return true;
		}
	}
	return false;
}

sirStringStatus_t SIRStringPool::acquireTiered( uint32_t nIndex, SIRStringAtom atom, SIRStringLease& lease ) noexcept
{
	m_cacheLock.lock();
	if ( tryAcquireHot( nIndex, atom.getId(), lease ) ) {
		m_cacheLock.unlock();
		return SIR_STRING_OK;
	}
	const uint32_t nBytes = m_pCooked[nIndex].nLength;
	uint32_t nVictim = m_nCacheSlots;
	uint32_t nSelectedTier = SIR_STRING_CACHE_TIER_COUNT;
	uint32_t nTargetActive = 0;
	for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
		if ( nBytes > m_nTierBytes[iTier] || m_nTierSlots[iTier] == 0 ) {
			continue;
		}
		const uint32_t nStart = m_nTierStart[iTier];
		const uint32_t nSlots = m_nTierSlots[iTier];
		nTargetActive = m_nActiveTierSlots[iTier];
		if ( m_nTierNext[iTier] < nSlots ) {
			if ( m_nTierNext[iTier] >= nTargetActive ) {
				nTargetActive = grownBudget( nTargetActive, m_nTierNext[iTier] + 1, nSlots );
			}
			if ( m_nTierNext[iTier] < nTargetActive ) {
				const uint32_t iSlot = nStart + m_nTierNext[iTier];
				uint32_t nExpected = 0;
				if ( m_pCache[iSlot].nPins.compare_exchange_strong( nExpected, UINT32_MAX, std::memory_order_acq_rel,
				                                                    std::memory_order_acquire ) ) {
					nVictim = iSlot;
					nSelectedTier = iTier;
					break;
				}
			}
		}
		/* Reuse only slots already made active. When the first class is
		   fully pinned, a larger compatible class can absorb the miss. */
		for ( uint32_t nProbe = 0; nProbe < m_nTierNext[iTier]; ++nProbe ) {
			const uint32_t iLocal = ( m_nTierCursor[iTier] + nProbe ) % nSlots;
			const uint32_t iSlot = nStart + iLocal;
			uint32_t nExpected = 0;
			if ( m_pCache[iSlot].nPins.compare_exchange_strong( nExpected, UINT32_MAX, std::memory_order_acq_rel,
			                                                    std::memory_order_acquire ) ) {
				nVictim = iSlot;
				nSelectedTier = iTier;
				break;
			}
		}
		if ( nVictim != m_nCacheSlots ) {
			break;
		}
	}
	if ( nVictim == m_nCacheSlots ) {
		m_cacheLock.unlock();
		return nBytes > m_nTierBytes[SIR_STRING_CACHE_TIER_COUNT - 1] ? SIR_STRING_OUTPUT_TOO_SMALL : SIR_STRING_ALL_SLOTS_PINNED;
	}
	sirStringPoolCacheSlot_t& slot = m_pCache[nVictim];
	const uint32_t nOldId = slot.nAtomId.load( std::memory_order_relaxed );
	if ( nOldId >= m_nBaseId && m_nBaseId != 0 ) {
		const uint32_t nOldIndex = nOldId - m_nBaseId;
		if ( nOldIndex < m_nCookedNames ) {
			m_pCookedHot[nOldIndex].store( 0, std::memory_order_release );
		}
	}
	slot.nAtomId.store( 0, std::memory_order_release );
	uint32_t nWritten = 0;
	if ( !decodeCooked( nIndex, slot.pszData, ( size_t )slot.nCapacity + 1, nWritten ) ) {
		slot.nPins.store( 0, std::memory_order_release );
		m_cacheLock.unlock();
		return SIR_STRING_CORRUPT_CATALOG;
	}
	slot.nBytes = nWritten;
	++slot.nGeneration;
	if ( slot.nGeneration == 0 ) {
		++slot.nGeneration;
	}
	if ( nOldId == 0 ) {
		++m_nTierNext[nSelectedTier];
	}
	m_nActiveTierSlots[nSelectedTier] = nTargetActive;
	m_nTierCursor[nSelectedTier] = ( nVictim - m_nTierStart[nSelectedTier] + 1 ) % m_nTierSlots[nSelectedTier];
	slot.nAtomId.store( atom.getId(), std::memory_order_release );
	slot.nPins.store( 1, std::memory_order_release );
	m_pCookedHot[nIndex].store( nVictim + 1, std::memory_order_release );
	lease.bind( this, { nVictim, atom.getId(), slot.nGeneration, slot.pszData, nWritten } );
	m_cacheLock.unlock();
	return SIR_STRING_OK;
}

sirStringStatus_t SIRStringPool::acquire( SIRStringAtom atom, SIRStringLease& lease ) noexcept
{
	if ( lease.m_pPool == this && lease.m_nAtomId == atom.getId() ) {
		return SIR_STRING_OK;
	}
	lease.release();
	uint32_t index = 0;
	if ( !isInitialized() ) {
		return SIR_STRING_NOT_INITIALIZED;
	}
	if ( !resolveIndex( atom, index ) ) {
		return SIR_STRING_INVALID_ATOM;
	}
	if ( index >= m_nCookedNames || ( m_pCooked[index].nFlags & 0xc000U ) == 0x8000U ) {
		const char *pText;
		uint32_t bytes;
		if ( index >= m_nCookedNames ) {
			const sirStringPoolDynamicEntry_t& entry = m_pDynamicEntries[index - m_nCookedNames];
			pText = ( const char * )m_pDynamic + entry.nOffset;
			bytes = entry.nLength;
		} else {
			const sirStringPoolCookedEntry_t& entry = m_pCooked[index];
			pText = ( const char * )m_pBlob + entry.nOffset;
			bytes = entry.nLength;
		}
		m_nDirectLeases.fetch_add( 1, std::memory_order_acq_rel );
		lease.bind( this, { SIR_DIRECT_TEXT_SLOT, atom.getId(), 0, pText, bytes } );
		return SIR_STRING_OK;
	}
	if ( tryAcquireHot( index, atom.getId(), lease ) ) {
		return SIR_STRING_OK;
	}
	if ( m_bTieredCache ) {
		return acquireTiered( index, atom, lease );
	}
	m_cacheLock.lock();
	if ( tryAcquireHot( index, atom.getId(), lease ) ) {
		m_cacheLock.unlock();
		return SIR_STRING_OK;
	}
	const uint32_t bytes = m_pCooked[index].nLength;
	uint32_t victim = m_nCacheSlots;
	uint32_t nTargetShortSlots = m_nActiveShortSlots;
	uint32_t nTargetLongSlots = m_nActiveLongSlots;
	/* The cache lock exclusively owns these virgin cursors. Until its
	   first successful decode a slot has no published atom and cannot
	   be pinned by another thread. Selecting one is constant time. */
	if ( bytes <= m_nShortBytes && m_nNextUnusedShort < m_nShortSlots ) {
		if ( m_nNextUnusedShort >= nTargetShortSlots ) {
			nTargetShortSlots = grownBudget( nTargetShortSlots, m_nNextUnusedShort + 1, m_nShortSlots );
		}
		if ( m_nNextUnusedShort < nTargetShortSlots ) {
			victim = m_nNextUnusedShort;
		}
	}
	if ( victim == m_nCacheSlots && bytes <= m_nLongBytes && m_nNextUnusedLong < m_nCacheSlots ) {
		const uint32_t nLongUsed = m_nNextUnusedLong - m_nShortSlots;
		if ( nLongUsed >= nTargetLongSlots ) {
			nTargetLongSlots = grownBudget( nTargetLongSlots, nLongUsed + 1, m_nLongSlots );
		}
		if ( nLongUsed < nTargetLongSlots ) {
			victim = m_nNextUnusedLong;
		}
	}
	if ( victim < m_nCacheSlots ) {
		uint32_t expected = 0;
		if ( !m_pCache[victim].nPins.compare_exchange_strong( expected, UINT32_MAX, std::memory_order_acq_rel,
		                                                      std::memory_order_acquire ) ) {
			m_cacheLock.unlock();
			return SIR_STRING_ALL_SLOTS_PINNED;
		}
	} else {
		/* Once the compatible virgin slots are used, a single circular
		   scan finds an unpinned resident. No speculative empty scan. */
		uint32_t i = m_nCacheCursor;
		for ( uint32_t inspected = 0; inspected < m_nCacheSlots; ++inspected ) {
			if ( ( i >= m_nActiveShortSlots && i < m_nShortSlots ) || i >= m_nShortSlots + m_nActiveLongSlots ) {
				++i;
				if ( i == m_nCacheSlots ) {
					i = 0;
				}
				continue;
			}
			sirStringPoolCacheSlot_t& slot = m_pCache[i];
			if ( slot.nCapacity >= bytes ) {
				uint32_t expected = 0;
				if ( slot.nPins.compare_exchange_strong( expected, UINT32_MAX, std::memory_order_acq_rel, std::memory_order_acquire ) ) {
					victim = i;
					break;
				}
			}
			++i;
			if ( i == m_nCacheSlots ) {
				i = 0;
			}
		}
	}
	if ( victim == m_nCacheSlots ) {
		m_cacheLock.unlock();
		return bytes > m_nLongBytes || m_nCacheSlots == 0 ? SIR_STRING_OUTPUT_TOO_SMALL : SIR_STRING_ALL_SLOTS_PINNED;
	}
	sirStringPoolCacheSlot_t& slot = m_pCache[victim];
	const uint32_t oldId = slot.nAtomId.load( std::memory_order_relaxed );
	if ( oldId >= m_nBaseId && m_nBaseId != 0 ) {
		const uint32_t oldIndex = oldId - m_nBaseId;
		if ( oldIndex < m_nCookedNames ) {
			m_pCookedHot[oldIndex].store( 0, std::memory_order_release );
		}
	}
	slot.nAtomId.store( 0, std::memory_order_release );
	uint32_t written = 0;
	if ( !decodeCooked( index, slot.pszData, ( size_t )slot.nCapacity + 1, written ) ) {
		slot.nPins.store( 0, std::memory_order_release );
		m_cacheLock.unlock();
		return SIR_STRING_CORRUPT_CATALOG;
	}
	slot.nBytes = written;
	++slot.nGeneration;
	if ( slot.nGeneration == 0 ) {
		++slot.nGeneration;
	}
	m_nActiveShortSlots = nTargetShortSlots;
	m_nActiveLongSlots = nTargetLongSlots;
	slot.nAtomId.store( atom.getId(), std::memory_order_release );
	slot.nPins.store( 1, std::memory_order_release );
	m_pCookedHot[index].store( victim + 1, std::memory_order_release );
	if ( oldId == 0 ) {
		if ( victim < m_nShortSlots ) {
			++m_nNextUnusedShort;
		} else {
			++m_nNextUnusedLong;
		}
	}
	m_nCacheCursor = victim + 1;
	if ( m_nCacheCursor == m_nCacheSlots ) {
		m_nCacheCursor = 0;
	}
	lease.bind( this, { victim, atom.getId(), slot.nGeneration, slot.pszData, written } );
	m_cacheLock.unlock();
	return SIR_STRING_OK;
}

void SIRStringPool::releaseLease( uint32_t slotIndex, uint32_t generation ) noexcept
{
	if ( slotIndex == SIR_DIRECT_TEXT_SLOT ) {
		const uint32_t previous = m_nDirectLeases.fetch_sub( 1, std::memory_order_acq_rel );
		assert( previous != 0 );
		return;
	}
	sirStringPoolCacheSlot_t& slot = m_pCache[slotIndex];
	assert( slot.nGeneration == generation );
	const uint32_t previous = slot.nPins.fetch_sub( 1, std::memory_order_acq_rel );
	assert( previous != 0 );
}

const char *SIRStringPool::peekRaw( SIRStringAtom atom, uint32_t& nBytes ) const noexcept
{
	nBytes = 0;
	uint32_t index = 0;
	if ( !resolveIndex( atom, index ) ) {
		return NULL;
	}
	if ( index >= m_nCookedNames ) {
		const sirStringPoolDynamicEntry_t& entry = m_pDynamicEntries[index - m_nCookedNames];
		nBytes = entry.nLength;
		return ( const char * )m_pDynamic + entry.nOffset;
	}
	const sirStringPoolCookedEntry_t& entry = m_pCooked[index];
	if ( ( entry.nFlags & 0xc000U ) != 0x8000U ) {
		return NULL;
	}
	nBytes = entry.nLength;
	return ( const char * )m_pBlob + entry.nOffset;
}

sirStringStatus_t SIRStringPool::decodeInto( SIRStringAtom atom, char *out, size_t capacity, uint32_t& written ) const noexcept
{
	written = 0;
	if ( !isInitialized() ) {
		return SIR_STRING_NOT_INITIALIZED;
	}
	uint32_t index;
	if ( !resolveIndex( atom, index ) ) {
		return SIR_STRING_INVALID_ATOM;
	}
	const uint32_t bytes = index < m_nCookedNames ? m_pCooked[index].nLength : m_pDynamicEntries[index - m_nCookedNames].nLength;
	if ( !out || capacity <= bytes ) {
		return SIR_STRING_OUTPUT_TOO_SMALL;
	}
	if ( index < m_nCookedNames && ( m_pCooked[index].nFlags & 0xc000U ) == 0 ) {
		SIRStringLease lease;
		if ( const_cast<SIRStringPool *>( this )->acquire( atom, lease ) == SIR_STRING_OK ) {
			memcpy( out, lease.data(), bytes );
			out[bytes] = 0;
			written = bytes;
			return SIR_STRING_OK;
		}
		return decodeCooked( index, out, capacity, written ) ? SIR_STRING_OK : SIR_STRING_CORRUPT_CATALOG;
	}
	const char *pText = index < m_nCookedNames ? ( const char * )m_pBlob + m_pCooked[index].nOffset
	                                           : ( const char * )m_pDynamic + m_pDynamicEntries[index - m_nCookedNames].nOffset;
	memcpy( out, pText, bytes );
	out[bytes] = 0;
	written = bytes;
	return SIR_STRING_OK;
}

uint32_t SIRStringPool::getDecodedByteLength( SIRStringAtom atom ) const noexcept
{
	uint32_t index;
	if ( !resolveIndex( atom, index ) ) {
		return 0;
	}
	return index < m_nCookedNames ? m_pCooked[index].nLength : m_pDynamicEntries[index - m_nCookedNames].nLength;
}
uint64_t SIRStringPool::getContentHash( SIRStringAtom atom ) const noexcept
{
	uint32_t index;
	if ( !resolveIndex( atom, index ) ) {
		return 0;
	}
	return index < m_nCookedNames ? m_pCookedHashes[index] : m_pDynamicEntries[index - m_nCookedNames].nHash;
}
uint32_t SIRStringPool::getStringCount( void ) const noexcept
{
	return m_nNames.load( std::memory_order_acquire );
}
uint32_t SIRStringPool::getSlotCount( void ) const noexcept
{
	return m_nHashBuckets;
}
uint32_t SIRStringPool::getBlockCount( void ) const noexcept
{
	return isInitialized() ? 1 : 0;
}
uint64_t SIRStringPool::getReservedBytes( void ) const noexcept
{
	return m_nMemoryBytes;
}
sirStringPoolStats_t SIRStringPool::getStats( void ) const noexcept
{
	sirStringPoolStats_t stats;
	memset( &stats, 0, sizeof( stats ) );
	stats.nStringCount = getStringCount();
	stats.nSlotCount = getSlotCount();
	stats.nDecodedSlotCount = m_nCacheSlots;
	stats.nBlockCount = getBlockCount();
	stats.nReservedBytes = getReservedBytes();
	m_registryLock.lock();
	stats.nDynamicTextBytes = m_nDynamicUsed;
	stats.nActiveNameBudget = m_nActiveNameBudget;
	stats.nActiveDynamicTextBudget = m_nActiveDynamicTextBudget;
	m_registryLock.unlock();
	stats.nPinnedLeases = m_nDirectLeases.load( std::memory_order_acquire );
	if ( m_pCache ) {
		m_cacheLock.lock();
		if ( m_bTieredCache ) {
			for ( uint32_t iTier = 0; iTier < SIR_STRING_CACHE_TIER_COUNT; ++iTier ) {
				stats.nActiveTierSlots[iTier] = m_nActiveTierSlots[iTier];
				if ( iTier < 3 ) {
					stats.nActiveShortSlots += m_nActiveTierSlots[iTier];
				} else {
					stats.nActiveLongSlots += m_nActiveTierSlots[iTier];
				}
			}
		} else {
			stats.nActiveShortSlots = m_nActiveShortSlots;
			stats.nActiveLongSlots = m_nActiveLongSlots;
		}
		for ( uint32_t i = 0; i < m_nCacheSlots; ++i ) {
			if ( m_pCache[i].nAtomId.load( std::memory_order_acquire ) ) {
				++stats.nCachedStrings;
			}
			stats.nPinnedLeases += m_pCache[i].nPins.load( std::memory_order_acquire );
		}
		m_cacheLock.unlock();
	}
	return stats;
}
