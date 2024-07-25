#ifndef __UTL_STRING_HPP__
#define __UTL_STRING_HPP__

#pragma once

#include <Engine/Core/SIREngine.hpp>
#include <Engine/Util/UtlMemory.hpp>

class UtlString
{
public:
    UtlString( void );
    UtlString( const char *pString );
    UtlString( const UtlString& other );
    ~UtlString();

    UtlString& operator=( const UtlString& other );
    UtlString& operator=( const char *other );
    UtlString& operator+=( const char *other );
    UtlString& operator+=( const UtlString& other );
    UtlString& operator+=( char c );
    UtlString& operator+=( int rhs );
    UtlString& operator+=( float rhs );

    UtlString operator+( const UtlString& other ) const;
    UtlString operator+( const char *other ) const;

    bool operator==( const UtlString& other ) const;
    bool operator!=( const UtlString& other ) const;
    bool operator==( const char *other ) const;
    bool operator!=( const char *other ) const;

    // If these are not defined, CUtlString as rhs will auto-convert
	// to const char* and do logical operations on the raw pointers. Ugh.
    SIRENGINE_FORCEINLINE friend bool operator==( const char *lhs, const UtlString &rhs ) { return rhs.operator==( lhs ); }
	SIRENGINE_FORCEINLINE friend bool operator!=( const char *lhs, const UtlString &rhs ) { return rhs.operator!=( lhs ); }

    void Format( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format( printf, 2, 3 ));

    UtlString Slice( int64_t nStart = 0, int64_t nEnd = SIRENGINE_INT64_MAX ) const;
    UtlString Left( uint64_t nChars ) const;
    UtlString Right( uint64_t nChars ) const;

    void ToLower( void );
    void ToUpper( void );

    operator const char *( void ) const;
    const char *c_str( void ) const;
    char *data( void );

    char *begin( void );
    char *end( void );
    const char *begin( void ) const;
    const char *end( void ) const;

    uint64_t size( void ) const;
    uint64_t length( void ) const;

    void assign( const UtlString& other, uint64_t nChars = 0 );
    void assign( const char *other, uint64_t nChars = 0 );

    bool empty( void ) const;

    void clear( void );
    void resize( uint64_t nBytes );
    void reserve( uint64_t nBytes );
private:
    union {
        char m_szBaseBuffer[64];
        char *m_pBuffer;
    };
    uint64_t m_nLength;
    uint64_t m_nAllocated;

    void EnsureAllocated( uint64_t nBytes );
};

SIRENGINE_FORCEINLINE UtlString::UtlString( void )
    : m_pBuffer( NULL ), m_nLength( 0 ), m_nAllocated( 0 )
{
}

SIRENGINE_FORCEINLINE UtlString::UtlString( const char *pString )
    : m_pBuffer( NULL ), m_nLength( 0 ), m_nAllocated( 0 )
{
    assign( pString );
}

SIRENGINE_FORCEINLINE UtlString::UtlString( const UtlString& other )
    : m_pBuffer( NULL ), m_nLength( 0 ), m_nAllocated( 0 )
{
    assign( other );
}

SIRENGINE_FORCEINLINE UtlString::~UtlString()
{
    clear();
}

UtlString& UtlString::operator=( const UtlString& other );
UtlString& UtlString::operator=( const char *other );
UtlString& UtlString::operator+=( const char *other );
UtlString& UtlString::operator+=( const UtlString& other );
UtlString& UtlString::operator+=( char c );
UtlString& UtlString::operator+=( int rhs );
UtlString& UtlString::operator+=( float rhs );

UtlString UtlString::operator+( const UtlString& other ) const
{

}

UtlString UtlString::operator+( const char *other ) const
{

}

SIRENGINE_FORCEINLINE bool UtlString::operator==( const UtlString& other ) const
{
    if ( m_nLength != other.m_nLength ) {
        return false;
    }
    return memcmp( m_pBuffer, other.m_pBuffer, m_nLength ) == 0;
}

SIRENGINE_FORCEINLINE bool UtlString::operator!=( const UtlString& other ) const
{
    if ( m_nLength != other.m_nLength ) {
        return true;
    }
    return memcmp( m_pBuffer, other.m_pBuffer, m_nLength ) != 0;
}

SIRENGINE_FORCEINLINE bool UtlString::operator==( const char *other ) const
{
    if ( m_nLength != strlen( other ) ) {
        return false;
    }
    return memcmp( m_pBuffer, other, m_nLength ) == 0;
}

