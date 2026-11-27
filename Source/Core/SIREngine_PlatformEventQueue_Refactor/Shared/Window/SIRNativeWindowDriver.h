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

#ifndef __SIR_NATIVE_WINDOW_DRIVER_H__
#define __SIR_NATIVE_WINDOW_DRIVER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRWindowTypes.h"
#include "SIREventTypes.h"
#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Memory/Pointers/SIRSharedPtr.h>

/// The primary manager of the window instances.
class SIRNativeWindowDriver
{
public:
	explicit SIRNativeWindowDriver( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept;
	constexpr SIRNativeWindowDriver( const SIRNativeWindowDriver& ) noexcept = delete;
	constexpr SIRNativeWindowDriver( SIRNativeWindowDriver&& ) noexcept = delete;
	virtual ~SIRNativeWindowDriver() noexcept = default;

	SIRNativeWindowDriver& operator=( const SIRNativeWindowDriver& ) noexcept = delete;
	SIRNativeWindowDriver& operator=( SIRNativeWindowDriver&& ) noexcept = delete;

	const SIRNativeWindowConfiguration& getConfig( void ) const noexcept;

	virtual SIRWindowResult setTitle( const char *pszWindowTitle ) noexcept = 0;
	virtual SIRWindowResult setPosition( uint32_t nPositionX, uint32_t nPositionY ) noexcept = 0;
	virtual SIRWindowResult setSize( uint32_t nWidth, uint32_t nHeight ) noexcept = 0;

	virtual SIRWindowResult pumpEvents( void ) noexcept = 0;

	virtual SIRWindowResult initialize( void ) noexcept = 0;
	virtual SIRWindowResult createWindow( const SIRNativeWindowConfiguration& config ) noexcept = 0;
	virtual void shutdown( void ) noexcept = 0;

protected:
	// Borrowed from the platform host; the queue outlives every native driver.
	SIRSPSCEventRing<SIREventData>& m_EventQueue;
};

SIR_FORCEINLINE SIRNativeWindowDriver::SIRNativeWindowDriver( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept
	: m_EventQueue( eventQueue )
{
}

#endif // __SIR_NATIVE_WINDOW_DRIVER_H__
