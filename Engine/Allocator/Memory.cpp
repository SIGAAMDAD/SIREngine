#include <Engine/Allocator/Memory.hpp>

typedef struct memtag_block_s {
    uint64_t tag;
    uint64_t size;
    struct memtag_block_s *next;
    struct memtag_block_s *prev;
} memtag_block_t;

typedef struct memtag_arena_s {
    char *name;
    memtag_block_t blocklist;
    uint64_t tag;
    uint64_t size;
    struct memtag_arena_s *next;
} memtag_arena_t;

class CMemoryManager
{
public:
    CMemoryManager( void );
    ~CMemoryManager();

    memtag_arena_t *AllocTag( uint64_t iNum );
private:
    memtag_arena_t *m_pArenas;
    uint64_t m_nArenaCount;
};

CMemoryManager::CMemoryManager( void )
{
    m_pArenas = NULL;
    m_nArenaCount = 0;

    AllocTag( MEMTAG_RENDERER );
    AllocTag( MEMTAG_SOUNDSYSTEM );
}

CMemoryManager::~CMemoryManager()
{
}

memtag_arena_t *CMemoryManager::AllocTag( uint64_t iNum )
{
    memtag_arena_t *arena;
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

void SIRENGINE_DLL_EXPORT Mem_CreateTag( uint64_t iTag, const char *pszName ) {
}

void SIRENGINE_DLL_EXPORT Mem_ClearTagMemory( uint64_t iTag ) {
}

void SIRENGINE_DLL_EXPORT Mem_DeleteTag( uint64_t iTag ) {
}

