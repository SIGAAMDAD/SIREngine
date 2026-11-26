/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Platform/Shared/Storage/SIRPlatformStorageQuery.h>

#if defined( __linux__ )
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

class SIRLinuxStorageObservation final
{
public:
	explicit SIRLinuxStorageObservation( void ) = delete;
	explicit SIRLinuxStorageObservation( const SIRLinuxStorageObservation& ) = delete;
	explicit SIRLinuxStorageObservation( SIRLinuxStorageObservation&& ) = delete;
	~SIRLinuxStorageObservation() = delete;

	SIRLinuxStorageObservation& operator=( const SIRLinuxStorageObservation& ) = delete;
	SIRLinuxStorageObservation& operator=( SIRLinuxStorageObservation&& ) = delete;

	static sirPlatformStorageResult reportError( int32_t nError, sirPlatformStorageObservation_t *pObservation ) noexcept;
	static void readMount( int32_t nFile, sirPlatformStorageObservation_t *pObservation ) noexcept;
	static void readSpace( int32_t nFile, sirPlatformStorageObservation_t *pObservation ) noexcept;
	static bool isComplete( const sirPlatformStorageObservation_t& rObservation ) noexcept;

private:
	static bool copyFileSystemType( const char *pszLine, sirPlatformStorageObservation_t *pObservation ) noexcept;
	static bool blocksToBytes( uint64_t nBlocks, uint64_t nUnit, uint64_t *pBytes ) noexcept;
};

sirPlatformStorageResult SIRLinuxStorageObservation::reportError( int32_t nError, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	pObservation->nNativeError = static_cast<uint32_t>( nError );
	switch ( nError ) {
	case EACCES:
	case EPERM:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_ACCESS_DENIED;
	case ENOENT:
	case ENOTDIR:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNKNOWN;
	case EBADF:
	case EINVAL:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	default:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OS_ERROR;
	}
}

void SIRLinuxStorageObservation::readMount( int32_t nFile, sirPlatformStorageObservation_t *pObservation ) noexcept
{
#if defined( STATX_MNT_ID )
	struct statx extended
	{
	};

	if ( statx( nFile, "", AT_EMPTY_PATH | AT_STATX_DONT_SYNC, STATX_MNT_ID, &extended ) != 0 || ( extended.stx_mask & STATX_MNT_ID ) == 0 ) {
		return;
	}

	pObservation->nMountId = extended.stx_mnt_id;
	pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_MOUNT_ID );
	FILE *pMounts = fopen( "/proc/self/mountinfo", "re" );
	if ( pMounts == nullptr ) {
		return;
	}

	// Read complete records or their first chunk. Skip the rest of long
	// records so an option or escaped mountpoint cannot look like a new row.
	char szLine[4096];
	bool bFirstChunk = true;
	while ( fgets( szLine, sizeof( szLine ), pMounts ) != nullptr ) {
		if ( bFirstChunk && copyFileSystemType( szLine, pObservation ) ) {
			break;
		}
		bFirstChunk = strchr( szLine, '\n' ) != nullptr;
	}
	fclose( pMounts );
#else
	( void )nFile;
	( void )pObservation;
#endif
}

bool SIRLinuxStorageObservation::copyFileSystemType( const char *pszLine, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	const char *pszIdEnd = strchr( pszLine, ' ' );
	if ( pszIdEnd == nullptr ) {
		return false;
	}
	uint64_t nId{};
	const std::from_chars_result parse = std::from_chars( pszLine, pszIdEnd, nId );
	if ( parse.ec != std::errc{} || parse.ptr != pszIdEnd || nId != pObservation->nMountId ) {
		return false;
	}

	const char *pszSeparator = strstr( pszIdEnd, " - " );
	if ( pszSeparator == nullptr ) {
		return false;
	}
	const char *pszType = pszSeparator + 3;
	const char *pszTypeEnd = strchr( pszType, ' ' );
	if ( pszTypeEnd == nullptr || pszTypeEnd == pszType ) {
		return false;
	}
	const size_t nLength = static_cast<size_t>( pszTypeEnd - pszType );
	if ( nLength >= sizeof( pObservation->szFileSystemType ) ) {
		return false;
	}
	memcpy( pObservation->szFileSystemType, pszType, nLength );
	pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FILE_SYSTEM_TYPE );
	return true;
}

