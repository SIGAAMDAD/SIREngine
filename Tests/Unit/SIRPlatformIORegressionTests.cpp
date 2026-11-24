/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/Platform/Shared/Process/SIRChildProcess.h>
#include <Core/Platform/Shared/File/SIRPlatformFile.h>
#include <Core/Platform/Shared/File/SIRPlatformFileMapping.h>
#include <Core/Platform/Shared/File/SIRPlatformFileSystem.h>
#include <Core/Platform/Shared/Process/SIRPlatformPipe.h>
#include <Core/Platform/Shared/Threading/SIRPlatformThreadControl.h>
#include <Core/Platform/Shared/Threading/SIRSemaphore.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#if defined( __linux__ )
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#endif

#define SIR_CHECK( condition )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static constexpr const char *SIR_IO_FILE_PATH = "SIRPlatformIORegression.bin";
static constexpr const char *SIR_IO_DIRECTORY = "SIRPlatformIORegressionDirectory";
static constexpr uint32_t SIR_IO_WORKERS = 4;
static constexpr uint32_t SIR_IO_BLOCKS = 512;
static constexpr uint32_t SIR_IO_BLOCK_BYTES = 64;

static uint8_t sirIOExpectedByte( uint32_t iWorker, uint32_t iByte ) noexcept
{
	return ( uint8_t )( ( iWorker * 67 + iByte * 13 + ( iByte >> 3 ) ) & 255 );
}

