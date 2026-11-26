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

#ifndef __SIR_HARDWARETYPES_H__
#define __SIR_HARDWARETYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Diagnostics/SIRCPUInfo.h>
#include <Core/Platform/Shared/Memory/SIRPlatformMemory.h>
#include <Core/Platform/Shared/Storage/SIRPlatformStorageQuery.h>

enum class SIRHardwareResult : uint8_t
{
	SIR_HARDWARE_OK,
	SIR_HARDWARE_PARTIAL,
	SIR_HARDWARE_UNKNOWN,
	SIR_HARDWARE_UNSUPPORTED,
	SIR_HARDWARE_ACCESS_DENIED,
	SIR_HARDWARE_STALE,
	SIR_HARDWARE_OUT_OF_CAPACITY,
	SIR_HARDWARE_INVALID,
	SIR_HARDWARE_OS_ERROR
};

enum class SIRCPUDescriptorField : uint32_t
{
	SIR_CPU_DESCRIPTOR_LOGICAL_COUNT = 1U << 0,
	SIR_CPU_DESCRIPTOR_PHYSICAL_COUNT = 1U << 1,
	SIR_CPU_DESCRIPTOR_VENDOR = 1U << 2,
	SIR_CPU_DESCRIPTOR_BRAND = 1U << 3,
	SIR_CPU_DESCRIPTOR_FEATURES = 1U << 4,
	SIR_CPU_DESCRIPTOR_CACHE_LINE = 1U << 5,
	SIR_CPU_DESCRIPTOR_L1_DATA_CACHE = 1U << 6,
	SIR_CPU_DESCRIPTOR_L2_CACHE = 1U << 7,
	SIR_CPU_DESCRIPTOR_L3_CACHE = 1U << 8
};

typedef struct sirCPUDescriptor_s
{
	sirCPUInfo_t info;
	uint32_t nValidFields;
} sirCPUDescriptor_t;

enum class SIRMemoryDescriptorField : uint32_t
{
	SIR_MEMORY_DESCRIPTOR_TOTAL_PHYSICAL = 1U << 0,
	SIR_MEMORY_DESCRIPTOR_AVAILABLE_PHYSICAL = 1U << 1
};

typedef struct sirMemoryDescriptor_s
{
	// The raw records retain their own validity masks for optional fields.
	sirSystemMemoryStats_t system;
	sirProcessMemoryStats_t process;
	uint32_t nValidFields;
} sirMemoryDescriptor_t;

enum class SIRStorageTransport : uint8_t
{
	SIR_STORAGE_TRANSPORT_UNKNOWN,
	SIR_STORAGE_TRANSPORT_SATA,
	SIR_STORAGE_TRANSPORT_SAS,
	SIR_STORAGE_TRANSPORT_NVME,
	SIR_STORAGE_TRANSPORT_USB,
	SIR_STORAGE_TRANSPORT_EMMC,
	SIR_STORAGE_TRANSPORT_UFS,
	SIR_STORAGE_TRANSPORT_VIRTUAL,
	SIR_STORAGE_TRANSPORT_NETWORK,
	SIR_STORAGE_TRANSPORT_OTHER
};

enum class SIRStorageMedia : uint8_t
{
	SIR_STORAGE_MEDIA_UNKNOWN,
	SIR_STORAGE_MEDIA_ROTATIONAL,
	SIR_STORAGE_MEDIA_SOLID_STATE,
	SIR_STORAGE_MEDIA_HYBRID,
	SIR_STORAGE_MEDIA_MEMORY,
	SIR_STORAGE_MEDIA_OTHER
};

enum class SIRStorageDeviceField : uint32_t
{
	SIR_STORAGE_DEVICE_CAPACITY = 1U << 0,
	SIR_STORAGE_DEVICE_LOGICAL_SECTOR = 1U << 1,
	SIR_STORAGE_DEVICE_PHYSICAL_SECTOR = 1U << 2,
	SIR_STORAGE_DEVICE_TRANSPORT = 1U << 3,
	SIR_STORAGE_DEVICE_MEDIA = 1U << 4
};

typedef struct sirStorageDeviceDescriptor_s
{
	uint64_t nCapacityBytes;
	uint32_t nLogicalSectorBytes;
	uint32_t nPhysicalSectorBytes;
	uint32_t nValidFields;
	SIRStorageTransport eTransport;
	SIRStorageMedia eMedia;
} sirStorageDeviceDescriptor_t;

typedef struct sirStorageDescriptor_s
{
	sirPlatformStorageObservation_t volume;
	// This slice publishes volume observations only. Backing device fields are invalid.
	sirStorageDeviceDescriptor_t device;
	uint32_t nGeneration;
} sirStorageDescriptor_t;

typedef struct sirStorageQuery_s
{
	const char *pszUtf8Path;
	uint32_t nGeneration;
} sirStorageQuery_t;

typedef struct sirHardwareSnapshot_s
{
	sirCPUDescriptor_t cpu;
	sirMemoryDescriptor_t memory;
	uint32_t nGeneration;
} sirHardwareSnapshot_t;

#endif // __SIR_HARDWARETYPES_H__
