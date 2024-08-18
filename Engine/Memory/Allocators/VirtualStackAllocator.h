#ifndef __VIRTUAL_STACK_ALLOCATOR_H__
#define __VIRTUAL_STACK_ALLOCATOR_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "AllocatorBase.h"

enum class EVirtualStackAllocatorDecommitMode : uint8_t {
	// Default mode, does not decommit pages until the allocator is destroyed
	AllOnDestruction = 0,
	// Decommits all pages once none are in use
	AllOnStackEmpty = 1,
	// Tracks the high water mark and uses it to free excess memory that is not expected to be used again soon
	// This enables us to quickly release memory consumed by a one-off spikey usage pattern while avoiding frequent page management in the steady state
	// See DecommitUnusedPages() for details of the heuristic used
	ExcessOnStackEmpty = 2,
	NumModes
};

class CVirtualStackAllocator : public IAllocatorBase
{
public:
	CVirtualStackAllocator( const char *pAllocatorName, size_t nRequestedSize, EVirtualStackAllocatorDecommitMode decommitMode );
	virtual ~CVirtualStackAllocator();

	virtual void Init( void ) override
	{ }
	virtual void Shutdown( void ) override
	{ }

	virtual uint64_t BytesUsed( void ) const override
	{ return (byte *)m_pNextAllocationStart - (byte *)m_pMemory; }
	uint64_t CommittedBytes( void )
	{ return (byte *)m_pNextUncommittedPage - (byte *)m_pMemory; }

	virtual void *Allocate( uint64_t nBytes, uint64_t nAlignment = 16 ) override;
	virtual void Deallocate( void *pMemory ) override
	{ }

	void DecommitUnusedPages( void );
private:
	CVirtualStackAllocator( const CVirtualStackAllocator& ) = delete;
	CVirtualStackAllocator( CVirtualStackAllocator&& ) = delete;
	CVirtualStackAllocator& operator=( const CVirtualStackAllocator& ) = delete;
	CVirtualStackAllocator& operator=( CVirtualStackAllocator&& ) = delete;

	CVirtualStackAllocator( void );

	void *m_pNextUncommittedPage;
	void *m_pNextAllocationStart;

	size_t m_nTotalReservationSize;

	void *m_pMemory;
	size_t m_nVirtualMemorySize;

	void *m_pRecentHighWaterMark;

	EVirtualStackAllocatorDecommitMode m_DecommitMode = EVirtualStackAllocatorDecommitMode::AllOnDestruction;
};

template<typename T>
class CVirtualStackAllocatorTemplate
{
public:
	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
public:
	CVirtualStackAllocatorTemplate( CVirtualStackAllocator *pBase )
		: m_pBase( pBase )
	{ }

	CVirtualStackAllocatorTemplate( const char * = NULL )
	{ }

	CVirtualStackAllocatorTemplate( const CVirtualStackAllocatorTemplate<T>& )
	{ }

	CVirtualStackAllocatorTemplate( const CVirtualStackAllocatorTemplate<T>&, const char * )
	{ }

	CVirtualStackAllocatorTemplate& operator=( const CVirtualStackAllocatorTemplate<T>& other )
	{ m_pBase = const_cast<CVirtualStackAllocatorTemplate *>( eastl::addressof( other ) )->m_pBase; return *this; }

	SIRENGINE_CONSTEXPR bool operator==( const CVirtualStackAllocatorTemplate<T>& )
	{ return true; }

	SIRENGINE_CONSTEXPR bool operator!=( const CVirtualStackAllocatorTemplate<T>& )
	{ return false; }
		
	SIRENGINE_FORCEINLINE T *allocate( size_t n, int /*flags*/ = 0 )
	{ return (T *)m_pBase->Allocate( n, 64 ); }

	SIRENGINE_FORCEINLINE T *allocate( size_t n, size_t alignment, size_t alignmentOffset, int /*flags*/ = 0 )
	{ return (T *)m_pBase->Allocate( n, alignment ); }

	SIRENGINE_FORCEINLINE void deallocate( void *pMemory )
	{ }
	SIRENGINE_FORCEINLINE void deallocate( void *, size_t  )
	{ }

	CVirtualStackAllocator *m_pBase;
};

#endif