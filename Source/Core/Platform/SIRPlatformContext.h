/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/
#ifndef __SIRENGINE_SIRPLATFORMCONTEXT_H__
#define __SIRENGINE_SIRPLATFORMCONTEXT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirPlatformKind_e : uint32_t
{
	SIR_PLATFORM_WINDOWS = 1,
	SIR_PLATFORM_LINUX = 2
} sirPlatformKind_t;

typedef struct sirWindowsPlatformContext_s
{
	uintptr_t nInstance;
	uint32_t nProcessId;
	uint32_t nOwnerThreadId;
} sirWindowsPlatformContext_t;

typedef struct sirLinuxPlatformContext_s
{
	uint64_t nOwnerThreadId;
	uint32_t nProcessId;
	uint32_t nReserved;
} sirLinuxPlatformContext_t;

typedef union sirPlatformNativeContext_u
{
	sirWindowsPlatformContext_t windows;
	sirLinuxPlatformContext_t linux;
} sirPlatformNativeContext_t;

// A process-and-calling-thread snapshot. No window, device, or middleware
// resources are owned by this POD. The same value can be copied to drivers.
typedef struct sirPlatformContext_s
{
	uint32_t nAbiVersion;
	sirPlatformKind_t ePlatform;
	uint64_t nCapabilities;
	sirPlatformNativeContext_t native;
} sirPlatformContext_t;

// Device identity has no ownership or native pointer. A zero native ID denotes
// an unspecified/default endpoint; providers resolve and own concrete devices.
typedef enum sirPlatformDeviceBackend_e : uint32_t
{
	SIR_PLATFORM_DEVICE_NONE = 0,
	SIR_PLATFORM_DEVICE_FMOD = 0x10000000,
	SIR_PLATFORM_DEVICE_INPUT_BASE = 0x20000000
} sirPlatformDeviceBackend_t;

typedef struct sirPlatformDeviceIdentity_s
{
	uint64_t nNativeId;
	uint64_t nPhysicalId;
	uint64_t nGeneration;
	uint32_t nBackendId;
	uint32_t nFlags;
} sirPlatformDeviceIdentity_t;

constexpr uint32_t SIR_PLATFORM_CONTEXT_ABI_VERSION = 1;

// Snapshot the current process and calling OS thread. The caller must not
// substitute this thread identity for a different owner lane.
bool sirPlatformCaptureContext( sirPlatformContext_t *pContext ) noexcept;

static_assert( sizeof( sirWindowsPlatformContext_t ) == 16, "Windows native context ABI" );
static_assert( sizeof( sirLinuxPlatformContext_t ) == 16, "Linux native context ABI" );
static_assert( sizeof( sirPlatformContext_t ) == 32, "Platform context ABI" );
static_assert( sizeof( sirPlatformDeviceIdentity_t ) == 32, "Platform device identity ABI" );
static_assert( __is_trivially_copyable( sirPlatformContext_t ), "Platform context must be a POD" );

#endif // __SIRENGINE_SIRPLATFORMCONTEXT_H__
