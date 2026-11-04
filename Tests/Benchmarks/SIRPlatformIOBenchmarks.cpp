/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/Platform/SIRPlatformFile.h>
#include <Core/Platform/SIRPlatformFileMapping.h>
#include <Core/Platform/SIRPlatformFileSystem.h>
#include <Core/Platform/SIRPlatformPipe.h>
#include <Core/Platform/SIRSemaphore.h>

#include <benchmark/benchmark.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

static constexpr const char *SIR_PLATFORM_IO_BENCH_PATH = "SIRPlatformIOBenchmarks.bin";
static constexpr uint32_t SIR_PLATFORM_IO_BENCH_BYTES = 2 * 1024 * 1024;

class SIRPlatformIOBenchmarkFixture final
{
private:
	SIRPlatformFile m_File;
	std::vector<uint8_t> m_szExpected;

public:
	bool init( void )
	{
		( void )SIRPlatformFile::remove( SIR_PLATFORM_IO_BENCH_PATH );
		m_szExpected.resize( SIR_PLATFORM_IO_BENCH_BYTES );
		for ( uint32_t iByte = 0; iByte < SIR_PLATFORM_IO_BENCH_BYTES; ++iByte ) {
			m_szExpected[iByte] = ( uint8_t )( ( iByte * 17 + ( iByte >> 6 ) ) & 255 );
		}
		const uint32_t nFlags = SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE | SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE;
		if ( m_File.open( SIR_PLATFORM_IO_BENCH_PATH, nFlags ) != SIR_FILE_OK ) {
			return false;
		}
		uint64_t nTransferred = 0;
		return m_File.writeAt( 0, m_szExpected.data(), m_szExpected.size(), &nTransferred ) == SIR_FILE_OK &&
		       nTransferred == m_szExpected.size();
	}

	~SIRPlatformIOBenchmarkFixture( void )
	{
		( void )m_File.close();
		( void )SIRPlatformFile::remove( SIR_PLATFORM_IO_BENCH_PATH );
	}

	SIRPlatformFile &getFile( void ) noexcept
	{
		return m_File;
	}

	const uint8_t *getExpected( void ) const noexcept
	{
		return m_szExpected.data();
	}
};

static void SIRBenchmarkPlatformReadAt( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "readAt fixture init failed" );
		return;
	}
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	const uint64_t nOffset = ( uint64_t )state.range( 1 );
	std::vector<uint8_t> szOutput( ( size_t )nBytes );
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		uint64_t nTransferred = 0;
		if ( fixture.getFile().readAt( nOffset, szOutput.data(), nBytes, &nTransferred ) != SIR_FILE_OK || nTransferred != nBytes ) {
			state.SkipWithError( "readAt failed" );
			bFailed = true;
			break;
		}
		benchmark::DoNotOptimize( szOutput.data() );
	}
	if ( !bFailed ) {
		if ( memcmp( szOutput.data(), fixture.getExpected() + nOffset, ( size_t )nBytes ) != 0 ) {
			state.SkipWithError( "readAt payload mismatch" );
		}
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
}

static void SIRBenchmarkPlatformWriteAt( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "writeAt fixture init failed" );
		return;
	}
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	const uint64_t nOffset = 37;
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		uint64_t nTransferred = 0;
		if ( fixture.getFile().writeAt( nOffset, fixture.getExpected(), nBytes, &nTransferred ) != SIR_FILE_OK ||
		     nTransferred != nBytes ) {
			state.SkipWithError( "writeAt failed" );
			bFailed = true;
			break;
		}
		benchmark::ClobberMemory();
	}
	if ( !bFailed ) {
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
}

static void SIRBenchmarkPlatformSequentialReadSeek( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "sequential read fixture init failed" );
		return;
	}
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	std::vector<uint8_t> szOutput( ( size_t )nBytes );
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		uint64_t nTransferred = 0;
		if ( fixture.getFile().seek( 0, SIR_FILE_SEEK_BEGIN, nullptr ) != SIR_FILE_OK ||
		     fixture.getFile().read( szOutput.data(), nBytes, &nTransferred ) != SIR_FILE_OK || nTransferred != nBytes ) {
			state.SkipWithError( "sequential seek/read failed" );
			bFailed = true;
			break;
		}
		benchmark::DoNotOptimize( szOutput.data() );
	}
	if ( !bFailed ) {
		if ( memcmp( szOutput.data(), fixture.getExpected(), ( size_t )nBytes ) != 0 ) {
			state.SkipWithError( "sequential read payload mismatch" );
		}
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
}

