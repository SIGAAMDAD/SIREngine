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

#ifndef __SIR_PLATFORMNATIVEWINDOW_H__
#define __SIR_PLATFORMNATIVEWINDOW_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRWindowTypes.h>

enum class sirLinuxDisplayKind : uint32_t {
	SIR_LINUX_DISPLAY_NONE = 0,
	SIR_LINUX_DISPLAY_X11 = 1,
	SIR_LINUX_DISPLAY_WAYLAND = 2
};

// Borrowed views, valid only while the SIRWindowSystem lease is held.
// Neither structure may outlive that lease or close its native resources.
typedef struct sirWindowsNativeWindow_s
{
	uintptr_t nHWND;
	uintptr_t nHInstance;
	uint64_t nWindowGeneration;
} sirWindowsNativeWindow_t;

typedef struct sirLinuxNativeWindow_s
{
	uintptr_t nDisplay;
	uintptr_t nSurface;
	uint64_t nWindowGeneration;
	sirLinuxDisplayKind eDisplayType;
	uint32_t nReserved;
} sirLinuxNativeWindow_t;

// Decode the existing, authoritative native view without reacquiring or
// retaining an OS resource. The lease remains entirely with the caller.
class SIRPlatformNativeWindow final
{
public:
	explicit SIRPlatformNativeWindow( void ) = delete;
	explicit SIRPlatformNativeWindow( const SIRPlatformNativeWindow& ) = delete;
	explicit SIRPlatformNativeWindow( SIRPlatformNativeWindow&& ) = delete;

	SIRPlatformNativeWindow& operator=( const SIRPlatformNativeWindow& ) = delete;
	SIRPlatformNativeWindow& operator=( SIRPlatformNativeWindow&& ) = delete;
	[[nodiscard]] static bool decodeWindows( const sirWindowNativeView_t& view, sirWindowsNativeWindow_t *pWindow ) noexcept;
	[[nodiscard]] static bool decodeLinux( const sirWindowNativeView_t& view, sirLinuxNativeWindow_t *pWindow ) noexcept;
};

static_assert( sizeof( sirWindowsNativeWindow_t ) == 24, "Windows window view ABI" );
static_assert( sizeof( sirLinuxNativeWindow_t ) == 32, "Linux window view ABI" );
static_assert( __is_trivially_copyable( sirLinuxNativeWindow_t ), "Native window view must be POD" );

#endif // __SIR_PLATFORMNATIVEWINDOW_H__
