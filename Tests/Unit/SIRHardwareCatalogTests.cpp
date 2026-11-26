/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/Hardware/SIRHardwareCatalog.h>
#include <Core/Platform/Shared/Storage/SIRPlatformStorageQuery.h>

#if defined( __linux__ )
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#endif

#define SIR_REQUIRE( condition ) \
	do { \
		if ( !( condition ) ) { \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition ); \
			abort(); \
		} \
	} while ( 0 )

static_assert( std::is_trivial_v<sirHardwareSnapshot_t> && std::is_standard_layout_v<sirHardwareSnapshot_t> );
static_assert( std::is_trivial_v<sirStorageDescriptor_t> && std::is_standard_layout_v<sirStorageDescriptor_t> );

class SIRHardwareCatalogTests final
{
public:
	explicit SIRHardwareCatalogTests( void ) = delete;
	explicit SIRHardwareCatalogTests( const SIRHardwareCatalogTests& ) = delete;
	explicit SIRHardwareCatalogTests( SIRHardwareCatalogTests&& ) = delete;
	~SIRHardwareCatalogTests() = delete;

	SIRHardwareCatalogTests& operator=( const SIRHardwareCatalogTests& ) = delete;
	SIRHardwareCatalogTests& operator=( SIRHardwareCatalogTests&& ) = delete;

	static void testCatalog( void );
#if defined( __linux__ )
	static void testStorage( void );
#endif
};

void SIRHardwareCatalogTests::testCatalog( void )
{
	SIRHardwareCatalog catalog;
	sirHardwareSnapshot_t first{};
	first.nGeneration = 99;
	SIR_REQUIRE( catalog.copySnapshot( &first ) == sirHardwareResult::SIR_HARDWARE_INVALID );
	SIR_REQUIRE( first.nGeneration == 0 );
	SIR_REQUIRE( catalog.refresh() == sirHardwareResult::SIR_HARDWARE_INVALID );

	sirCPUInfo_t cpu{};
	sirSystemMemoryStats_t memory{};
	SIR_REQUIRE( SIRCPUInfo::query( &cpu ) );
	SIR_REQUIRE( SIRPlatformMemory::querySystem( &memory ) );
	const sirHardwareResult eInitial = catalog.initialize();
	SIR_REQUIRE( eInitial == sirHardwareResult::SIR_HARDWARE_OK || eInitial == sirHardwareResult::SIR_HARDWARE_PARTIAL );
	SIR_REQUIRE( catalog.initialize() == sirHardwareResult::SIR_HARDWARE_INVALID );
	SIR_REQUIRE( catalog.copySnapshot( &first ) == eInitial );
	SIR_REQUIRE( first.nGeneration != 0 );
	SIR_REQUIRE( first.cpu.info.nLogicalProcessorCount == cpu.nLogicalProcessorCount );
	SIR_REQUIRE( first.cpu.info.nFeatures == cpu.nFeatures );
	SIR_REQUIRE( first.cpu.nValidFields & static_cast<uint32_t>( sirCPUDescriptorField::SIR_CPU_DESCRIPTOR_LOGICAL_COUNT ) );
	SIR_REQUIRE( ( ( first.cpu.nValidFields & static_cast<uint32_t>( sirCPUDescriptorField::SIR_CPU_DESCRIPTOR_PHYSICAL_COUNT ) ) != 0 ) ==
	             ( cpu.nPhysicalCoreCount != 0 ) );
	SIR_REQUIRE( first.memory.system.nTotalPhysicalBytes == memory.nTotalPhysicalBytes );
	SIR_REQUIRE( first.memory.nValidFields & static_cast<uint32_t>( sirMemoryDescriptorField::SIR_MEMORY_DESCRIPTOR_TOTAL_PHYSICAL ) );

	const sirStorageQuery_t oldQuery{ ".", first.nGeneration };
	const sirHardwareResult eRefresh = catalog.refresh();
	sirHardwareSnapshot_t second{};
	SIR_REQUIRE( eRefresh == sirHardwareResult::SIR_HARDWARE_OK || eRefresh == sirHardwareResult::SIR_HARDWARE_PARTIAL );
	SIR_REQUIRE( catalog.copySnapshot( &second ) == eRefresh );
	SIR_REQUIRE( second.nGeneration > first.nGeneration );
	sirStorageDescriptor_t storage{};
	SIR_REQUIRE( catalog.queryStorage( oldQuery, &storage ) == sirHardwareResult::SIR_HARDWARE_STALE );
	SIR_REQUIRE( storage.nGeneration == 0 );

	catalog.shutdown();
	SIR_REQUIRE( catalog.copySnapshot( &first ) == sirHardwareResult::SIR_HARDWARE_INVALID );
	const sirHardwareResult eRestart = catalog.initialize();
	SIR_REQUIRE( eRestart == sirHardwareResult::SIR_HARDWARE_OK || eRestart == sirHardwareResult::SIR_HARDWARE_PARTIAL );
	SIR_REQUIRE( catalog.copySnapshot( &first ) == eRestart );
	SIR_REQUIRE( first.nGeneration > second.nGeneration );
	const sirStorageQuery_t currentQuery{ ".", first.nGeneration };
	SIR_REQUIRE( catalog.queryStorage( currentQuery, &storage ) == sirHardwareResult::SIR_HARDWARE_PARTIAL );
	SIR_REQUIRE( storage.nGeneration == first.nGeneration );
	SIR_REQUIRE( storage.volume.nSourceFlags & static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_ADVISORY_PATH ) );
	SIR_REQUIRE( storage.device.nValidFields == 0 );
}