static void SIRBenchmarkPlatformOpenClose( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "open/close fixture init failed" );
		return;
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		SIRPlatformFile file;
		if ( file.open( SIR_PLATFORM_IO_BENCH_PATH, SIR_FILE_OPEN_READ ) != SIR_FILE_OK || file.close() != SIR_FILE_OK ) {
			state.SkipWithError( "file open/close failed" );
			bFailed = true;
			break;
		}
	}
	if ( !bFailed ) {
		state.SetItemsProcessed( state.iterations() );
	}
}

static void SIRBenchmarkPlatformMapAndCopy( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "map/copy fixture init failed" );
		return;
	}
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	std::vector<uint8_t> szOutput( ( size_t )nBytes );
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		SIRPlatformFileMapping mapping;
		if ( mapping.map( fixture.getFile(), 37, nBytes, SIR_FILE_MAP_READ_ONLY ) != SIR_FILE_OK ) {
			state.SkipWithError( "map failed" );
			bFailed = true;
			break;
		}
		memcpy( szOutput.data(), mapping.getData(), ( size_t )nBytes );
		benchmark::DoNotOptimize( szOutput.data() );
		if ( mapping.unmap() != SIR_FILE_OK ) {
			state.SkipWithError( "unmap failed" );
			bFailed = true;
			break;
		}
	}
	if ( !bFailed ) {
		if ( memcmp( szOutput.data(), fixture.getExpected() + 37, ( size_t )nBytes ) != 0 ) {
			state.SkipWithError( "map/copy payload mismatch" );
		}
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
}

static void SIRBenchmarkPlatformMappedReuseCopy( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "mapping reuse fixture init failed" );
		return;
	}
	SIRPlatformFileMapping mapping;
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	if ( mapping.map( fixture.getFile(), 37, nBytes, SIR_FILE_MAP_READ_ONLY ) != SIR_FILE_OK ) {
		state.SkipWithError( "persistent map failed" );
		return;
	}
	std::vector<uint8_t> szOutput( ( size_t )nBytes );
	for ( auto _ : state ) {
		( void )_;
		memcpy( szOutput.data(), mapping.getData(), ( size_t )nBytes );
		benchmark::DoNotOptimize( szOutput.data() );
	}
	if ( memcmp( szOutput.data(), fixture.getExpected() + 37, ( size_t )nBytes ) != 0 ) {
		state.SkipWithError( "persistent mapping payload mismatch" );
	}
	state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	if ( mapping.unmap() != SIR_FILE_OK ) {
		state.SkipWithError( "persistent mapping unmap failed" );
	}
}

static void SIRBenchmarkPlatformFileInfo( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "metadata fixture init failed" );
		return;
	}
	for ( auto _ : state ) {
		( void )_;
		sirFileInfo_t info = {};
		if ( SIRPlatformFile::getInfo( SIR_PLATFORM_IO_BENCH_PATH, &info ) != SIR_FILE_OK ||
		     info.nSizeBytes != SIR_PLATFORM_IO_BENCH_BYTES ) {
			state.SkipWithError( "file metadata query failed" );
			break;
		}
		benchmark::DoNotOptimize( info.nSizeBytes );
	}
	state.SetItemsProcessed( state.iterations() );
}

static void SIRBenchmarkPlatformFileSystemQuery( benchmark::State &state )
{
	SIRPlatformIOBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "filesystem query fixture init failed" );
		return;
	}
	for ( auto _ : state ) {
		( void )_;
		sirFileSystemInfo_t info = {};
		if ( SIRPlatformFileSystem::query( SIR_PLATFORM_IO_BENCH_PATH, &info ) != SIR_FILESYSTEM_OK ||
		     info.nSizeBytes != SIR_PLATFORM_IO_BENCH_BYTES ) {
			state.SkipWithError( "filesystem metadata query failed" );
			break;
		}
		benchmark::DoNotOptimize( info.nSizeBytes );
	}
	state.SetItemsProcessed( state.iterations() );
}

static void SIRBenchmarkPlatformSemaphoreUncontended( benchmark::State &state )
{
	SIRSemaphore semaphore;
	if ( semaphore.create( 0 ) != SIR_SEMAPHORE_OK ) {
		state.SkipWithError( "semaphore creation failed" );
		return;
	}
	for ( auto _ : state ) {
		( void )_;
		if ( semaphore.post() != SIR_SEMAPHORE_OK || semaphore.tryWait() != SIR_SEMAPHORE_OK ) {
			state.SkipWithError( "semaphore post/tryWait failed" );
			break;
		}
	}
	state.SetItemsProcessed( state.iterations() );
}

