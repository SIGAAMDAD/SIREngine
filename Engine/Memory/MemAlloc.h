#ifndef __MEMALLOC_H__
#define __MEMALLOC_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Memory.h"

class IMemAlloc
{
public:
	IMemAlloc( void )
	{ }
	virtual ~IMemAlloc()
	{ }

	virtual void Shutdown( void ) = 0;

	// Release versions
	virtual void *Alloc( size_t nSize ) = 0;
	virtual void *Realloc( void *pMemory, size_t nSize ) = 0;
	virtual void Free( void *pMemory ) = 0;

	// Debug versions
	virtual void *Alloc( size_t nSize, const char *pFileName, uint64_t nLineNumber ) = 0;
	virtual void *Realloc( void *pMemory, size_t nSize, const char *pFileName, uint64_t nLineNumber ) = 0;
	virtual void  Free( void *pMemory, const char *pFileName, uint64_t nLineNumber ) = 0;

	virtual size_t GetAllocSize( void *pMemory ) = 0;

	virtual void DumpStats( void ) = 0;
	virtual void DumpStatsFileBase( char const *pchFileBase ) = 0;

	virtual bool IsDebugHeap( void ) const = 0;

	virtual void GetMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) = 0;
};

extern IMemAlloc *g_pMemAlloc;

#if defined(SIRENGINE_DEBUG)
#define Mem_Alloc( ... ) Mem_AllocDebug( __VA_ARGS__, __FILE__, __FUNC__, __LINE__ )
#define Mem_ClearedAlloc( size ) Mem_ClearedAllocDebug( size, __FILE__, __FUNC__, __LINE__ )
#define Mem_Free( ptr ) Mem_FreeDebug( ptr, __FILE__, __FUNC__, __LINE__ )
extern void SIRENGINE_DLL_EXPORT *Mem_AllocDebug( size_t nBytes, size_t nAlignment, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
extern void SIRENGINE_DLL_EXPORT *Mem_AllocDebug( size_t nBytes, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
extern void SIRENGINE_DLL_EXPORT *Mem_ClearedAllocDebug( size_t nBytes, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
extern void SIRENGINE_DLL_EXPORT *Mem_RellocDebug( void *pOriginalPointer, size_t nBytes, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
extern void SIRENGINE_DLL_EXPORT Mem_FreeDebug( void *pMemory, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
#else
extern void SIRENGINE_DLL_EXPORT *Mem_Alloc( size_t nBytes );
extern void SIRENGINE_DLL_EXPORT *Mem_Alloc( size_t nBytes, size_t nAlignment );
extern void SIRENGINE_DLL_EXPORT *Mem_ClearedAlloc( size_t nBytes );
extern void SIRENGINE_DLL_EXPORT *Mem_Realloc( void *pOriginalPointer, size_t nBytes );
extern void SIRENGINE_DLL_EXPORT Mem_Free( void *pMemory );
#endif

template<typename T = char>
struct MemoryAllocator
{
	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;

	MemoryAllocator( const char * = NULL )
	{ }

	MemoryAllocator( const MemoryAllocator<T>& )
	{ }

	MemoryAllocator( const MemoryAllocator<T>&, const char * )
	{ }

	SIRENGINE_CONSTEXPR MemoryAllocator& operator=( const MemoryAllocator<T>& )
	{ return *this; }

	SIRENGINE_CONSTEXPR bool operator==( const MemoryAllocator<T>& )
	{ return true; }

	SIRENGINE_CONSTEXPR bool operator!=( const MemoryAllocator<T>& )
	{ return false; }
		
	T *allocate( size_t n, int /*flags*/ = 0 )
	{ return (T *)Mem_Alloc( n ); }

	T *allocate( size_t n, size_t alignment, size_t alignmentOffset, int /*flags*/ = 0 )
	{
		return (T *)Mem_Alloc( SIRENGINE_PAD( n, alignment ) );
		/*
		#if defined(EASTL_ALIGNED_MALLOC_AVAILABLE) && !defined(_WIN32)
			if((alignmentOffset % alignment) == 0) // We check for (offset % alignmnent == 0) instead of (offset == 0) because any block which is aligned on e.g. 64 also is aligned at an offset of 64 by definition. 
				return (T *)memalign(alignment, n); // memalign is more consistently available than posix_memalign.
		#else
			if((alignment <= EASTL_SYSTEM_ALLOCATOR_MIN_ALIGNMENT) && ((alignmentOffset % alignment) == 0))
				return (T *)malloc(n);
		#endif
		return NULL;
		*/
	}

	void deallocate( void *p, size_t /*n*/ = 0 )
	{ Mem_Free( p ); }
	const char* get_name( void ) const
	{ return "IMemAlloc"; }

	void set_name( const char * )
	{ }
};

#if defined(SIRENGINE_REPLACE_NEW_AND_DELETE) && !defined(SIRENGINE_NEW_AND_DELETE_OVERRIDE)
#include <new>
#undef new
#undef delete

SIRENGINE_FORCEINLINE void *operator new( size_t nSize )
{ return Mem_ClearedAlloc( nSize ); }

SIRENGINE_FORCEINLINE void *operator new( size_t nSize, size_t alignment )
{ return Mem_ClearedAlloc( nSize ); }

SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize )
{ return ::operator new( nSize ); }
SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize, size_t alignment )
{ return ::operator new( nSize, alignment ); }

SIRENGINE_FORCEINLINE void *operator new( size_t nSize, const std::nothrow_t& )
{ return ::operator new( nSize ); }
SIRENGINE_FORCEINLINE void *operator new( size_t nSize, size_t alignment, const std::nothrow_t& )
{ return ::operator new( nSize, alignment ); }

SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize, const std::nothrow_t& )
{ return ::operator new[]( nSize ); }
SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize, size_t alignment, const std::nothrow_t& )
{ return ::operator new[]( nSize, alignment ); }

SIRENGINE_FORCEINLINE void operator delete( void *pMemory ) noexcept
{ Mem_Free( pMemory ); }

SIRENGINE_FORCEINLINE void operator delete[]( void *pMemory ) noexcept
{ ::operator delete( pMemory ); }

SIRENGINE_FORCEINLINE void operator delete( void *pMemory, size_t ) noexcept
{ ::operator delete( pMemory ); }
SIRENGINE_FORCEINLINE void operator delete[]( void *pMemory, size_t ) noexcept
{ ::operator delete[]( pMemory ); }

SIRENGINE_FORCEINLINE void operator delete( void *pMemory, size_t, size_t ) noexcept
{ ::operator delete( pMemory ); }
SIRENGINE_FORCEINLINE void operator delete[]( void *pMemory, size_t, size_t ) noexcept
{ ::operator delete[]( pMemory ); }

#endif

#endif