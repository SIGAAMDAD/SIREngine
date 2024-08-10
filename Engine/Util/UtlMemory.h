#ifndef __UTL_MEMORY_HPP__
#define __UTL_MEMORY_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif


/*
//-----------------------------------------------------------------------------
// The UtlMemory class:
// A growable memory class which doubles in size by default.
//-----------------------------------------------------------------------------
template<typename T>
class UtlMemory
{
public:
	// constructor, destructor
	UtlMemory( uint64_t nGrowSize = 0, uint64_t nInitSize = 0 );
	UtlMemory( T *pMemory, uint64_t numElements );
	UtlMemory( const T *pMemory, uint64_t numElements );
	~UtlMemory();

	// Set the size by which the memory grows
	void Init( uint64_t nGrowSize = 0, uint64_t nInitSize = 0 );

	class Iterator_t
	{
	public:
		Iterator_t( I i ) : index( i ) {}
		I index;

		bool operator==( const Iterator_t it ) const	{ return index == it.index; }
		bool operator!=( const Iterator_t it ) const	{ return index != it.index; }
	};
	Iterator_t First( void ) const							{ return Iterator_t( IsIdxValid( 0 ) ? 0 : InvalidIndex( void ) ); }
	Iterator_t Next( const Iterator_t &it ) const		{ return Iterator_t( IsIdxValid( it.index + 1 ) ? it.index + 1 : InvalidIndex( void ) ); }
	uint64_t GetIndex( const Iterator_t &it ) const			{ return it.index; }
	bool IsIdxAfter( uint64_t i, const Iterator_t &it ) const	{ return i > it.index; }
	bool IsValidIterator( const Iterator_t &it ) const	{ return IsIdxValid( it.index ); }
	Iterator_t InvalidIterator( void ) const					{ return Iterator_t( InvalidIndex( void ) ); }

	// element access
	T& operator[]( uint64_t i );
	const T& operator[]( uint64_t i ) const;
	T& Element( uint64_t i );
	const T& Element( uint64_t i ) const;

	// Can we use this index?
	bool IsIdxValid( uint64_t i ) const;

	// Specify the invalid ('null') index that we'll only return on failure
	static const uint64_t INVALID_INDEX = ( I )-1; // For use with COMPILE_TIME_ASSERT
	static uint64_t InvalidIndex( void ) { return INVALID_INDEX; }

	// Gets the base address (can change when adding elements!)
	T *Base( void );
	const T *Base( void ) const;

	// Attaches the buffer to external memory....
	void SetExternalBuffer( T *pMemory, uint64_t numElements );
	void SetExternalBuffer( const T *pMemory, uint64_t numElements );
	void AssumeMemory( T *pMemory, uint64_t nSize );
	T *Detach( void );
	void *DetachMemory( void );

	// Fast swap
	void Swap( UtlMemory<T> &mem );

	// Switches the buffer from an external memory buffer to a reallocatable buffer
	// Will copy the current contents of the external buffer to the reallocatable buffer
	void ConvertToGrowableMemory( uint64_t nGrowSize );

	// Size
	uint64_t NumAllocated( void ) const;
	uint64_t Count( void ) const;

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( uint64_t num = 1 );

	// Makes sure we've got at least this much memory
	void EnsureCapacity( uint64_t num );

	// Memory deallocation
	void Purge( void );

	// Purge all but the given number of elements
	void Purge( uint64_t numElements );

	// is the memory externally allocated?
	bool IsExternallyAllocated( void ) const;

	// is the memory read only?
	bool IsReadOnly( void ) const;

	// Set the size by which the memory grows
	void SetGrowSize( uint64_t size );
protected:
	void ValidateGrowSize( void ) {
#ifdef _X360
		if ( m_nGrowSize && m_nGrowSize != EXTERNAL_BUFFER_MARKER ) {
			// Max grow size at 128 bytes on XBOX
			const uint64_t MAX_GROW = 128;
			if ( m_nGrowSize * sizeof( T ) > MAX_GROW ) {
				m_nGrowSize = max( 1, MAX_GROW / sizeof(T) );
			}
		}
#endif
	}

	enum {
		EXTERNAL_BUFFER_MARKER = -1,
		EXTERNAL_CONST_BUFFER_MARKER = -2,
	};

	T *m_pMemory;
	uint64_t m_nAllocationCount;
	uint64_t m_nGrowSize;
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

template<typename T>
SIRENGINE_FORCEINLINE UtlMemory<T>::UtlMemory( uint64_t nGrowSize, uint64_t nInitAllocationCount )
	: m_pMemory( 0 ), m_nAllocationCount( nInitAllocationCount ), m_nGrowSize( nGrowSize )
{
	ValidateGrowSize();
	assert( nGrowSize >= 0 );
	if ( m_nAllocationCount ) {
		m_pMemory = (T *)malloc( m_nAllocationCount * sizeof( T ) );
	}
}

template<typename T>
SIRENGINE_FORCEINLINE UtlMemory<T>::UtlMemory( T *pMemory, uint64_t numElements )
	: m_pMemory( pMemory ), m_nAllocationCount( numElements )
{
	// Special marker indicating externally supplied modifyable memory
	m_nGrowSize = EXTERNAL_BUFFER_MARKER;
}

template<typename T>
SIRENGINE_FORCEINLINE UtlMemory<T>::UtlMemory( const T *pMemory, uint64_t numElements )
	: m_pMemory( (T *)pMemory ), m_nAllocationCount( numElements )
{
	// Special marker indicating externally supplied modifyable memory
	m_nGrowSize = EXTERNAL_CONST_BUFFER_MARKER;
}

template<typename T>
SIRENGINE_FORCEINLINE UtlMemory<T>::~UtlMemory()
{
	Purge();

#ifdef _DEBUG
	m_pMemory = reinterpret_cast<T *>( 0xFEFEBAAD );
	m_nAllocationCount = 0x7BADF00D;
#endif
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::Init( uint64_t nGrowSize, uint64_t nInitSize )
{
	Purge();

	m_nGrowSize = nGrowSize;
	m_nAllocationCount = nInitSize;
	ValidateGrowSize();
	assert( nGrowSize >= 0 );
	if ( m_nAllocationCount ) {
		m_pMemory = (T *)malloc( m_nAllocationCount * sizeof( T ) );
	}
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::Swap( UtlMemory<T>& mem )
{
	SIREngine_Swap( m_nGrowSize, mem.m_nGrowSize );
	SIREngine_Swap( m_pMemory, mem.m_pMemory );
	SIREngine_Swap( m_nAllocationCount, mem.m_nAllocationCount );
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::ConvertToGrowableMemory( uint64_t nGrowSize )
{
	if ( !IsExternallyAllocated() ) {
		return;
	}

	m_nGrowSize = nGrowSize;
	if ( m_nAllocationCount ) {
		const uint64_t nNumBytes = m_nAllocationCount * sizeof( T );
		T *pMemory = (T *)malloc( nNumBytes );
		memcpy( pMemory, m_pMemory, nNumBytes ); 
		m_pMemory = pMemory;
	}
	else {
		m_pMemory = NULL;
	}
}


template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::SetExternalBuffer( T *pMemory, uint64_t numElements )
{
	Purge();

	m_pMemory = pMemory;
	m_nAllocationCount = numElements;

	// indicate that we don't own the memory
	m_nGrowSize = EXTERNAL_BUFFER_MARKER;
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::SetExternalBuffer( const T *pMemory, uint64_t numElements )
{
	Purge();

	m_pMemory = const_cast<T *>( pMemory );
	m_nAllocationCount = numElements;

	// indicate that we don't own the memory
	m_nGrowSize = EXTERNAL_CONST_BUFFER_MARKER;
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::AssumeMemory( T *pMemory, uint64_t numElements )
{
	Purge();

	// simply take the pointer but don't mark us as external
	m_pMemory = pMemory;
	m_nAllocationCount = numElements;
}

template<typename T>
SIRENGINE_FORCEINLINE void *UtlMemory<T>::DetachMemory( void )
{
	if ( IsExternallyAllocated() ) {
		return NULL;
	}

	void *pMemory = m_pMemory;
	m_pMemory = 0;
	m_nAllocationCount = 0;
	return pMemory;
}

template<typename T>
SIRENGINE_FORCEINLINE T *UtlMemory<T>::Detach( void )
{
	return (T *)DetachMemory();
}

template<typename T>
SIRENGINE_FORCEINLINE T& UtlMemory<T>::operator[]( uint64_t i )
{
	assert( !IsReadOnly() );
	assert( IsIdxValid( i ) );
	return m_pMemory[i];
}

template<typename T>
SIRENGINE_FORCEINLINE const T& UtlMemory<T>::operator[]( uint64_t i ) const
{
	assert( IsIdxValid( i ) );
	return m_pMemory[i];
}

template<typename T>
SIRENGINE_FORCEINLINE T& UtlMemory<T>::Element( uint64_t i )
{
	assert( !IsReadOnly() );
	assert( IsIdxValid( i ) );
	return m_pMemory[i];
}

template<typename T>
SIRENGINE_FORCEINLINE const T& UtlMemory<T>::Element( uint64_t i ) const
{
	assert( IsIdxValid( i ) );
	return m_pMemory[i];
}

template<typename T>
SIRENGINE_FORCEINLINE bool UtlMemory<T>::IsExternallyAllocated( void ) const
{
	return ( m_nGrowSize < 0 );
}

template<typename T>
SIRENGINE_FORCEINLINE bool UtlMemory<T>::IsReadOnly( void ) const
{
	return ( m_nGrowSize == EXTERNAL_CONST_BUFFER_MARKER );
}


template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::SetGrowSize( uint64_t nSize )
{
	assert( !IsExternallyAllocated() );
	assert( nSize >= 0 );
	m_nGrowSize = nSize;
	ValidateGrowSize();
}

template<typename T>
SIRENGINE_FORCEINLINE T *UtlMemory<T>::Base( void )
{
	assert( !IsReadOnly() );
	return m_pMemory;
}

template<typename T>
SIRENGINE_FORCEINLINE const T *UtlMemory<T>::Base( void ) const
{
	return m_pMemory;
}

template<typename T>
SIRENGINE_FORCEINLINE uint64_t UtlMemory<T>::NumAllocated( void ) const
{
	return m_nAllocationCount;
}

template<typename T>
SIRENGINE_FORCEINLINE uint64_t UtlMemory<T>::Count( void ) const
{
	return m_nAllocationCount;
}

template<typename T>
SIRENGINE_FORCEINLINE bool UtlMemory<T>::IsIdxValid( uint64_t i ) const
{
	return ( i >= 0 ) && ( i < m_nAllocationCount );
}

SIRENGINE_FORCEINLINE uint64_t UtlMemory_CalcNewAllocationCount( uint64_t nAllocationCount, uint64_t nGrowSize, uint64_t nNewSize, uint64_t nBytesItem )
{
	if ( nGrowSize ) {
		nAllocationCount = ( ( 1 + ( ( nNewSize - 1 ) / nGrowSize ) ) * nGrowSize );
	}
	else {
		if ( !nAllocationCount ) {
			// Compute an allocation which is at least as big as a cache line...
			nAllocationCount = ( 31 + nBytesItem ) / nBytesItem;
		}

		while ( nAllocationCount < nNewSize ) {
#if !defined(SIRENGINE_XBOX360)
			nAllocationCount *= 2;
#else
			const uint64_t nNewAllocationCount = ( nAllocationCount * 9 ) / 8; // 12.5 %
			if ( nNewAllocationCount > nAllocationCount ) {
				nAllocationCount = nNewAllocationCount;
			} else {
				nAllocationCount *= 2;
			}
#endif
		}
	}

	return nAllocationCount;
}


template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::Grow( uint64_t num )
{
	assert( num > 0 );

	if ( IsExternallyAllocated() ) {
		// can't grow a buffer whose memory isn't ours
		assert( false );
		return;
	}

	// Make sure we have at least numallocated + num allocations.
	// Use the grow rules specified for this memory (in m_nGrowSize)
	const uint64_t nAllocationRequested = m_nAllocationCount + num;
	uint64_t nNewAllocationCount = UtlMemory_CalcNewAllocationCount( m_nAllocationCount, m_nGrowSize, nAllocationRequested, sizeof(T) );

	// if m_nAllocationRequested wraps index type I, recalculate
	if ( nNewAllocationCount < nAllocationRequested ) {
		if ( nNewAllocationCount == 0 && nNewAllocationCount - 1 >= nAllocationRequested ) {
			--nNewAllocationCount; // deal w/ the common case of m_nAllocationCount == MAX_USHORT + 1
		}
		else {
			if ( nAllocationRequested != nAllocationRequested ) {
				// we've been asked to grow memory to a size s.t. the index type can't address the requested amount of memory
				Assert( 0 );
				return;
			}
			while ( nNewAllocationCount < nAllocationRequested ) {
				nNewAllocationCount = ( nNewAllocationCount + nAllocationRequested ) / 2;
			}
		}
	}

	m_nAllocationCount = nNewAllocationCount;

	if ( m_pMemory ) {
		m_pMemory = (T *)realloc( m_pMemory, m_nAllocationCount * sizeof( T ) );
		assert( m_pMemory );
	}
	else {
		m_pMemory = (T *)malloc( m_nAllocationCount * sizeof(T) );
		assert( m_pMemory );
	}
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::EnsureCapacity( uint64_t num )
{
    if ( m_nAllocationCount >= num ) {
        return;
    }

    if ( IsExternallyAllocated() ) {
        // Can't grow a buffer whose memory was externally allocated 
		assert( 0 );
		return;
    }

	m_nAllocationCount = num;

	if ( m_pMemory ) {
		m_pMemory = (T *)realloc( m_pMemory, m_nAllocationCount * sizeof( T ) );
	} else {
		m_pMemory = (T *)malloc( m_nAllocationCount * sizeof( T ) );
	}
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::Purge( void )
{
	if ( !IsExternallyAllocated() ) {
		if ( m_pMemory ) {
			free( (void *)m_pMemory );
			m_pMemory = NULL;
		}
		m_nAllocationCount = 0;
	}
}

template<typename T>
SIRENGINE_FORCEINLINE void UtlMemory<T>::Purge( uint64_t numElements )
{
	assert( numElements >= 0 );

	if ( numElements > m_nAllocationCount ) {
		// ensure this isn't a grow request in disguise
		assert( numElements <= m_nAllocationCount );
		return;
	}
	
	// if we have zero elements, simply do a purge
	if ( numElements == 0 ) {
		Purge();
		return;
	}

	if ( IsExternallyAllocated() ) {
		// can't shrink a buffer whose memory was externally allocated, fail silently like purge
		return;
	}

	// if the number of elements is the same as the allocation count, we are done.
	if ( numElements == m_nAllocationCount ) {
		return;
	}

	if ( !m_pMemory ) {
		// allocation count is non zero, but memory is null
		assert( m_pMemory );
		return;
	}

	m_nAllocationCount = numElements;

	// allocation > 0, shrink it down
	m_pMemory = (T *)realloc( m_pMemory, m_nAllocationCount * sizeof( T ) );
}
*/


