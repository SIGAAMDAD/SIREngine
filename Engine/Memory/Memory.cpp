#include <Engine/Memory/Memory.h>
#include <Engine/Memory/MemAlloc.h>

IMemAlloc *g_pMemAlloc;

void Mem_Init( void )
{
#if defined(USE_ARENA_ALLOC)
    g_pMemAlloc = ::new ( malloc( sizeof( CTagArenaAllocator ) ) ) CTagArenaAllocator( "MainArena", 128ull * 1024 * 1024 );
#elif defined(USE_SMMALLOC)
    g_pMemAlloc = _sm_allocator_create( 2, ( 72 * 1024 * 1024 ) );
#endif
}

void Mem_Shutdown( void )
{
#if defined(USE_ARENA_ALLOC)
    g_pMemAlloc->Shutdown();
    free( g_pMemAlloc );
#elif defined(USE_SMMALLOC)
    _sm_allocator_destroy( g_pMemAlloc );
#endif
}

void *Mem_Alloc( size_t nBytes, size_t nAlignment )
{
#if defined(USE_ARENA_ALLOC)
    return g_pMemAlloc->Alloc( nBytes );
#elif defined(USE_SMMALLOC)
    return _sm_malloc( g_pMemAlloc, nBytes, nAlignment );
#endif
}

void Mem_Free( void *pMemory )
{
#if defined(USE_ARENA_ALLOC)
    g_pMemAlloc->Free( pMemory );
#elif defined(USE_SMMALLOC)
    _sm_free( g_pMemAlloc, pMemory );
#endif
}