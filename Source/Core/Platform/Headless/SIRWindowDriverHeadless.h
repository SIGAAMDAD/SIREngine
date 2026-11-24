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
#ifndef __SIR_WINDOW_DRIVER_HEADLESS_H__
#define __SIR_WINDOW_DRIVER_HEADLESS_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRWindowDriver.h>

class SIRWindowDriverHeadless final : public SIRWindowDriver
{
public:
	explicit SIRWindowDriverHeadless( void ) noexcept = default;
	~SIRWindowDriverHeadless() noexcept override = default;

	[[nodiscard]] sirWindowResult initialize( void ) noexcept override;
	[[nodiscard]] sirWindowResult createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override;
	[[nodiscard]] sirWindowResult destroyWindow( uint32_t nSlot ) noexcept override;
	[[nodiscard]] sirWindowResult mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override;
	[[nodiscard]] sirWindowResult pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override;
	void shutdown( void ) noexcept override;

	private:
	bool m_bInitialized = false;
};
#endif // __SIR_WINDOW_DRIVER_HEADLESS_H__
