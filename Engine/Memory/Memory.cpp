#include <Engine/Memory/Memory.h>
#include <Engine/Memory/MemAlloc.h>

typedef struct memtag_block_s {
    uint64_t tag;
    uint64_t size;
    struct memtag_block_s *next;
    struct memtag_block_s *prev;
} memtag_block_t;

typedef struct memtag_arena_s {
    memtag_block_t blocklist;
    uint64_t tag;
    uint64_t size;
    uint64_t used;
    struct memtag_arena_s *prev;
    struct memtag_arena_s *next;
} memtag_arena_t;

class CMemoryManager
{
public:
    CMemoryManager( void );
    ~CMemoryManager();

    void *Alloc( uint64_t nBytes, uint64_t nAlignment, uint64_t nTag );
    void Free( void *pBuffer );
private:
    memtag_arena_t *AllocatePage( uint64_t nTag );
    void ReleasePage( memtag_arena_t *pArena );
    memtag_arena_t *GetFreePage( uint64_t nBytes, uint64_t nAlignment, uint64_t nTag );
    void ReleaseUnusedPages( void );

    memtag_arena_t *m_pszArenas[MEMTAG_COUNT];
    uint64_t m_nArenaCount;

    static uint64_t nPageSize;
};

CMemoryManager::CMemoryManager( void )
{
    memset( m_pszArenas, 0, sizeof( m_pszArenas ) );
    m_nArenaCount = 0;
    nPageSize = g_pApplication->GetOSPageSize();
}

CMemoryManager::~CMemoryManager()
{
    memtag_arena_t *arena;
}

void CMemoryManager::ReleaseUnusedPages( void )
{
    uint64_t i;
    uint64_t bytes, count;
    memtag_arena_t *arena, *next;

    bytes = count = 0;
    for ( i = 0; i < MEMTAG_COUNT; i++ ) {
        for ( arena = m_pszArenas[ i ]->next; arena->next != m_pszArenas[ i ]; arena = next ) {
            next = arena->next;
            if ( !arena->used ) {
                count++;
                bytes += arena->size;
                ReleasePage( arena );
            }
        }
    }
}

memtag_arena_t *CMemoryManager::AllocatePage( uint64_t nTag )
{
    memtag_arena_t *arena;
    uint64_t size;

    size = 0;
    size += SIRENGINE_PAD( sizeof( *arena ), sizeof ( uintptr_t ) );
    size += SIRENGINE_PAD( nPageSize, sizeof( uintptr_t ) );

    arena = (memtag_arena_t *)g_pMemAlloc->Alloc( size );
    if ( !arena ) {
        ReleaseUnusedPages(); // attempt to squeeze more memory out of the machine, free up anything not be used

        arena = (memtag_arena_t *)g_pMemAlloc->Alloc( size );
        if ( !arena ) {

        }
    }

    memset( arena, 0, size );
    arena->size = size - sizeof( *arena );
    arena->used = 0;
    arena->tag = nTag;
    arena->blocklist.next =
    arena->blocklist.prev =
        &arena->blocklist;
    
    arena->prev = m_pszArenas[ nTag ]->prev;
    arena->next = m_pszArenas[ nTag ];
    m_pszArenas[ nTag ]->prev->next = arena;

    return arena;
}

void CMemoryManager::ReleasePage( memtag_arena_t *arena )
{
    assert( arena );
    if ( !arena ) {

    }

    arena->prev->next = arena->next;
    arena->next->prev = arena->prev;

    g_pMemAlloc->Free( arena );
}

memtag_arena_t *CMemoryManager::GetFreePage( uint64_t nBytes, uint64_t nAlignemnt, uint64_t nTag )
{
}

static CMemoryManager *s_pMemory;

void SIRENGINE_DLL_EXPORT Mem_Init( void )
{
    s_pMemory = new CMemoryManager;
}

void SIRENGINE_DLL_EXPORT Mem_Shutdown( void )
{
    delete s_pMemory;
}

void SIRENGINE_DLL_EXPORT *Mem_Alloc( uint64_t nBytes ) {
}

void SIRENGINE_DLL_EXPORT Mem_Free( void *pMemory ) {
}

void SIRENGINE_DLL_EXPORT Mem_ClearTagMemory( uint64_t iTag ) {
}

