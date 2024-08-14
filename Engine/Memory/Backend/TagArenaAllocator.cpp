#include "TagArenaAllocator.h"
#include <Engine/Core/ThreadSystem/Thread.h>

#define UNOWNED    ((void *)666)
#define ZONEID     0xa21d49

using namespace SIREngine;

CVar<uint64_t> mem_NewSegmentSize(
	"mem.NewSegmentSize",
	8*1024*1024, // originally 1<<21
	Cvar_Save,
	"Sets the size of each new arena segment allocated.",
	CVG_SYSTEMINFO
);

int CTagArenaAllocator::nMinFragment = MIN_FRAGMENT;

typedef struct memblock_s {
	struct memblock_s		*next, *prev;
	uint64_t				size;	// including the header and possibly tiny fragments
	uint64_t				tag;	// a tag of 0 is a free block
	int64_t					id;		// should be ZONEID
#if defined(SIRENGINE_MEMORY_DEBUG)
	zonedebug_t d;
#endif
} memblock_t;

typedef struct freeblock_s {
	struct freeblock_s *prev;
	struct freeblock_s *next;
} freeblock_t;

typedef struct memzone_s {
	uint64_t				size;
	uint64_t				used;
	memblock_t				blocklist;
#ifdef USE_MULTI_SEGMENT
	memblock_t				dummy0;			// just to allocate some space before freelist
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

static void InitFree( freeblock_t *fb )
{
	memblock_t *block = (memblock_t*)( (byte*)fb - sizeof( memblock_t ) );
	memset( block, 0, sizeof( *block ) );
}


static void RemoveFree( memblock_t *block )
{
	freeblock_t *fb = (freeblock_t*)( block + 1 );
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

//	size = SIRENGINE_PAD( size, 1<<21 ); // round up to 2M blocks
	size = SIRENGINE_PAD( size, mem_NewSegmentSize.GetValue() );

	// allocate separator block before new free block
	alloc_size = size + sizeof( *sep );

//	sep = (memblock_t *)Application::Get()->VirtualAlloc( &alloc_size, 64 );
	sep = (memblock_t *)calloc( alloc_size, 1 );
	if ( sep == NULL ) {
		SIRENGINE_ERROR( "Z_Malloc: failed on allocation of %lu bytes", size );
		return NULL;
	}
	memset( sep, 0, alloc_size );
	block = sep+1;

//	Application::Get()->CommitMemory( sep, 0, alloc_size );

	// link separator with prev
	prev->next = sep;
	sep->prev = prev;

	// link separator with block
	sep->next = block;
	block->prev = sep;

	// link block with next
	block->next = next;
	next->prev = block;

	sep->tag = TAG_STATIC; // in-use block
	sep->id = -ZONEID;
	sep->size = 0;

	block->tag = TAG_FREE;
	block->id = ZONEID;
	block->size = size;

	// update zone statistics
	zone->size += alloc_size;
	zone->used += sizeof( *sep );

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
		if ( base->size >= size ) {
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
	memblock_t	*block;
	uint64_t min_fragment;

#ifdef USE_MULTI_SEGMENT
	min_fragment = sizeof( memblock_t ) + sizeof( freeblock_t );
#else
	min_fragment = sizeof( memblock_t );
#endif

	if ( CTagArenaAllocator::nMinFragment < min_fragment ) {
		// in debug mode size of memblock_t may exceed MIN_FRAGMENT
		CTagArenaAllocator::nMinFragment = SIRENGINE_PAD( min_fragment, sizeof( intptr_t ) );
		SIRENGINE_LOG( "CTagArenaAllocator::nMinFragment adjusted to %i bytes", CTagArenaAllocator::nMinFragment );
	}

	// set the entire zone to one free block
	zone->blocklist.next = zone->blocklist.prev = block = (memblock_t *)( zone + 1 );
	zone->blocklist.tag = TAG_STATIC; // in use block
	zone->blocklist.id = -ZONEID;
	zone->blocklist.size = 0;
#ifndef USE_MULTI_SEGMENT
	zone->rover = block;
#endif
	zone->size = size;
	zone->used = 0;

	block->prev = block->next = &zone->blocklist;
	block->tag = TAG_FREE;	// free block
	block->id = ZONEID;

	block->size = size - sizeof(memzone_t);

#ifdef USE_MULTI_SEGMENT
	InitFree( &zone->freelist );
	zone->freelist.next = zone->freelist.prev = &zone->freelist;

	InitFree( &zone->freelist_medium );
	zone->freelist_medium.next = zone->freelist_medium.prev = &zone->freelist_medium;

	InitFree( &zone->freelist_small );
	zone->freelist_small.next = zone->freelist_small.prev = &zone->freelist_small;

	InitFree( &zone->freelist_tiny );
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
	return (1024*1024*1024); // unlimited
#else
	return zone->size - zone->used;
#endif
}

static void MergeBlock( memblock_t *curr_free, const memblock_t *next )
{
	curr_free->size += next->size;
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
		return;
	}

	block = (memblock_t *) ( (byte *)ptr - sizeof( memblock_t ) );
	if ( block->id != ZONEID ) {
		SIRENGINE_ERROR( "Z_Free: freed a pointer without ZONEID" );
	}

	if ( block->tag == TAG_FREE ) {
		SIRENGINE_ERROR( "Z_Free: freed a freed pointer" );
	}

	// check the memory trash tester
#ifdef USE_TRASH_TEST
	if ( *(int32_t *)((byte *)block + block->size - 4 ) != ZONEID ) {
		SIRENGINE_ERROR( "Z_Free: memory block wrote past end" );
	}
#endif

	CThreadAutoLock<CThreadMutex> lock( zone->allocLock );
	zone->used -= block->size;

	// set the block to something that should cause problems
	// if it is referenced...
//	memset( ptr, 0xaa, block->size - sizeof( *block ) );

	block->tag = TAG_FREE; // mark as free
	block->id = ZONEID;

	other = block->prev;
	if ( other->tag == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
		RemoveFree( other );
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

	other = block->next;
	if ( other->tag == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
		RemoveFree( other );
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
	memblock_t	*block, *freed;

	count = 0;
	size = 0;
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag == tag && block->id == ZONEID ) {
			if ( block->prev->tag == TAG_FREE ) {
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
	uint32_t extra;
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
	if ( size < (sizeof( freeblock_t ) ) ) {
		size = (sizeof( freeblock_t ) );
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

	size = SIRENGINE_PAD( size, sizeof( uintptr_t ) );		// align to 32/64 bit boundary

	CThreadAutoLock<CThreadMutex> lock( zone->allocLock );

#ifdef USE_MULTI_SEGMENT
	base = SearchFree( zone, size );

	RemoveFree( base );
#else

	base = rover = zone->rover;
	start = base->prev;

	do {
		if ( rover == start ) {
			// scanned all the way around the list
#ifdef SIRENGINE_MEMORY_DEBUG
			Z_LogHeap();
			SIRENGINE_ERROR( "Z_Malloc: failed on allocation of %lu bytes: %s, line: %d (%s)",
								size, file, line, label );
#else
			SIRENGINE_ERROR( "Z_Malloc: failed on allocation of %lu bytes",
								size );
#endif
			return NULL;
		}
		if ( rover->tag != TAG_FREE ) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while (base->tag != TAG_FREE || base->size < size);
#endif

	//
	// found a block big enough
	//
	extra = base->size - size;
	if ( extra >= CTagArenaAllocator::nMinFragment ) {
		memblock_t *fragment;
		// there will be a free fragment after the allocated block
		fragment = (memblock_t *)( (byte *)base + size );
		fragment->size = extra;
		fragment->tag = TAG_FREE; // free block
		fragment->id = ZONEID;
		fragment->prev = base;
		fragment->next = base->next;
		fragment->next->prev = fragment;
		base->next = fragment;
		base->size = size;
#ifdef USE_MULTI_SEGMENT
		InsertFree( zone, fragment );
#endif
	}

#ifndef USE_MULTI_SEGMENT
	zone->rover = base->next;	// next allocation will start looking here
#endif
	zone->used += base->size;

	base->tag = tag;			// no longer a free block
	base->id = ZONEID;

#ifdef SIRENGINE_MEMORY_DEBUG
	base->d.label = label;
	base->d.file = file;
	base->d.line = line;
	base->d.allocSize = allocSize;
#endif

#ifdef USE_TRASH_TEST
	// marker for memory trash testing
	*(int32_t *)((byte *)base + base->size - 4) = ZONEID;
#endif

	return (void *) ( base + 1 );
}

#ifdef SIRENGINE_MEMORY_DEBUG
void *Z_ReallocDebug( memzone_t *zone, void *ptr, uint64_t nsize, uint64_t tag, const char *label, const char *file, uint32_t line ) {
	void *p;

	p = Z_AllocDebug( nsize, tag, label, file, line );
	if (ptr) {
		memblock_t *block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));
		memcpy(p, ptr, block->size <= nsize ? nsize : block->size);
		Z_Free(ptr);
	}
	return p;
}
#else
void *Z_Realloc( memzone_t *zone, void *ptr, uint64_t nsize, uint64_t tag ) {
	void *p;

	p = Z_Alloc( zone, nsize, tag );
	if (ptr) {
		memblock_t *block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));
		memcpy( p, ptr, block->size <= nsize ? nsize : block->size );
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
void Z_CheckHeap( const memzone_t *zone )
{
	const memblock_t *block;

	for ( block = zone->blocklist.next ; ; ) {
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next ) {
#ifdef USE_MULTI_SEGMENT
			const memblock_t *next = block->next;
			if ( next->size == 0 && next->id == -ZONEID && next->tag == TAG_STATIC ) {
				block = next; // new zone segment
			} else
#endif
			SIRENGINE_ERROR( "Z_CheckHeap: block size does not touch the next block" );
		}
		if ( block->next->prev != block ) {
			SIRENGINE_ERROR( "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
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
	char dump[32], *ptr;
	uint32_t i, j;
	memblock_t	*block;
	uint64_t size, allocSize, numBlocks;

	size = numBlocks = 0;
	allocSize = 0;

	SIRENGINE_LOG( "================" );
	SIRENGINE_LOG( "%s log", name );
	SIRENGINE_LOG( "================" );
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag != TAG_FREE ) {
			ptr = ((char *) block) + sizeof(memblock_t);
			j = 0;
#ifdef SIRENGINE_MEMORY_DEBUG
			for (i = 0; i < 20 && i < block->d.allocSize; i++)
#else
			for ( i = 0; i < 20 && i < block->size; i++ )
#endif
			{
				if (ptr[i] >= 32 && ptr[i] < 127) {
					dump[j++] = ptr[i];
				}
				else {
					dump[j++] = '_';
				}
			}
			dump[j] = '\0';
#ifdef SIRENGINE_MEMORY_DEBUG
			SIRENGINE_LOG( "size = %-8lu: %-8s, line: %4u (%s) [%s]", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump );
			allocSize += block->d.allocSize;
#else
			SIRENGINE_LOG( "size = %-8lu [%s]", block->size, dump );
			allocSize += block->size;
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
	
	SIRENGINE_LOG( "%lu %s memory in %lu blocks", size, name, numBlocks );
	SIRENGINE_LOG( "%lu %s memory overhead", size - allocSize, name );
}

CTagArenaAllocator::CTagArenaAllocator( const char *pName, uint64_t nSize, uint64_t nFlags )
{
	m_pName = pName;
//	m_pZone = (memzone_t *)Application::Get()->VirtualAlloc( &nSize, 64 );
	m_pZone = (memzone_t *)calloc( 1, nSize );
	if ( !m_pZone ) {
		Application::Get()->OnOutOfMemory();
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
		if ( (byte *)block + block->size != (byte *)block->next ) {
#ifdef USE_MULTI_SEGMENT
			memblock_t *next = block->next;
			if ( next->size == 0 && next->id == -ZONEID && next->tag == TAG_STATIC ) {
				if ( seg ) {
//					Application::Get()->VirtualFree( seg );
					::free( seg );
				}
				seg = next;
				block = next; // new zone segment
			} else
#endif
			SIRENGINE_ERROR( "Z_CheckHeap: block size does not touch the next block" );
		}
		if ( block->next->prev != block ) {
			SIRENGINE_ERROR( "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
			SIRENGINE_ERROR( "Z_CheckHeap: two consecutive free blocks" );
		}
		block = block->next;
	}
//	Application::Get()->VirtualFree( m_pZone );
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
	const memblock_t *block = (const memblock_t *)pMemory - 1;

	if ( block->id != ZONEID ) {
		SIRENGINE_ERROR( "CTagArenaAllocator::GetAllocSize: not a memblock, id = %li", block->id );
	}

	return block->size;
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
	*pUsedMemory = m_pZone->used;
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

