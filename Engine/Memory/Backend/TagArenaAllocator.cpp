#include "TagArenaAllocator.h"
#include <Engine/Core/ThreadSystem/Thread.h>

#define UNOWNED    ((void *)666)
#define ZONEID     0xa21d49

int CTagArenaAllocator::nMinFragment = MIN_FRAGMENT;

typedef struct memblock_s {
	struct memblock_s		*next, *prev;
	CThreadAtomic<uint64_t>	size;	// including the header and possibly tiny fragments
	CThreadAtomic<uint64_t>	tag;	// a tag of 0 is a free block
	CThreadAtomic<int64_t>	id;		// should be ZONEID
#if defined(SIRENGINE_MEMORY_DEBUG)
	zonedebug_t d;
#endif
} memblock_t;

typedef struct freeblock_s {
	struct freeblock_s *prev;
	struct freeblock_s *next;
} freeblock_t;

typedef struct memzone_s {
	CThreadAtomic<uint64_t>	size;			// total bytes malloced, including header
	CThreadAtomic<uint64_t>	used;			// total bytes used
	memblock_t				blocklist;		// start / end cap for linked list
#if defined(USE_MULTI_SEGMENT)
	memblock_t				dummy0;		// just to allocate some space before freelist
	freeblock_t				freelist_tiny;
	memblock_t				dummy1;
	freeblock_t				freelist_small;
	memblock_t				dummy2;
	freeblock_t				freelist_medium;
	memblock_t				dummy3;
	freeblock_t				freelist;
#else
	memblock_t				*rover;
#endif
	CThreadMutex 			allocLock;
	CThreadMutex			deallocLock;
} memzone_t;

#ifdef USE_MULTI_SEGMENT

static void InitFree( memzone_t *zone, freeblock_t *fb )
{
	memblock_t *block = (memblock_t *)( (byte *)fb - sizeof( memblock_t ) );
	memset( block, 0, sizeof( *block ) );
}


static void RemoveFree( memzone_t *zone, memblock_t *block )
{
	freeblock_t *fb = (freeblock_t *)( block + 1 );
	freeblock_t *prev;
	freeblock_t *next;

#ifdef SIRENGINE_MEMORY_DEBUG
	if ( fb->next == NULL || fb->prev == NULL || fb->next == fb || fb->prev == fb ) {
		SIRENGINE_ERROR( "RemoveFree: bad pointers fb->next: %p, fb->prev: %p\n", fb->next, fb->prev );
	}
#endif

	prev = fb->prev;
	next = fb->next;

	prev->next = next;
	next->prev = prev;
}


static void InsertFree( memzone_t *zone, memblock_t *block )
{
	freeblock_t *fb = (freeblock_t*)( block + 1 );
	freeblock_t *prev, *next;

#ifdef TINY_SIZE
	if ( block->size <= TINY_SIZE )
		prev = &zone->freelist_tiny;
	else
#endif
#ifdef SMALL_SIZE
	if ( block->size <= SMALL_SIZE )
		prev = &zone->freelist_small;
	else
#endif
#ifdef MEDIUM_SIZE
	if ( block->size <= MEDIUM_SIZE )
		prev = &zone->freelist_medium;
	else
#endif
		prev = &zone->freelist;

	next = prev->next;

#ifdef SIRENGINE_MEMORY_DEBUG
	if ( block->size < sizeof( *fb ) + sizeof( *block ) ) {
		SIRENGINE_ERROR( "InsertFree: bad block size: %lu\n", block->size );
	}
#endif

	prev->next = fb;
	next->prev = fb;

	fb->prev = prev;
	fb->next = next;
}


