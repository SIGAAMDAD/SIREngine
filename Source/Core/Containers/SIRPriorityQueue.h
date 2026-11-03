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

#ifndef __SIRENGINE_SIRPRIORITYQUEUE_H__
#define __SIRENGINE_SIRPRIORITYQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <functional>
#include <type_traits>
#include <utility>

// The highest-priority element (according to Compare) is stored at index zero.
template <typename T>
class SIRPriorityLess
{
public:
    bool operator()( const T &left, const T &right ) const noexcept( noexcept( left < right ) )
    {
        return left < right;
    }
};

template <typename T, typename Compare = SIRPriorityLess<T>>
class SIRPriorityQueue
{
private:
    static_assert( std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value,
        "Priority queue updates require noexcept movable values" );
    static_assert( std::is_nothrow_invocable_r<bool, Compare, const T &, const T &>::value,
        "Priority comparisons must be noexcept" );
    SIRVector<T> m_values;
    Compare m_compare;

    void siftUp( uint32_t iChild ) noexcept
    {
        if ( iChild == 0 ) {
            return;
        }
        uint32_t iParent = ( iChild - 1 ) / 2;
        if ( !m_compare( m_values[iParent], m_values[iChild] ) ) {
            return;
        }
        T value = std::move( m_values[iChild] );
        do {
            m_values[iChild] = std::move( m_values[iParent] );
            iChild = iParent;
            if ( iChild == 0 ) {
                break;
            }
            iParent = ( iChild - 1 ) / 2;
        } while ( m_compare( m_values[iParent], value ) );
        m_values[iChild] = std::move( value );
    }

    void siftDown( uint32_t iParent ) noexcept
    {
        const uint32_t nCount = m_values.getSize();
        T value = std::move( m_values[iParent] );
        while ( iParent < nCount / 2 ) {
            uint32_t iChild = iParent * 2 + 1;
            if ( iChild + 1 < nCount && m_compare( m_values[iChild], m_values[iChild + 1] ) ) {
                ++iChild;
            }
            if ( !m_compare( value, m_values[iChild] ) ) {
                break;
            }
            m_values[iParent] = std::move( m_values[iChild] );
            iParent = iChild;
        }
        m_values[iParent] = std::move( value );
    }

public:
    explicit SIRPriorityQueue( const sirContainerAllocator_t *pAllocator, Compare compare = Compare() )
        : m_values( pAllocator ), m_compare( std::move( compare ) )
    {
    }

    uint32_t getSize( void ) const noexcept { return m_values.getSize(); }
    bool isEmpty( void ) const noexcept { return m_values.isEmpty(); }
    const T *top( void ) const noexcept { return m_values.tryGet( 0 ); }
    sirContainerFootprint_t getFootprint( void ) const noexcept { return m_values.getFootprint(); }
    sirContainerResult_t tryReserve( uint32_t nCapacity ) { return m_values.tryReserve( nCapacity ); }

    template <typename... Args>
    sirContainerResult_t tryEmplace( Args &&...args )
    {
        sirContainerResult_t eResult = m_values.tryEmplaceBack( std::forward<Args>( args )... );
        if ( eResult == SIR_CONTAINER_OK ) {
            siftUp( m_values.getSize() - 1 );
        }
        return eResult;
    }

    sirContainerResult_t tryPush( const T &value )
    {
        sirContainerResult_t eResult = m_values.tryPushBack( value );
        if ( eResult == SIR_CONTAINER_OK ) {
            siftUp( m_values.getSize() - 1 );
        }
        return eResult;
    }

    sirContainerResult_t tryPop( void ) noexcept
    {
        if ( m_values.isEmpty() ) {
            return SIR_CONTAINER_EMPTY;
        }
        sirContainerResult_t eResult = m_values.tryEraseSwap( 0 );
        if ( eResult == SIR_CONTAINER_OK && !m_values.isEmpty() ) {
            siftDown( 0 );
        }
        return eResult;
    }

    sirContainerResult_t tryShrinkToFit( void ) { return m_values.tryShrinkToFit(); }
    void clear( void ) noexcept { m_values.clear(); }
};

#endif // __SIRENGINE_SIRPRIORITYQUEUE_H__
