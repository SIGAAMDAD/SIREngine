#ifndef __SIRENGINE_UTIL_H__
#define __SIRENGINE_UTIL_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Compiler.h"
#include <Engine/Util/CString.h>

extern const char *SIRENGINE_ATTRIBUTE(format(printf, 1, 2)) SIRENGINE_TEMP_VSTRING( const char *fmt, ... );
extern size_t SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) SIREngine_snprintf( char *pszBuffer, size_t nMaxSize, const char *fmt, ... );
extern size_t SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) SIREngine_snprintf_append( char *pszBuffer, size_t nMaxSize, const char *fmt, ... );

extern void SIREngine_strncpyz( char *pDest, const char *pSource, size_t nLength );
extern int SIREngine_stricmp( const char *pString1, const char *pString2 );
extern const char *SIREngine_stristr( const char *pHaystack, const char *pNeedle );
extern const char *SIREngine_GetMemoryString( size_t nBytes );
extern const char *SIREngine_GetMemoryString( double nBytes );

const CString String_Contains( const CString& str1, const CString& str2, uint64_t len2 );
bool String_FilterExt( const CString& filter, const char *name );
bool String_FilterExt( const CString& filter, const char *name );
int String_Filter( const CString& filter, const char *name );
int String_FilterPath( const CString& filter, const CString& name );
bool String_HasPatterns( const CString& str );

#define SIREngine_BoolToString( value ) ( ( value ) ? "true" : "false" )
#define SIREngine_StaticArrayLength( arr ) ( sizeof( arr ) / sizeof( *arr ) )
#define SIREngine_CreateStackObject( objType, ... ) new ( alloca( sizeof( objType ) ) ) objType( __VA_ARGS__ )

template<typename SubClass, typename BaseClass>
inline SubClass *Cast( BaseClass *pClass )
{ return dynamic_cast<SubClass *>( pClass ); }

SIRENGINE_CONSTEXPR bool static_equal( const char *str1, const char *str2 )
{
	return ( *str1 == *str2 ) && ( *str1 == '\0' || static_equal( str1 + 1, str2 + 1 ) );
}

#endif