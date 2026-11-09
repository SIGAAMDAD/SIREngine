#include "rendering/rd/vt/runtime/terrain_3d_composed_surface_mapped_file.h"
#include <string>
#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static bool t3d_comp_surf_utf8_to_wide( const char *pUtf8, std::wstring& out )
{
	if ( !pUtf8 || !pUtf8[0] )
		return false;
	const int count = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pUtf8, -1, nullptr, 0 );
	if ( count < 2 )
		return false;
	out.resize( size_t( count ) );
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pUtf8, -1, &out[0], count ) != count )
		return false;
	out.resize( size_t( count - 1 ) );
	return true;
}
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

bool Terrain3DComposedSurfaceMappedFile::openCold( const char *pUtf8Path, const uint64_t nExpectedFingerprint )
{
	closeCold();
	if ( !pUtf8Path || !pUtf8Path[0] || !nExpectedFingerprint )
		return false;
#if defined( _WIN32 )
	std::wstring wide;
	if ( !t3d_comp_surf_utf8_to_wide( pUtf8Path, wide ) )
		return false;
	HANDLE file = CreateFileW( wide.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	                           FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( file == INVALID_HANDLE_VALUE )
		return false;
	LARGE_INTEGER length{};
	if ( !GetFileSizeEx( file, &length ) || length.QuadPart <= 0 || uint64_t( length.QuadPart ) > SIZE_MAX ) {
		CloseHandle( file );
		return false;
	}
	HANDLE mapping = CreateFileMappingW( file, nullptr, PAGE_READONLY, 0u, 0u, nullptr );
	if ( !mapping ) {
		CloseHandle( file );
		return false;
	}
	const void *bytes = MapViewOfFile( mapping, FILE_MAP_READ, 0u, 0u, 0u );
	if ( !bytes ) {
		CloseHandle( mapping );
		CloseHandle( file );
		return false;
	}
	m_pFile = file;
	m_pFileMapping = mapping;
	m_pMapping = const_cast<void *>( bytes );
	m_nBytes = uint64_t( length.QuadPart );
#else
	const int file = open( pUtf8Path, O_RDONLY );
	if ( file < 0 )
		return false;
	struct stat info{};
	if ( fstat( file, &info ) != 0 || info.st_size <= 0 || uint64_t( info.st_size ) > SIZE_MAX ) {
		close( file );
		return false;
	}
	const void *bytes = mmap( nullptr, size_t( info.st_size ), PROT_READ, MAP_PRIVATE, file, 0 );
	close( file );
	if ( bytes == MAP_FAILED )
		return false;
	m_pMapping = const_cast<void *>( bytes );
	m_nBytes = uint64_t( info.st_size );
#endif
	if ( !t3d_comp_surf_open_sidecar_cold( static_cast<const uint8_t *>( m_pMapping ), size_t( m_nBytes ), nExpectedFingerprint,
	                                       m_view ) ) {
		closeCold();
		return false;
	}
	return true;
}

void Terrain3DComposedSurfaceMappedFile::closeCold()
{
#if defined( _WIN32 )
	if ( m_pMapping )
		UnmapViewOfFile( m_pMapping );
	if ( m_pFileMapping )
		CloseHandle( static_cast<HANDLE>( m_pFileMapping ) );
	if ( m_pFile )
		CloseHandle( static_cast<HANDLE>( m_pFile ) );
	m_pFile = nullptr;
	m_pFileMapping = nullptr;
#else
	if ( m_pMapping )
		munmap( m_pMapping, size_t( m_nBytes ) );
#endif
	m_pMapping = nullptr;
	m_nBytes = 0u;
	m_view = {};
}
