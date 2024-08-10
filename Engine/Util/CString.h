#ifndef __UTL_STRING_HPP__
#define __UTL_STRING_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/ResourceDef.h>
#include <EASTL/allocator_malloc.h>
#include <EASTL/fixed_string.h>

class CString : public eastl::fixed_string<char, MAX_RESOURCE_PATH, true, eastl::allocator_malloc<char>>
{
public:
    typedef eastl::fixed_string<char, MAX_RESOURCE_PATH, true, eastl::allocator_malloc<char>> base_type;

    inline CString( void )
        : base_type()
    { }
    inline CString( const base_type& other )
        : base_type( other )
    { }
    inline CString( base_type&& other )
        : base_type( eastl::move( other ) )
    { }
    inline CString( const char *pString )
        : base_type( pString )
    { }
    inline CString( const CString& other )
        : base_type( other )
    { }
    inline CString( CString&& other )
        : base_type( eastl::move( other ) )
    { }
    inline CString( const char *pString, size_t nChars )
        : base_type( pString, nChars )
    { }
    inline ~CString()
    { }

    // FIXME: broken, just returns a jargled mess
//    inline operator const char *( void ) const
//    { return internalLayout().BeginPtr(); }

    inline const CString& operator=( const CString& other )
    {
        base_type::operator=( other );
        return *this;
    }
    inline const CString& operator=( CString&& other )
    {
        base_type::operator=( eastl::move( other ) );
        return *this;
    }

    inline void ToLower( void )
    {
        for ( auto it = internalLayout().BeginPtr(); it != internalLayout().EndPtr(); it++ ) {
            *it = tolower( *it );
        }
    }

    inline void ToUpper( void )
    {
        for ( auto it = internalLayout().BeginPtr(); it != internalLayout().EndPtr(); it++ ) {
            *it = toupper( *it );
        }
    }

    inline void StripLeading( const char *pString )
    {
        const uint64_t nLength = strlen( pString );

        if ( nLength > 0 ) {
            while ( !compare( 0, nLength, pString ) ) {
                memmove( internalLayout().BeginPtr(), internalLayout().BeginPtr() + nLength,
                    internalLayout().GetSize() - nLength + 1 );
                internalLayout().SetSize( internalLayout().GetSize() - nLength );
            }
        }
    }
};

namespace eastl {
    template<> struct hash<CString> {
		size_t operator()( const CString& str ) const {
			const unsigned char *p = (const unsigned char *)str.c_str(); // To consider: limit p to at most 256 chars.
			unsigned int c, result = 2166136261U; // We implement an FNV-like string hash.
			while((c = *p++) != 0) // Using '!=' disables compiler warnings.
				result = (result * 16777619) ^ c;
			return (size_t)result;
		}
	};
};

