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

#ifndef __SIR_NATIVE_WINDOW_HOST_H__
#define __SIR_NATIVE_WINDOW_HOST_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRWindowDriver.h>

class SIRWindowSystem;
struct sirWindowSnapshot_s;
struct sirWindowNativeView_s;
typedef bool ( *sirNativeWindowFrameFn_t )( SIRWindowSystem&, sirWindowHandle_t, const sirWindowSnapshot_s&, const sirWindowNativeView_s&, void * ) noexcept;

// Driver lifetime is borrowed for the duration of runWithDriver.
class SIRNativeWindowHost final
{
public:
	explicit SIRNativeWindowHost( void ) = delete;
	explicit SIRNativeWindowHost( const SIRNativeWindowHost& ) = delete;
	explicit SIRNativeWindowHost( SIRNativeWindowHost&& ) = delete;
	~SIRNativeWindowHost() = delete;

	SIRNativeWindowHost& operator=( const SIRNativeWindowHost& ) = delete;
	SIRNativeWindowHost& operator=( SIRNativeWindowHost&& ) = delete;
	[[nodiscard]] static int32_t runInput( bool bMultiwindow, uint64_t nMaxFrames ) noexcept;
	[[nodiscard]] static int32_t runNative( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept;
	[[nodiscard]] static int32_t runWithDriver( SIRWindowDriver& driver, const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept;
	[[nodiscard]] static int32_t runNativeWithFrame( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows,
	                                                  sirNativeWindowFrameFn_t pFrame, void *pContext ) noexcept;
	[[nodiscard]] static int32_t runWithDriverFrame( SIRWindowDriver& driver, const sirWindowCreateDesc_t *pDescs,
	                                                  uint32_t nWindows, sirNativeWindowFrameFn_t pFrame, void *pContext ) noexcept;
};

#endif // __SIR_NATIVE_WINDOW_HOST_H__
