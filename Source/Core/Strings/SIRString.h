/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_STRING_H__
#define __SIR_STRING_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorStorage.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

typedef struct sirStringView_s
{
	const char *pBytes;
	uint32_t nBytes;
} sirStringView_t;

// Byte-oriented UTF-8 storage; operations use byte offsets, not code points.
// All dynamic allocations use the supplied memory domain. The terminating
// NUL is always present, including when the value contains embedded NULs.
//
// Fallible try* operations return SIRContainerResult without modifying the
// string on allocation failure. Standard-style methods throw on failure.
class SIRString
{
public:
	using size_type = uint32_t;
	using iterator = char *;
	using const_iterator = const char *;

	static constexpr size_type npos = UINT32_MAX;

	explicit SIRString( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator ),
	      m_nLength( 0 ),
	      m_nCapacity( INLINE_CAPACITY )
	{
		m_szInline[0] = '\0';
	}

	SIRString( const sirContainerAllocator_t *pAllocator, const char *szText )
	    : SIRString( szText, pAllocator )
	{
	}

	SIRString( const char *szText, const sirContainerAllocator_t *pAllocator )
	    : SIRString( pAllocator )
	{
		assign( szText );
	}

	SIRString( const char *pBytes, size_type nBytes, const sirContainerAllocator_t *pAllocator )
	    : SIRString( pAllocator )
	{
		assign( pBytes, nBytes );
	}

	SIRString( std::string_view text, const sirContainerAllocator_t *pAllocator )
	    : SIRString( pAllocator )
	{
		assign( text );
	}

	SIRString( size_type nCount, char nValue, const sirContainerAllocator_t *pAllocator )
	    : SIRString( pAllocator )
	{
		assign( nCount, nValue );
	}

	SIRString( const SIRString& other )
	    : SIRString( other.m_pAllocator )
	{
		assign( other );
	}

	SIRString( const SIRString& other, const sirContainerAllocator_t *pAllocator )
	    : SIRString( pAllocator )
	{
		assign( other );
	}

	SIRString( SIRString&& other ) noexcept
	    : m_pAllocator( other.m_pAllocator ),
	      m_nLength( other.m_nLength ),
	      m_nCapacity( other.m_nCapacity )
	{
		if ( other.isInline() ) {
			memcpy( m_szInline, other.m_szInline, m_nLength + 1 );
		} else {
			m_pHeap = other.m_pHeap;
		}

		other.resetInline();
	}

	~SIRString( void ) noexcept
	{
		release();
	}

	SIRString& operator=( const SIRString& other )
	{
		if ( this != &other ) {
			assign( other );
		}
		return *this;
	}

	// Move assignment transfers the source's allocator domain along with its
	// allocation. Copy assignment retains the destination's allocator domain.
	SIRString& operator=( SIRString&& other ) noexcept
	{
		if ( this != &other ) {
			release();
			m_pAllocator = other.m_pAllocator;
			m_nLength = other.m_nLength;
			m_nCapacity = other.m_nCapacity;

			if ( other.isInline() ) {
				memcpy( m_szInline, other.m_szInline, m_nLength + 1 );
			} else {
				m_pHeap = other.m_pHeap;
			}

			other.resetInline();
		}
		return *this;
	}

	SIRString& operator=( const char *szText )
	{
		assign( szText );
		return *this;
	}

	SIRString& operator=( std::string_view text )
	{
		assign( text );
		return *this;
	}

	SIRString& operator=( char nValue )
	{
		assign( 1, nValue );
		return *this;
	}

	const sirContainerAllocator_t *getAllocator( void ) const noexcept
	{
		return m_pAllocator;
	}

	size_type size( void ) const noexcept
	{
		return m_nLength;
	}

	size_type length( void ) const noexcept
	{
		return m_nLength;
	}

	size_type getByteLength( void ) const noexcept
	{
		return m_nLength;
	}

	bool empty( void ) const noexcept
	{
		return m_nLength == 0;
	}

	// Usable character slots, excluding the terminating NUL.
	size_type capacity( void ) const noexcept
	{
		return m_nCapacity - 1;
	}

