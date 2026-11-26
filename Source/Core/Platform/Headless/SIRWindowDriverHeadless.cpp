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

#include <Core/Platform/Headless/SIRWindowDriverHeadless.h>

sirWindowResult SIRWindowDriverHeadless::initialize( void ) noexcept
{
	if ( m_bInitialized ) {
		return sirWindowResult::SIR_WINDOW_BUSY;
	}
	m_bInitialized = true;
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowDriverHeadless::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                         sirWindowNativeView_t *pView ) noexcept
{
	static_cast<void>(desc);
	static_cast<void>(nSlot);
	static_cast<void>(pView);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_UNSUPPORTED : sirWindowResult::SIR_WINDOW_STOPPED;
}

sirWindowResult SIRWindowDriverHeadless::destroyWindow( uint32_t nSlot ) noexcept
{
	static_cast<void>(nSlot);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_INVALID_HANDLE : sirWindowResult::SIR_WINDOW_STOPPED;
}

sirWindowResult SIRWindowDriverHeadless::mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	static_cast<void>(nSlot);
	static_cast<void>(mutation);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_UNSUPPORTED : sirWindowResult::SIR_WINDOW_STOPPED;
}

sirWindowResult SIRWindowDriverHeadless::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	static_cast<void>(nBudget);
	static_cast<void>(pCallback);
	static_cast<void>(pContext);
	return m_bInitialized ? sirWindowResult::SIR_WINDOW_OK : sirWindowResult::SIR_WINDOW_STOPPED;
}

void SIRWindowDriverHeadless::shutdown( void ) noexcept
{
	m_bInitialized = false;
}
