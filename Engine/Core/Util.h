#ifndef __SIRENGINE_UTIL_H__
#define __SIRENGINE_UTIL_H__

#pragma once

#include <Engine/Core/SIREngine.h>

extern const char *SIRENGINE_ATTRIBUTE(format(printf, 1, 2)) SIRENGINE_TEMP_VSTRING( const char *fmt, ... );
extern size_t SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) SIREngine_snprintf( char *pszBuffer, size_t nMaxSize, const char *fmt, ... );

extern void SIREngine_strcpyz( char *pDest, const char *pSource, size_t nLength );
extern int SIREngine_stricmp( const char *str1, const char *str2 );

#define SIREngine_StaticArrayLength( arr ) ( sizeof( arr ) / sizeof( *arr ) )

#endif