/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_SHAREDPTR_H__
#define __SIR_SHAREDPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>

#include <limits.h>
#include <new>
#include <stdlib.h>
#include <type_traits>
#include <utility>

// Ownership allocations are explicitly supplied by the caller. The allocator
// context and its backing heap must outlive every strong and weak reference.
// The callbacks must support the requested size and alignment, and return
// nullptr rather than throwing on allocation failure.
typedef struct sirSharedAllocationCallbacks_s
{
	void *pContext;
	void *( *pAllocate )( void *pContext, size_t nBytes, size_t nAlignment ) noexcept;
	void ( *pDeallocate )( void *pContext, void *pMemory ) noexcept;
} sirSharedAllocationCallbacks_t;

// std::atomic is the engine's only atomic implementation used by these wrappers.
class SIRSharedAtomicCounter
{
public:
	explicit SIRSharedAtomicCounter( uint32_t nInitial ) noexcept : m_nCount( nInitial )
	{
	}
	SIRSharedAtomicCounter( const SIRSharedAtomicCounter& ) = delete;
	SIRSharedAtomicCounter& operator=( const SIRSharedAtomicCounter& ) = delete;

	uint32_t get( void ) const noexcept
	{
		return m_nCount.load( std::memory_order_acquire );
	}
	void add( void ) noexcept
	{
		// An existing owner guarantees that the count cannot already be zero.
		// Weak promotion still uses tryAdd() to reject an expired object.
		const uint32_t nPrevious = m_nCount.fetch_add( 1, std::memory_order_relaxed );
		if ( nPrevious == 0 || nPrevious == UINT32_MAX ) {
			abort();
		}
	}
	bool tryAdd( void ) noexcept
	{
		uint32_t nCurrent = m_nCount.load( std::memory_order_relaxed );
		while ( nCurrent != 0 ) {
			if ( nCurrent == UINT32_MAX ) {
				abort();
			}
			if ( m_nCount.compare_exchange_weak( nCurrent, nCurrent + 1, std::memory_order_acquire, std::memory_order_relaxed ) ) {
				return true;
			}
		}
		return false;
	}
	bool release( void ) noexcept
	{
		const uint32_t nOld = m_nCount.fetch_sub( 1, std::memory_order_acq_rel );
		if ( nOld == 0 ) {
			abort();
		}
		return nOld == 1;
	}

private:
	std::atomic<uint32_t> m_nCount;
};

// Single-thread-only ownership. Copies, weak lock, and destruction must all
// occur on the same thread (or under external serialization).
class SIRSharedLocalCounter
{
public:
	explicit SIRSharedLocalCounter( uint32_t nInitial ) noexcept : m_nCount( nInitial )
	{
	}
	SIRSharedLocalCounter( const SIRSharedLocalCounter& ) = delete;
	SIRSharedLocalCounter& operator=( const SIRSharedLocalCounter& ) = delete;

	uint32_t get( void ) const noexcept
	{
		return m_nCount;
	}
	void add( void ) noexcept
	{
		if ( !tryAdd() ) {
			abort();
		}
	}
	bool tryAdd( void ) noexcept
	{
		if ( m_nCount == 0 ) {
			return false;
		}
		if ( m_nCount == UINT32_MAX ) {
			abort();
		}
		++m_nCount;
		return true;
	}
	bool release( void ) noexcept
	{
		if ( m_nCount == 0 ) {
			abort();
		}
		--m_nCount;
		return m_nCount == 0;
	}

private:
	uint32_t m_nCount;
};

template <typename TCounter>
class SIRSharedControlBlock
{
public:
	typedef void ( *sirDestroyCallback_t )( SIRSharedControlBlock *pBlock ) noexcept;

	SIRSharedControlBlock( sirDestroyCallback_t pDestroyObject, sirDestroyCallback_t pDestroyBlock ) noexcept
	    : m_Strong( 1 ), m_Weak( 1 ), m_pDestroyObject( pDestroyObject ), m_pDestroyBlock( pDestroyBlock )
	{
	}
	SIRSharedControlBlock( const SIRSharedControlBlock& ) = delete;
	SIRSharedControlBlock& operator=( const SIRSharedControlBlock& ) = delete;