/*
================
NewBlock

Allocates new free block within specified memory zone

Separator is needed to avoid additional runtime checks in Z_Free()
to prevent merging it with previous free block
================
*/
static freeblock_t *NewBlock( memzone_t *zone, uint64_t size )
{
	memblock_t *prev, *next;
	memblock_t *block, *sep;
	uint64_t alloc_size;

	// zone->prev is pointing on last block in the list
	prev = zone->blocklist.prev;
	next = prev->next;

	size = SIRENGINE_PAD( size, 1<<21 ); // round up to 2M blocks
	// allocate separator block before new free block
	alloc_size = size + sizeof( *sep );

	sep = (memblock_t *)calloc( alloc_size, 1 );
//	sep = (memblock_t *)g_pApplication->VirtualAlloc( &alloc_size, 64 );
	if ( sep == NULL ) {
		g_pApplication->OnOutOfMemory();
		return NULL;
	}
	block = sep+1;

//	g_pApplication->CommitMemory( sep, 0, alloc_size );

	{
		// link separator with prev
		prev->next = sep;
		sep->prev = prev;

		// link separator with block
		sep->next = block;
		block->prev = sep;

		// link block with next
		block->next = next;
		next->prev = block;

		sep->tag.store( TAG_STATIC ); // in-use block
		sep->id.store( -ZONEID );
		sep->size.store( 0 );

		block->tag.store( TAG_FREE );
		block->id.store( ZONEID );
		block->size.store( size );

		// update zone statistics
		zone->size.fetch_add( alloc_size );
		zone->used.fetch_add( sizeof( *sep ) );
	}

	InsertFree( zone, block );

	return (freeblock_t *)( block + 1 );
}


static memblock_t *SearchFree( memzone_t *zone, uint64_t size )
{
	const freeblock_t *fb;
	memblock_t *base;

#ifdef TINY_SIZE
	if ( size <= TINY_SIZE )
		fb = zone->freelist_tiny.DIRECTION;
	else
#endif
#ifdef SMALL_SIZE
	if ( size <= SMALL_SIZE )
		fb = zone->freelist_small.DIRECTION;
	else
#endif
#ifdef MEDIUM_SIZE
	if ( size <= MEDIUM_SIZE )
		fb = zone->freelist_medium.DIRECTION;
	else
#endif
		fb = zone->freelist.DIRECTION;

	for ( ;; ) {
		// not found, allocate new segment?
		if ( fb == &zone->freelist ) {
			fb = NewBlock( zone, size );
		} else {
#ifdef TINY_SIZE
			if ( fb == &zone->freelist_tiny ) {
				fb = zone->freelist_small.DIRECTION;
				continue;
			}
#endif
#ifdef SMALL_SIZE
			if ( fb == &zone->freelist_small ) {
				fb = zone->freelist_medium.DIRECTION;
				continue;
			}
#endif
#ifdef MEDIUM_SIZE
			if ( fb == &zone->freelist_medium ) {
				fb = zone->freelist.DIRECTION;
				continue;
			}
#endif
		}
		base = (memblock_t *)( (byte *) fb - sizeof( *base ) );
		fb = fb->DIRECTION;
		if ( base->size.load() >= size ) {
			return base;
		}
	}
	return NULL;
}
#endif // USE_MULTI_SEGMENT


/*
========================
Z_ClearZone
========================
*/
static void Z_ClearZone( memzone_t *zone, memzone_t *head, uint64_t size, uint64_t segnum )
{
	memblock_t *block;
	int min_fragment;

#ifdef USE_MULTI_SEGMENT
	min_fragment = sizeof( memblock_t ) + sizeof( freeblock_t );
#else
	min_fragment = sizeof( memblock_t );
#endif

	CThreadAutoLock<CThreadMutex> _( zone->allocLock );
	if ( CTagArenaAllocator::nMinFragment < min_fragment ) {
		// in debug mode size of memblock_t may exceed MIN_FRAGMENT
		CTagArenaAllocator::nMinFragment = SIRENGINE_PAD( min_fragment, sizeof( intptr_t ) );
		SIRENGINE_LOG( "minFragment adjusted to %i bytes", CTagArenaAllocator::nMinFragment );
	}

	// set the entire zone to one free block
	zone->blocklist.next = zone->blocklist.prev = block = (memblock_t *)( zone + 1 );
	zone->blocklist.tag.store( TAG_STATIC ); // in use block
	zone->blocklist.id.store( -ZONEID );
	zone->blocklist.size.store( 0 );
#ifndef USE_MULTI_SEGMENT
	zone->rover = block;
#endif
	zone->size.store( size );
	zone->used.store( 0 );

	block->prev = block->next = &zone->blocklist;
	block->tag.store( TAG_FREE );	// free block
	block->id.store( ZONEID );

	block->size.store( size - sizeof( memzone_t ) );

#ifdef USE_MULTI_SEGMENT
	InitFree( zone, &zone->freelist );
	zone->freelist.next = zone->freelist.prev = &zone->freelist;

	InitFree( zone, &zone->freelist_medium );
	zone->freelist_medium.next = zone->freelist_medium.prev = &zone->freelist_medium;

	InitFree( zone, &zone->freelist_small );
	zone->freelist_small.next = zone->freelist_small.prev = &zone->freelist_small;

	InitFree( zone, &zone->freelist_tiny );
	zone->freelist_tiny.next = zone->freelist_tiny.prev = &zone->freelist_tiny;

	InsertFree( zone, block );
#endif
}

