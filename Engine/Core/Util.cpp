#include <Engine/Core/SIREngine.h>
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

const char *SIRENGINE_ATTRIBUTE(format(printf, 1, 2)) SIRENGINE_TEMP_VSTRING( const char *fmt, ... )
{
    va_list argptr;
    int length;
    char *str;
    static int index = 0;
    static char msg[8][8192];

    index++;
    str = msg[ index % 8 ];

    va_start( argptr, fmt );
    length = SIREngine_Vsnprintf( str, sizeof( *msg ) - 1, fmt, argptr );
    va_end( argptr );

    return str;
}

size_t SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) SIREngine_snprintf( char *pszBuffer, size_t nMaxSize, const char *fmt, ... )
{
    va_list argptr;
    size_t nLength;

    va_start( argptr, fmt );
    nLength = SIREngine_Vsnprintf( pszBuffer, nMaxSize, fmt, argptr );
    va_end( argptr );

    return nLength;
}

size_t SIRENGINE_ATTRIBUTE(format(printf, 3, 4)) SIREngine_snprintf_append( char *pszBuffer, size_t nMaxSize, const char *fmt, ... )
{
	va_list argptr;
	char szBuffer[20000];
    size_t nLength;

    va_start( argptr, fmt );
    nLength = SIREngine_Vsnprintf( szBuffer, sizeof( szBuffer ) - 1, fmt, argptr );
    va_end( argptr );

	strncat( pszBuffer, szBuffer, nMaxSize );

    return nLength;
}

void SIREngine_strncpyz( char *pDest, const char *pSource, size_t nLength )
{
    if ( !pDest ) {
        SIRENGINE_ERROR( "SIREngine_strncpyz: NULL destination" );
    }
    if ( !pSource ) {
        SIRENGINE_ERROR( "SIREngine_strncpyz: NULL source" );
    }
    if ( nLength < 1 ) {
        SIRENGINE_ERROR( "SIREngine_strncpyz: funny length" );
    }

    strncpy( pDest, pSource, nLength - 1 );
    pDest[ nLength - 1 ] = '\0';
}

int SIREngine_stricmp( const char *pString1, const char *pString2 )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
    return stricmp( pString1, pString2 );
#else
    return strcasecmp( pString1, pString2 );
#endif
}

const char *SIREngine_stristr( const char *pHaystack, const char *pNeedle )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
    return stristr( pHaystack, pNeedle );
#else
    return strcasestr( pHaystack, pNeedle );
#endif
}

const char *SIREngine_GetMemoryString( size_t nBytes )
{
    static char szBuffer[64];
    const char *pSuffix;

    pSuffix = "B";
    if ( nBytes > 1024 ) {
        nBytes /= 1024;
        pSuffix = "Kb";
    }
    if ( nBytes > 1024 ) {
        nBytes /= 1024;
        pSuffix = "Mb";
    }
    if ( nBytes > 1024 ) {
        nBytes /= 1024;
        pSuffix = "Gb";
    }

    memset( szBuffer, 0, sizeof( szBuffer ) );
    SIREngine_snprintf( szBuffer, sizeof( szBuffer ) - 1, "%lu %s", nBytes, pSuffix );

    return szBuffer;
}

const char *SIREngine_GetMemoryString( double nBytes )
{
    static char szBuffer[64];
    const char *pSuffix;

    pSuffix = "B";
    if ( nBytes > 1024.0f ) {
        nBytes /= 1024.0f;
        pSuffix = "Kb";
    }
    if ( nBytes > 1024.0f ) {
        nBytes /= 1024.0f;
        pSuffix = "Mb";
    }
    if ( nBytes > 1024.0f ) {
        nBytes /= 1024.0f;
        pSuffix = "Gb";
    }

    memset( szBuffer, 0, sizeof( szBuffer ) );
    SIREngine_snprintf( szBuffer, sizeof( szBuffer ) - 1, "%0.4f %s", nBytes, pSuffix );

    return szBuffer;
}

SIRENGINE_EXPORT_DEMANGLE void SIREngine_AssertionFailure( const char *pAssertion, const char *pFileName, uint64_t nLineNumber )
{
    printf( "Assertion failure [%s:%lu] -> %s\n", pFileName, nLineNumber, pAssertion );
    delete SIREngine::Application::g_pApplication;
    exit( EXIT_FAILURE );
}

const byte g_szLocase[ 256 ] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
	0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
	0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
	0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
	0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
	0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
	0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
	0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
	0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
	0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
	0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
	0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
	0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
	0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
	0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
	0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};


