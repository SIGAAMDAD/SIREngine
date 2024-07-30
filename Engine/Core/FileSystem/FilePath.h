#ifndef __SIRENGINE_FILEPATH_H__
#define __SIRENGINE_FILEPATH_H__

#pragma once

#include <Engine/Util/CString.h>

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
        : CString( other )
    { }
    ~CFilePath()
    { }

    SIRENGINE_FORCEINLINE CString GetExtension( void ) const
    {
        const char *dot = strrchr( m_pBuffer, '.' ), *slash;
    	if ( dot && ( ( slash = strrchr( m_pBuffer, '/' ) ) == NULL || slash < dot ) ) {
    		return dot + 1;
        } else {
    		return "";
        }
    }
    SIRENGINE_FORCEINLINE void StripExtension( void )
    {
        const char *dot = strrchr( m_pBuffer, '.' ), *slash;

	    if ( dot && ( ( slash = strrchr( m_pBuffer, '/' ) ) == NULL || slash < dot ) ) {
	    	m_nLength = ( m_nLength < dot - m_pBuffer + 1 ? m_nLength : dot - m_pBuffer + 1 );
        }
	    if ( m_nLength > 1 ) {
	    	m_pBuffer[ m_nLength - 1 ] = '\0';
        }
    }

    SIRENGINE_FORCEINLINE CString& GetString( void )
    { return *dynamic_cast<CString *>( this ); }
    SIRENGINE_FORCEINLINE const CString& GetString( void ) const
    { return *dynamic_cast<const CString *>( this ); }
};

#endif