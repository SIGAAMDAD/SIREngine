#include <Engine/Memory/Memory.h>
#include <Engine/Memory/MemAlloc.h>

class CMemoryManager
{
public:
    CMemoryManager( void );
    ~CMemoryManager();

    void *Alloc( uint64_t nBytes, uint64_t nAlignment, uint64_t nTag );
    void Free( void *pBuffer );
};