const CString String_Contains( const CString& str1, const CString& str2, uint64_t len2 )
{
	uint64_t len, i, j;

	len = str1.size() - len2;
    auto it = str1.cbegin();
	for ( i = 0; i <= len; i++, it++ ) {
		for ( j = 0; str2[j]; j++ ) {
			if ( g_szLocase[ (byte)str1[ j ] ] != g_szLocase[ (byte)str2[ j ] ] ) {
				break;
			}
		}
		if ( !str2[ j ] ) {
			return it;
		}
	}
	return "";
}


bool String_FilterExt( const CString& filter, const char *name )
{
    char buf[1024];
	CString ptr;
	uint32_t i;

    auto it = filter.cbegin();
	while ( *it ) {
		if ( *it == '*' ) {
			it++;
			for ( i = 0; *it != '\0' && i < sizeof( buf ) - 1; i++ ) {
				if ( *it == '*' || *it == '?' ) {
					break;
                }
				buf[i] = *it++;
			}
			buf[ i ] = '\0';
			if ( i ) {
				ptr = String_Contains( name, buf, i );
				if ( !ptr.size() ) {
					return false;
                }
				name = ptr.cbegin() + i;
			} else if ( *it == '\0' ) {
				return true;
			}
		}
		else if ( *it == '?' ) {
			if ( *name == '\0' ) {
				return false;
            }
			it++;
			name++;
		}
		else {
			if ( g_szLocase[ (byte)*it ] != g_szLocase[ (byte)*name ] ) {
				return false;
            }
			it++;
			name++;
		}
	}
	if ( *name ) {
		return false;
	}
	return true;
}


int String_Filter( const CString& filter, const char *name )
{
	char buf[ 1024 ];
	CString ptr;
	int i, found;

    auto it = filter.cbegin();
	while ( *it ) {
		if ( *it == '*' ) {
			it++;
			for ( i = 0; *it; i++ ) {
				if ( *it == '*' || *it == '?' ) {
					break;
                }
				buf[i] = *it;
				it++;
			}
			buf[i] = '\0';
			if ( i ) {
				ptr = String_Contains( name, buf, i );
				if ( !ptr.size() ) {
					return false;
                }
				it = ptr.cbegin() + i;
			}
		}
		else if ( *it == '?' ) {
			it++;
			name++;
		}
		else if ( *it == '[' && *( it + 1 ) == '[' ) {
			it++;
		}
		else if ( *it == '[' ) {
			it++;
			found = false;
			while( *it && !found ) {
				if ( *it == ']' && *( it + 1 ) != ']' ) {
                    break;
                }
				if ( *( it + 1 ) == '-' && *( it + 2 ) && ( *( it + 2 ) != ']' || *( it + 3 ) == ']' ) ) {
					if ( g_szLocase[ (byte)*name ] >= g_szLocase[ (byte)*it ] &&
						g_szLocase[ (byte)*name ] <= g_szLocase[ (byte)*( it + 2 ) ] )
                    {
						found = true;
                    }
					it += 3;
				}
				else {
					if ( g_szLocase[ (byte)*it ] == g_szLocase[ (byte)*name ] ) {
						found = true;
                    }
					it++;
				}
			}
			if ( !found ) {
                return false;
            }
			while ( *it ) {
				if ( *it == ']' && *( it + 1 ) != ']' ) {
                    break;
                }
				it++;
			}
			it++;
			name++;
		}
		else {
			if ( g_szLocase[ (byte)*it ] != g_szLocase[ (byte)*name ] ) {
				return false;
            }
			it++;
			name++;
		}
	}
	return true;
}

int String_FilterPath( const CString& filter, const CString& name )
{
	int i;
    CString szNewFilter;
    CString szNewName;

    memset( szNewFilter.data(), 0, szNewFilter.kMaxSize + 1 );
    memset( szNewName.data(), 0, szNewName.kMaxSize + 1 );

    for ( auto it = filter.cbegin(); it != filter.cend(); it++ ) {
        if ( *it == '\\' || *it ==  ':' ) {
            szNewFilter[i] = '/';
        } else {
            szNewFilter[i] = *it;
        }
    }
    for ( auto it = name.cbegin(); it != name.cend(); it++, i++ ) {
        if ( *it == '\\' || *it == ':' ) {
            szNewName[i] = '/';
        } else {
            szNewName[i] = *it;
        }
    }
	return String_Filter( szNewFilter, szNewName.c_str() );
}

bool String_HasPatterns( const CString& str )
{
	for ( auto it = str.cbegin(); it != str.cend(); it++ ) {
        if ( *it == '*' || *it == '?' ) {
            return true;
        }
    }

	return false;
}