	uint32_t strongCount( void ) const noexcept
	{
		return m_Strong.get();
	}
	void addStrong( void ) noexcept
	{
		m_Strong.add();
	}
	bool tryAddStrong( void ) noexcept
	{
		return m_Strong.tryAdd();
	}
	void releaseStrong( void ) noexcept
	{
		if ( m_Strong.release() ) {
			m_pDestroyObject( this );
			releaseWeak(); // Drop the implicit weak ownership of the strong group.
		}
	}
	void addWeak( void ) noexcept
	{
		m_Weak.add();
	}
	void releaseWeak( void ) noexcept
	{
		if ( m_Weak.release() ) {
			m_pDestroyBlock( this );
		}
	}

private:
	TCounter m_Strong;
	TCounter m_Weak;
	sirDestroyCallback_t m_pDestroyObject;
	sirDestroyCallback_t m_pDestroyBlock;
};

template <typename T, typename TCounter>
class SIRSharedObjectBlock final : public SIRSharedControlBlock<TCounter>
{
public:
	template <typename... TArgs>
	explicit SIRSharedObjectBlock( const sirSharedAllocationCallbacks_t& rCallbacks, TArgs&&...rArgs )
	    : SIRSharedControlBlock<TCounter>( &SIRSharedObjectBlock::destroyObject, &SIRSharedObjectBlock::destroyBlock ),
	      m_pAllocatorContext( rCallbacks.pContext ), m_pDeallocate( rCallbacks.pDeallocate )
	{
		new ( m_szStorage ) T( std::forward<TArgs>( rArgs )... );
	}

	T *get( void ) noexcept
	{
		return std::launder( reinterpret_cast<T *>(m_szStorage) );
	}

private:
	static void destroyObject( SIRSharedControlBlock<TCounter> *pBase ) noexcept
	{
		SIRSharedObjectBlock *pBlock = static_cast<SIRSharedObjectBlock *>( pBase );
		pBlock->get()->~T();
	}
	static void destroyBlock( SIRSharedControlBlock<TCounter> *pBase ) noexcept
	{
		SIRSharedObjectBlock *pBlock = static_cast<SIRSharedObjectBlock *>( pBase );
		void *pContext = pBlock->m_pAllocatorContext;
		void ( *pDeallocate )( void *, void * ) noexcept = pBlock->m_pDeallocate;
		pBlock->~SIRSharedObjectBlock();
		pDeallocate( pContext, pBlock );
	}

	void *m_pAllocatorContext;
	void ( *m_pDeallocate )( void *, void * ) noexcept;
	alignas( T ) unsigned char m_szStorage[sizeof( T )];
};

class SIRSharedFactory;

template <typename T, typename TCounter>
class SIRBasicWeakPtr;

template <typename T, typename TCounter>
class SIRBasicSharedPtr
{
public:
	SIRBasicSharedPtr( void ) noexcept : m_pObject( nullptr ), m_pControl( nullptr )
	{
	}
	SIRBasicSharedPtr( const SIRBasicSharedPtr& rOther ) noexcept : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		retain();
	}
	template <typename U, typename std::enable_if<std::is_convertible<U *, T *>::value, int>::type = 0>
	SIRBasicSharedPtr( const SIRBasicSharedPtr<U, TCounter>& rOther ) noexcept
	    : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		retain();
	}
	SIRBasicSharedPtr( SIRBasicSharedPtr&& rOther ) noexcept : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		rOther.clear();
	}
	template <typename U, typename std::enable_if<std::is_convertible<U *, T *>::value, int>::type = 0>
	SIRBasicSharedPtr( SIRBasicSharedPtr<U, TCounter>&& rOther ) noexcept : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		rOther.clear();
	}
	~SIRBasicSharedPtr( void ) noexcept
	{
		reset();
	}
	SIRBasicSharedPtr& operator=( const SIRBasicSharedPtr& rOther ) noexcept
	{
		if ( m_pControl == rOther.m_pControl && m_pObject == rOther.m_pObject ) {
			return *this;
		}

		SIRBasicSharedPtr copy( rOther );
		swap( copy );
		return *this;
	}
	SIRBasicSharedPtr& operator=( SIRBasicSharedPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			reset();
			m_pObject = rOther.m_pObject;
			m_pControl = rOther.m_pControl;
			rOther.clear();
		}
		return *this;
	}
	T *get( void ) const noexcept
	{
		return m_pObject;
	}
	T *operator->( void ) const noexcept
	{
		return m_pObject;
	}
	T& operator*( void ) const noexcept
	{
		return *m_pObject;
	}
	explicit operator bool( void ) const noexcept
	{
		return isValid();
	}
	bool isValid( void ) const noexcept
	{
		return m_pObject != nullptr;
	}
	uint32_t useCount( void ) const noexcept
	{
		return m_pControl != nullptr ? m_pControl->strongCount() : 0;
	}
	void reset( void ) noexcept
	{
		SIRSharedControlBlock<TCounter> *pControl = m_pControl;
		clear();
		if ( pControl != nullptr ) {
			pControl->releaseStrong();
		}
	}
	void swap( SIRBasicSharedPtr& rOther ) noexcept
	{
		T *pObject = m_pObject;
		SIRSharedControlBlock<TCounter> *pControl = m_pControl;
		m_pObject = rOther.m_pObject;
		m_pControl = rOther.m_pControl;
		rOther.m_pObject = pObject;
		rOther.m_pControl = pControl;
	}

