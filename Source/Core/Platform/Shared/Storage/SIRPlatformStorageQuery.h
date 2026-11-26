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

#ifndef __SIR_PLATFORMSTORAGEQUERY_H__
#define __SIR_PLATFORMSTORAGEQUERY_H__

#include <Core/SIRPCH.h>

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

enum class sirPlatformStorageResult : uint8_t
{
	SIR_PLATFORM_STORAGE_OK,
	SIR_PLATFORM_STORAGE_PARTIAL,
	SIR_PLATFORM_STORAGE_UNKNOWN,
	SIR_PLATFORM_STORAGE_UNSUPPORTED,
	SIR_PLATFORM_STORAGE_ACCESS_DENIED,
	SIR_PLATFORM_STORAGE_INVALID,
	SIR_PLATFORM_STORAGE_OS_ERROR
};

enum class sirStorageField : uint32_t
{
	SIR_STORAGE_FILE_SYSTEM_DEVICE = 1U << 0,
	SIR_STORAGE_MOUNT_ID = 1U << 1,
	SIR_STORAGE_FILE_SYSTEM_TYPE = 1U << 2,
	SIR_STORAGE_ALLOCATION_UNIT = 1U << 3,
	SIR_STORAGE_TOTAL_BYTES = 1U << 4,
	SIR_STORAGE_FREE_BYTES = 1U << 5,
	SIR_STORAGE_AVAILABLE_BYTES = 1U << 6,
	SIR_STORAGE_READ_ONLY = 1U << 7,
	SIR_STORAGE_OBSERVATION_TIME = 1U << 8
};

enum class sirStorageSource : uint32_t
{
	SIR_STORAGE_SOURCE_ADVISORY_PATH = 1U << 0,
	SIR_STORAGE_SOURCE_OPEN_HANDLE = 1U << 1
};

enum class sirStorageVolumeAttribute : uint32_t
{
	SIR_STORAGE_VOLUME_READ_ONLY = 1U << 0
};

// Device major/minor identify a Linux filesystem, not a physical disk.
// The mount ID is local to the mount namespace and can be reused after unmount.
typedef struct sirPlatformStorageObservation_s
{
	uint64_t nMountId;
	uint64_t nAllocationUnitBytes;
	uint64_t nTotalBytes;
	uint64_t nFreeBytes;
	uint64_t nAvailableBytes;
	uint64_t nObservedMonotonicNanoseconds;
	uint32_t nDeviceMajor;
	uint32_t nDeviceMinor;
	uint32_t nValidFields;
	uint32_t nSourceFlags;
	uint32_t nVolumeAttributes;
	uint32_t nNativeError;
	char szFileSystemType[32];
} sirPlatformStorageObservation_t;

class SIRPlatformStorageQuery final
{
public:
	explicit SIRPlatformStorageQuery( void ) = delete;
	explicit SIRPlatformStorageQuery( const SIRPlatformStorageQuery& ) = delete;
	explicit SIRPlatformStorageQuery( SIRPlatformStorageQuery&& ) = delete;
	~SIRPlatformStorageQuery() = delete;

	SIRPlatformStorageQuery& operator=( const SIRPlatformStorageQuery& ) = delete;
	SIRPlatformStorageQuery& operator=( SIRPlatformStorageQuery&& ) = delete;

	// Cold, read-only observation of an existing path. A later open may resolve to another volume.
	[[nodiscard]] static sirPlatformStorageResult queryPath( const char *pszUtf8Path, sirPlatformStorageObservation_t *pObservation ) noexcept;

	// Borrow a Linux descriptor or Windows HANDLE for the duration of the call.
	// The caller must keep it open and prevent concurrent replacement.
	[[nodiscard]] static sirPlatformStorageResult queryNativeFile( uintptr_t nHandle, sirPlatformStorageObservation_t *pObservation ) noexcept;
};

#endif // __SIR_PLATFORMSTORAGEQUERY_H__
