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
#ifndef __SIR_PLATFORMCONTEXT_H__
#define __SIR_PLATFORMCONTEXT_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class sirPlatformKind : uint32_t {
	SIR_PLATFORM_WINDOWS = 1,
	SIR_PLATFORM_LINUX = 2
};

// Snapshot of the process and the calling thread; no native resources are owned.
typedef struct sirPlatformContext_s
{
	sirPlatformKind ePlatform;
	uint32_t nProcessId;
	uint64_t nThreadId;
	uint64_t nCapabilities;
} sirPlatformContext_t;

// Device identity has no ownership or native pointer. A zero native ID denotes
// an unspecified/default endpoint; providers resolve and own concrete devices.
enum class sirPlatformDeviceBackend : uint32_t {
	SIR_PLATFORM_DEVICE_NONE = 0,
	SIR_PLATFORM_DEVICE_FMOD = 0x10000000,
	SIR_PLATFORM_DEVICE_INPUT_BASE = 0x20000000
};

typedef struct sirPlatformDeviceIdentity_s
{
	uint64_t nNativeId;
	uint64_t nPhysicalId;
	uint64_t nGeneration;
	uint32_t nBackendId;
	uint32_t nFlags;
} sirPlatformDeviceIdentity_t;

// Snapshot the current process and calling OS thread. The caller must not
// substitute this thread identity for a different owner lane.
class SIRPlatformContext final
{
public:
	explicit SIRPlatformContext( void ) = delete;
	explicit SIRPlatformContext( const SIRPlatformContext& ) = delete;
	explicit SIRPlatformContext( SIRPlatformContext&& ) = delete;

	SIRPlatformContext& operator=( const SIRPlatformContext& ) = delete;
	SIRPlatformContext& operator=( SIRPlatformContext&& ) = delete;
	[[nodiscard]] static bool capture( sirPlatformContext_t *pContext ) noexcept;
};

static_assert( sizeof( sirPlatformDeviceIdentity_t ) == 32, "Platform device identity ABI" );
static_assert( __is_trivially_copyable( sirPlatformContext_t ), "Platform context must be a POD" );

#endif // __SIR_PLATFORMCONTEXT_H__