static void sirTestPositionalIOAndMapping( void )
{
	( void )SIRPlatformFileSystem::removeFile( SIR_IO_FILE_PATH );

	SIRPlatformFile file;
	uint64_t nTransferred = UINT64_MAX;
	SIR_CHECK( file.readAt( 0, nullptr, 0, &nTransferred ) == sirFileResult::SIR_FILE_CLOSED && nTransferred == 0 );
	SIR_CHECK( file.open( SIR_IO_FILE_PATH, 1u << 23 ) == sirFileResult::SIR_FILE_INVALID_ARGUMENT );
	SIR_CHECK( file.open( SIR_IO_FILE_PATH, static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) | static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_APPEND ) | static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE ) ) ==
	           sirFileResult::SIR_FILE_INVALID_ARGUMENT );
	SIR_CHECK( file.open( SIR_IO_FILE_PATH, static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) | static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_READ ) | static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_CREATE ) | static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE ) ) ==
	           sirFileResult::SIR_FILE_OK );
	SIR_CHECK( file.readAt( 0, nullptr, 0, &nTransferred ) == sirFileResult::SIR_FILE_OK && nTransferred == 0 );
	SIR_CHECK( file.writeAt( 0, nullptr, 0, &nTransferred ) == sirFileResult::SIR_FILE_OK && nTransferred == 0 );

	std::atomic<uint32_t> nFinished{ 0 };
	std::thread szWriters[SIR_IO_WORKERS];
	for ( uint32_t iWorker = 0; iWorker < SIR_IO_WORKERS; ++iWorker ) {
		szWriters[iWorker] = std::thread( [&file, &nFinished, iWorker]() {
			uint8_t szBlock[SIR_IO_BLOCK_BYTES];
			for ( uint32_t iBlock = 0; iBlock < SIR_IO_BLOCKS; ++iBlock ) {
				const uint32_t iBase = iBlock * SIR_IO_BLOCK_BYTES;
				for ( uint32_t iByte = 0; iByte < SIR_IO_BLOCK_BYTES; ++iByte ) {
					szBlock[iByte] = sirIOExpectedByte( iWorker, iBase + iByte );
				}
				const uint64_t nOffset = ( ( uint64_t )iWorker * SIR_IO_BLOCKS + iBlock ) * SIR_IO_BLOCK_BYTES;
				uint64_t nWritten = 0;
				SIR_CHECK( file.writeAt( nOffset, szBlock, sizeof( szBlock ), &nWritten ) == sirFileResult::SIR_FILE_OK );
				SIR_CHECK( nWritten == sizeof( szBlock ) );
			}
			nFinished.fetch_add( 1, std::memory_order_relaxed );
		} );
	}
	for ( uint32_t iWorker = 0; iWorker < SIR_IO_WORKERS; ++iWorker ) {
		szWriters[iWorker].join();
	}
	SIR_CHECK( nFinished.load( std::memory_order_relaxed ) == SIR_IO_WORKERS );

	constexpr uint32_t N_TOTAL_BYTES = SIR_IO_WORKERS * SIR_IO_BLOCKS * SIR_IO_BLOCK_BYTES;
	std::vector<uint8_t> szContents( N_TOTAL_BYTES );
	SIR_CHECK( file.readAt( 0, szContents.data(), szContents.size(), &nTransferred ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( nTransferred == N_TOTAL_BYTES );
	for ( uint32_t iWorker = 0; iWorker < SIR_IO_WORKERS; ++iWorker ) {
		for ( uint32_t iByte = 0; iByte < SIR_IO_BLOCKS * SIR_IO_BLOCK_BYTES; ++iByte ) {
			const uint32_t iIndex = iWorker * SIR_IO_BLOCKS * SIR_IO_BLOCK_BYTES + iByte;
			SIR_CHECK( szContents[iIndex] == sirIOExpectedByte( iWorker, iByte ) );
		}
	}
	uint8_t szTail[128] = {};
	SIR_CHECK( file.readAt( N_TOTAL_BYTES - 17, szTail, sizeof( szTail ), &nTransferred ) == sirFileResult::SIR_FILE_END_OF_FILE );
	SIR_CHECK( nTransferred == 17 && memcmp( szTail, szContents.data() + N_TOTAL_BYTES - 17, 17 ) == 0 );
	SIR_CHECK( file.readAt( N_TOTAL_BYTES, szTail, 1, &nTransferred ) == sirFileResult::SIR_FILE_END_OF_FILE && nTransferred == 0 );
	SIR_CHECK( file.seek( INT64_MAX, sirFileSeekOrigin::SIR_FILE_SEEK_BEGIN, nullptr ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( file.seek( 1, sirFileSeekOrigin::SIR_FILE_SEEK_CURRENT, nullptr ) == sirFileResult::SIR_FILE_OUT_OF_RANGE );
	uint64_t nPosition = 0;
	SIR_CHECK( file.tell( &nPosition ) == sirFileResult::SIR_FILE_OK && nPosition == ( uint64_t )INT64_MAX );
	SIR_CHECK( file.seek( 0, sirFileSeekOrigin::SIR_FILE_SEEK_BEGIN, nullptr ) == sirFileResult::SIR_FILE_OK );

	SIRPlatformFileMapping mapping;
	SIR_CHECK( mapping.map( file, 4095, 97, sirFileMapMode::SIR_FILE_MAP_COPY_ON_WRITE ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( mapping.getSize() == 97 && mapping.getOffset() == 4095 );
	SIR_CHECK( memcmp( mapping.getData(), szContents.data() + 4095, 97 ) == 0 );
	SIR_CHECK( mapping.getWritableData() != nullptr );
	mapping.getWritableData()[0] ^= 0xFF;
	SIR_CHECK( mapping.flush() == sirFileResult::SIR_FILE_INVALID_ARGUMENT );
	SIR_CHECK( mapping.unmap() == sirFileResult::SIR_FILE_OK );
	uint8_t nOriginal = 0;
	SIR_CHECK( file.readAt( 4095, &nOriginal, 1, &nTransferred ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( nOriginal == szContents[4095] );

	SIR_CHECK( mapping.map( file, 4097, 123, sirFileMapMode::SIR_FILE_MAP_READ_WRITE ) == sirFileResult::SIR_FILE_OK );
	mapping.getWritableData()[7] = 0xB9;
	SIR_CHECK( mapping.flush() == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( mapping.unmap() == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( file.readAt( 4104, &nOriginal, 1, &nTransferred ) == sirFileResult::SIR_FILE_OK && nOriginal == 0xB9 );
	for ( uint32_t iIteration = 0; iIteration < 128; ++iIteration ) {
		const uint64_t nOffset = iIteration * 101;
		SIR_CHECK( mapping.map( file, nOffset, 37, sirFileMapMode::SIR_FILE_MAP_READ_ONLY ) == sirFileResult::SIR_FILE_OK );
		SIR_CHECK( mapping.getData()[0] == szContents[( size_t )nOffset] || nOffset == 4104 );
		SIR_CHECK( mapping.getWritableData() == nullptr );
		SIR_CHECK( mapping.unmap() == sirFileResult::SIR_FILE_OK );
	}
	SIR_CHECK( file.truncate( 31 ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( mapping.map( file, 31, 0, sirFileMapMode::SIR_FILE_MAP_READ_ONLY ) == sirFileResult::SIR_FILE_OUT_OF_RANGE );
	SIR_CHECK( file.flush() == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( file.close() == sirFileResult::SIR_FILE_OK );

	SIR_CHECK( file.open( SIR_IO_FILE_PATH, static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_READ ) ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( file.writeAt( 0, szTail, 1, &nTransferred ) == sirFileResult::SIR_FILE_ACCESS_DENIED && nTransferred == 0 );
	SIR_CHECK( file.truncate( 0 ) == sirFileResult::SIR_FILE_ACCESS_DENIED );
	SIR_CHECK( file.close() == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( SIRPlatformFileSystem::removeFile( SIR_IO_FILE_PATH ) == sirFileResult::SIR_FILE_OK );
}

static void sirTestPipeStream( void )
{
	SIRPlatformPipe pipe;
	SIR_CHECK( pipe.create() == sirPipeResult::SIR_PIPE_OK );
	SIR_CHECK( pipe.create() == sirPipeResult::SIR_PIPE_INVALID_ARGUMENT );

	constexpr uint32_t N_BYTES = 2 * 1024 * 1024 + 59;
	std::vector<uint8_t> szInput( N_BYTES );
	std::vector<uint8_t> szOutput( N_BYTES );
	for ( uint32_t iByte = 0; iByte < N_BYTES; ++iByte ) {
		szInput[iByte] = ( uint8_t )( ( iByte * 37 + ( iByte >> 5 ) ) & 255 );
	}
	std::atomic<bool> bProducerOK{ false };
	std::thread producer( [&]() {
		uint64_t nTransferred = 0;
		const sirPipeResult eResult = pipe.write( szInput.data(), N_BYTES, &nTransferred );
		bProducerOK.store( eResult == sirPipeResult::SIR_PIPE_OK && nTransferred == N_BYTES, std::memory_order_relaxed );
		SIR_CHECK( pipe.closeWrite() == sirPipeResult::SIR_PIPE_OK );
	} );
	uint64_t nConsumed = 0;
	while ( nConsumed < N_BYTES ) {
		uint64_t nTransferred = 0;
		const uint64_t nRequested = ( nConsumed % 12289 ) + 1;
		const uint64_t nRemaining = N_BYTES - nConsumed;
		const uint64_t nChunk = nRequested < nRemaining ? nRequested : nRemaining;
		const sirPipeResult eResult = pipe.read( szOutput.data() + nConsumed, nChunk, &nTransferred );
		SIR_CHECK( eResult == sirPipeResult::SIR_PIPE_OK && nTransferred != 0 && nTransferred <= nChunk );
		nConsumed += nTransferred;
	}
	producer.join();
	SIR_CHECK( bProducerOK.load( std::memory_order_relaxed ) );
	SIR_CHECK( memcmp( szInput.data(), szOutput.data(), N_BYTES ) == 0 );
	uint64_t nTransferred = UINT64_MAX;
	SIR_CHECK( pipe.read( szOutput.data(), 1, &nTransferred ) == sirPipeResult::SIR_PIPE_END_OF_STREAM && nTransferred == 0 );
	SIR_CHECK( pipe.close() == sirPipeResult::SIR_PIPE_OK );
	SIR_CHECK( pipe.create() == sirPipeResult::SIR_PIPE_OK );
	SIR_CHECK( pipe.closeRead() == sirPipeResult::SIR_PIPE_OK );
	SIR_CHECK( pipe.write( szInput.data(), 1, &nTransferred ) == sirPipeResult::SIR_PIPE_BROKEN && nTransferred == 0 );
	SIR_CHECK( pipe.close() == sirPipeResult::SIR_PIPE_OK );
}

static void sirTestSemaphoreContention( void )
{
	SIRSemaphore semaphore;
	SIR_CHECK( semaphore.create( 0 ) == sirSemaphoreResult::SIR_SEMAPHORE_OK );
	SIR_CHECK( semaphore.tryWait() == sirSemaphoreResult::SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_CHECK( semaphore.waitFor( 10 ) == sirSemaphoreResult::SIR_SEMAPHORE_TIMED_OUT );

	constexpr uint32_t N_PER_WORKER = 1024;
	std::thread szWorkers[SIR_IO_WORKERS];
	for ( uint32_t iWorker = 0; iWorker < SIR_IO_WORKERS; ++iWorker ) {
		szWorkers[iWorker] = std::thread( [&semaphore, N_PER_WORKER]() {
			for ( uint32_t iPermit = 0; iPermit < N_PER_WORKER; ++iPermit ) {
				SIR_CHECK( semaphore.post() == sirSemaphoreResult::SIR_SEMAPHORE_OK );
			}
		} );
	}
	for ( uint32_t iPermit = 0; iPermit < N_PER_WORKER * SIR_IO_WORKERS; ++iPermit ) {
		SIR_CHECK( semaphore.waitFor( 10000 ) == sirSemaphoreResult::SIR_SEMAPHORE_OK );
	}
	for ( uint32_t iWorker = 0; iWorker < SIR_IO_WORKERS; ++iWorker ) {
		szWorkers[iWorker].join();
	}
	SIR_CHECK( semaphore.tryWait() == sirSemaphoreResult::SIR_SEMAPHORE_WOULD_BLOCK );
	SIR_CHECK( semaphore.close() == sirSemaphoreResult::SIR_SEMAPHORE_OK );
	SIR_CHECK( semaphore.wait() == sirSemaphoreResult::SIR_SEMAPHORE_CLOSED );
	SIR_CHECK( semaphore.create( INT32_MAX ) == sirSemaphoreResult::SIR_SEMAPHORE_OK );
	SIR_CHECK( semaphore.post() == sirSemaphoreResult::SIR_SEMAPHORE_LIMIT_REACHED );
	SIR_CHECK( semaphore.close() == sirSemaphoreResult::SIR_SEMAPHORE_OK );
}

typedef struct sirIOEnumeration_s
{
	uint32_t nEntries;
	bool bStop;
} sirIOEnumeration_t;

static bool sirIOVisitEntry( const char *pszName, const sirFileSystemInfo_t *pInfo, void *pContext ) noexcept
{
	SIR_CHECK( pszName != nullptr && pszName[0] != '\0' );
	SIR_CHECK( pInfo != nullptr );
	sirIOEnumeration_t *pState = ( sirIOEnumeration_t * )pContext;
	++pState->nEntries;
	return !pState->bStop;
}

static void sirTestFileSystem( void )
{
	const std::string szRoot = SIR_IO_DIRECTORY;
	const std::string szMiddle = szRoot + "/middle";
	const std::string szLeaf = szMiddle + "/leaf";
	const std::string szSource = szLeaf + "/source.bin";
	const std::string szDestination = szLeaf + "/destination.bin";
	SIR_CHECK( SIRPlatformFileSystem::createDirectory( szLeaf.c_str(), true ) == sirFileSystemResult::SIR_FILESYSTEM_OK );

	SIRPlatformFile file;
	uint8_t nMarker = 0x5C;
	uint64_t nTransferred = 0;
	SIR_CHECK( file.open( szSource.c_str(), static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_CREATE ) | static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) | static_cast<uint32_t>( sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE ) ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( file.write( &nMarker, 1, &nTransferred ) == sirFileResult::SIR_FILE_OK && nTransferred == 1 );
	SIR_CHECK( file.close() == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( SIRPlatformFileSystem::renameReplace( szSource.c_str(), szDestination.c_str() ) == sirFileSystemResult::SIR_FILESYSTEM_OK );

	sirFileSystemInfo_t info = {};
	SIR_CHECK( SIRPlatformFileSystem::query( szDestination.c_str(), &info ) == sirFileSystemResult::SIR_FILESYSTEM_OK );
	SIR_CHECK( info.eType == sirFileSystemEntryType::SIR_FILESYSTEM_REGULAR_FILE && info.nSizeBytes == 1 );
	sirIOEnumeration_t enumeration = {};
	SIR_CHECK( SIRPlatformFileSystem::enumerate( szLeaf.c_str(), sirIOVisitEntry, &enumeration ) == sirFileSystemResult::SIR_FILESYSTEM_OK );
	SIR_CHECK( enumeration.nEntries == 1 );
	enumeration = { 0, true };
	SIR_CHECK( SIRPlatformFileSystem::enumerate( szLeaf.c_str(), sirIOVisitEntry, &enumeration ) == sirFileSystemResult::SIR_FILESYSTEM_STOPPED );
	SIR_CHECK( enumeration.nEntries == 1 );
	SIR_CHECK( SIRPlatformFileSystem::query( "", &info ) == sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT );

#if defined( __linux__ )
	struct timespec szTimes[2] = {};
	szTimes[0].tv_sec = -1;
	szTimes[0].tv_nsec = 0;
	szTimes[1].tv_sec = -1;
	szTimes[1].tv_nsec = 0;
	if ( utimensat( AT_FDCWD, szDestination.c_str(), szTimes, 0 ) == 0 ) {
		SIR_CHECK( SIRPlatformFileSystem::query( szDestination.c_str(), &info ) == sirFileSystemResult::SIR_FILESYSTEM_OK );
		SIR_CHECK( info.nModifiedNanoseconds == 0 );
	}
#endif

	SIR_CHECK( SIRPlatformFileSystem::removeFile( szDestination.c_str() ) == sirFileResult::SIR_FILE_OK );
	SIR_CHECK( SIRPlatformFileSystem::removeDirectory( szLeaf.c_str() ) == sirFileSystemResult::SIR_FILESYSTEM_OK );
	SIR_CHECK( SIRPlatformFileSystem::removeDirectory( szMiddle.c_str() ) == sirFileSystemResult::SIR_FILESYSTEM_OK );
	SIR_CHECK( SIRPlatformFileSystem::removeDirectory( szRoot.c_str() ) == sirFileSystemResult::SIR_FILESYSTEM_OK );
}

static void sirTestThreadControl( void )
{
	std::thread worker( []() {
		SIR_CHECK( SIRPlatformThreadControl::setCurrentName( "sir-io-regress" ) == sirThreadControlResult::SIR_THREAD_CONTROL_OK );
		char szName[64] = {};
		SIR_CHECK( SIRPlatformThreadControl::getCurrentName( szName, sizeof( szName ) ) == sirThreadControlResult::SIR_THREAD_CONTROL_OK );
		SIR_CHECK( strcmp( szName, "sir-io-regress" ) == 0 );
		SIR_CHECK( SIRPlatformThreadControl::setCurrentName( nullptr ) == sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT );
		SIR_CHECK( SIRPlatformThreadControl::getCurrentAffinity( nullptr ) == sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT );
		sirThreadAffinity_t affinity = {};
		const sirThreadControlResult eAffinity = SIRPlatformThreadControl::getCurrentAffinity( &affinity );
		if ( eAffinity == sirThreadControlResult::SIR_THREAD_CONTROL_OK ) {
			SIR_CHECK( affinity.nMask != 0 );
			SIR_CHECK( SIRPlatformThreadControl::setCurrentAffinity( affinity ) == sirThreadControlResult::SIR_THREAD_CONTROL_OK );
		} else {
			SIR_CHECK( eAffinity == sirThreadControlResult::SIR_THREAD_CONTROL_UNSUPPORTED );
		}
		SIR_CHECK( SIRPlatformThreadControl::setCurrentPriority( static_cast<sirThreadPriority>( 255 ) ) == sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT );
	} );
	worker.join();
}

static void sirTestChildProcess( const char *pszExecutable )
{
	SIRChildProcess process;
	SIR_CHECK( process.poll( nullptr, nullptr ) == sirChildProcessResult::SIR_CHILD_PROCESS_INVALID_ARGUMENT );
	SIR_CHECK( process.wait( nullptr ) == sirChildProcessResult::SIR_CHILD_PROCESS_CLOSED );
	const char *szExit[] = { "--sir-io-exit" };
	const sirChildProcessLaunch_t launch = { pszExecutable, szExit, 1, nullptr };
	for ( uint32_t iIteration = 0; iIteration < 12; ++iIteration ) {
		SIR_CHECK( process.start( launch ) == sirChildProcessResult::SIR_CHILD_PROCESS_OK );
		SIR_CHECK( process.start( launch ) == sirChildProcessResult::SIR_CHILD_PROCESS_ALREADY_OPEN );
		uint32_t nExitCode = 0;
		SIR_CHECK( process.waitFor( 10000, &nExitCode ) == sirChildProcessResult::SIR_CHILD_PROCESS_OK && nExitCode == 23 );
		bool bExited = false;
		SIR_CHECK( process.poll( &bExited, &nExitCode ) == sirChildProcessResult::SIR_CHILD_PROCESS_OK && bExited && nExitCode == 23 );
		SIR_CHECK( process.close() == sirChildProcessResult::SIR_CHILD_PROCESS_OK );
	}

	const char *szSleep[] = { "--sir-io-sleep" };
	const sirChildProcessLaunch_t sleeping = { pszExecutable, szSleep, 1, nullptr };
	SIR_CHECK( process.start( sleeping ) == sirChildProcessResult::SIR_CHILD_PROCESS_OK );
	SIR_CHECK( process.waitFor( 1, nullptr ) == sirChildProcessResult::SIR_CHILD_PROCESS_TIMED_OUT );
	SIR_CHECK( process.terminate() == sirChildProcessResult::SIR_CHILD_PROCESS_OK );
	SIR_CHECK( process.close() == sirChildProcessResult::SIR_CHILD_PROCESS_OK );
	SIR_CHECK( process.close() == sirChildProcessResult::SIR_CHILD_PROCESS_OK );
}

#if defined( __linux__ )
static uint32_t sirIOCountDescriptors( void )
{
	DIR *pDirectory = opendir( "/proc/self/fd" );
	SIR_CHECK( pDirectory != nullptr );
	uint32_t nCount = 0;
	while ( readdir( pDirectory ) != nullptr ) {
		++nCount;
	}
	SIR_CHECK( closedir( pDirectory ) == 0 );
	return nCount;
}
#endif

int main( int argc, char **argv )
{
	if ( argc > 1 ) {
		if ( strcmp( argv[1], "--sir-io-exit" ) == 0 ) {
			return 23;
		}
		if ( strcmp( argv[1], "--sir-io-sleep" ) == 0 ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
			return 31;
		}
	}
#if defined( __linux__ )
	const uint32_t nDescriptorsBefore = sirIOCountDescriptors();
#endif
	sirTestPositionalIOAndMapping();
	sirTestPipeStream();
	sirTestSemaphoreContention();
	sirTestFileSystem();
	sirTestThreadControl();

	const std::string szExecutable = std::filesystem::absolute( argv[0] ).string();
	sirTestChildProcess( szExecutable.c_str() );

#if defined( __linux__ )
	SIR_CHECK( sirIOCountDescriptors() == nDescriptorsBefore );
#endif
	return 0;
}