#if defined( __linux__ )
void SIRHardwareCatalogTests::testStorage( void )
{
	char szPath[] = "/tmp/SIRHardwareCatalogTests-XXXXXX";
	const int32_t nFile = mkstemp( szPath );
	SIR_REQUIRE( nFile >= 0 );

	struct stat native
	{
	};

	struct statvfs space
	{
	};

	SIR_REQUIRE( fstat( nFile, &native ) == 0 );
	SIR_REQUIRE( fstatvfs( nFile, &space ) == 0 );

	sirPlatformStorageObservation_t path{};
	SIR_REQUIRE( SIRPlatformStorageQuery::queryPath( szPath, &path ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OK );
	SIR_REQUIRE( path.nSourceFlags & static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_ADVISORY_PATH ) );
	SIR_REQUIRE( path.nValidFields & static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FILE_SYSTEM_DEVICE ) );
	SIR_REQUIRE( path.nDeviceMajor == major( native.st_dev ) );
	SIR_REQUIRE( path.nDeviceMinor == minor( native.st_dev ) );
	SIR_REQUIRE( path.nValidFields & static_cast<uint32_t>( sirStorageField::SIR_STORAGE_ALLOCATION_UNIT ) );
	SIR_REQUIRE( path.nAllocationUnitBytes == space.f_frsize );
	SIR_REQUIRE( path.nValidFields & static_cast<uint32_t>( sirStorageField::SIR_STORAGE_TOTAL_BYTES ) );
	SIR_REQUIRE( path.nTotalBytes == space.f_blocks * space.f_frsize );

	SIR_REQUIRE( unlink( szPath ) == 0 );
	sirPlatformStorageObservation_t opened{};
	SIR_REQUIRE( SIRPlatformStorageQuery::queryNativeFile( static_cast<uintptr_t>( nFile ), &opened ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OK );
	SIR_REQUIRE( opened.nSourceFlags & static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_OPEN_HANDLE ) );
	SIR_REQUIRE( !( opened.nSourceFlags & static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_ADVISORY_PATH ) ) );
	SIR_REQUIRE( opened.nDeviceMajor == path.nDeviceMajor );
	SIR_REQUIRE( opened.nDeviceMinor == path.nDeviceMinor );
	SIR_REQUIRE( opened.nMountId == path.nMountId );
	SIR_REQUIRE( fcntl( nFile, F_GETFD ) != -1 );
	SIR_REQUIRE( close( nFile ) == 0 );

	SIR_REQUIRE( SIRPlatformStorageQuery::queryPath( szPath, &path ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNKNOWN );
	SIR_REQUIRE( path.nValidFields == 0 );
	SIR_REQUIRE( path.nNativeError == ENOENT );
	SIR_REQUIRE( SIRPlatformStorageQuery::queryNativeFile( UINTPTR_MAX, &opened ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID );
	SIR_REQUIRE( opened.nValidFields == 0 );
	SIR_REQUIRE( SIRPlatformStorageQuery::queryPath( "", &path ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID );
	SIR_REQUIRE( path.nNativeError == 0 );
	SIR_REQUIRE( SIRPlatformStorageQuery::queryPath( ".", nullptr ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID );

	SIR_REQUIRE( SIRPlatformStorageQuery::queryPath( "/proc/self/status", &path ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OK );
	SIR_REQUIRE( path.nValidFields & static_cast<uint32_t>( sirStorageField::SIR_STORAGE_MOUNT_ID ) );
	SIR_REQUIRE( path.nValidFields & static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FILE_SYSTEM_TYPE ) );
	SIR_REQUIRE( strcmp( path.szFileSystemType, "proc" ) == 0 );
	SIR_REQUIRE( path.nValidFields & static_cast<uint32_t>( sirStorageField::SIR_STORAGE_TOTAL_BYTES ) );
	SIR_REQUIRE( path.nTotalBytes == 0 );
	SIR_REQUIRE( SIRPlatformStorageQuery::queryPath( "/dev/null", &path ) == sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNSUPPORTED );
	SIR_REQUIRE( path.nValidFields == 0 );
}
#endif

int main()
{
	SIRHardwareCatalogTests::testCatalog();
#if defined( __linux__ )
	SIRHardwareCatalogTests::testStorage();
#endif
	return 0;
}
