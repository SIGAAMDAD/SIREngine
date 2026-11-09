#include "authoring/vt/io/terrain_3d_composed_surface_sidecar_writer.h"
#include "data/runtime/terrain_3d_composed_surface_sidecar.h"
#include <stdio.h>
#include <string>
#if defined( _WIN32 )
#include <windows.h>
#include <io.h>
static bool t3d_comp_surf_write_utf8_to_wide( const char *pUtf8, std::wstring& out )
{
	if ( !pUtf8 || !pUtf8[0] )
		return false;
	const int length = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pUtf8, -1, nullptr, 0 );
	if ( length < 2 )
		return false;
	out.resize( size_t( length ) );
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pUtf8, -1, &out[0], length ) != length )
		return false;
	out.resize( size_t( length - 1 ) );
	return true;
}
#else
#include <unistd.h>
#endif

bool t3d_comp_surf_write_sidecar_cold( const char *pDestination, const uint8_t *pBytes, const uint64_t nBytes )
{
	if ( !pDestination || !pDestination[0] || !pBytes || nBytes > SIZE_MAX ||
	     !t3d_comp_surf_validate_sidecar_bytes( pBytes, size_t( nBytes ), true ) )
		return false;
	// Identical validated cold sidecars perform zero writes, preserving file
	// timestamps and avoiding a needless synchronous disk flush on no-op bakes.
	// Compare in bounded chunks rather than loading a second full sidecar.
#if defined( _WIN32 )
	std::wstring wide_dest;
	if ( !t3d_comp_surf_write_utf8_to_wide( pDestination, wide_dest ) )
		return false;
	FILE *pExisting = _wfopen( wide_dest.c_str(), L"rb" );
#else
	FILE *pExisting = fopen( pDestination, "rb" );
#endif
	if ( pExisting ) {
		uint8_t scratch[16384];
		uint64_t offset = 0u;
		bool identical = true;
		while ( offset < nBytes ) {
			const size_t block = size_t( ( nBytes - offset ) < sizeof( scratch ) ? ( nBytes - offset ) : sizeof( scratch ) );
			if ( fread( scratch, 1u, block, pExisting ) != block || memcmp( scratch, pBytes + offset, block ) != 0 ) {
				identical = false;
				break;
			}
			offset += block;
		}
		if ( identical )
			identical = fgetc( pExisting ) == EOF && !ferror( pExisting );
		fclose( pExisting );
		if ( identical )
			return true;
	}
	const std::string temporary = std::string( pDestination ) + ".t3sc.tmp";
#if defined( _WIN32 )
	std::wstring wide_temp;
	if ( !t3d_comp_surf_write_utf8_to_wide( temporary.c_str(), wide_temp ) )
		return false;
	FILE *pFile = _wfopen( wide_temp.c_str(), L"wb" );
#else
	FILE *pFile = fopen( temporary.c_str(), "wb" );
#endif
	if ( !pFile )
		return false;
	bool ok = fwrite( pBytes, 1u, size_t( nBytes ), pFile ) == size_t( nBytes );
	if ( ok )
		ok = fflush( pFile ) == 0;
#if defined( _WIN32 )
	if ( ok )
		ok = _commit( _fileno( pFile ) ) == 0;
#else
	if ( ok )
		ok = fsync( fileno( pFile ) ) == 0;
#endif
	if ( fclose( pFile ) != 0 )
		ok = false;
	if ( ok ) {
#if defined( _WIN32 )
		ok = MoveFileExW( wide_temp.c_str(), wide_dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) != 0;
#else
		ok = rename( temporary.c_str(), pDestination ) == 0;
#endif
	}
	if ( !ok ) {
#if defined( _WIN32 )
		_wremove( wide_temp.c_str() );
#else
		remove( temporary.c_str() );
#endif
	}
	return ok;
}
