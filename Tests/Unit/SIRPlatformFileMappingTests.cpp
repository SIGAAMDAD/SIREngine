#include <Core/Platform/SIRPlatformFile.h>
#include <Core/Platform/SIRPlatformFileMapping.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SIR_REQUIRE( condition )                                                                                                           \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static void sirTestMappings( void )
{
	const char *pszPath = "SIRPlatformFileMappingTests.bin";
	( void )SIRPlatformFile::remove( pszPath );
	SIRPlatformFile file;
	SIRPlatformFileMapping mapping;
	SIR_REQUIRE( mapping.getData() == nullptr );
	SIR_REQUIRE( mapping.unmap() == SIR_FILE_OK );
	SIR_REQUIRE( mapping.flush() == SIR_FILE_CLOSED );
	SIR_REQUIRE( mapping.map( file, 0, 1, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_INVALID_ARGUMENT );

	SIR_REQUIRE( file.open( pszPath, SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) ==
	             SIR_FILE_OK );
	SIR_REQUIRE( mapping.map( file, 0, 0, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_OUT_OF_RANGE );
	uint8_t szContents[8192];
	for ( uint32_t i = 0; i < sizeof( szContents ); ++i ) {
		szContents[i] = ( uint8_t )( i ^ ( i >> 5 ) );
	}
	uint64_t nTransferred = 0;
	SIR_REQUIRE( file.write( szContents, sizeof( szContents ), &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( nTransferred == sizeof( szContents ) );

	SIR_REQUIRE( mapping.map( file, UINT64_MAX, 4, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_OUT_OF_RANGE );
	SIR_REQUIRE( mapping.map( file, 8192, 0, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_OUT_OF_RANGE );
	SIR_REQUIRE( mapping.map( file, 8191, 2, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_OUT_OF_RANGE );
	SIR_REQUIRE( mapping.map( file, 37, 4099, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_OK );
	SIR_REQUIRE( mapping.isMapped() && mapping.getSize() == 4099 && mapping.getOffset() == 37 );
	SIR_REQUIRE( mapping.getWritableData() == nullptr );
	SIR_REQUIRE( memcmp( mapping.getData(), szContents + 37, 4099 ) == 0 );
	SIR_REQUIRE( mapping.flush() == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( mapping.map( file, 0, 1, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );
	// The underlying mapping retains its own native lifetime after closing file.
	SIR_REQUIRE( mapping.getData()[4098] == szContents[37 + 4098] );
	SIR_REQUIRE( mapping.unmap() == SIR_FILE_OK );
	SIR_REQUIRE( mapping.unmap() == SIR_FILE_OK );

	SIR_REQUIRE( file.open( pszPath, SIR_FILE_OPEN_READ ) == SIR_FILE_OK );
	SIR_REQUIRE( mapping.map( file, 1, 8, SIR_FILE_MAP_READ_WRITE ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( mapping.map( file, 1, 8, SIR_FILE_MAP_COPY_ON_WRITE ) == SIR_FILE_OK );
	mapping.getWritableData()[0] = 0xEE;
	SIR_REQUIRE( mapping.getData()[0] == 0xEE );
	SIR_REQUIRE( mapping.flush() == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( mapping.unmap() == SIR_FILE_OK );
	uint8_t nOriginal = 0;
	SIR_REQUIRE( file.readAt( 1, &nOriginal, 1, &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( nOriginal == szContents[1] );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );

	SIR_REQUIRE( file.open( pszPath, SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE ) == SIR_FILE_OK );
	SIR_REQUIRE( mapping.map( file, 13, 256, SIR_FILE_MAP_READ_WRITE ) == SIR_FILE_OK );
	mapping.getWritableData()[5] = 0x5D;
	SIR_REQUIRE( mapping.flush() == SIR_FILE_OK );
	SIR_REQUIRE( mapping.unmap() == SIR_FILE_OK );
	uint8_t nChanged = 0;
	SIR_REQUIRE( file.readAt( 18, &nChanged, 1, &nTransferred ) == SIR_FILE_OK && nChanged == 0x5D );

	// Sparse 64-bit offset: no large in-process allocation should be needed.
	const uint64_t nOffset = ( UINT64_C( 1 ) << 32 ) + 123;
	const uint8_t nMarker = 0xBF;
	SIR_REQUIRE( file.writeAt( nOffset, &nMarker, 1, &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( mapping.map( file, nOffset, 1, SIR_FILE_MAP_READ_ONLY ) == SIR_FILE_OK );
	SIR_REQUIRE( mapping.getData()[0] == nMarker );
	SIR_REQUIRE( mapping.unmap() == SIR_FILE_OK );
	SIR_REQUIRE( file.truncate( 8192 ) == SIR_FILE_OK );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );
	SIR_REQUIRE( SIRPlatformFile::remove( pszPath ) == SIR_FILE_OK );
}

int main( void )
{
	sirTestMappings();
	return 0;
}