/*
========================
Z_AvailableZoneMemory
========================
*/
static uint64_t Z_AvailableZoneMemory( const memzone_t *zone ) {
#ifdef USE_MULTI_SEGMENT
	return ( 1024 * 1024 * 1024 ); // unlimited
#else
	return zone->size.load() - zone->used.load();
#endif
}



static void MergeBlock( memblock_t *curr_free, const memblock_t *next )
{
	curr_free->size.fetch_add( next->size );
	curr_free->next = next->next;
	curr_free->next->prev = curr_free;
}


/*
========================
Z_Free
========================
*/
void Z_Free( memzone_t *zone, void *ptr ) {
	memblock_t *block, *other;

	if ( !ptr ) {
		SIRENGINE_WARNING( "Z_Free: NULL pointer" );
	}

	CThreadAutoLock<CThreadMutex> _( zone->deallocLock );

	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if ( block->id.load() != ZONEID ) {
		SIRENGINE_ERROR( "Z_Free: freed a pointer without ZONEID" );
	}

	if ( block->tag.load() == TAG_FREE ) {
		SIRENGINE_ERROR( "Z_Free: freed a freed pointer" );
	}

	// check the memory trash tester
#ifdef USE_TRASH_TEST
	if ( *(int32_t *)((byte *)block + block->size.load() - 4 ) != ZONEID ) {
		SIRENGINE_ERROR( "Z_Free: memory block wrote past end" );
	}
#endif

	{
		zone->used.fetch_sub( block->size.load() );

		// set the block to something that should cause problems
		// if it is referenced...
		memset( ptr, 0xaa, block->size - sizeof( *block ) );

		block->tag.store( TAG_FREE ); // mark as free
		block->id.store( ZONEID );

		other = block->prev;
		if ( other->tag == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
			RemoveFree( zone, other );
#endif
			// merge with previous free block
			MergeBlock( other, block );
#ifndef USE_MULTI_SEGMENT
			if ( block == zone->rover ) {
				zone->rover = other;
			}
#endif
			block = other;
		}

#ifndef USE_MULTI_SEGMENT
		zone->rover = block;
#endif
	}

	other = block->next;

	if ( other->tag.load() == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
		RemoveFree( zone, other );
#endif
		// merge the next free block onto the end
		MergeBlock( block, other );
	}

#ifdef USE_MULTI_SEGMENT
	InsertFree( zone, block );
#endif
}


/*
================
Z_FreeTags
================
*/
uint64_t Z_FreeTags( memzone_t *zone, uint64_t tag )
{
	uint64_t count;
	uint64_t size;
	memblock_t *block, *freed;

	count = 0;
	size = 0;
	CThreadAutoLock<CThreadMutex> _( zone->deallocLock );
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag.load() == tag && block->id.load() == ZONEID ) {
			if ( block->prev->tag.load() == TAG_FREE ) {
				freed = block->prev;  // current block will be merged with previous
			} else {
				freed = block; // will leave in place
			}
			
			size += block->size;
			Z_Free( zone, (void *)( block + 1 ) );

			block = freed;
			count++;
		}
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		block = block->next;
	}
	SIRENGINE_LOG( "Z_FreeTags: tag %i, %lu bytes released.", (int)tag, size );

	return count;
}


