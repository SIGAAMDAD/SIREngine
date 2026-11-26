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

#ifndef __SIR_WINDOW_DRIVER_H__
#define __SIR_WINDOW_DRIVER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRWindowTypes.h>

// Internal, same-toolchain backend interface. Only the platform owner thread
// may invoke these methods. The callback must be synchronous, never retained.
typedef void ( *sirWindowDriverEventFn_t )( void *pContext, const sirWindowBackendEvent_t& event ) noexcept;

class SIRWindowDriver
{
public:
	explicit SIRWindowDriver( void ) noexcept = default;
	explicit SIRWindowDriver( const SIRWindowDriver& ) = delete;
	explicit SIRWindowDriver( SIRWindowDriver&& ) = delete;
	virtual ~SIRWindowDriver() noexcept = default;

	SIRWindowDriver& operator=( const SIRWindowDriver& ) = delete;
	SIRWindowDriver& operator=( SIRWindowDriver&& ) = delete;

	virtual sirWindowResult initialize( void ) noexcept = 0;
	virtual sirWindowResult createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept = 0;
	virtual sirWindowResult destroyWindow( uint32_t nSlot ) noexcept = 0;
	virtual sirWindowResult mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept = 0;
	virtual sirWindowResult pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept = 0;
	virtual void shutdown( void ) noexcept = 0;
};
#endif // __SIR_SIRWINDOWDRIVER_H__
