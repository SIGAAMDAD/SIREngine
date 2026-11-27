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

#ifndef __SIR_NATIVE_WINDOW_DRIVER_WIN32_H__
#define __SIR_NATIVE_WINDOW_DRIVER_WIN32_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Platform/Shared/Window/SIRNativeWindowDriver.h>
#include <Core/Platform/Shared/Window/SIREventTypes.h>

class SIRNativeWindowDriverWin32 final : public SIRNativeWindowDriver
{
public:
	explicit SIRNativeWindowDriverWin32( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept;
	~SIRNativeWindowDriverWin32() noexcept;

	SIRWindowResult initialize( void ) noexcept override;
	SIRWindowResult createWindow( const SIRNativeWindowConfiguration& config ) noexcept override;
	void shutdown( void ) noexcept override;

	SIRWindowResult setTitle( const char *pszWindowTitle ) noexcept override;
	SIRWindowResult setPosition( uint32_t nPositionX, uint32_t nPositionY ) noexcept override;
	SIRWindowResult setSize( uint32_t nWidth, uint32_t nHeight ) noexcept override;
	SIRWindowResult pumpEvents( void ) noexcept override;

	SIRWindowResult destroyWindow( void ) noexcept;
	bool isServerLost( void ) const noexcept;

private:
};

#endif // __SIR_NATIVE_WINDOW_DRIVER_WIN32_H__