/* FIXME: needs better implementation

template<typename CharType>
class CString
{
public:
    typedef CharType value_type;
    typedef CharType* pointer;
    typedef const CharType* const_pointer;
    typedef CharType& reference;
    typedef const CharType& const_reference;
    typedef CharType* iterator;
    typedef const CharType* const_iterator;
    typedef CReverseIterator<CharType> reverse_iterator;
    typedef const CReverseIterator<const CharType> const_reverse_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    static const size_type npos = (size_type)-1;
public:
    CString( void );
    CString( const char *pString );
    CString( const CString& other );
    CString( CString&& other );
    ~CString();

    CString& operator=( const CString& other );
    CString& operator=( CString&& other );
    CString& operator=( const char *other );
    CString& operator+=( const char *other );
    CString& operator+=( const CString& other );
    CString& operator+=( char c );
    CString& operator+=( int rhs );
    CString& operator+=( float rhs );

    CString operator+( const CString& other ) const;
    CString operator+( const char *other ) const;

    bool operator==( const CString& other ) const;
    bool operator!=( const CString& other ) const;
    bool operator==( const char *other ) const;
    bool operator!=( const char *other ) const;

    // If these are not defined, CCString as rhs will auto-convert
	// to const char* and do logical operations on the raw pointers. Ugh.
    SIRENGINE_FORCEINLINE friend bool operator==( const char *lhs, const CString &rhs ) { return rhs.operator==( lhs ); }
	SIRENGINE_FORCEINLINE friend bool operator!=( const char *lhs, const CString &rhs ) { return rhs.operator!=( lhs ); }

    CString& append_sprintf( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format( printf, 2, 3 ));
    CString& append_sprintf_va_list( const char *fmt, va_list argptr );

    CString Slice( int64_t nStart = 0, int64_t nEnd = SIRENGINE_INT64_MAX ) const;
    CString Left( uint64_t nChars ) const;
    CString Right( uint64_t nChars ) const;

    void ToLower( void );
    void ToUpper( void );

    operator const char *( void ) const;
    const char *c_str( void ) const;
    char *data( void );
    char *GetBuffer( void );
    const char *GetBuffer( void ) const;

    char *begin( void );
    char *end( void );
    const char *begin( void ) const;
    const char *end( void ) const;

    void SetPointer( char *pData, uint64_t nLength, uint64_t nAllocated );

    uint64_t Size( void ) const;
    uint64_t Length( void ) const;

    void Assign( const CString& other, uint64_t nChars = 0 );
    void Assign( const char *other, uint64_t nChars = 0 );
    void Append( const CString& other );
    void Append( const char *other );

    bool Empty( void ) const;

    void Copy( const CString& other );
    void Move( CString&& other );

    void Clear( void );
    void Resize( uint64_t nBytes );
    void Reserve( uint64_t nBytes );
protected:
    union {
        char m_szBaseBuffer[64];
        char *m_pBuffer;
    };
    uint64_t m_nLength;
    uint64_t m_nAllocated;

    void EnsureAllocated( uint64_t nBytes );
};

SIRENGINE_FORCEINLINE CString::CString( void )
    : m_pBuffer( m_szBaseBuffer ), m_nLength( 0 ), m_nAllocated( 0 )
{
    memset( m_szBaseBuffer, 0, sizeof( m_szBaseBuffer ) );
}

SIRENGINE_FORCEINLINE CString::CString( const char *pString )
    : m_pBuffer( m_szBaseBuffer ), m_nLength( 0 ), m_nAllocated( 0 )
{
    memset( m_szBaseBuffer, 0, sizeof( m_szBaseBuffer ) );
    Assign( pString );
}

SIRENGINE_FORCEINLINE CString::CString( const CString& other )
    : m_pBuffer( m_szBaseBuffer ), m_nLength( 0 ), m_nAllocated( 0 )
{
    memset( m_szBaseBuffer, 0, sizeof( m_szBaseBuffer ) );
    Copy( other );
}

SIRENGINE_FORCEINLINE CString::CString( CString&& other )
    : m_pBuffer( m_szBaseBuffer ), m_nLength( 0 ), m_nAllocated( 0 )
{
    Move( eastl::move( other ) );
}

SIRENGINE_FORCEINLINE CString::~CString()
{
    Clear();
}

SIRENGINE_FORCEINLINE CString& CString::operator=( const CString& other )
{
    Copy( other );
    return *this;
}

SIRENGINE_FORCEINLINE CString& CString::operator=( CString&& other )
{
    Move( eastl::move( other ) );
    return *this;
}

SIRENGINE_FORCEINLINE CString& CString::operator=( const char *other )
{
    Copy( other );
    return *this;
}

SIRENGINE_FORCEINLINE CString& CString::operator+=( const char *other )
{
    Append( other );
    return *this;
}

SIRENGINE_FORCEINLINE CString& CString::operator+=( const CString& other )
{
    Append( other );
    return *this;
}

SIRENGINE_FORCEINLINE CString& CString::operator+=( char c )
{
    char str[64];
    SIREngine_snprintf( str, sizeof( str ) - 1, "%c", c );
    Append( str );
    return *this;
}

SIRENGINE_FORCEINLINE CString& CString::operator+=( int rhs )
{
    char str[64];
    SIREngine_snprintf( str, sizeof( str ) - 1, "%u", rhs );
    Append( str );
    return *this;
}

SIRENGINE_FORCEINLINE CString& CString::operator+=( float rhs )
{
    char str[64];
    SIREngine_snprintf( str, sizeof( str ) - 1, "%f", rhs );
    Append( str );
    return *this;
}

SIRENGINE_FORCEINLINE CString CString::operator+( const CString& other ) const
{
    CString str;
    size_t nLength;

    nLength = m_nLength + other.Size() + 1;
    str.Resize( nLength );
    SIREngine_snprintf( str.GetBuffer(), str.Size(), "%s%s", m_pBuffer, other );

    return str;
}

SIRENGINE_FORCEINLINE CString CString::operator+( const char *other ) const
{
    CString str;
    size_t nLength;

    nLength = m_nLength + strlen( other ) + 1;
    str.Resize( nLength );
    SIREngine_snprintf( str.GetBuffer(), str.Size(), "%s%s", m_pBuffer, other );

    return str;
}

SIRENGINE_FORCEINLINE bool CString::operator==( const CString& other ) const
{
    if ( m_nLength != other.m_nLength ) {
        return false;
    }
    return memcmp( m_pBuffer, other.m_pBuffer, m_nLength ) == 0;
}

SIRENGINE_FORCEINLINE bool CString::operator!=( const CString& other ) const
{
    if ( m_nLength != other.m_nLength ) {
        return true;
    }
    return memcmp( m_pBuffer, other.m_pBuffer, m_nLength ) != 0;
}

SIRENGINE_FORCEINLINE bool CString::operator==( const char *other ) const
{
    if ( m_nLength != strlen( other ) ) {
        return false;
    }
    return memcmp( m_pBuffer, other, m_nLength ) == 0;
}

SIRENGINE_FORCEINLINE bool CString::operator!=( const char *other ) const
{
    if ( m_nLength != strlen( other ) ) {
        return true;
    }
    return memcmp( m_pBuffer, other, m_nLength ) != 0;
}

SIRENGINE_FORCEINLINE CString CString::Slice( int64_t nStart, int64_t nEnd ) const
{
    if ( nStart < 0  ) {
		nStart = Size() - ( -nStart % Size() );
    } else if ( nStart >= Size() ) {
		nStart = Size();
    }

	if ( nEnd == SIRENGINE_INT32_MAX ) {
		nEnd = Size();
    } else if ( nEnd < 0 ) {
		nEnd = Size() - ( -nEnd % Size() );
	} else if ( nEnd >= Size() ) {
		nEnd = Size();
    }
	
	if ( nStart >= nEnd ) {
		return CString( "" );
    }

	const char *pIn = CString();

	CString ret;
    ret.Resize( nEnd - nStart + 1 );
    char *pOut = ret.data();

	memcpy( ret.data(), &pIn[nStart], nEnd - nStart );
	pOut[ nEnd - nStart ] = 0;

	return ret;
}

SIRENGINE_FORCEINLINE CString CString::Left( uint64_t nChars ) const
{
    Slice( 0, nChars );
}

SIRENGINE_FORCEINLINE CString CString::Right( uint64_t nChars ) const
{
    Slice( -nChars );
}

SIRENGINE_FORCEINLINE void CString::ToLower( void )
{
}

SIRENGINE_FORCEINLINE void CString::ToUpper( void )
{
}

SIRENGINE_FORCEINLINE CString::operator const char *( void ) const
{
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE const char *CString::c_str( void ) const {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE char *CString::data( void ) {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE char *CString::begin( void ) {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE char *CString::end( void ) {
    return m_pBuffer + m_nLength;
}

SIRENGINE_FORCEINLINE const char *CString::begin( void ) const {
    return begin();
}

SIRENGINE_FORCEINLINE const char *CString::end( void ) const {
    return end();
}

SIRENGINE_FORCEINLINE char *CString::GetBuffer( void ) {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE const char *CString::GetBuffer( void ) const {
    return m_pBuffer;
}

SIRENGINE_FORCEINLINE void CString::SetPointer( char *pData, uint64_t nLength, uint64_t nAllocated )
{
    m_pBuffer = pData;
    m_nLength = nLength;
    m_nAllocated = nAllocated;
}

SIRENGINE_FORCEINLINE uint64_t CString::Size( void ) const {
    return m_nLength;
}

SIRENGINE_FORCEINLINE uint64_t CString::Length( void ) const {
    return m_nLength;
}

SIRENGINE_FORCEINLINE void CString::Copy( const CString& other )
{
    Clear();

    if ( other.m_pBuffer ) {
        m_nLength = other.m_nLength;
        m_nAllocated = other.m_nAllocated;
        m_pBuffer = (char *)malloc( m_nLength );
        memset( m_pBuffer, 0, m_nLength );
        memcpy( m_pBuffer, other.m_pBuffer, m_nLength );
    }
}

SIRENGINE_FORCEINLINE void CString::Move( CString&& other )
{
    memcpy( m_szBaseBuffer, other.m_szBaseBuffer, sizeof( m_szBaseBuffer ) );
    m_pBuffer = other.m_pBuffer;
    m_nLength = other.m_nLength;
    m_nAllocated = other.m_nAllocated;

    memset( other.m_szBaseBuffer, 0, sizeof( other.m_szBaseBuffer ) );
    other.m_pBuffer = NULL;
    other.m_nLength = 0;
    other.m_nAllocated = 0;
}

SIRENGINE_FORCEINLINE void CString::Assign( const CString& other, uint64_t nChars = 0 )
{
    Assign( other.c_str() );
}

SIRENGINE_FORCEINLINE void CString::Assign( const char *other, uint64_t nChars = 0 )
{
    if ( !other ) {
        nChars = 0;
    } else if ( nChars == 0 ) {
        nChars = strlen( other );
    }

    Clear();
    EnsureAllocated( nChars );
    memcpy( m_pBuffer, other, nChars );
}

SIRENGINE_FORCEINLINE void CString::Append( const CString& other )
{
    Append( other.c_str() );
}

SIRENGINE_FORCEINLINE void CString::Append( const char *other )
{
    uint64_t nLength;

    nLength = strlen( other );
    EnsureAllocated( nLength + 1 );

    m_nLength += nLength;

    strncat( m_pBuffer, other, nLength );
}

SIRENGINE_FORCEINLINE bool CString::Empty( void ) const {
    return m_nLength == 0;
}

SIRENGINE_FORCEINLINE void CString::Clear( void ) {
    if ( !m_nAllocated ) {
        memset( m_szBaseBuffer, 0, sizeof( m_szBaseBuffer ) );
    } else {
        free( m_pBuffer );
    }

    m_nAllocated = 0;
    m_nLength = 0;
    m_pBuffer = NULL;
}

SIRENGINE_FORCEINLINE void CString::Resize( uint64_t nBytes ) {
    m_nLength = nBytes;

    Clear();

    if ( m_nAllocated != nBytes ) {
        if ( nBytes >= sizeof ( m_szBaseBuffer ) - 1 ) {
            void *newBuffer = malloc( nBytes );
        } else {
            m_szBaseBuffer[ nBytes ] = '\0';
        }
    }
}

SIRENGINE_FORCEINLINE void CString::Reserve( uint64_t nBytes ) {
    EnsureAllocated( nBytes );
}

void CString::EnsureAllocated( uint64_t nBytes )
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

CString& CString::append_sprintf_va_list( const char *fmt, va_list argptr )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
    uint64_t nInitialLength = m_nLength;
    uint64_t nReturnvalue;

#if SIRENGINE_VA_COPY_ENABLED
    va_list argumentsSaved;
    va_copy( argumentsSaved, argptr );
#endif

    nReturnValue = SIREngine_Vsnprintf( NULL, 0, fmt, argptr );

    if ( nReturnValue > 0 ) {
        Resize( nReturnValue + nInitialSize );
    }
#else
#endif
}

CString& CString::append_sprintf( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3))
{
    va_list argptr;

    va_start( argptr, fmt );
    append_sprintf_va_list( fmt, argptr );
    va_end( argptr );

    return *this;
}
*/

#endif