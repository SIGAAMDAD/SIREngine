#include <Streaming/Pack/SIRCompressionRegistry.h>
#include <Core/Compression/SIRZstdCodec.h>
#include <Core/Compression/SIRLZ4Codec.h>
#include <Core/IO/SIRIOBufferPool.h>
#include <Core/IO/SIRIORequestPlanner.h>
#include <Streaming/SIRStreamingUnitResolver.h>
#include <Streaming/SIRPredictionSystem.h>
#include <string.h>

static bool check( bool b ) { return b; }

int main( void )
{
	uint8_t sourceBytes[4] = { 1, 2, 3, 4 }, decodedBytes[4] = {};
	SIRCompressionRegistry codecs;
	sirCompressionDesc_t compression { { sourceBytes, 4 }, { decodedBytes, 4 }, {}, 1, 0 };
	if ( !check( codecs.decompress( SIR_PACK_CODEC_NONE, compression ) == SIRPackResult::SIR_PACK_OK ) || !check( memcmp( sourceBytes, decodedBytes, 4 ) == 0 ) ) return 1;
	SIRZstdCodec zstd;
	SIRLZ4Codec lz4;
	uint8_t encoded[128] = {};
	SIRSpan<uint8_t> input( sourceBytes, sizeof( sourceBytes ) );
	SIRSpan<uint8_t> encodedSpan( encoded, sizeof( encoded ) );
	SIRSpan<uint8_t> scratch;
	size_t nEncoded = 0;
	if ( !check( zstd.encode( input, encodedSpan, scratch, nEncoded ) == SIRCompressionResult::SIR_COMPRESSION_SUCCESS ) ||
	     !check( codecs.registerCodec( zstd ) == SIRPackResult::SIR_PACK_OK ) ) return 1;
	memset( decodedBytes, 0, sizeof( decodedBytes ) );
	sirCompressionDesc_t compressed { { encoded, nEncoded }, { decodedBytes, sizeof( decodedBytes ) }, {}, 1, 0 };
	if ( !check( codecs.decompress( SIR_PACK_CODEC_ZSTD, compressed ) == SIRPackResult::SIR_PACK_OK ) || !check( memcmp( sourceBytes, decodedBytes, 4 ) == 0 ) ) return 1;
	if ( !check( codecs.registerCodec( lz4 ) == SIRPackResult::SIR_PACK_OK ) ) return 1;

	uint8_t poolStorage[64] = {};
	sirIOBufferLease_t poolLeases[2] = {};
	SIRIOBufferPool pool;
	if ( pool.initialize( { poolStorage, sizeof( poolStorage ), poolLeases, 2, 8 } ) != SIR_IO_OK ) return 2;
	sirIOBufferLease_t lease {};
	if ( pool.acquire( 16, 8, &lease ) != SIR_IO_OK || pool.release( lease.handle ) != SIR_IO_OK || pool.shutdown() != SIR_IO_OK ) return 3;

	sirStreamingChunkDemand_t chunk {};
	sirStreamingUnitKey_t key { 1, 2, 3, 4, SIRStreamingUnitKind::SIR_STREAMING_UNIT_WORLD_CELL, 0 };
	sirStreamingUnitRecord_t record { key, &chunk, 1 };
	sirStreamingUnitRecord_t records[1] = {};
	SIRStreamingUnitResolver resolver;
	if ( resolver.initialize( { records, 1 } ) != SIRStreamingResult::SIR_STREAMING_OK || resolver.replaceManifest( { 1, &record, 1 } ) != SIRStreamingResult::SIR_STREAMING_OK ) return 4;
	sirStreamingChunkDemand_t output[1] = {};
	sirStreamingUnitPlan_t plan { output, 1, 0 };
	if ( resolver.resolve( key, &plan ) != SIRStreamingResult::SIR_STREAMING_OK || plan.nRequired != 1 ) return 5;
	return 0;
}
