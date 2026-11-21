/* SIREngine MPLv2 - vector specialization regression tests. */
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRVectorAppendWriter.h>
#include <Core/Containers/SIRInlineVector.h>
#include <Core/Containers/SIRInlineVectorAppendWriter.h>
#include <Core/Containers/SIRStaticVector.h>
#include <Core/Containers/SIRPagedArray.h>
#include <Core/Containers/SIRLockFreeVector.h>
#include <Core/Containers/SIRVirtualVector.h>
#include <Core/Containers/SIRPackedIntegerVectorBuilder.h>
#include <Core/Containers/SIRVectorAlgorithms.h>
#include <Core/Containers/SIRContainerArenaAdapter.h>
#include <Core/Containers/SIRAoSoAMultiVector.h>
#include <Core/Containers/SIRColony.h>
#include <vector>
#include <Core/Platform/SIRThread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>

#define CHECK( expr )                                                                                                                      \
	do {                                                                                                                                   \
		if ( !( expr ) ) {                                                                                                                 \
			fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #expr );                                                                      \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

struct TestAlloc
{
	uint64_t nLive = 0;
};

static void *alloc( void *pCtx, size_t nBytes, size_t nAlign )
{
	if ( nAlign < sizeof( void * ) ) {
		nAlign = sizeof( void * );
	}
	void *p = nullptr;
	if ( posix_memalign( &p, nAlign, nBytes ) != 0 ) {
		return nullptr;
	}
	( ( TestAlloc * )pCtx )->nLive += nBytes;
	return p;
}

static void dealloc( void *pCtx, void *p, size_t nBytes, size_t )
{
	( ( TestAlloc * )pCtx )->nLive -= nBytes;
	free( p );
}

struct TestExpandableHeap
{
	alignas( 64 ) uint8_t szStorage[2048] = {};
	bool bAllocated = false;
	uint32_t nAllocations = 0;
};
static void *expandAllocate( void *pContext, size_t nBytes, size_t nAlignment )
{
	TestExpandableHeap *pHeap = ( TestExpandableHeap * )pContext;
	if ( pHeap->bAllocated || nBytes > sizeof( pHeap->szStorage ) || nAlignment > 64 ) {
		return nullptr;
	}
	pHeap->bAllocated = true;
	++pHeap->nAllocations;
	return pHeap->szStorage;
}
static void expandDeallocate( void *pContext, void *pPointer, size_t, size_t )
{
	TestExpandableHeap *pHeap = ( TestExpandableHeap * )pContext;
	CHECK( pHeap->bAllocated && pPointer == pHeap->szStorage );
	pHeap->bAllocated = false;
}
static bool tryExpandHeap( void *pContext, void *pPointer, size_t nOld, size_t nNew, size_t, size_t *pActual ) noexcept
{
	TestExpandableHeap *pHeap = ( TestExpandableHeap * )pContext;
	if ( !pHeap->bAllocated || pPointer != pHeap->szStorage || nOld > nNew || nNew > sizeof( pHeap->szStorage ) ) {
		return false;
	}
	*pActual = sizeof( pHeap->szStorage );
	return true;
}
static bool getUsableHeapSize( void *pContext, const void *pPointer, size_t, size_t, size_t *pActual ) noexcept
{
	TestExpandableHeap *pHeap = ( TestExpandableHeap * )pContext;
	if ( !pHeap->bAllocated || pPointer != pHeap->szStorage ) {
		return false;
	}
	*pActual = sizeof( pHeap->szStorage );
	return true;
}

struct Producer
{
	SIRLockFreeVector<uint32_t> *pVector;
	uint32_t nStart;
	uint32_t nCount;
};

static void runProducer( void *p )
{
	Producer *pTask = ( Producer * )p;
	for ( uint32_t i = 0; i < pTask->nCount; ++i ) {
		uint32_t iIndex = UINT32_MAX;
		CHECK( pTask->pVector->tryEmplaceBack( &iIndex, pTask->nStart + i ) == SIR_CONTAINER_OK );
		CHECK( iIndex < pTask->pVector->getCapacity() );
	}
}

struct ThrowCopy
{
	static int nLive;
	static int nCopiesUntilThrow;
	uint32_t nValue;
	explicit ThrowCopy( uint32_t n = 0 ) noexcept : nValue( n )
	{
		++nLive;
	}
	ThrowCopy( const ThrowCopy& other ) : nValue( other.nValue )
	{
		if ( nCopiesUntilThrow == 0 ) {
			throw std::runtime_error( "injected copy failure" );
		}
		if ( nCopiesUntilThrow > 0 ) {
			--nCopiesUntilThrow;
		}
		++nLive;
	}
	ThrowCopy( ThrowCopy&& other ) noexcept : nValue( other.nValue )
	{
		++nLive;
	}
	ThrowCopy& operator=( const ThrowCopy& ) = default;
	~ThrowCopy( void ) noexcept
	{
		--nLive;
	}
};
int ThrowCopy::nLive = 0;
int ThrowCopy::nCopiesUntilThrow = -1;

int main( void )
{
	TestAlloc state;
	sirContainerAllocator_t allocator = { &state, alloc, dealloc, 0, 100 };
	CHECK( SIRVectorStorage::calculateGrowth( UINT32_MAX - 2, UINT32_MAX - 1, nullptr ) == SIR_CONTAINER_INVALID_ARGUMENT );
	uint32_t nCapacity = 0;
	CHECK( SIRVectorStorage::calculateGrowth( UINT32_MAX - 2, UINT32_MAX - 1, &nCapacity ) == SIR_CONTAINER_OK );
	CHECK( nCapacity == UINT32_MAX );
	CHECK( SIRVectorStorage::calculateGrowthForPolicy( 16, 17, SIR_CONTAINER_GROWTH_DOUBLE, &nCapacity ) == SIR_CONTAINER_OK );
	CHECK( nCapacity == 32 );
	CHECK( SIRVectorStorage::calculateGrowthForPolicy( 16, 17, SIR_CONTAINER_GROWTH_EXACT, &nCapacity ) == SIR_CONTAINER_OK );
	CHECK( nCapacity == 17 );
	{
		SIRVector<uint32_t> vec( &allocator );
		uint32_t source[] = { 1, 2, 3, 4 };
		CHECK( vec.tryAppendRange( source, 4 ) == SIR_CONTAINER_OK );
		CHECK( vec.getReadOnlyView().getSize() == 4 );
		CHECK( vec.getWriteOnlyView().tryWrite( 1, 17 ) == SIR_CONTAINER_OK );
		CHECK( vec[1] == 17 );
		CHECK( vec.tryAppendRange( vec.data(), 2 ) == SIR_CONTAINER_INVALID_ARGUMENT );
		CHECK( vec.tryEraseRange( 1, 2 ) == SIR_CONTAINER_OK );
		CHECK( vec.getSize() == 2 && vec[0] == 1 && vec[1] == 4 );
		{
			SIRVectorAppendWriter<uint32_t> writer( vec );
			CHECK( writer.tryBeginAppend( 3 ) == SIR_CONTAINER_OK );
			CHECK( writer.tryEmplaceNext( 5 ) == SIR_CONTAINER_OK );
			CHECK( writer.tryEmplaceNext( 6 ) == SIR_CONTAINER_OK );
			CHECK( writer.commit() == SIR_CONTAINER_BUSY );
		}
		CHECK( vec.getSize() == 2 );
		{
			SIRVectorAppendWriter<uint32_t> writer( vec );
			CHECK( writer.tryBeginAppend( 2 ) == SIR_CONTAINER_OK );
			CHECK( writer.tryEmplaceNext( 8 ) == SIR_CONTAINER_OK );
			CHECK( writer.tryEmplaceNext( 9 ) == SIR_CONTAINER_OK );
			CHECK( writer.commit() == SIR_CONTAINER_OK );
		}
		CHECK( vec.getSize() == 4 && vec[2] == 8 && vec[3] == 9 );
	}
	{
		SIRVector<uint32_t> vec( &allocator );
		uint32_t values[] = { 1, 2, 3, 4 };
		CHECK( vec.tryAssignRange( values, 4 ) == SIR_CONTAINER_OK );
		CHECK( vec.tryInsertRange( 2, vec.data(), 2 ) == SIR_CONTAINER_OK );
		CHECK( vec.getSize() == 6 && vec[2] == 1 && vec[3] == 2 && vec[4] == 3 );
		CHECK( vec.tryAssignRange( vec.data() + 1, 3 ) == SIR_CONTAINER_OK );
		CHECK( vec.getSize() == 3 && vec[0] == 2 && vec[1] == 1 && vec[2] == 2 );
		CHECK( vec.tryInsertRange( 4, values, 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
		CHECK( vec.tryInsertRange( 0, nullptr, 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
		CHECK( vec.tryAssignRange( nullptr, 0 ) == SIR_CONTAINER_OK && vec.isEmpty() );
	}
	{
		TestExpandableHeap heap;
		sirContainerAllocator_t alloc = { &heap, expandAllocate, expandDeallocate, 0, 101 };
		sirContainerAllocatorCapabilities_t capabilities = { &heap, tryExpandHeap, getUsableHeapSize };
		{
			SIRVector<uint64_t> values( &alloc );
			CHECK( values.tryReserve( 4 ) == SIR_CONTAINER_OK );
			CHECK( values.tryEmplaceBack( 13 ) == SIR_CONTAINER_OK );
			const uint64_t *pOriginal = values.data();
			CHECK( values.tryReserveExtended( 128, &capabilities ) == SIR_CONTAINER_OK );
			CHECK( values.data() == pOriginal && values.getCapacity() == 128 );
			CHECK( values[0] == 13 && heap.nAllocations == 1 );
			CHECK( values.tryReserveExtended( 1024, &capabilities ) == SIR_CONTAINER_OUT_OF_MEMORY );
			CHECK( values.data() == pOriginal && values[0] == 13 );
		}
		CHECK( !heap.bAllocated );
	}
	{
		SIRVector<ThrowCopy> vec( &allocator );
		CHECK( vec.tryEmplaceBack( 31 ) == SIR_CONTAINER_OK );
		CHECK( vec.tryEmplaceBack( 32 ) == SIR_CONTAINER_OK );
		ThrowCopy value( 44 );
		const int nOldLive = ThrowCopy::nLive;
		ThrowCopy::nCopiesUntilThrow = 1;
		bool caught = false;
		try {
			vec.tryInsertRange( 1, &value, 1 );
		} catch ( const std::runtime_error& ) {
			caught = true;
		}
		CHECK( caught && ThrowCopy::nLive == nOldLive );
		CHECK( vec.getSize() == 2 && vec[0].nValue == 31 && vec[1].nValue == 32 );
		ThrowCopy::nCopiesUntilThrow = -1;
		CHECK( vec.tryInsertRange( 1, &value, 1 ) == SIR_CONTAINER_OK );
		CHECK( vec.getSize() == 3 && vec[1].nValue == 44 );
	}
	CHECK( ThrowCopy::nLive == 0 );
	{
		SIRPagedArray<uint32_t, 3> pages( &allocator );
		for ( uint32_t i = 0; i < 11; ++i ) {
			CHECK( pages.tryEmplaceBack( i ) == SIR_CONTAINER_OK );
		}
		CHECK( pages.getPageView( 3 ).getSize() == 2 );
		CHECK( pages.getPageView( 2 )[0] == 6 );
	}
	{
		SIRInlineVector<uint32_t, 4> inlineVec( &allocator );
		CHECK( inlineVec.getReadOnlyView().isEmpty() );
		CHECK( inlineVec.tryEmplaceBack( 11 ) == SIR_CONTAINER_OK );
		CHECK( inlineVec.getWriteOnlyView().tryWrite( 0, 12 ) == SIR_CONTAINER_OK );
		CHECK( inlineVec[0] == 12 );
		{
			SIRInlineVectorAppendWriter<uint32_t, 4> writer( inlineVec );
			CHECK( writer.tryBeginAppend( 5 ) == SIR_CONTAINER_OK );
			CHECK( writer.tryEmplaceNext( 13 ) == SIR_CONTAINER_OK );
			CHECK( writer.commit() == SIR_CONTAINER_BUSY );
		}
		CHECK( inlineVec.getSize() == 1 && inlineVec[0] == 12 );
		{
			SIRInlineVectorAppendWriter<uint32_t, 4> writer( inlineVec );
			CHECK( writer.tryBeginAppend( 2 ) == SIR_CONTAINER_OK );
			CHECK( writer.tryEmplaceNext( 13 ) == SIR_CONTAINER_OK );
			CHECK( writer.tryEmplaceNext( 14 ) == SIR_CONTAINER_OK );
			CHECK( writer.commit() == SIR_CONTAINER_OK );
		}
		CHECK( inlineVec.getSize() == 3 && inlineVec[2] == 14 );
		uint32_t nExtra[] = { 31, 32, 33 };
		CHECK( inlineVec.tryAppendRange( nExtra, 3 ) == SIR_CONTAINER_OK );
		CHECK( inlineVec.getSize() == 6 && inlineVec[5] == 33 );
		CHECK( inlineVec.tryAppendRange( inlineVec.data(), 1 ) == SIR_CONTAINER_INVALID_ARGUMENT );
		CHECK( inlineVec.tryEraseRange( 1, 3 ) == SIR_CONTAINER_OK );
		CHECK( inlineVec.getSize() == 3 && inlineVec[1] == 32 && inlineVec[2] == 33 );
	}
	{
		SIRGenerationColony<uint32_t, 4> colony( &allocator );
		SIRGenerationColony<uint32_t, 4>::Handle oldHandle;
		CHECK( colony.tryEmplaceHandle( &oldHandle, 21 ) == SIR_CONTAINER_OK );
		CHECK( *colony.tryResolveHandle( oldHandle ) == 21 );
		CHECK( colony.eraseHandle( oldHandle ) );
		CHECK( colony.tryResolveHandle( oldHandle ) == nullptr );
		SIRGenerationColony<uint32_t, 4>::Handle freshHandle;
		CHECK( colony.tryEmplaceHandle( &freshHandle, 22 ) == SIR_CONTAINER_OK );
		CHECK( freshHandle.pValue == oldHandle.pValue );
		CHECK( freshHandle.nGeneration != oldHandle.nGeneration );
		CHECK( !colony.eraseHandle( oldHandle ) );
		CHECK( colony.eraseHandle( freshHandle ) );
	}
	{
		SIRAoSoAMultiVector<8, uint32_t, float, uint64_t> blocks( &allocator );
		for ( uint32_t i = 0; i < 19; ++i ) {
			const uint32_t a = i;
			const float b = float( i ) + 0.5f;
			const uint64_t c = i * 10;
			CHECK( blocks.tryEmplaceBack( a, b, c ) == SIR_CONTAINER_OK );
		}
		CHECK( blocks.getBlockCount() == 3 && blocks.getSize() == 19 );
		CHECK( *blocks.tryGet<0>( 18 ) == 18 && *blocks.tryGet<1>( 17 ) == 17.5f );
		CHECK( *blocks.tryGet<2>( 17 ) == 170 );
		CHECK( blocks.getBlockColumnView<0>( 2 ).getSize() == 3 );
		CHECK( blocks.getBlockColumnView<1>( 0 ).getSize() == 8 );
		CHECK( blocks.tryGet<0>( 19 ) == nullptr );
	}
	{
		SIRVirtualVector<uint32_t> v;
		CHECK( v.tryInitialize( 10000 ) == SIR_CONTAINER_OK );
		uint32_t *pStart = v.data();
		for ( uint32_t i = 0; i < 8193; ++i ) {
			CHECK( v.tryEmplaceBack( i ) == SIR_CONTAINER_OK );
		}
		CHECK( v.data() == pStart && v.getSize() == 8193 && v.data()[8192] == 8192 );
		for ( uint32_t i = 0; i < 8193; ++i ) {
			CHECK( v.tryPopBack() == SIR_CONTAINER_OK );
		}
		CHECK( v.tryDecommitTail() );
		CHECK( v.getCommittedBytes() == 0 );
	}
	{
		SIRLockFreeVector<uint32_t> concurrent( &allocator );
		CHECK( concurrent.tryInitialize( 2048 ) == SIR_CONTAINER_OK );
		Producer tasks[4];
		SIRThread workers[4];
		for ( uint32_t i = 0; i < 4; ++i ) {
			tasks[i] = { &concurrent, i * 512, 512 };
			CHECK( workers[i].start( runProducer, &tasks[i] ) );
		}
		for ( uint32_t i = 0; i < 4; ++i ) {
			CHECK( workers[i].join() );
		}
		CHECK( concurrent.getClaimedCount() == 2048 && concurrent.getPublishedCount() == 2048 );
		CHECK( concurrent.trySeal() == SIR_CONTAINER_OK );
		SIRReadOnlyVectorView<uint32_t> read;
		CHECK( concurrent.tryGetSealedView( &read ) == SIR_CONTAINER_OK );
		CHECK( read.getSize() == 2048 );
		uint32_t count[2048] = {};
		for ( uint32_t i = 0; i < 2048; ++i ) {
			CHECK( read[i] < 2048 );
			++count[read[i]];
		}
		for ( uint32_t i = 0; i < 2048; ++i ) {
			CHECK( count[i] == 1 );
		}
		uint32_t iIndex = 0;
		CHECK( concurrent.tryEmplaceBack( &iIndex, uint32_t( 1 ) ) == SIR_CONTAINER_CLOSED );
		concurrent.clearAfterQuiescence();
		CHECK( concurrent.getClaimedCount() == 0 );
	}
	{
		SIRPackedIntegerVectorBuilder builder( &allocator );
		for ( uint32_t i = 0; i < 1500; ++i ) {
			CHECK( builder.tryAppend( ( uint64_t )( i * 7 ) & 0x1fff ) == SIR_CONTAINER_OK );
		}
		SIRPackedIntegerVector packed( &allocator );
		CHECK( builder.tryBuild( 13, &packed ) == SIR_CONTAINER_OK );
		CHECK( packed.isSealed() );
		std::vector<uint8_t> bytes( ( size_t )packed.getEncodedBytes() );
		CHECK( packed.tryEncode( bytes.data(), bytes.size() ) == SIR_CONTAINER_OK );
		SIRPackedIntegerReadView reader;
		CHECK( SIRPackedIntegerReadView::tryDecode( bytes.data(), bytes.size(), &reader ) == SIR_CONTAINER_OK );
		CHECK( reader.getSize() == 1500 );
		for ( uint32_t i = 0; i < 1500; ++i ) {
			uint64_t original = UINT64_MAX, decoded = UINT64_MAX;
			CHECK( packed.tryGet( i, &original ) && reader.tryGet( i, &decoded ) );
			CHECK( original == decoded && decoded == ( ( i * 7 ) & 0x1fff ) );
		}
		bytes[4] = 7;
		CHECK( SIRPackedIntegerReadView::tryDecode( bytes.data(), bytes.size(), &reader ) == SIR_CONTAINER_INVALID_ARGUMENT );
	}
	{
		SIRVector<uint32_t> output( &allocator );
		uint32_t numbers[] = { 10, 20, 30, 40, 50, 60, 70, 80 };
		uint8_t flags[] = { 1, 0, 1, 0, 1, 1, 0, 1 };
		CHECK( SIRVectorAlgorithms::tryAppendFiltered<uint32_t>( { numbers, 8 }, flags, &output ) == SIR_CONTAINER_OK );
		CHECK( output.getSize() == 5 && output[0] == 10 && output[4] == 80 );
		SIRReadOnlyVectorView<uint32_t> shards[] = { { numbers, 2 }, { numbers + 4, 4 } };
		CHECK( SIRVectorAlgorithms::tryMergeShards<uint32_t>( shards, 2, &output ) == SIR_CONTAINER_OK );
		CHECK( output.getSize() == 11 && output[5] == 10 && output[7] == 50 );
	}
	CHECK( state.nLive == 0 );
	puts( "SIRVectorSpecializationTests PASS" );
	return 0;
}