class UtlMallocAllocator
{
public:
    inline UtlMallocAllocator( void )
	{ }
	inline UtlMallocAllocator( const char *pAllocatorName)
	{ }
    ~UtlMallocAllocator()
	{ }

    inline void *allocate( size_t nSize, size_t nAlignment = 16, size_t nOffset = 16 )
	{ return malloc( nSize ); }
    inline void deallocate( void *pMemory, size_t nSize = 0 )
	{ free( pMemory ); }
};

class UtlGenericAllocator
{
public:
	inline UtlGenericAllocator( void )
	{ }
	inline UtlGenericAllocator( const char *pAllocatorName )
	{ }
    ~UtlGenericAllocator()
	{ }

    inline void *allocate( size_t nSize, size_t nAlignment = 16, size_t nOffset = 16 )
	{ return new char[ nSize ]; }
    inline void deallocate( void *pMemory, size_t nSize = 0 )
	{ delete[] (char *)pMemory; }
};

template<typename AllocatorType, typename T = uint8_t>
class UtlAllocatorAdaptor
{
public:
    inline UtlAllocatorAdaptor( void )
	{ }
	inline UtlAllocatorAdaptor( const char *pAllocatorName )
	{ }
    ~UtlAllocatorAdaptor()
	{ }

    inline void *allocate( size_t nSize, size_t nAlignment = 16, size_t nOffset = 16 ) {
        return m_Allocator.Alloc( nSize );
    }
    inline void deallocate( void *pMemory, size_t nSize = 0 ) {
        m_Allocator.Free( pMemory );
    }
private:
    AllocatorType m_Allocator;
};

#endif