void SIRLinuxStorageObservation::readSpace( int32_t nFile, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	struct statvfs space
	{
	};

	if ( fstatvfs( nFile, &space ) != 0 ) {
		pObservation->nNativeError = static_cast<uint32_t>( errno );
		return;
	}
	if ( ( space.f_flag & ST_RDONLY ) != 0 ) {
		pObservation->nVolumeAttributes |= static_cast<uint32_t>( sirStorageVolumeAttribute::SIR_STORAGE_VOLUME_READ_ONLY );
	}
	pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_READ_ONLY );

	const uint64_t nUnit = static_cast<uint64_t>( space.f_frsize );
	if ( nUnit == 0 ) {
		return;
	}
	pObservation->nAllocationUnitBytes = nUnit;
	pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_ALLOCATION_UNIT );
	if ( space.f_blocks != static_cast<fsblkcnt_t>( -1 ) && blocksToBytes( space.f_blocks, nUnit, &pObservation->nTotalBytes ) ) {
		pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_TOTAL_BYTES );
	}
	if ( space.f_bfree != static_cast<fsblkcnt_t>( -1 ) && blocksToBytes( space.f_bfree, nUnit, &pObservation->nFreeBytes ) ) {
		pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FREE_BYTES );
	}
	if ( space.f_bavail != static_cast<fsblkcnt_t>( -1 ) && blocksToBytes( space.f_bavail, nUnit, &pObservation->nAvailableBytes ) ) {
		pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_AVAILABLE_BYTES );
	}
}

bool SIRLinuxStorageObservation::blocksToBytes( uint64_t nBlocks, uint64_t nUnit, uint64_t *pBytes ) noexcept
{
	if ( nBlocks > UINT64_MAX / nUnit ) {
		return false;
	}
	*pBytes = nBlocks * nUnit;
	return true;
}

bool SIRLinuxStorageObservation::isComplete( const sirPlatformStorageObservation_t& rObservation ) noexcept
{
	constexpr uint32_t nRequired =
	    static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FILE_SYSTEM_DEVICE ) | static_cast<uint32_t>( sirStorageField::SIR_STORAGE_MOUNT_ID ) |
	    static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FILE_SYSTEM_TYPE ) | static_cast<uint32_t>( sirStorageField::SIR_STORAGE_ALLOCATION_UNIT ) |
	    static_cast<uint32_t>( sirStorageField::SIR_STORAGE_TOTAL_BYTES ) | static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FREE_BYTES ) |
	    static_cast<uint32_t>( sirStorageField::SIR_STORAGE_AVAILABLE_BYTES ) | static_cast<uint32_t>( sirStorageField::SIR_STORAGE_READ_ONLY ) |
	    static_cast<uint32_t>( sirStorageField::SIR_STORAGE_OBSERVATION_TIME );
	return ( rObservation.nValidFields & nRequired ) == nRequired;
}

sirPlatformStorageResult SIRPlatformStorageQuery::queryPath( const char *pszUtf8Path, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	if ( pObservation == nullptr ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}
	*pObservation = {};
	if ( pszUtf8Path == nullptr || pszUtf8Path[0] == '\0' ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}
	const int32_t nFile = open( pszUtf8Path, O_PATH | O_CLOEXEC );
	if ( nFile < 0 ) {
		return SIRLinuxStorageObservation::reportError( errno, pObservation );
	}
	const sirPlatformStorageResult eResult = queryNativeFile( static_cast<uintptr_t>( nFile ), pObservation );
	close( nFile );
	if ( pObservation->nValidFields != 0 ) {
		pObservation->nSourceFlags = static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_ADVISORY_PATH );
	}
	return eResult;
}

sirPlatformStorageResult SIRPlatformStorageQuery::queryNativeFile( uintptr_t nHandle, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	if ( pObservation == nullptr ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}
	*pObservation = {};
	if ( nHandle > INT32_MAX ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}
	const int32_t nFile = static_cast<int32_t>( nHandle );

	struct stat info
	{
	};

	if ( fstat( nFile, &info ) != 0 ) {
		return SIRLinuxStorageObservation::reportError( errno, pObservation );
	}
	if ( !S_ISREG( info.st_mode ) && !S_ISDIR( info.st_mode ) ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNSUPPORTED;
	}

	pObservation->nDeviceMajor = major( info.st_dev );
	pObservation->nDeviceMinor = minor( info.st_dev );
	pObservation->nValidFields = static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FILE_SYSTEM_DEVICE );
	pObservation->nSourceFlags = static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_OPEN_HANDLE );
	SIRLinuxStorageObservation::readMount( nFile, pObservation );
	SIRLinuxStorageObservation::readSpace( nFile, pObservation );

	pObservation->nObservedMonotonicNanoseconds = SIRPlatformTime::getMonotonicNanoseconds();
	if ( pObservation->nObservedMonotonicNanoseconds != 0 ) {
		pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_OBSERVATION_TIME );
	}
	return SIRLinuxStorageObservation::isComplete( *pObservation ) ? sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OK
	                                                               : sirPlatformStorageResult::SIR_PLATFORM_STORAGE_PARTIAL;
}
#endif // __linux__
