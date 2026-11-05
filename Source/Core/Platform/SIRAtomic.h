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

#ifndef __SIRENGINE_SIRATOMIC_H__
#define __SIRENGINE_SIRATOMIC_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

// The legacy NMD ordering constants are retained numerically for binary data
// interchange. Convert explicitly: std::memory_order has no fixed numeric ABI.
typedef enum sirMemoryOrder_e : uint8_t
{
	SIR_MEMORY_ORDER_RELAXED = 0,
	SIR_MEMORY_ORDER_ACQUIRE = 2,
	SIR_MEMORY_ORDER_RELEASE = 3,
	SIR_MEMORY_ORDER_ACQUIRE_RELEASE = 4,
	SIR_MEMORY_ORDER_SEQUENTIAL = 5
} sirMemoryOrder_t;

class SIRAtomicOrder
{
public:
	SIRAtomicOrder( void ) = delete;
	static constexpr std::memory_order toNative( sirMemoryOrder_t eOrder ) noexcept;
	static constexpr sirMemoryOrder_t failureOrder( sirMemoryOrder_t eSuccessOrder ) noexcept;
	static constexpr sirMemoryOrder_t normalizeFailure( sirMemoryOrder_t eSuccessOrder, sirMemoryOrder_t eFailureOrder ) noexcept;
	static constexpr sirMemoryOrder_t normalizeLoad( sirMemoryOrder_t eOrder ) noexcept;
	static constexpr sirMemoryOrder_t normalizeStore( sirMemoryOrder_t eOrder ) noexcept;
};

constexpr std::memory_order SIRAtomicOrder::toNative( sirMemoryOrder_t eOrder ) noexcept
{
	switch ( eOrder ) {
	case SIR_MEMORY_ORDER_RELAXED:
		return std::memory_order_relaxed;
	case SIR_MEMORY_ORDER_ACQUIRE:
		return std::memory_order_acquire;
	case SIR_MEMORY_ORDER_RELEASE:
		return std::memory_order_release;
	case SIR_MEMORY_ORDER_ACQUIRE_RELEASE:
		return std::memory_order_acq_rel;
	case SIR_MEMORY_ORDER_SEQUENTIAL:
	default:
		return std::memory_order_seq_cst;
	}
}

constexpr sirMemoryOrder_t SIRAtomicOrder::failureOrder( sirMemoryOrder_t eSuccessOrder ) noexcept
{
	if ( eSuccessOrder == SIR_MEMORY_ORDER_RELEASE ) {
		return SIR_MEMORY_ORDER_RELAXED;
	}
	if ( eSuccessOrder == SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) {
		return SIR_MEMORY_ORDER_ACQUIRE;
	}
	return eSuccessOrder;
}

constexpr sirMemoryOrder_t SIRAtomicOrder::normalizeFailure( sirMemoryOrder_t eSuccessOrder, sirMemoryOrder_t eFailureOrder ) noexcept
{
	if ( eFailureOrder == SIR_MEMORY_ORDER_RELEASE ) {
		eFailureOrder = SIR_MEMORY_ORDER_RELAXED;
	} else if ( eFailureOrder == SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) {
		eFailureOrder = SIR_MEMORY_ORDER_ACQUIRE;
	}

	switch ( eSuccessOrder ) {
	case SIR_MEMORY_ORDER_RELAXED:
		return SIR_MEMORY_ORDER_RELAXED;
	case SIR_MEMORY_ORDER_RELEASE:
		return SIR_MEMORY_ORDER_RELAXED;
	case SIR_MEMORY_ORDER_ACQUIRE:
	case SIR_MEMORY_ORDER_ACQUIRE_RELEASE:
		return eFailureOrder == SIR_MEMORY_ORDER_SEQUENTIAL ? SIR_MEMORY_ORDER_ACQUIRE : eFailureOrder;
	case SIR_MEMORY_ORDER_SEQUENTIAL:
	default:
		return eFailureOrder;
	}
}

constexpr sirMemoryOrder_t SIRAtomicOrder::normalizeLoad( sirMemoryOrder_t eOrder ) noexcept
{
	return ( eOrder == SIR_MEMORY_ORDER_RELEASE || eOrder == SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) ? SIR_MEMORY_ORDER_SEQUENTIAL : eOrder;
}

constexpr sirMemoryOrder_t SIRAtomicOrder::normalizeStore( sirMemoryOrder_t eOrder ) noexcept
{
	return ( eOrder == SIR_MEMORY_ORDER_ACQUIRE || eOrder == SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) ? SIR_MEMORY_ORDER_SEQUENTIAL : eOrder;
}

