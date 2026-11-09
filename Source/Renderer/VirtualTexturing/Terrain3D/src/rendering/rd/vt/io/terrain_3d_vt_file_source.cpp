#include "rendering/rd/vt/io/terrain_3d_vt_file_source.h"
#include "data/formats/terrain_3d_vt_pack_format.h"
#include "core/utility/terrain_3d_hash_bytes.h"
#include <godot_cpp/classes/project_settings.hpp>
#include <limits.h>
#include <string.h>

void t3d_vt_file_source_close( t3dVTFileSource_s& rSource )
{
	if ( rSource.pNativeFile != NULL ) {
		fclose( rSource.pNativeFile );
		rSource.pNativeFile = NULL;
	}
	rSource.m_File.unref();
	rSource.nFileBytes = 0u;
	rSource.nPayloadOffset = 0u;
	rSource.nPayloadBytes = 0u;
	rSource.nSignature = 0u;
	rSource.nReadCount = 0u;
}

bool t3d_vt_file_source_open_cold( const godot::String& rPath, t3dVTFileSource_s& rSource, godot::PackedByteArray& rMetadata,
                                   const bool bStrictHash )
{
	t3d_vt_file_source_close( rSource );
	rMetadata.clear();
	if ( rPath.is_empty() ) {
		return false;
	}
	godot::Ref<godot::FileAccess> file = godot::FileAccess::open( rPath, godot::FileAccess::READ );
	if ( file.is_null() ) {
		return false;
	}
	const uint64_t nLength = file->get_length();
	if ( nLength < T3D_VT_PACK_HEADER_BYTES ) {
		return false;
	}
	const godot::PackedByteArray headerBytes = file->get_buffer( T3D_VT_PACK_HEADER_BYTES );
	if ( headerBytes.size() != T3D_VT_PACK_HEADER_BYTES ) {
		return false;
	}
	t3dVTPackHeader_t Header{};
	memcpy( &Header, headerBytes.ptr(), sizeof( Header ) );
	if ( Header.m_nMagic != T3D_VT_FILE_MAGIC || Header.m_nVersion != T3D_VT_FILE_FORMAT_VERSION || Header.m_nPackageBytes != nLength ||
	     Header.m_nPayloadDataOffset < T3D_VT_PACK_HEADER_BYTES || Header.m_nPayloadDataOffset > nLength ||
	     Header.m_nPayloadDataOffset > 256u * 1024u * 1024u || Header.m_nPayloadDataBytes != nLength - Header.m_nPayloadDataOffset ) {
		return false;
	}
	// Bound resident package memory to the directory size rather than the texture payload.
	file->seek( 0u );
	rMetadata = file->get_buffer( Header.m_nPayloadDataOffset );
	if ( ( uint64_t )rMetadata.size() != Header.m_nPayloadDataOffset ) {
		return false;
	}
	if ( bStrictHash ) {
		// Cold verification uses the exact persisted FNV-1a signature but never
		// materializes the file. This strict scan can be disabled for trusted cooks.
		file->seek( T3D_VT_PACK_HEADER_BYTES );
		uint64_t nHash = 1469598103934665603ull;
		uint64_t nRemaining = nLength - T3D_VT_PACK_HEADER_BYTES;
		while ( nRemaining != 0u ) {
			const uint64_t nChunk = nRemaining < 65536u ? nRemaining : 65536u;
			const godot::PackedByteArray chunk = file->get_buffer( nChunk );
			if ( ( uint64_t )chunk.size() != nChunk ) {
				rMetadata.clear();
				return false;
			}
			nHash = t3dHashFnv1a64( chunk.ptr(), nChunk, nHash );
			nRemaining -= nChunk;
		}
		if ( nHash != Header.m_nContentSignature ) {
			rMetadata.clear();
			return false;
		}
	}
	rSource.nFileBytes = nLength;
	rSource.nPayloadOffset = Header.m_nPayloadDataOffset;
	rSource.nPayloadBytes = Header.m_nPayloadDataBytes;
	rSource.nSignature = Header.m_nContentSignature;
	rSource.m_File = file;
	// Native filesystem route avoids any allocation in streaming page reads;
	// FileAccess remains the PCK-compatible fallback.
	godot::ProjectSettings *const settings = godot::ProjectSettings::get_singleton();
	const godot::String nativePath = settings != NULL ? settings->globalize_path( rPath ) : rPath;
	if ( !nativePath.is_empty() ) {
		const godot::CharString utf8 = nativePath.utf8();
		rSource.pNativeFile = fopen( utf8.get_data(), "rb" );
		if ( rSource.pNativeFile != NULL ) {
			// A loose project file can coexist with an exported PCK. Do not mix
			// a PCK metadata directory with bytes from an unrelated loose file.
			t3dVTPackHeader_t nativeHeader{};
			bool same = fread( &nativeHeader, 1u, sizeof( nativeHeader ), rSource.pNativeFile ) == sizeof( nativeHeader ) &&
			            nativeHeader.m_nPackageBytes == nLength && nativeHeader.m_nHeaderChecksum == Header.m_nHeaderChecksum &&
			            nativeHeader.m_nContentSignature == Header.m_nContentSignature;
			if ( !same ) {
				fclose( rSource.pNativeFile );
				rSource.pNativeFile = NULL;
			}
		}
	}
	return true;
}

bool t3d_vt_file_source_read_payload( t3dVTFileSource_s& rSource, const uint64_t nOffset, uint8_t *const pDestination,
                                      const uint32_t nLength )
{
	if ( pDestination == NULL || nLength == 0u || rSource.nFileBytes == 0u || nOffset > rSource.nPayloadBytes ||
	     nLength > rSource.nPayloadBytes - nOffset || rSource.nPayloadOffset > UINT64_MAX - nOffset ) {
		return false;
	}
	const uint64_t nPhysical = rSource.nPayloadOffset + nOffset;
	if ( rSource.pNativeFile != NULL ) {
#if defined( _WIN32 )
		if ( _fseeki64( rSource.pNativeFile, ( __int64 )nPhysical, SEEK_SET ) != 0 ) {
			return false;
		}
#else
		if ( nPhysical > ( uint64_t )INT64_MAX || fseeko( rSource.pNativeFile, ( off_t )nPhysical, SEEK_SET ) != 0 ) {
			return false;
		}
#endif
		if ( fread( pDestination, 1u, nLength, rSource.pNativeFile ) != nLength ) {
			return false;
		}
	} else {
		if ( rSource.m_File.is_null() ) {
			return false;
		}
		rSource.m_File->seek( nPhysical );
		const godot::PackedByteArray bytes = rSource.m_File->get_buffer( nLength );
		if ( ( uint32_t )bytes.size() != nLength ) {
			return false;
		}
		memcpy( pDestination, bytes.ptr(), nLength );
	}
	++rSource.nReadCount;
	return true;
}
