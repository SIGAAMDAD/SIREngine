#ifndef __SIRENGINE_FILEPATH_H__
#define __SIRENGINE_FILEPATH_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/Compiler.h>
#include <Engine/Util/CString.h>

namespace FileSystem {
    class CFilePath : public CString
    {
    public:
        CFilePath( void )
            : CString()
        { }
        CFilePath( const char *pString )
            : CString( pString )
        { }
        CFilePath( const CFilePath& other )
            : CString( other )
        { }
        CFilePath( CFilePath&& other )
            : CString( eastl::move( other ) )
        { }

        SIRENGINE_FORCEINLINE const CFilePath& operator=( const CFilePath& other )
        {
            CString::operator=( other );
            return *this;
        }
        SIRENGINE_FORCEINLINE const CFilePath& operator=( CFilePath&& other )
        {
            CString::operator=( eastl::move( other ) );
            return *this;
        }
    
        SIRENGINE_FORCEINLINE static CString GetExtension( const char *pszFilePath )
        {
            const char *dot = strrchr( pszFilePath, '.' ), *slash;
        	if ( dot && ( ( slash = strrchr( pszFilePath, '/' ) ) == NULL || slash < dot ) ) {
        		return dot + 1;
            }
            return "";

        }
        SIRENGINE_FORCEINLINE CString GetFileName( void ) const
        {
            const char *pLast, *it;
	
        	pLast = internalLayout().BeginPtr();
            it = internalLayout().BeginPtr();
        	while ( *it ) {
        		if ( *it == '/' ) {
        			pLast = it + 1;
                }
        		it++;
        	}
        	return pLast;
        }
        SIRENGINE_FORCEINLINE void StripExtension( void )
        {
            const char *dot = strrchr( data(), '.' ), *slash;

    	    if ( dot && ( ( slash = strrchr( data(), '/' ) ) == NULL || slash < dot ) ) {
    	        internalLayout().SetSize(
                    ( size() < (uintptr_t)( dot - data() ) + 1 ? size() : dot - data() + 1 ) );
            }
    	    if ( size() > 1 ) {
    	    	internalLayout().BeginPtr()[ size() - 1 ] = '\0';
            }
        }

        SIRENGINE_FORCEINLINE CString& GetString( void )
        { return *dynamic_cast<CString *>( this ); }
        SIRENGINE_FORCEINLINE const CString& GetString( void ) const
        { return *dynamic_cast<const CString *>( this ); }
    };
};

namespace eastl {
    template<> struct hash<FileSystem::CFilePath> {
		size_t operator()( const FileSystem::CFilePath& str ) const {
			const unsigned char *p = (const unsigned char *)str.c_str(); // To consider: limit p to at most 256 chars.
			unsigned int c, result = 2166136261U; // We implement an FNV-like string hash.
			while((c = *p++) != 0) // Using '!=' disables compiler warnings.
				result = (result * 16777619) ^ c;
			return (size_t)result;
		}
	};
};

#endif