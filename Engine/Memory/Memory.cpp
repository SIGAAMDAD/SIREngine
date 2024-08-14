#include <Engine/Memory/Memory.h>
#include <Engine/Memory/MemAlloc.h>

IMemAlloc *g_pMemAlloc;
IMemAlloc *g_pSmallHeap;
IMemAlloc *g_pLargeHeap;

void *operator new[]( size_t nBytes, char const *, int, unsigned int, char const *, int )
{
    return ::operator new[]( nBytes );
}

void *operator new[]( size_t nBytes, unsigned long, unsigned long, char const*, int, unsigned int, char const*, int )
{
    return ::operator new[]( nBytes );
}

void Mem_Init( void )
{
#if defined(USE_ARENA_ALLOC)
	g_pMemAlloc = ::new ( malloc( sizeof( CTagArenaAllocator ) ) ) CTagArenaAllocator( "MainArena", 72ull * 1024 * 1024 );
#elif defined(USE_SMMALLOC)
	g_pMemAlloc = _sm_allocator_create( 2, ( 72 * 1024 * 1024 ) );
#endif
}

void Mem_Shutdown( void )
{
#if defined(USE_ARENA_ALLOC)
	g_pMemAlloc->Shutdown();
//	g_pSmallHeap->Shutdown();
//	g_pLargeHeap->Shutdown();
	free( g_pMemAlloc );
#elif defined(USE_SMMALLOC)
	_sm_allocator_destroy( g_pMemAlloc );
#endif
}

void *Mem_Alloc( size_t nBytes, size_t nAlignment )
{
#if defined(USE_ARENA_ALLOC)
	return Mem_Alloc( nBytes );
#elif defined(USE_SMMALLOC)
	return _sm_malloc( g_pMemAlloc, nBytes, nAlignment );
#endif
}

void *Mem_Alloc( size_t nBytes )
{
#if defined(USE_ARENA_ALLOC)
//	if ( !( nBytes & ~256 ) ) {
//		return g_pSmallHeap->Alloc( nBytes );
//	}
//	if ( !( nBytes & SIRENGINE_UINT32_MAX ) ) {
//		return g_pMemAlloc->Alloc( nBytes );
//	}
//	return g_pLargeHeap->Alloc( nBytes );
	return g_pMemAlloc->Alloc( nBytes );
#elif defined(USE_SMMALLOC)
	return _sm_malloc( g_pMemAlloc, nBytes, 16 );
#endif
}

void *Mem_Realloc( void *pOriginal, size_t nBytes )
{
#if defined(USE_ARENA_ALLOC)
//	if ( !( nBytes & ~256 ) ) {
//		return g_pSmallHeap->Realloc( pOriginal, nBytes );
//	}
//	if ( !( nBytes & SIRENGINE_UINT32_MAX ) ) {
//		return g_pMemAlloc->Realloc( pOriginal, nBytes );
//	}
//	return g_pLargeHeap->Realloc( pOriginal, nBytes );
	return g_pMemAlloc->Realloc( pOriginal, nBytes );
#elif defined(USE_SMMALLOC)
	return _sm_realloc( g_pMemAlloc, nBytes, 16 );
#endif
}

void *Mem_ClearedAlloc( size_t nBytes )
{
	return memset( Mem_Alloc( nBytes ), 0, nBytes );
}

void Mem_Free( void *pMemory )
{
#if defined(USE_ARENA_ALLOC)
//	const uint64_t nBytes = g_pMemAlloc->GetAllocSize( pMemory );
//	if ( !( nBytes & ~256 ) ) {
//		g_pSmallHeap->Free( pMemory );
//		return;
//	}
//	if ( !( nBytes & SIRENGINE_UINT32_MAX ) ) {
//		g_pMemAlloc->Free( pMemory );
//		return;
//	}
//	g_pLargeHeap->Free( pMemory );
	g_pMemAlloc->Free( pMemory );
#elif defined(USE_SMMALLOC)
	_sm_free( g_pMemAlloc, pMemory );
#endif
}