#include <Engine/Memory/Memory.h>
#include <Engine/Memory/MemAlloc.h>
#include <Engine/Core/Logging/Logger.h>
#include <Engine/Core/ConsoleManager.h>
#include "MemoryStats.h"

CGlobalMemoryStats CGlobalMemoryStats::g_MemStats;

SIRENGINE_DEFINE_LOG_CATEGORY( Memory, ELogLevel::Warning );

static uint64_t s_nArenaDefaultSize = 72ull;
SIREngine::CVarRef<uint64_t> mem_StartingArenaSize(
	"mem.StartingArenaSize",
	s_nArenaDefaultSize,
	SIREngine::Cvar_Save | SIREngine::Cvar_Developer,
	"Sets the size of the main memory arena to be allocated in MiB.",
	SIREngine::CVG_SYSTEMINFO
);

IMemAlloc *g_pMemAlloc;

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
//	static IMemAlloc *pAlloc = new ( malloc( sizeof( CTagArenaAllocator ) ) ) CTagArenaAllocator( "MainArena",
//		s_nArenaDefaultSize * 1024 * 1024 );

	g_pMemAlloc = new ( malloc( sizeof( CTagArenaAllocator ) ) ) CTagArenaAllocator( "MainArena",
		s_nArenaDefaultSize * 1024 * 1024 );
}

void Mem_Shutdown( void )
{
	g_pMemAlloc->Shutdown();
}

void *Mem_Alloc( size_t nBytes, size_t nAlignment )
{
	return Mem_Alloc( SIRENGINE_PAD( nBytes, nAlignment ) );
}

void *Mem_Alloc( size_t nBytes )
{
	return g_pMemAlloc->Alloc( nBytes );
}

void *Mem_Realloc( void *pOriginal, size_t nBytes )
{
	return g_pMemAlloc->Realloc( pOriginal, nBytes );
}

void *Mem_ClearedAlloc( size_t nBytes )
{
	return memset( Mem_Alloc( nBytes ), 0, nBytes );
}

void Mem_Free( void *pMemory )
{
	if ( pMemory ) {
		g_pMemAlloc->Free( pMemory );
	}
}

#if defined(SIRENGINE_DEBUG)
void CGlobalMemoryStats::AddFree( size_t nSize, uintptr_t pAddress, size_t nFrameNumber, const char *pFileName, const char *pFuncName,
	size_t nLineNumber )
{
	if ( m_Deallocations.size() == 256 ) {
		m_Deallocations.pop_front();
	}
	m_FrameMemCalls.nFreeCalls++;
	FreeCall_t& freeInfo = m_Deallocations.push_back( { nSize, pAddress, nFrameNumber } );
	freeInfo.SetDebugInfo( pFileName, pFuncName, nLineNumber );
}
#else
void CGlobalMemoryStats::AddFree( size_t nSize, uintptr_t pAddress, size_t nFrameNumber )
{
	if ( m_Deallocations.size() == 256 ) {
		m_Deallocations.pop_front();
	}
	m_FrameMemCalls.nFreeCalls++;
	m_Deallocations.push_back( { nSize, pAddress, nFrameNumber } );
}
#endif

#if defined(SIRENGINE_DEBUG)
void CGlobalMemoryStats::AddMalloc( size_t nSize, size_t nAlignment, size_t nFrameNumber, bool bIsRealloc, const char *pFileName, const char *pFuncName,
	size_t nLineNumber )
{
	if ( bIsRealloc ) {
		m_FrameMemCalls.nReallocCalls++;
	} else {
		m_FrameMemCalls.nMallocCalls++;
	}
	if ( m_Allocations.size() == 256 ) {
		m_Allocations.pop_front();
	}
	AllocationCall_t& allocInfo = m_Allocations.push_back( { nSize, nAlignment, nFrameNumber, bIsRealloc } );
	allocInfo.SetDebugInfo( pFileName, pFuncName, nLineNumber );
}
#else
void CGlobalMemoryStats::AddMalloc( size_t nSize, size_t nAlignment, size_t nFrameNumber, bool bIsRealloc )
{
	if ( bIsRealloc ) {
		m_FrameMemCalls.nReallocCalls++;
	} else {
		m_FrameMemCalls.nMallocCalls++;
	}
	if ( m_Allocations.size() == 256 ) {
		m_Allocations.pop_front();
	}
	m_Allocations.push_back( { nSize, nAlignment, nFrameNumber, bIsRealloc } );
}
#endif