	// Backward-compatible: physical byte capacity, including the NUL slot.
	size_type getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}

	static constexpr size_type max_size( void ) noexcept
	{
		return UINT32_MAX - 1;
	}

	char *data( void ) noexcept
	{
		return isInline() ? m_szInline : m_pHeap;
	}

	const char *data( void ) const noexcept
	{
		return isInline() ? m_szInline : m_pHeap;
	}

	const char *c_str( void ) const noexcept
	{
		return data();
	}

	const char *cStr( void ) const noexcept
	{
		return data();
	}

	std::string_view view( void ) const noexcept
	{
		return std::string_view( data(), m_nLength );
	}

	operator std::string_view( void ) const noexcept
	{
		return view();
	}

	sirStringView_t getView( void ) const noexcept
	{
		return { data(), m_nLength };
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return { INLINE_CAPACITY, isInline() ? 0u : m_nCapacity, 0, 0, static_cast<uint64_t>( m_nLength ) + 1 };
	}

	iterator begin( void ) noexcept
	{
		return data();
	}

	const_iterator begin( void ) const noexcept
	{
		return data();
	}

	const_iterator cbegin( void ) const noexcept
	{
		return data();
	}

	iterator end( void ) noexcept
	{
		return data() + m_nLength;
	}

	const_iterator end( void ) const noexcept
	{
		return data() + m_nLength;
	}

	const_iterator cend( void ) const noexcept
	{
		return data() + m_nLength;
	}

	char& operator[]( size_type iIndex ) noexcept
	{
		assert( iIndex <= m_nLength );
		return data()[iIndex];
	}

	const char& operator[]( size_type iIndex ) const noexcept
	{
		assert( iIndex <= m_nLength );
		return data()[iIndex];
	}

	char& at( size_type iIndex )
	{
		if ( iIndex >= m_nLength ) {
			throw std::out_of_range( "SIRString::at" );
		}
		return data()[iIndex];
	}

	const char& at( size_type iIndex ) const
	{
		if ( iIndex >= m_nLength ) {
			throw std::out_of_range( "SIRString::at" );
		}
		return data()[iIndex];
	}

	char& front( void ) noexcept
	{
		assert( !empty() );
		return data()[0];
	}

	const char& front( void ) const noexcept
	{
		assert( !empty() );
		return data()[0];
	}

	char& back( void ) noexcept
	{
		assert( !empty() );
		return data()[m_nLength - 1];
	}

	const char& back( void ) const noexcept
	{
		assert( !empty() );
		return data()[m_nLength - 1];
	}

	SIRContainerResult tryReserve( size_type nCharacters ) noexcept
	{
		if ( nCharacters > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		return tryReserveBytes( nCharacters + 1 );
	}

	void reserve( size_type nCharacters )
	{
		checkResult( tryReserve( nCharacters ) );
	}

	SIRContainerResult tryShrinkToFit( void ) noexcept
	{
		if ( isInline() || m_nCapacity == m_nLength + 1 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}

		if ( m_nLength + 1 <= INLINE_CAPACITY ) {
			char *pOld = m_pHeap;
			size_type nOldCapacity = m_nCapacity;
			memcpy( m_szInline, pOld, m_nLength + 1 );
			m_nCapacity = INLINE_CAPACITY;
			SIRRawStorage::deallocate( m_pAllocator, pOld, nOldCapacity, alignof( char ) );
			return SIRContainerResult::SIR_CONTAINER_OK;
		}

		void *pMemory = nullptr;
		const size_type nRequired = m_nLength + 1;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nRequired, alignof( char ), &pMemory );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		memcpy( pMemory, m_pHeap, nRequired );
		SIRRawStorage::deallocate( m_pAllocator, m_pHeap, m_nCapacity, alignof( char ) );
		m_pHeap = static_cast<char *>( pMemory );
		m_nCapacity = nRequired;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void shrink_to_fit( void )
	{
		checkResult( tryShrinkToFit() );
	}

	void clear( void ) noexcept
	{
		m_nLength = 0;
		data()[0] = '\0';
	}

	SIRContainerResult tryResize( size_type nCharacters, char nValue = '\0' ) noexcept
	{
		if ( nCharacters > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( nCharacters > m_nLength ) {
			SIRContainerResult eResult = tryEnsureCapacity( nCharacters + 1 );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			memset( data() + m_nLength, nValue, nCharacters - m_nLength );
		}

		m_nLength = nCharacters;
		data()[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void resize( size_type nCharacters, char nValue = '\0' )
	{
		checkResult( tryResize( nCharacters, nValue ) );
	}

	SIRContainerResult tryAppendChar( char nValue ) noexcept
	{
		if ( m_nLength == max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		SIRContainerResult eResult = tryEnsureCapacity( m_nLength + 2 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		data()[m_nLength] = nValue;
		++m_nLength;
		data()[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryAppendBytes( const char *pBytes, size_type nBytes ) noexcept
	{
		if ( nBytes == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( pBytes == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( static_cast<uint64_t>( m_nLength ) + nBytes > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		size_type iSource = 0;
		bool bAliased = sourceOffset( pBytes, &iSource );
		if ( bAliased && static_cast<uint64_t>( iSource ) + nBytes > static_cast<uint64_t>( m_nLength ) + 1 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		SIRContainerResult eResult = tryEnsureCapacity( m_nLength + nBytes + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		char *pDestination = data() + m_nLength;
		if ( bAliased ) {
			memmove( pDestination, data() + iSource, nBytes );
		} else {
			memcpy( pDestination, pBytes, nBytes );
		}

		m_nLength += nBytes;
		data()[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryAssignBytes( const char *pBytes, size_type nBytes ) noexcept
	{
		if ( pBytes == nullptr && nBytes != 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nBytes > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		size_type iSource = 0;
		bool bAliased = sourceOffset( pBytes, &iSource );
		if ( bAliased && static_cast<uint64_t>( iSource ) + nBytes > static_cast<uint64_t>( m_nLength ) + 1 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		SIRContainerResult eResult = tryEnsureCapacity( nBytes + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		if ( nBytes != 0 ) {
			if ( bAliased ) {
				memmove( data(), data() + iSource, nBytes );
			} else {
				memcpy( data(), pBytes, nBytes );
			}
		}

		m_nLength = nBytes;
		data()[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryAppend( const SIRString& other ) noexcept
	{
		return tryAppendBytes( other.data(), other.size() );
	}

	SIRContainerResult tryAppend( std::string_view text ) noexcept
	{
		if ( text.size() > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		return tryAppendBytes( text.data(), static_cast<size_type>( text.size() ) );
	}

	SIRContainerResult tryAppend( sirStringView_t text ) noexcept
	{
		return tryAppendBytes( text.pBytes, text.nBytes );
	}

	SIRContainerResult tryAppend( const char *szText ) noexcept
	{
		size_type nBytes = 0;
		SIRContainerResult eResult = tryTextLength( szText, &nBytes );
		return eResult == SIRContainerResult::SIR_CONTAINER_OK ? tryAppendBytes( szText, nBytes ) : eResult;
	}

	SIRContainerResult tryAppend( size_type nCount, char nValue ) noexcept
	{
		if ( static_cast<uint64_t>( m_nLength ) + nCount > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		SIRContainerResult eResult = tryEnsureCapacity( m_nLength + nCount + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		memset( data() + m_nLength, nValue, nCount );
		m_nLength += nCount;
		data()[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryAssign( const SIRString& other ) noexcept
	{
		return tryAssignBytes( other.data(), other.size() );
	}

	SIRContainerResult tryAssign( std::string_view text ) noexcept
	{
		if ( text.size() > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		return tryAssignBytes( text.data(), static_cast<size_type>( text.size() ) );
	}

	SIRContainerResult tryAssign( sirStringView_t text ) noexcept
	{
		return tryAssignBytes( text.pBytes, text.nBytes );
	}

	SIRContainerResult tryAssign( const char *szText ) noexcept
	{
		size_type nBytes = 0;
		SIRContainerResult eResult = tryTextLength( szText, &nBytes );
		return eResult == SIRContainerResult::SIR_CONTAINER_OK ? tryAssignBytes( szText, nBytes ) : eResult;
	}

	SIRContainerResult tryAssign( size_type nCount, char nValue ) noexcept
	{
		if ( nCount > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		SIRContainerResult eResult = tryEnsureCapacity( nCount + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		memset( data(), nValue, nCount );
		m_nLength = nCount;
		data()[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRString& append( const SIRString& other )
	{
		checkResult( tryAppend( other ) );
		return *this;
	}

	SIRString& append( std::string_view text )
	{
		checkResult( tryAppend( text ) );
		return *this;
	}

	SIRString& append( const char *szText )
	{
		checkResult( tryAppend( szText ) );
		return *this;
	}

	SIRString& append( const char *pBytes, size_type nBytes )
	{
		checkResult( tryAppendBytes( pBytes, nBytes ) );
		return *this;
	}

	SIRString& append( size_type nCount, char nValue )
	{
		checkResult( tryAppend( nCount, nValue ) );
		return *this;
	}

	SIRString& assign( const SIRString& other )
	{
		checkResult( tryAssign( other ) );
		return *this;
	}

	SIRString& assign( std::string_view text )
	{
		checkResult( tryAssign( text ) );
		return *this;
	}

	SIRString& assign( const char *szText )
	{
		checkResult( tryAssign( szText ) );
		return *this;
	}

	SIRString& assign( const char *pBytes, size_type nBytes )
	{
		checkResult( tryAssignBytes( pBytes, nBytes ) );
		return *this;
	}

	SIRString& assign( size_type nCount, char nValue )
	{
		checkResult( tryAssign( nCount, nValue ) );
		return *this;
	}

	void push_back( char nValue )
	{
		checkResult( tryAppendChar( nValue ) );
	}

	void pop_back( void ) noexcept
	{
		assert( !empty() );
		--m_nLength;
		data()[m_nLength] = '\0';
	}

	SIRString& operator+=( const SIRString& other )
	{
		return append( other );
	}

	SIRString& operator+=( std::string_view text )
	{
		return append( text );
	}

	SIRString& operator+=( const char *szText )
	{
		return append( szText );
	}

	SIRString& operator+=( char nValue )
	{
		push_back( nValue );
		return *this;
	}

	SIRContainerResult tryInsertBytes( size_type iIndex, const char *pBytes, size_type nBytes ) noexcept
	{
		if ( iIndex > m_nLength ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nBytes == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( pBytes == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( static_cast<uint64_t>( m_nLength ) + nBytes > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		size_type iSource = 0;
		const bool bAliased = sourceOffset( pBytes, &iSource );
		if ( bAliased && static_cast<uint64_t>( iSource ) + nBytes > static_cast<uint64_t>( m_nLength ) + 1 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		SIRContainerResult eResult = tryEnsureCapacity( m_nLength + nBytes + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		char *pBuffer = data();
		memmove( pBuffer + iIndex + nBytes, pBuffer + iIndex, m_nLength - iIndex + 1 );
		if ( bAliased ) {
			copyInsertedAlias( iIndex, iSource, nBytes );
		} else {
			memcpy( pBuffer + iIndex, pBytes, nBytes );
		}

		m_nLength += nBytes;
		pBuffer[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryInsert( size_type iIndex, const SIRString& other ) noexcept
	{
		return tryInsertBytes( iIndex, other.data(), other.size() );
	}

	SIRContainerResult tryInsert( size_type iIndex, std::string_view text ) noexcept
	{
		if ( text.size() > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		return tryInsertBytes( iIndex, text.data(), static_cast<size_type>( text.size() ) );
	}

	SIRContainerResult tryInsert( size_type iIndex, const char *szText ) noexcept
	{
		size_type nBytes = 0;
		SIRContainerResult eResult = tryTextLength( szText, &nBytes );
		return eResult == SIRContainerResult::SIR_CONTAINER_OK ? tryInsertBytes( iIndex, szText, nBytes ) : eResult;
	}

	SIRContainerResult tryInsert( size_type iIndex, size_type nCount, char nValue ) noexcept
	{
		if ( iIndex > m_nLength ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( static_cast<uint64_t>( m_nLength ) + nCount > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		SIRContainerResult eResult = tryEnsureCapacity( m_nLength + nCount + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		memmove( data() + iIndex + nCount, data() + iIndex, m_nLength - iIndex + 1 );
		memset( data() + iIndex, nValue, nCount );
		m_nLength += nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRString& insert( size_type iIndex, const SIRString& other )
	{
		checkInsertResult( iIndex, tryInsert( iIndex, other ) );
		return *this;
	}

	SIRString& insert( size_type iIndex, std::string_view text )
	{
		checkInsertResult( iIndex, tryInsert( iIndex, text ) );
		return *this;
	}

	SIRString& insert( size_type iIndex, const char *szText )
	{
		checkInsertResult( iIndex, tryInsert( iIndex, szText ) );
		return *this;
	}

	SIRString& insert( size_type iIndex, const char *pBytes, size_type nBytes )
	{
		checkInsertResult( iIndex, tryInsertBytes( iIndex, pBytes, nBytes ) );
		return *this;
	}

	SIRString& insert( size_type iIndex, size_type nCount, char nValue )
	{
		checkInsertResult( iIndex, tryInsert( iIndex, nCount, nValue ) );
		return *this;
	}

	SIRContainerResult tryErase( size_type iIndex = 0, size_type nCount = npos ) noexcept
	{
		if ( iIndex > m_nLength ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		const size_type nRemoved = minCount( nCount, m_nLength - iIndex );
		memmove( data() + iIndex, data() + iIndex + nRemoved, m_nLength - iIndex - nRemoved + 1 );
		m_nLength -= nRemoved;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRString& erase( size_type iIndex = 0, size_type nCount = npos )
	{
		if ( iIndex > m_nLength ) {
			throw std::out_of_range( "SIRString::erase" );
		}
		tryErase( iIndex, nCount );
		return *this;
	}

	SIRContainerResult tryReplaceBytes( size_type iIndex, size_type nCount, const char *pBytes, size_type nBytes ) noexcept
	{
		if ( iIndex > m_nLength || ( pBytes == nullptr && nBytes != 0 ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		const size_type nRemoved = minCount( nCount, m_nLength - iIndex );
		const uint64_t nNewLength = static_cast<uint64_t>( m_nLength - nRemoved ) + nBytes;
		if ( nNewLength > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}

		size_type iSource = 0;
		const bool bAliased = sourceOffset( pBytes, &iSource );
		if ( bAliased && static_cast<uint64_t>( iSource ) + nBytes > static_cast<uint64_t>( m_nLength ) + 1 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		// Only self-replacement needs staging. The ordinary path allocates
		// nothing beyond any required capacity growth.
		SIRString szStaged( m_pAllocator );
		if ( bAliased ) {
			SIRContainerResult eResult = szStaged.tryAssignBytes( pBytes, nBytes );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			pBytes = szStaged.data();
		}

		SIRContainerResult eResult = tryEnsureCapacity( static_cast<size_type>( nNewLength ) + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		char *pBuffer = data();
		memmove( pBuffer + iIndex + nBytes, pBuffer + iIndex + nRemoved, m_nLength - iIndex - nRemoved + 1 );
		if ( nBytes != 0 ) {
			memcpy( pBuffer + iIndex, pBytes, nBytes );
		}

		m_nLength = static_cast<size_type>( nNewLength );
		pBuffer[m_nLength] = '\0';
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryReplace( size_type iIndex, size_type nCount, std::string_view text ) noexcept
	{
		if ( text.size() > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		return tryReplaceBytes( iIndex, nCount, text.data(), static_cast<size_type>( text.size() ) );
	}

	SIRContainerResult tryReplace( size_type iIndex, size_type nCount, const SIRString& other ) noexcept
	{
		return tryReplaceBytes( iIndex, nCount, other.data(), other.size() );
	}

	SIRContainerResult tryReplace( size_type iIndex, size_type nCount, const char *szText ) noexcept
	{
		size_type nBytes = 0;
		SIRContainerResult eResult = tryTextLength( szText, &nBytes );
		return eResult == SIRContainerResult::SIR_CONTAINER_OK ? tryReplaceBytes( iIndex, nCount, szText, nBytes ) : eResult;
	}

	SIRString& replace( size_type iIndex, size_type nCount, std::string_view text )
	{
		checkInsertResult( iIndex, tryReplace( iIndex, nCount, text ) );
		return *this;
	}

	SIRString& replace( size_type iIndex, size_type nCount, const SIRString& other )
	{
		checkInsertResult( iIndex, tryReplace( iIndex, nCount, other ) );
		return *this;
	}

	SIRString& replace( size_type iIndex, size_type nCount, const char *szText )
	{
		checkInsertResult( iIndex, tryReplace( iIndex, nCount, szText ) );
		return *this;
	}

	SIRString& replace( size_type iIndex, size_type nCount, const char *pBytes, size_type nBytes )
	{
		checkInsertResult( iIndex, tryReplaceBytes( iIndex, nCount, pBytes, nBytes ) );
		return *this;
	}

	SIRString substr( size_type iIndex = 0, size_type nCount = npos ) const
	{
		if ( iIndex > m_nLength ) {
			throw std::out_of_range( "SIRString::substr" );
		}

		SIRString result( m_pAllocator );
		result.assign( data() + iIndex, minCount( nCount, m_nLength - iIndex ) );
		return result;
	}

	void swap( SIRString& other ) noexcept
	{
		if ( this != &other ) {
			SIRString temporary( std::move( other ) );
			other = std::move( *this );
			*this = std::move( temporary );
		}
	}

	int compare( std::string_view text ) const noexcept
	{
		const size_t nShared = m_nLength < text.size() ? m_nLength : text.size();
		const int nDifference = nShared != 0 ? memcmp( data(), text.data(), nShared ) : 0;
		if ( nDifference != 0 ) {
			return nDifference < 0 ? -1 : 1;
		}
		return m_nLength == text.size() ? 0 : ( m_nLength < text.size() ? -1 : 1 );
	}

	int compare( const SIRString& other ) const noexcept
	{
		return compare( other.view() );
	}

	int compare( const char *szText ) const
	{
		return compare( std::string_view( szText ) );
	}

	size_type find( std::string_view needle, size_type iStart = 0 ) const noexcept
	{
		if ( iStart > m_nLength || needle.size() > m_nLength - iStart ) {
			return npos;
		}
		if ( needle.empty() ) {
			return iStart;
		}

		if ( needle.size() == 1 ) {
			return find( needle[0], iStart );
		}

		const char *pSearch = data() + iStart;
		const char *pEnd = data() + m_nLength - needle.size() + 1;
		while ( pSearch < pEnd ) {
			const char *pFound = static_cast<const char *>( memchr( pSearch, needle[0], pEnd - pSearch ) );
			if ( pFound == nullptr ) {
				break;
			}
			if ( memcmp( pFound + 1, needle.data() + 1, needle.size() - 1 ) == 0 ) {
				return static_cast<size_type>( pFound - data() );
			}
			pSearch = pFound + 1;
		}
		return npos;
	}

	size_type find( const SIRString& needle, size_type iStart = 0 ) const noexcept
	{
		return find( needle.view(), iStart );
	}

	size_type find( char nValue, size_type iStart = 0 ) const noexcept
	{
		if ( iStart >= m_nLength ) {
			return npos;
		}
		const void *pFound = memchr( data() + iStart, nValue, m_nLength - iStart );
		return pFound == nullptr ? npos : static_cast<size_type>( static_cast<const char *>( pFound ) - data() );
	}

	size_type rfind( std::string_view needle, size_type iStart = npos ) const noexcept
	{
		if ( needle.size() > m_nLength ) {
			return npos;
		}

		size_type iIndex = minCount( iStart, m_nLength - static_cast<size_type>( needle.size() ) );
		if ( needle.empty() ) {
			return iIndex;
		}

		for ( ;; ) {
			if ( data()[iIndex] == needle[0] && memcmp( data() + iIndex, needle.data(), needle.size() ) == 0 ) {
				return iIndex;
			}
			if ( iIndex == 0 ) {
				break;
			}
			--iIndex;
		}
		return npos;
	}

	size_type rfind( char nValue, size_type iStart = npos ) const noexcept
	{
		if ( empty() ) {
			return npos;
		}

		size_type iIndex = minCount( iStart, m_nLength - 1 );
		for ( ;; ) {
			if ( data()[iIndex] == nValue ) {
				return iIndex;
			}
			if ( iIndex == 0 ) {
				break;
			}
			--iIndex;
		}
		return npos;
	}

	bool contains( std::string_view needle ) const noexcept
	{
		return find( needle ) != npos;
	}

	bool starts_with( std::string_view prefix ) const noexcept
	{
		return prefix.size() <= m_nLength && ( prefix.empty() || memcmp( data(), prefix.data(), prefix.size() ) == 0 );
	}

	bool ends_with( std::string_view suffix ) const noexcept
	{
		return suffix.size() <= m_nLength && ( suffix.empty() || memcmp( data() + m_nLength - suffix.size(), suffix.data(), suffix.size() ) == 0 );
	}

	friend bool operator==( const SIRString& left, const SIRString& right ) noexcept
	{
		return left.compare( right ) == 0;
	}

	friend bool operator!=( const SIRString& left, const SIRString& right ) noexcept
	{
		return left.compare( right ) != 0;
	}

	friend bool operator<( const SIRString& left, const SIRString& right ) noexcept
	{
		return left.compare( right ) < 0;
	}

	friend bool operator<=( const SIRString& left, const SIRString& right ) noexcept
	{
		return left.compare( right ) <= 0;
	}

	friend bool operator>( const SIRString& left, const SIRString& right ) noexcept
	{
		return left.compare( right ) > 0;
	}

	friend bool operator>=( const SIRString& left, const SIRString& right ) noexcept
	{
		return left.compare( right ) >= 0;
	}

	friend bool operator==( const SIRString& left, std::string_view right ) noexcept
	{
		return left.compare( right ) == 0;
	}

	friend bool operator==( std::string_view left, const SIRString& right ) noexcept
	{
		return right.compare( left ) == 0;
	}

	friend bool operator<( const SIRString& left, std::string_view right ) noexcept
	{
		return left.compare( right ) < 0;
	}

	friend bool operator<( std::string_view left, const SIRString& right ) noexcept
	{
		return right.compare( left ) > 0;
	}

	friend bool operator<=( const SIRString& left, std::string_view right ) noexcept
	{
		return left.compare( right ) <= 0;
	}

	friend bool operator<=( std::string_view left, const SIRString& right ) noexcept
	{
		return right.compare( left ) >= 0;
	}

	friend bool operator>( const SIRString& left, std::string_view right ) noexcept
	{
		return left.compare( right ) > 0;
	}

	friend bool operator>( std::string_view left, const SIRString& right ) noexcept
	{
		return right.compare( left ) < 0;
	}

	friend bool operator>=( const SIRString& left, std::string_view right ) noexcept
	{
		return left.compare( right ) >= 0;
	}

	friend bool operator>=( std::string_view left, const SIRString& right ) noexcept
	{
		return right.compare( left ) <= 0;
	}

	friend bool operator!=( const SIRString& left, std::string_view right ) noexcept
	{
		return left.compare( right ) != 0;
	}

	friend bool operator!=( std::string_view left, const SIRString& right ) noexcept
	{
		return right.compare( left ) != 0;
	}

	friend SIRString operator+( SIRString left, const SIRString& right )
	{
		left.append( right );
		return left;
	}

	friend SIRString operator+( SIRString left, std::string_view right )
	{
		left.append( right );
		return left;
	}

	friend SIRString operator+( std::string_view left, const SIRString& right )
	{
		SIRString result( right.m_pAllocator );
		if ( static_cast<uint64_t>( left.size() ) + right.size() > max_size() ) {
			throw std::length_error( "SIRString: length overflow" );
		}
		result.reserve( static_cast<size_type>( left.size() ) + right.size() );
		result.append( left );
		result.append( right );
		return result;
	}

	friend SIRString operator+( SIRString left, char nValue )
	{
		left.push_back( nValue );
		return left;
	}

private:
	static constexpr size_type INLINE_CAPACITY = 24;

	const sirContainerAllocator_t *m_pAllocator;
	size_type m_nLength;
	size_type m_nCapacity;

	union
	{
		char m_szInline[INLINE_CAPACITY];
		char *m_pHeap;
	};

	static size_type minCount( size_type nLeft, size_type nRight ) noexcept
	{
		return nLeft < nRight ? nLeft : nRight;
	}

	bool isInline( void ) const noexcept
	{
		return m_nCapacity == INLINE_CAPACITY;
	}

	void resetInline( void ) noexcept
	{
		m_nCapacity = INLINE_CAPACITY;
		m_nLength = 0;
		m_szInline[0] = '\0';
	}

	bool sourceOffset( const char *pBytes, size_type *piOffset ) const noexcept
	{
		if ( pBytes == nullptr ) {
			return false;
		}

		const uintptr_t nStart = reinterpret_cast<uintptr_t>( data() );
		const uintptr_t nSource = reinterpret_cast<uintptr_t>( pBytes );
		if ( nSource < nStart || nSource - nStart > m_nLength ) {
			return false;
		}

		*piOffset = static_cast<size_type>( nSource - nStart );
		return true;
	}

	void copyInsertedAlias( size_type iIndex, size_type iSource, size_type nBytes ) noexcept
	{
		char *pBuffer = data();
		if ( static_cast<uint64_t>( iSource ) + nBytes <= iIndex ) {
			memmove( pBuffer + iIndex, pBuffer + iSource, nBytes );
		} else if ( iSource >= iIndex ) {
			memmove( pBuffer + iIndex, pBuffer + iSource + nBytes, nBytes );
		} else {
			const size_type nBefore = iIndex - iSource;
			memmove( pBuffer + iIndex, pBuffer + iSource, nBefore );
			memmove( pBuffer + iIndex + nBefore, pBuffer + iIndex + nBytes, nBytes - nBefore );
		}
	}

	static SIRContainerResult tryTextLength( const char *szText, size_type *pnBytes ) noexcept
	{
		if ( szText == nullptr || pnBytes == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const size_t nLength = strlen( szText );
		if ( nLength > max_size() ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		*pnBytes = static_cast<size_type>( nLength );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryEnsureCapacity( size_type nRequiredBytes ) noexcept
	{
		if ( nRequiredBytes <= m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}

		size_type nGrowth = 0;
		SIRContainerResult eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, nRequiredBytes, &nGrowth );
		if ( eResult == SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW ) {
			// The geometric target overflowed; the requested size still fits.
			nGrowth = nRequiredBytes;
		} else if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		return tryReserveBytes( nGrowth );
	}

	SIRContainerResult tryReserveBytes( size_type nCapacity ) noexcept
	{
		if ( nCapacity <= m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}

		void *pStorage = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nCapacity, alignof( char ), &pStorage );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}

		memcpy( pStorage, data(), m_nLength + 1 );
		if ( !isInline() ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pHeap, m_nCapacity, alignof( char ) );
		}

		m_pHeap = static_cast<char *>( pStorage );
		m_nCapacity = nCapacity;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void release( void ) noexcept
	{
		if ( !isInline() ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pHeap, m_nCapacity, alignof( char ) );
		}
	}

	static void checkResult( SIRContainerResult eResult )
	{
		switch ( eResult ) {
		case SIRContainerResult::SIR_CONTAINER_OK:
			return;
		case SIRContainerResult::SIR_CONTAINER_OUT_OF_MEMORY:
			throw std::bad_alloc();
		case SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW:
			throw std::length_error( "SIRString: length overflow" );
		case SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT:
			throw std::invalid_argument( "SIRString: invalid argument" );
		default:
			throw std::runtime_error( "SIRString: container operation failed" );
		}
	}

	void checkInsertResult( size_type iIndex, SIRContainerResult eResult ) const
	{
		if ( iIndex > m_nLength ) {
			throw std::out_of_range( "SIRString: position out of range" );
		}
		checkResult( eResult );
	}
};

#endif // __SIR_STRING_H__