/*
================
Z_Alloc
================
*/
#ifdef SIRENGINE_MEMORY_DEBUG
void *Z_AllocDebug( memzone_t *zone, uint64_t size, uint64_t tag, const char *label, const char *file, uint32_t line )
#else
void *Z_Alloc( memzone_t *zone, uint64_t size, uint64_t tag )
#endif
{
	int extra;
#ifdef SIRENGINE_MEMORY_DEBUG
	uint64_t allocSize;
#endif
#ifndef USE_MULTI_SEGMENT
	memblock_t *start, *rover;
#endif
	memblock_t *base;

	if ( tag == TAG_FREE ) {
		SIRENGINE_ERROR( "Z_Malloc: tried to use with TAG_FREE" );
	}
#ifdef SIRENGINE_MEMORY_DEBUG
	allocSize = size;
#endif

#ifdef USE_MULTI_SEGMENT
	if ( size < ( sizeof( freeblock_t ) ) ) {
		size = ( sizeof( freeblock_t ) );
	}
#endif

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof( *base );	// account for size of block header
#ifdef USE_TRASH_TEST
	size += 4;					// space for memory trash tester
#endif

	size = SIRENGINE_PAD( size, sizeof( uintptr_t ) ); // align to 32/64 bit boundary

	CThreadAutoLock<CThreadMutex> _( zone->allocLock );
#ifdef USE_MULTI_SEGMENT
	base = SearchFree( zone, size );

	RemoveFree( zone, base );
#else

	base = rover = zone->rover;
	start = base->prev;

	do {
		if ( rover == start ) {
			// scanned all the way around the list
#ifdef SIRENGINE_MEMORY_DEBUG
			Z_LogHeap();
			SIRENGINE_ERROR( "Z_Malloc: failed on allocation of %lu bytes from the %s zone: %s, line: %d (%s)",
								size, zone == smallzone ? "small" : "main", file, line, label );
#else
			SIRENGINE_ERROR( "Z_Malloc: failed on allocation of %lu bytes from the %s zone" );
#endif
			return NULL;
		}
		if ( rover->tag != TAG_FREE ) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while ( base->tag.load() != TAG_FREE || base->size.load() < size );
#endif

	//
	// found a block big enough
	//

	extra = base->size.load() - size;
	if ( extra >= CTagArenaAllocator::nMinFragment ) {
		memblock_t *fragment;
		// there will be a free fragment after the allocated block
		fragment = (memblock_t *)( (byte *)base + size );
		fragment->size.store( extra );
		fragment->tag.store( TAG_FREE ); // free block
		fragment->id.store( ZONEID );
		fragment->prev = base;
		fragment->next = base->next;
		fragment->next->prev = fragment;
		base->next = fragment;
		base->size.store( size );
#ifdef USE_MULTI_SEGMENT
		InsertFree( zone, fragment );
#endif
	}

#ifndef USE_MULTI_SEGMENT
	zone->rover = base->next;	// next allocation will start looking here
#endif
	zone->used.fetch_add( base->size.load() );

	base->tag.store( tag );		// no longer a free block
	base->id.store( ZONEID );

#ifdef SIRENGINE_MEMORY_DEBUG
	base->d.label = label;
	base->d.file = file;
	base->d.line = line;
	base->d.allocSize = allocSize;
#endif

#ifdef USE_TRASH_TEST
	// marker for memory trash testing
	*(int32_t *)( (byte *)base + base->size.load() - 4 ) = ZONEID;
#endif

	return (void *)( base + 1 );
}

#ifdef SIRENGINE_MEMORY_DEBUG
void *Z_ReallocDebug( memzone_t *zone, void *ptr, uint64_t nsize, uint64_t tag, const char *label, const char *file, uint32_t line ) {
	void *p;

	p = Z_AllocDebug( zone, nsize, tag, label, file, line );
	if ( ptr ) {
		memblock_t *block = (memblock_t *)( (byte *)ptr - sizeof( memblock_t ) );
		memcpy( p, ptr, block->size.load() <= nsize ? nsize : block->size.load() );
		Z_Free( zone, ptr );
	}
	return p;
}
#else
void *Z_Realloc( memzone_t *zone, void *ptr, uint64_t nsize, uint64_t tag ) {
	void *p;

	p = Z_Alloc( zone, nsize, tag );
	if ( ptr ) {
		memblock_t *block = (memblock_t *)( (byte *)ptr - sizeof( memblock_t ) );
		memcpy( p, ptr, block->size.load() <= nsize ? nsize : block->size.load() );
		Z_Free( zone, ptr );
	}
	return p;
}
#endif

/*
========================
Z_Malloc
========================
*/
#ifdef SIRENGINE_MEMORY_DEBUG
void *Z_MallocDebug( memzone_t *zone, uint64_t size, uint64_t tag, const char *label, const char *file, uint32_t line ) {
	return Z_AllocDebug( zone, size, tag, label, file, line );
}
#else
void *Z_Malloc( memzone_t *zone, uint64_t size, uint64_t tag ) {
	return Z_Alloc( zone, size, tag );
}
#endif


/*
========================
Z_CheckHeap
========================
*/
void Z_CheckHeap( memzone_t *zone )
{
	const memblock_t *block;

	CThreadAutoLock<CThreadMutex> _( zone->allocLock );
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		if ( (byte *)block + block->size.load() != (byte *)block->next) {
#ifdef USE_MULTI_SEGMENT
			const memblock_t *next = block->next;
			if ( next->size.load() == 0 && next->id.load() == -ZONEID && next->tag.load() == TAG_STATIC ) {
				block = next; // new zone segment
			} else
#endif
			SIRENGINE_ERROR( "Z_CheckHeap: block size does not touch the next block" );
		}
		if ( block->next->prev != block) {
			SIRENGINE_ERROR( "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( block->tag.load() == TAG_FREE && block->next->tag.load() == TAG_FREE ) {
			SIRENGINE_ERROR( "Z_CheckHeap: two consecutive free blocks" );
		}
		block = block->next;
	}
}


/*
========================
Z_LogZoneHeap
========================
*/
void Z_LogZoneHeap( memzone_t *zone, const char *name )
{
	/*
#ifdef SIRENGINE_MEMORY_DEBUG
	char dump[32], *ptr;
	uint32_t i, j;
#endif
	memblock_t	*block;
	char buf[4096];
	uint64_t size, allocSize, numBlocks;
	uint64_t len;

	size = numBlocks = 0;
#ifdef SIRENGINE_MEMORY_DEBUG
	allocSize = 0;
#endif
	len = SIREngine_snprintf( buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name );
	FS_Write( buf, len, logfile );
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag != TAG_FREE ) {
#ifdef SIRENGINE_MEMORY_DEBUG
			ptr = ((char *) block) + sizeof(memblock_t);
			j = 0;
			for (i = 0; i < 20 && i < block->d.allocSize; i++) {
				if (ptr[i] >= 32 && ptr[i] < 127) {
					dump[j++] = ptr[i];
				}
				else {
					dump[j++] = '_';
				}
			}
			dump[j] = '\0';
			len = SIREngine_snprintf(buf, sizeof(buf), "size = %-8lu: %-8s, line: %4u (%s) [%s]\r\n", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump);
			FS_Write( buf, len, logfile );
			allocSize += block->d.allocSize;
#endif
			size += block->size;
			numBlocks++;
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
		block = block->next;
	}
#ifdef SIRENGINE_MEMORY_DEBUG
	// subtract debug memory
	size -= numBlocks * sizeof(zonedebug_t);
#else
	allocSize = numBlocks * sizeof(memblock_t); // + 32 bit alignment
#endif
	len = SIREngine_snprintf( buf, sizeof( buf ), "%lu %s memory in %lu blocks\r\n", size, name, numBlocks );
	FS_Write( buf, len, logfile );
	len = SIREngine_snprintf( buf, sizeof( buf ), "%lu %s memory overhead\r\n", size - allocSize, name );
	FS_Write( buf, len, logfile );
	FS_Flush( logfile );
	*/
}

CTagArenaAllocator::CTagArenaAllocator( const char *pName, uint64_t nSize )
{
	m_pName = pName;
//	m_pZone = (memzone_t *)g_pApplication->VirtualAlloc( &nSize, 64 );
	m_pZone = (memzone_t *)calloc( 1, nSize );
	if ( !m_pZone ) {
		g_pApplication->OnOutOfMemory();
	}

	Z_ClearZone( m_pZone, m_pZone, nSize, 1 );
}

CTagArenaAllocator::~CTagArenaAllocator()
{
}

void CTagArenaAllocator::Shutdown( void )
{
	memblock_t *block, *seg;
	seg = NULL;

	for ( block = m_pZone->blocklist.next ; ; ) {
		if ( block->next == &m_pZone->blocklist ) {
			break;	// all blocks have been hit
		}
		if ( (byte *)block + block->size.load() != (byte *)block->next ) {
#ifdef USE_MULTI_SEGMENT
			memblock_t *next = block->next;
			if ( next->size.load() == 0 && next->id.load() == -ZONEID && next->tag.load() == TAG_STATIC ) {
				if ( seg ) {
					::free( seg );
				}
				seg = next;
				block = next; // new zone segment
			} else
#endif
			SIRENGINE_ERROR( "Z_CheckHeap: block size does not touch the next block" );
		}
		if ( block->next->prev != block) {
			SIRENGINE_ERROR( "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( block->tag.load() == TAG_FREE && block->next->tag.load() == TAG_FREE ) {
			SIRENGINE_ERROR( "Z_CheckHeap: two consecutive free blocks" );
		}
		block = block->next;
	}
//	g_pApplication->VirtualFree( m_pZone );
	::free( m_pZone );
}

void *CTagArenaAllocator::Alloc( size_t nSize )
{
	return Z_Malloc( m_pZone, nSize, TAG_STATIC );
}

void *CTagArenaAllocator::Realloc( void *pMemory, size_t nSize )
{
	return Z_Realloc( m_pZone, pMemory, nSize, TAG_STATIC );
}

void CTagArenaAllocator::Free( void *pMemory )
{
	Z_Free( m_pZone, pMemory );
}

void *CTagArenaAllocator::Alloc( size_t nSize, uint64_t nTag )
{
	return Z_Malloc( m_pZone, nSize, nTag );
}

void *CTagArenaAllocator::Realloc( void *pMemory, size_t nSize, uint64_t nTag )
{
	return Z_Realloc( m_pZone, pMemory, nSize, nTag );
}

void *CTagArenaAllocator::Alloc( size_t nSize, const char *pFileName, uint64_t nLineNumber )
{
//	return Z_MallocDebug( m_pZone, nSize, pFileName, nLineNumber );
}

void *CTagArenaAllocator::Realloc( void *pMemory, size_t nSize, const char *pFileName, uint64_t nLineNumber )
{
//	return Z_ReallocDebug( m_pZone, pMemory, nSize, pFileName, nLineNumber );
}

void CTagArenaAllocator::Free( void *pMemory, const char *pFileName, uint64_t nLineNumber )
{
//	Z_FreeDebug( m_pZone, pMemory, pFileName, nLineNumber );
}

size_t CTagArenaAllocator::GetAllocSize( void *pMemory )
{
	const memblock_t *block = (const memblock_t *)pMemory;

	if ( block->id.load() != ZONEID ) {
		SIRENGINE_ERROR( "CTagArenaAllocator::GetAllocSize: not a memblock" );
	}

	return block->size.load();
}

void CTagArenaAllocator::DumpStats( void )
{
	Z_LogZoneHeap( m_pZone, m_pName );
}

void CTagArenaAllocator::DumpStatsFileBase( char const *pchFileBase )
{
}

bool CTagArenaAllocator::IsDebugHeap( void ) const
{
	return false;
}

void CTagArenaAllocator::GetMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory )
{
	*pFreeMemory = Z_AvailableZoneMemory( m_pZone );
	*pUsedMemory = m_pZone->used.load();
}

uint64_t CTagArenaAllocator::AllocateTagGroup( const char *pName )
{
	uint64_t nTag = m_TagList.size() + 1;

	if ( m_TagList.find( nTag ) == m_TagList.end() ) {
		m_TagList[ nTag ] = pName;
		SIRENGINE_LOG( "Added memoryTag group \"%s\" to tagged arena zone allocator \"%s\"", pName, m_pName );
	}

	return nTag;
}

void CTagArenaAllocator::ClearTagGroup( uint64_t nTag )
{
	if ( m_TagList.find( nTag ) != m_TagList.end() ) {
		Z_FreeTags( m_pZone, nTag );
	} else {
		SIRENGINE_WARNING( "CTagArenaAllocator::ClearTagGroup: no such tag %lu in arena \"%s\"", nTag, m_pName );
	}
}