private:
	friend class SIRSharedFactory;
	template <typename U, typename UCounter>
	friend class SIRBasicSharedPtr;
	template <typename U, typename UCounter>
	friend class SIRBasicWeakPtr;

	SIRBasicSharedPtr( T *pObject, SIRSharedControlBlock<TCounter> *pControl ) noexcept : m_pObject( pObject ), m_pControl( pControl )
	{
	}
	void retain( void ) noexcept
	{
		if ( m_pControl != nullptr ) {
			m_pControl->addStrong();
		}
	}
	void clear( void ) noexcept
	{
		m_pObject = nullptr;
		m_pControl = nullptr;
	}

	T *m_pObject;
	SIRSharedControlBlock<TCounter> *m_pControl;
};

template <typename T, typename TCounter>
class SIRBasicWeakPtr
{
public:
	SIRBasicWeakPtr( void ) noexcept : m_pObject( nullptr ), m_pControl( nullptr )
	{
	}
	SIRBasicWeakPtr( const SIRBasicWeakPtr& rOther ) noexcept : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		retain();
	}
	template <typename U, typename std::enable_if<std::is_convertible<U *, T *>::value, int>::type = 0>
	SIRBasicWeakPtr( const SIRBasicWeakPtr<U, TCounter>& rOther ) noexcept : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		retain();
	}
	template <typename U, typename std::enable_if<std::is_convertible<U *, T *>::value, int>::type = 0>
	SIRBasicWeakPtr( const SIRBasicSharedPtr<U, TCounter>& rOther ) noexcept
	    : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		retain();
	}
	SIRBasicWeakPtr( SIRBasicWeakPtr&& rOther ) noexcept : m_pObject( rOther.m_pObject ), m_pControl( rOther.m_pControl )
	{
		rOther.clear();
	}
	~SIRBasicWeakPtr( void ) noexcept
	{
		reset();
	}
	SIRBasicWeakPtr& operator=( const SIRBasicWeakPtr& rOther ) noexcept
	{
		if ( m_pControl == rOther.m_pControl && m_pObject == rOther.m_pObject ) {
			return *this;
		}

		SIRBasicWeakPtr copy( rOther );
		swap( copy );
		return *this;
	}
	template <typename U, typename std::enable_if<std::is_convertible<U *, T *>::value, int>::type = 0>
	SIRBasicWeakPtr& operator=( const SIRBasicSharedPtr<U, TCounter>& rOther ) noexcept
	{
		if ( m_pControl == rOther.m_pControl && m_pObject == rOther.m_pObject ) {
			return *this;
		}

		SIRBasicWeakPtr copy( rOther );
		swap( copy );
		return *this;
	}
	SIRBasicWeakPtr& operator=( SIRBasicWeakPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			reset();
			m_pObject = rOther.m_pObject;
			m_pControl = rOther.m_pControl;
			rOther.clear();
		}
		return *this;
	}
	bool expired( void ) const noexcept
	{
		return useCount() == 0;
	}
	uint32_t useCount( void ) const noexcept
	{
		return m_pControl != nullptr ? m_pControl->strongCount() : 0;
	}
	SIRBasicSharedPtr<T, TCounter> lock( void ) const noexcept
	{
		if ( m_pControl == nullptr || !m_pControl->tryAddStrong() ) {
			return {};
		}
		return SIRBasicSharedPtr<T, TCounter>( m_pObject, m_pControl );
	}
	void reset( void ) noexcept
	{
		SIRSharedControlBlock<TCounter> *pControl = m_pControl;
		clear();
		if ( pControl != nullptr ) {
			pControl->releaseWeak();
		}
	}
	void swap( SIRBasicWeakPtr& rOther ) noexcept
	{
		T *pObject = m_pObject;
		SIRSharedControlBlock<TCounter> *pControl = m_pControl;
		m_pObject = rOther.m_pObject;
		m_pControl = rOther.m_pControl;
		rOther.m_pObject = pObject;
		rOther.m_pControl = pControl;
	}