SIRENGINE_FORCEINLINE bool UtlString::operator!=( const char *other ) const
{
    if ( m_nLength != strlen( other ) ) {
        return true;
    }
    return memcmp( m_pBuffer, other, m_nLength ) != 0;
}

void UtlString::Format( const char *fmt, ... )
{
    va_list argptr;
    char buf[8192];
    int length;

    va_start( argptr, fmt );
    length = SIREngine_Vsnprintf( buf, sizeof( buf ) - 1, fmt, argptr );
    va_end( argptr );

    resize( length );
    assign( buf );
}

SIRENGINE_FORCEINLINE UtlString UtlString::Slice( int64_t nStart, int64_t nEnd ) const
{
    if ( nStart < 0  ) {
		nStart = size() - ( -nStart % size() );
    } else if ( nStart >= size() ) {
		nStart = size();
    }

	if ( nEnd == SIRENGINE_INT32_MAX ) {
		nEnd = size();
    } else if ( nEnd < 0 ) {
		nEnd = size() - ( -nEnd % size() );
	} else if ( nEnd >= size() ) {
		nEnd = size();
    }
	
	if ( nStart >= nEnd ) {
		return UtlString( "" );
    }

	const char *pIn = UtlString();

	UtlString ret;
    ret.resize( nEnd - nStart + 1 );
    char *pOut = ret.data();

	memcpy( ret.data(), &pIn[nStart], nEnd - nStart );
	pOut[ nEnd - nStart ] = 0;

	return ret;
}

SIRENGINE_FORCEINLINE UtlString UtlString::Left( uint64_t nChars ) const
{
    Slice( 0, nChars );
}

SIRENGINE_FORCEINLINE UtlString UtlString::Right( uint64_t nChars ) const
{
    Slice( -nChars );
}

SIRENGINE_FORCEINLINE void UtlString::ToLower( void )
{
}

SIRENGINE_FORCEINLINE void UtlString::ToUpper( void )
{
}

SIRENGINE_FORCEINLINE UtlString::operator const char *( void ) const
{
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE const char *UtlString::c_str( void ) const {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE char *UtlString::data( void ) {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE char *UtlString::begin( void ) {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE char *UtlString::end( void ) {
    return m_pBuffer + m_nLength;
}

SIRENGINE_FORCEINLINE const char *UtlString::begin( void ) const {
    return begin();
}

SIRENGINE_FORCEINLINE const char *UtlString::end( void ) const {
    return end();
}

SIRENGINE_FORCEINLINE uint64_t UtlString::size( void ) const {
    return m_nLength;
}

SIRENGINE_FORCEINLINE uint64_t UtlString::length( void ) const {
    return m_nLength;
}

SIRENGINE_FORCEINLINE void UtlString::assign( const char *other, uint64_t nChars )
{
    if ( !other ) {
        nChars = 0;
    } else if ( nChars == 0 ) {
        nChars = strlen( other );
    }

    EnsureAllocated( nChars );
    memcpy( m_pBuffer, other, nChars );
}

SIRENGINE_FORCEINLINE bool UtlString::empty( void ) const {
    return m_nLength == 0;
}

SIRENGINE_FORCEINLINE void UtlString::clear( void ) {
    if ( !m_nAllocated ) {
        memset( m_szBaseBuffer, 0, sizeof( m_szBaseBuffer ) );
    } else {
        free( m_pBuffer );
    }

    m_nAllocated = 0;
    m_nLength = 0;
    m_pBuffer = NULL;
}

SIRENGINE_FORCEINLINE void UtlString::resize( uint64_t nBytes ) {
    m_nLength = nBytes;

    clear();

    if ( m_nAllocated != nBytes ) {
        if ( nBytes >= sizeof ( m_szBaseBuffer ) - 1 ) {
            void *newBuffer = malloc( nBytes );
        } else {
            m_szBaseBuffer[ nBytes ] = '\0';
        }
    }
}

SIRENGINE_FORCEINLINE void UtlString::reserve( uint64_t nBytes ) {
    EnsureAllocated( nBytes );
}

void UtlString::EnsureAllocated( uint64_t nBytes )
{
    uint64_t newSize = m_nLength + nBytes;

    if ( newSize >= m_nAllocated ) {
        m_nAllocated *= 2;
        if ( m_nAllocated >= sizeof( m_szBaseBuffer ) - 1 ) {
            void *newBuffer = malloc( m_nAllocated );
            if ( m_pBuffer ) {
                memcpy( newBuffer, m_pBuffer, m_nLength );
                free( m_pBuffer );
            }
            m_pBuffer = (char *)newBuffer;
        } else {
            m_pBuffer = m_szBaseBuffer;
        }
    }
}

#endif