/* SIREngine MPLv2 - GRDG vector view integration and publication tests. */
#include <Resources/SIRGRDGVectorViews.h>
#include <Resources/SIRGRDGPackedIntegerView.h>
#include <Core/Containers/SIRPackedIntegerVectorBuilder.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK( e )                                                                                                                         \
	do {                                                                                                                                   \
		if ( !( e ) ) {                                                                                                                    \
			fprintf( stderr, "FAIL line %d %s\n", __LINE__, #e );                                                                          \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

struct GRDGTestAllocator
{
	uint64_t nLiveBytes = 0;
};
static void *allocate( void *pContext, uint64_t nBytes, uint32_t nAlign, uint32_t ) noexcept
{
	if ( nAlign < sizeof( void * ) ) {
		nAlign = sizeof( void * );
	}
	void *pMemory = nullptr;
	if ( posix_memalign( &pMemory, nAlign, nBytes ) != 0 ) {
		return nullptr;
	}
	( ( GRDGTestAllocator * )pContext )->nLiveBytes += nBytes;
	return pMemory;
}
static void deallocate( void *pContext, void *pMemory, uint64_t nBytes, uint32_t, uint32_t ) noexcept
{
	( ( GRDGTestAllocator * )pContext )->nLiveBytes -= nBytes;
	free( pMemory );
}
static SIRResourceGraphConfig config( GRDGTestAllocator *pState ) noexcept
{
	SIRResourceGraphConfig result = {};
	result.m_Allocator = { pState, allocate, deallocate };
	result.m_nMetadataDomain = 2;
	result.m_nMaxResources = 16;
	result.m_nMaxStaging = 16;
	result.m_nMaxProviderPins = 8;
	result.m_nMaxCommittedBytes = 1024 * 1024;
	return result;
}
static SIRGRDGVectorLayout layout( uint64_t nCount ) noexcept
{
	return { sizeof( uint32_t ),
	         alignof( uint32_t ),
	         sizeof( uint32_t ),
	         9,
	         SIR_GRDG_VECTOR_ENCODING_RAW_NATIVE,
	         1,
	         SIRGRDGVectorLayoutChecks::hostByteOrder(),
	         nCount,
	         0 };
}
int main( void )
{
	GRDGTestAllocator alloc = {};
	{
		SIRResourceGraph graph( config( &alloc ) );
		CHECK( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		CHECK( pWriter != nullptr );
		SIRGRDGVectorStagingTransaction txn;
		SIRGRDGVectorStagingRequest request = { { 700, 12, 0 }, layout( 8 ), 0, 6 };
		CHECK( txn.tryBegin( *pWriter, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( txn.trySealCompleted() == SIR_RESOURCE_RESULT_BUSY );
		SIRGRDGWriteOnlyVectorView<uint32_t> first;
		SIRGRDGWriteOnlyVectorView<uint32_t> second;
		CHECK( txn.tryLeaseRange<uint32_t>( { 0, 4 }, &first ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( txn.trySealCompleted() == SIR_RESOURCE_RESULT_BUSY );
		CHECK( txn.tryLeaseRange<uint32_t>( { 2, 4 }, &second ) == SIR_RESOURCE_RESULT_BUSY );
		CHECK( txn.tryLeaseRange<uint32_t>( { 4, 4 }, &second ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( txn.trySealCompleted() == SIR_RESOURCE_RESULT_BUSY );
		for ( uint32_t i = 0; i < 4; ++i ) {
			CHECK( first.tryWriteAt( i, i + 10 ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( second.tryWriteAt( i, i + 14 ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		CHECK( first.completeRange() == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( second.completeRange() == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( txn.trySealCompleted() == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceChunkPublication chunk = { { 0 }, { 991, 992, 9, 0 }, txn.getStagingHandle(), false };
		SIRResourcePublication publication = { request.keyResource, SIR_RESOURCE_KIND_BLOB, 1, 9, &chunk, 1, nullptr, 0 };
		SIRResourceHandle published = {};
		CHECK( txn.tryPublish( publication, nullptr, 0, &published ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( first.tryWriteAt( 0, 55 ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView resource;
			CHECK( graph.resolve( scope, published, &resource ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRGRDGReadOnlyVectorView<uint32_t> view;
			SIRGRDGVectorReadRequest read = { { 0 }, layout( 8 ), 1 };
			CHECK( SIRGRDGVectorViewFactory::tryResolveRead<uint32_t>( scope, resource, read, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( view.isValid() && view.getSize() == 8 );
			for ( uint32_t i = 0; i < 8; ++i ) {
				uint32_t nValue = 0;
				CHECK( view.tryReadAt( i, &nValue ) == SIR_RESOURCE_RESULT_SUCCESS );
				CHECK( nValue == i + 10 );
			}
			CHECK( view.tryReadAt( 8, nullptr ) != SIR_RESOURCE_RESULT_SUCCESS );
			read.nExpectedRevision = 999;
			CHECK( SIRGRDGVectorViewFactory::tryResolveRead<uint32_t>( scope, resource, read, &view ) ==
			       SIR_RESOURCE_RESULT_STALE_REVISION );
			read.nExpectedRevision = 1;
			read.layout.nEncodingId = 1;
			CHECK( SIRGRDGVectorViewFactory::tryResolveRead<uint32_t>( scope, resource, read, &view ) ==
			       SIR_RESOURCE_RESULT_INVALID_FORMAT );
		}
		// Packed format is published as encoded bytes, not an array of uint64_t.
		struct PackAllocState
		{
			uint64_t nLive = 0;
		} packState;
		auto packAlloc = []( void *pContext, size_t nBytes, size_t nAlignment ) -> void * {
			if ( nAlignment < sizeof( void * ) ) {
				nAlignment = sizeof( void * );
			}
			void *pMemory = nullptr;
			if ( posix_memalign( &pMemory, nAlignment, nBytes ) != 0 ) {
				return nullptr;
			}
			( ( PackAllocState * )pContext )->nLive += nBytes;
			return pMemory;
		};
		auto packFree = []( void *pContext, void *pMemory, size_t nBytes, size_t ) -> void {
			( ( PackAllocState * )pContext )->nLive -= nBytes;
			free( pMemory );
		};
		sirContainerAllocator_t packAllocator = { &packState, packAlloc, packFree, 0, 22 };
		{
			SIRPackedIntegerVectorBuilder builder( &packAllocator );
			for ( uint32_t i = 0; i < 128; ++i ) {
				CHECK( builder.tryAppend( i * 3 ) == SIR_CONTAINER_OK );
			}
			SIRPackedIntegerVector packed( &packAllocator );
			CHECK( builder.tryBuild( 10, &packed ) == SIR_CONTAINER_OK );
			const uint64_t nEncoded = packed.getEncodedBytes();
			SIRGRDGVectorStagingTransaction packTxn;
			SIRGRDGVectorStagingRequest packRequest = {
			    { 701, 12, 0 },
			    { 1, 1, 1, 13, SIR_GRDG_VECTOR_ENCODING_RAW_NATIVE, 2, SIRGRDGVectorLayoutChecks::hostByteOrder(), nEncoded, 0 },
			    0,
			    6 };
			CHECK( packTxn.tryBegin( *pWriter, packRequest ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRGRDGWriteOnlyVectorView<uint8_t> packWrite;
			CHECK( packTxn.tryLeaseRange<uint8_t>( { 0, nEncoded }, &packWrite ) == SIR_RESOURCE_RESULT_SUCCESS );
			uint8_t buffer[1024] = {};
			CHECK( nEncoded < sizeof( buffer ) );
			CHECK( packed.tryEncode( buffer, nEncoded ) == SIR_CONTAINER_OK );
			CHECK( packWrite.tryWriteRange( 0, buffer, nEncoded ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( packWrite.completeRange() == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( packTxn.trySealCompleted() == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceChunkPublication packChunk = { { 0 }, { 997, 992, 13, 0 }, packTxn.getStagingHandle(), false };
			SIRResourcePublication packPub = { packRequest.keyResource, SIR_RESOURCE_KIND_BLOB, 2, 13, &packChunk, 1, nullptr, 0 };
			SIRResourceHandle packHandle = {};
			CHECK( packTxn.tryPublish( packPub, nullptr, 0, &packHandle ) == SIR_RESOURCE_RESULT_SUCCESS );
			{
				SIRResourceReadScope packScope( graph );
				SIRResourceView resource;
				CHECK( graph.resolve( packScope, packHandle, &resource ) == SIR_RESOURCE_RESULT_SUCCESS );
				SIRGRDGVectorReadRequest packRead = {
				    { 0 }, { 8, 8, 8, 13, SIR_GRDG_VECTOR_ENCODING_FIXED_BITS, 2, SIRGRDGVectorLayoutChecks::hostByteOrder(), 128, 0 }, 1 };
				SIRGRDGPackedIntegerView packedView;
				CHECK( SIRGRDGPackedIntegerView::tryResolve( packScope, resource, packRead, &packedView ) == SIR_RESOURCE_RESULT_SUCCESS );
				for ( uint32_t i = 0; i < 128; ++i ) {
					uint64_t nValue = UINT64_MAX;
					CHECK( packedView.tryReadAt( i, &nValue ) == SIR_RESOURCE_RESULT_SUCCESS );
					CHECK( nValue == ( uint64_t )i * 3 );
				}
				packRead.layout.nElementCount = 129;
				CHECK( SIRGRDGPackedIntegerView::tryResolve( packScope, resource, packRead, &packedView ) ==
				       SIR_RESOURCE_RESULT_INVALID_FORMAT );
			}
		}
		CHECK( packState.nLive == 0 );
	}
	CHECK( alloc.nLiveBytes == 0 );
	puts( "SIRGRDGVectorViewTests PASS" );
	return 0;
}