private:
	template <typename U, typename UCounter>
	friend class SIRBasicWeakPtr;

	void retain( void ) noexcept
	{
		if ( m_pControl != nullptr ) {
			m_pControl->addWeak();
		}
	}
	void clear( void ) noexcept
	{
		m_pObject = nullptr;
		m_pControl = nullptr;
	}

	T *m_pObject;
	SIRSharedControlBlock<TCounter> *m_pControl;
};

class SIRSharedFactory
{
public:
	SIRSharedFactory( void ) = delete;

	template <typename T, typename TCounter, typename... TArgs>
	static SIRBasicSharedPtr<T, TCounter> create( const sirSharedAllocationCallbacks_t& rCallbacks, TArgs&&...rArgs )
	{
		if ( rCallbacks.pAllocate == nullptr || rCallbacks.pDeallocate == nullptr ) {
			return {};
		}

		typedef SIRSharedObjectBlock<T, TCounter> sirBlock_t;
		void *pStorage = rCallbacks.pAllocate( rCallbacks.pContext, sizeof( sirBlock_t ), alignof( sirBlock_t ) );
		if ( pStorage == nullptr ) {
			return {};
		}

#if defined( __cpp_exceptions ) || defined( _CPPUNWIND )
		try {
			sirBlock_t *pBlock = new ( pStorage ) sirBlock_t( rCallbacks, std::forward<TArgs>( rArgs )... );
			return SIRBasicSharedPtr<T, TCounter>( pBlock->get(), pBlock );
		} catch ( ... ) {
			rCallbacks.pDeallocate( rCallbacks.pContext, pStorage );
			throw;
		}
#else
		sirBlock_t *pBlock = new ( pStorage ) sirBlock_t( rCallbacks, std::forward<TArgs>( rArgs )... );
		return SIRBasicSharedPtr<T, TCounter>( pBlock->get(), pBlock );
#endif
	}
};

template <typename T>
using SIRSharedPtr = SIRBasicSharedPtr<T, SIRSharedAtomicCounter>;

template <typename T>
using SIRWeakPtr = SIRBasicWeakPtr<T, SIRSharedAtomicCounter>;

template <typename T>
using SIRLocalSharedPtr = SIRBasicSharedPtr<T, SIRSharedLocalCounter>;

template <typename T>
using SIRLocalWeakPtr = SIRBasicWeakPtr<T, SIRSharedLocalCounter>;

template <typename T, typename... TArgs>
SIRSharedPtr<T> SIRMakeShared( const sirSharedAllocationCallbacks_t& rCallbacks, TArgs&&...rArgs )
{
	return SIRSharedFactory::create<T, SIRSharedAtomicCounter>( rCallbacks, std::forward<TArgs>( rArgs )... );
}

template <typename T, typename... TArgs>
SIRLocalSharedPtr<T> SIRMakeLocalShared( const sirSharedAllocationCallbacks_t& rCallbacks, TArgs&&...rArgs )
{
	return SIRSharedFactory::create<T, SIRSharedLocalCounter>( rCallbacks, std::forward<TArgs>( rArgs )... );
}

#endif // __SIR_SHAREDPTR_H__
