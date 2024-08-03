#ifndef __SIRENGINE_UTIL_H__
#define __SIRENGINE_UTIL_H__

#pragma once

#include <Engine/Core/SIREngine.h>

extern const char *SIRENGINE_ATTRIBUTE(format(printf, 1, 2)) SIRENGINE_TEMP_VSTRING( const char *fmt, ... );
extern size_t SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) SIREngine_snprintf( char *pszBuffer, size_t nMaxSize, const char *fmt, ... );

extern void SIREngine_strncpyz( char *pDest, const char *pSource, size_t nLength );
extern int SIREngine_stricmp( const char *pString1, const char *pString2 );
extern const char *SIREngine_stristr( const char *pHaystack, const char *pNeedle );
extern const char *SIREngine_GetMemoryString( size_t nBytes );

const CString String_Contains( const CString& str1, const CString& str2, uint64_t len2 );
bool String_FilterExt( const CString& filter, const char *name );
bool String_FilterExt( const CString& filter, const char *name );
int String_Filter( const CString& filter, const char *name );
int String_FilterPath( const CString& filter, const CString& name );
bool String_HasPatterns( const CString& str );

#define SIREngine_StaticArrayLength( arr ) ( sizeof( arr ) / sizeof( *arr ) )

#endif