// Fixed-size, lock-free atomic primitive. No allocation, shared ownership,
// or fallback mutex. The selected compiler must supply lock-free storage.
template <typename T>
class SIRAtomic
{
public:
	typedef T value_type;

	static_assert( std::is_arithmetic<T>::value || std::is_enum<T>::value || std::is_pointer<T>::value,
	               "SIRAtomic accepts arithmetic, enum, or pointer types" );
	static_assert( !std::is_const<T>::value && !std::is_volatile<T>::value, "SIRAtomic requires mutable storage" );
	static_assert( sizeof( T ) == 1 || sizeof( T ) == 2 || sizeof( T ) == 4 || sizeof( T ) == 8,
	               "SIRAtomic only supports 1/2/4/8 byte values" );
	static_assert( std::atomic<T>::is_always_lock_free, "SIRAtomic requires a lock-free atomic for this target" );

	SIRAtomic( void ) noexcept;
	explicit SIRAtomic( T value ) noexcept;
	SIRAtomic( const SIRAtomic& ) = delete;
	SIRAtomic& operator=( const SIRAtomic& ) = delete;
	SIRAtomic( SIRAtomic&& ) = delete;
	SIRAtomic& operator=( SIRAtomic&& ) = delete;

	T load( sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) const noexcept;
	void store( T value, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	T exchange( T desired, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	bool compareExchangeStrong( T& expected, T desired, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	bool compareExchangeStrong( T& expected, T desired, sirMemoryOrder_t eSuccess, sirMemoryOrder_t eFailure ) noexcept;
	bool compareExchangeWeak( T& expected, T desired, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	bool compareExchangeWeak( T& expected, T desired, sirMemoryOrder_t eSuccess, sirMemoryOrder_t eFailure ) noexcept;

	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
	                                      !std::is_same<U, bool>::value,
	                                  int>::type = 0>
	T fetchAdd( T amount, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
	                                      !std::is_same<U, bool>::value,
	                                  int>::type = 0>
	T fetchSub( T amount, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	template <typename U = T, typename std::enable_if<
	                              std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type = 0>
	T fetchAnd( T mask, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	template <typename U = T, typename std::enable_if<
	                              std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type = 0>
	T fetchOr( T mask, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	template <typename U = T, typename std::enable_if<
	                              std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type = 0>
	T fetchXor( T mask, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;

	template <typename U = T, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
	                                                      std::is_object<typename std::remove_pointer<U>::type>::value,
	                                                  int>::type = 0>
	T fetchAdd( ptrdiff_t nAmount, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	template <typename U = T, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
	                                                      std::is_object<typename std::remove_pointer<U>::type>::value,
	                                                  int>::type = 0>
	T fetchSub( ptrdiff_t nAmount, sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;

	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
	                                      !std::is_same<U, bool>::value,
	                                  int>::type = 0>
	T operator+=( T amount ) noexcept;
	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
	                                      !std::is_same<U, bool>::value,
	                                  int>::type = 0>
	T operator-=( T amount ) noexcept;
	template <typename U = T, typename std::enable_if<
	                              std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type = 0>
	T operator&=( T mask ) noexcept;
	template <typename U = T, typename std::enable_if<
	                              std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type = 0>
	T operator|=( T mask ) noexcept;
	template <typename U = T, typename std::enable_if<
	                              std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type = 0>
	T operator^=( T mask ) noexcept;
	template <typename U = T, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
	                                                      std::is_object<typename std::remove_pointer<U>::type>::value,
	                                                  int>::type = 0>
	T operator+=( ptrdiff_t nAmount ) noexcept;
	template <typename U = T, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
	                                                      std::is_object<typename std::remove_pointer<U>::type>::value,
	                                                  int>::type = 0>
	T operator-=( ptrdiff_t nAmount ) noexcept;

	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value &&
	                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
	                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
	                                  int>::type = 0>
	T operator++( void ) noexcept;
	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value &&
	                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
	                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
	                                  int>::type = 0>
	T operator++( int ) noexcept;
	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value &&
	                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
	                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
	                                  int>::type = 0>
	T operator--( void ) noexcept;
	template <typename U = T,
	          typename std::enable_if<std::is_same<U, T>::value &&
	                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
	                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
	                                  int>::type = 0>
	T operator--( int ) noexcept;

	T operator=( T value ) noexcept;
	operator T() const noexcept;
	bool isLockFree( void ) const noexcept;
	static constexpr bool isAlwaysLockFree( void ) noexcept;

private:
	std::atomic<T> m_value;
};

template <typename T>
SIRAtomic<T>::SIRAtomic( void ) noexcept : m_value( T{} )
{
}

template <typename T>
SIRAtomic<T>::SIRAtomic( T value ) noexcept : m_value( value )
{
}

template <typename T>
T SIRAtomic<T>::load( sirMemoryOrder_t eOrder ) const noexcept
{
	return m_value.load( SIRAtomicOrder::toNative( SIRAtomicOrder::normalizeLoad( eOrder ) ) );
}

template <typename T>
void SIRAtomic<T>::store( T value, sirMemoryOrder_t eOrder ) noexcept
{
	m_value.store( value, SIRAtomicOrder::toNative( SIRAtomicOrder::normalizeStore( eOrder ) ) );
}

template <typename T>
T SIRAtomic<T>::exchange( T desired, sirMemoryOrder_t eOrder ) noexcept
{
	return m_value.exchange( desired, SIRAtomicOrder::toNative( eOrder ) );
}

template <typename T>
bool SIRAtomic<T>::compareExchangeStrong( T& expected, T desired, sirMemoryOrder_t eOrder ) noexcept
{
	return compareExchangeStrong( expected, desired, eOrder, SIRAtomicOrder::failureOrder( eOrder ) );
}

template <typename T>
bool SIRAtomic<T>::compareExchangeStrong( T& expected, T desired, sirMemoryOrder_t eSuccess, sirMemoryOrder_t eFailure ) noexcept
{
	const sirMemoryOrder_t eNormalized = SIRAtomicOrder::normalizeFailure( eSuccess, eFailure );
	return m_value.compare_exchange_strong( expected, desired, SIRAtomicOrder::toNative( eSuccess ),
	                                        SIRAtomicOrder::toNative( eNormalized ) );
}

template <typename T>
bool SIRAtomic<T>::compareExchangeWeak( T& expected, T desired, sirMemoryOrder_t eOrder ) noexcept
{
	return compareExchangeWeak( expected, desired, eOrder, SIRAtomicOrder::failureOrder( eOrder ) );
}

template <typename T>
bool SIRAtomic<T>::compareExchangeWeak( T& expected, T desired, sirMemoryOrder_t eSuccess, sirMemoryOrder_t eFailure ) noexcept
{
	const sirMemoryOrder_t eNormalized = SIRAtomicOrder::normalizeFailure( eSuccess, eFailure );
	return m_value.compare_exchange_weak( expected, desired, SIRAtomicOrder::toNative( eSuccess ),
	                                      SIRAtomicOrder::toNative( eNormalized ) );
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
                                      !std::is_same<U, bool>::value,
                                  int>::type>
T SIRAtomic<T>::fetchAdd( T amount, sirMemoryOrder_t eOrder ) noexcept
{
	if constexpr ( std::is_floating_point<U>::value ) {
		T expected = load( SIR_MEMORY_ORDER_RELAXED );
		for ( ;; ) {
			const T desired = ( T )( expected + amount );
			if ( compareExchangeWeak( expected, desired, eOrder ) ) {
				return expected;
			}
		}
	} else {
		return m_value.fetch_add( amount, SIRAtomicOrder::toNative( eOrder ) );
	}
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
                                      !std::is_same<U, bool>::value,
                                  int>::type>
T SIRAtomic<T>::fetchSub( T amount, sirMemoryOrder_t eOrder ) noexcept
{
	if constexpr ( std::is_floating_point<U>::value ) {
		T expected = load( SIR_MEMORY_ORDER_RELAXED );
		for ( ;; ) {
			const T desired = ( T )( expected - amount );
			if ( compareExchangeWeak( expected, desired, eOrder ) ) {
				return expected;
			}
		}
	} else {
		return m_value.fetch_sub( amount, SIRAtomicOrder::toNative( eOrder ) );
	}
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type>
T SIRAtomic<T>::fetchAnd( T mask, sirMemoryOrder_t eOrder ) noexcept
{
	return m_value.fetch_and( mask, SIRAtomicOrder::toNative( eOrder ) );
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type>
T SIRAtomic<T>::fetchOr( T mask, sirMemoryOrder_t eOrder ) noexcept
{
	return m_value.fetch_or( mask, SIRAtomicOrder::toNative( eOrder ) );
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type>
T SIRAtomic<T>::fetchXor( T mask, sirMemoryOrder_t eOrder ) noexcept
{
	return m_value.fetch_xor( mask, SIRAtomicOrder::toNative( eOrder ) );
}

template <typename T>
template <typename U, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
                                                  std::is_object<typename std::remove_pointer<U>::type>::value,
                                              int>::type>
T SIRAtomic<T>::fetchAdd( ptrdiff_t nAmount, sirMemoryOrder_t eOrder ) noexcept
{
	return m_value.fetch_add( nAmount, SIRAtomicOrder::toNative( eOrder ) );
}

template <typename T>
template <typename U, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
                                                  std::is_object<typename std::remove_pointer<U>::type>::value,
                                              int>::type>
T SIRAtomic<T>::fetchSub( ptrdiff_t nAmount, sirMemoryOrder_t eOrder ) noexcept
{
	return m_value.fetch_sub( nAmount, SIRAtomicOrder::toNative( eOrder ) );
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
                                      !std::is_same<U, bool>::value,
                                  int>::type>
T SIRAtomic<T>::operator+=( T amount ) noexcept
{
	if constexpr ( std::is_floating_point<U>::value ) {
		return ( T )( fetchAdd( amount ) + amount );
	} else {
		return m_value += amount;
	}
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && ( std::is_integral<U>::value || std::is_floating_point<U>::value ) &&
                                      !std::is_same<U, bool>::value,
                                  int>::type>
T SIRAtomic<T>::operator-=( T amount ) noexcept
{
	if constexpr ( std::is_floating_point<U>::value ) {
		return ( T )( fetchSub( amount ) - amount );
	} else {
		return m_value -= amount;
	}
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type>
T SIRAtomic<T>::operator&=( T mask ) noexcept
{
	return m_value &= mask;
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type>
T SIRAtomic<T>::operator|=( T mask ) noexcept
{
	return m_value |= mask;
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value && std::is_integral<U>::value && !std::is_same<U, bool>::value, int>::type>
T SIRAtomic<T>::operator^=( T mask ) noexcept
{
	return m_value ^= mask;
}

template <typename T>
template <typename U, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
                                                  std::is_object<typename std::remove_pointer<U>::type>::value,
                                              int>::type>
T SIRAtomic<T>::operator+=( ptrdiff_t nAmount ) noexcept
{
	return m_value += nAmount;
}

template <typename T>
template <typename U, typename std::enable_if<std::is_same<U, T>::value && std::is_pointer<U>::value &&
                                                  std::is_object<typename std::remove_pointer<U>::type>::value,
                                              int>::type>
T SIRAtomic<T>::operator-=( ptrdiff_t nAmount ) noexcept
{
	return m_value -= nAmount;
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value &&
                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
                                  int>::type>
T SIRAtomic<T>::operator++( void ) noexcept
{
	return ++m_value;
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value &&
                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
                                  int>::type>
T SIRAtomic<T>::operator++( int ) noexcept
{
	return m_value++;
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value &&
                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
                                  int>::type>
T SIRAtomic<T>::operator--( void ) noexcept
{
	return --m_value;
}

template <typename T>
template <typename U,
          typename std::enable_if<std::is_same<U, T>::value &&
                                      ( ( std::is_integral<U>::value && !std::is_same<U, bool>::value ) ||
                                        ( std::is_pointer<U>::value && std::is_object<typename std::remove_pointer<U>::type>::value ) ),
                                  int>::type>
T SIRAtomic<T>::operator--( int ) noexcept
{
	return m_value--;
}

template <typename T>
T SIRAtomic<T>::operator=( T value ) noexcept
{
	store( value );
	return value;
}

template <typename T>
SIRAtomic<T>::operator T() const noexcept
{
	return load();
}

template <typename T>
bool SIRAtomic<T>::isLockFree( void ) const noexcept
{
	return m_value.is_lock_free();
}

template <typename T>
constexpr bool SIRAtomic<T>::isAlwaysLockFree( void ) noexcept
{
	return std::atomic<T>::is_always_lock_free;
}

class SIRAtomicFlag
{
public:
	SIRAtomicFlag( void ) noexcept;
	SIRAtomicFlag( const SIRAtomicFlag& ) = delete;
	SIRAtomicFlag& operator=( const SIRAtomicFlag& ) = delete;
	bool test( sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) const noexcept;
	bool testAndSet( sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	void clear( sirMemoryOrder_t eOrder = SIR_MEMORY_ORDER_SEQUENTIAL ) noexcept;
	bool isLockFree( void ) const noexcept;

private:
	SIRAtomic<bool> m_bValue;
};

#endif // __SIRENGINE_SIRATOMIC_H__