static void SIRBenchmarkPlatformSemaphoreHandoff( benchmark::State &state )
{
	SIRSemaphore request;
	SIRSemaphore response;
	if ( request.create() != SIR_SEMAPHORE_OK || response.create() != SIR_SEMAPHORE_OK ) {
		state.SkipWithError( "semaphore handoff initialization failed" );
		return;
	}
	std::atomic<bool> bStop{ false };
	std::atomic<bool> bWorkerFailed{ false };
	std::thread worker( [&]() {
		for ( ;; ) {
			if ( request.wait() != SIR_SEMAPHORE_OK ) {
				bWorkerFailed.store( true, std::memory_order_relaxed );
				return;
			}
			if ( bStop.load( std::memory_order_acquire ) ) {
				return;
			}
			if ( response.post() != SIR_SEMAPHORE_OK ) {
				bWorkerFailed.store( true, std::memory_order_relaxed );
				return;
			}
		}
	} );
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		if ( request.post() != SIR_SEMAPHORE_OK || response.waitFor( 10000 ) != SIR_SEMAPHORE_OK ) {
			state.SkipWithError( "semaphore handoff failed" );
			bFailed = true;
			break;
		}
	}
	bStop.store( true, std::memory_order_release );
	( void )request.post();
	worker.join();
	if ( bWorkerFailed.load( std::memory_order_relaxed ) ) {
		state.SkipWithError( "semaphore worker failed" );
	}
	if ( !bFailed ) {
		state.SetItemsProcessed( state.iterations() );
	}
}

static void SIRBenchmarkPlatformPipeLifecycleTransfer( benchmark::State &state )
{
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	std::vector<uint8_t> szInput( ( size_t )nBytes );
	std::vector<uint8_t> szOutput( ( size_t )nBytes );
	for ( uint64_t iByte = 0; iByte < nBytes; ++iByte ) {
		szInput[( size_t )iByte] = ( uint8_t )( iByte * 29 + ( iByte >> 6 ) );
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		SIRPlatformPipe pipe;
		if ( pipe.create() != SIR_PIPE_OK ) {
			state.SkipWithError( "pipe create failed" );
			bFailed = true;
			break;
		}
		bool bProducerOK = false;
		std::thread producer( [&]() {
			uint64_t nTransferred = 0;
			bProducerOK = pipe.write( szInput.data(), nBytes, &nTransferred ) == SIR_PIPE_OK && nTransferred == nBytes;
			( void )pipe.closeWrite();
		} );
		uint64_t nConsumed = 0;
		while ( nConsumed < nBytes ) {
			uint64_t nTransferred = 0;
			const sirPipeResult_t eResult = pipe.read( szOutput.data() + nConsumed, nBytes - nConsumed, &nTransferred );
			if ( eResult != SIR_PIPE_OK || nTransferred == 0 ) {
				bFailed = true;
				break;
			}
			nConsumed += nTransferred;
		}
		producer.join();
		( void )pipe.close();
		if ( bFailed || !bProducerOK || nConsumed != nBytes || memcmp( szOutput.data(), szInput.data(), ( size_t )nBytes ) != 0 ) {
			state.SkipWithError( "pipe transfer integrity failure" );
			bFailed = true;
			break;
		}
		benchmark::DoNotOptimize( szOutput.data() );
	}
	if ( !bFailed ) {
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
}

// All timings are warm-cache and exclude fixture construction. Pipe lifecycle
// includes thread creation by design; "reuse" excludes map/unmap costs.
BENCHMARK( SIRBenchmarkPlatformReadAt )
    ->Args( { 64, 0 } )
    ->Args( { 4096, 0 } )
    ->Args( { 4096, 37 } )
    ->Args( { 65536, 37 } )
    ->Args( { 1048576, 37 } );
BENCHMARK( SIRBenchmarkPlatformWriteAt )->Arg( 64 )->Arg( 4096 )->Arg( 65536 );
BENCHMARK( SIRBenchmarkPlatformSequentialReadSeek )->Arg( 4096 )->Arg( 65536 );
BENCHMARK( SIRBenchmarkPlatformOpenClose );
BENCHMARK( SIRBenchmarkPlatformMapAndCopy )->Arg( 4096 )->Arg( 65536 )->Arg( 1048576 );
BENCHMARK( SIRBenchmarkPlatformMappedReuseCopy )->Arg( 4096 )->Arg( 65536 )->Arg( 1048576 );
BENCHMARK( SIRBenchmarkPlatformFileInfo );
BENCHMARK( SIRBenchmarkPlatformFileSystemQuery );
BENCHMARK( SIRBenchmarkPlatformSemaphoreUncontended );
BENCHMARK( SIRBenchmarkPlatformSemaphoreHandoff );
BENCHMARK( SIRBenchmarkPlatformPipeLifecycleTransfer )->Arg( 4096 )->Arg( 65536 );
