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

#include <Core/Platform/Window/SIRNativeWindowHost.h>
#include <stdio.h>

class SIRNativeWindowHostTestDriver final : public SIRWindowDriver
{
public:
	SIRNativeWindowHostTestDriver( void ) noexcept = default;
	~SIRNativeWindowHostTestDriver( void ) noexcept override = default;

	sirWindowResult_t initialize( void ) noexcept override
	{
		++m_nInitializes;
		return m_bFailInit ? SIR_WINDOW_NATIVE_FAILURE : SIR_WINDOW_OK;
	}
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override
	{
		++m_nCreates;
		if ( m_bFailCreate || ( m_nFailCreateOn != 0 && m_nCreates == m_nFailCreateOn ) || desc.nWidth == 0 ||
	     desc.nHeight == 0 || pView == nullptr || nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		m_bAlive[nSlot] = true;
		*pView = { SIR_WINDOW_PLATFORM_X11, ( uintptr_t )( nSlot + 1 ), 1, 0 };
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t destroyWindow( uint32_t nSlot ) noexcept override
	{
		if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || !m_bAlive[nSlot] ) {
			return SIR_WINDOW_INVALID_HANDLE;
		}
		m_bAlive[nSlot] = false;
		++m_nDestroys;
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override
	{
		( void )nSlot;
		( void )mutation;
		return SIR_WINDOW_UNSUPPORTED;
	}
	sirWindowResult_t pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override
	{
		if ( nBudget == 0 || pCallback == nullptr ) {
			return SIR_WINDOW_INVALID_ARGUMENT;
		}
		++m_nPumps;
		if ( m_bFailPump ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		if ( m_nPumps == 2 || m_nPumps == 3 ) {
			for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
				if ( m_bAlive[i] && ( m_nPumps == 3 || i == 0 ) ) {
					const sirWindowBackendEvent_t close = { i, ( uintptr_t )( i + 1 ), SIR_WINDOW_EVENT_CLOSE_REQUESTED, 0, 0 };
					pCallback( pContext, close );
				}
			}
		}
		return SIR_WINDOW_OK;
	}
	void shutdown( void ) noexcept override
	{
		++m_nShutdowns;
	}

	bool m_bAlive[SIR_WINDOW_MAX_WINDOWS] = {};
	uint32_t m_nFailCreateOn = 0;
	uint32_t m_nInitializes = 0;
	uint32_t m_nCreates = 0;
	uint32_t m_nDestroys = 0;
	uint32_t m_nPumps = 0;
	uint32_t m_nShutdowns = 0;
	bool m_bFailInit = false;
	bool m_bFailCreate = false;
	bool m_bFailPump = false;

protected:
private:
};

int main( void )
{
	SIRNativeWindowHostTestDriver successful;
	if ( sirRunWindowHostWithDriver( successful, { "SIREngine Editor", 1280, 720 } ) != 0 || successful.m_nInitializes != 1 ||
	     successful.m_nCreates != 1 || successful.m_nDestroys != 1 || successful.m_nPumps != 2 || successful.m_nShutdowns != 1 ) {
		fputs( "Editor native window: graceful close FAIL\n", stderr );
		return 1;
	}

	sirWindowCreateDesc_t multi[] = { { "primary", 800, 450 }, { "auxiliary", 640, 360 } };
	SIRNativeWindowHostTestDriver multiple;
	if ( sirRunWindowHostsWithDriver( multiple, multi, 2 ) != 0 || multiple.m_nCreates != 2 ||
	     multiple.m_nDestroys != 2 || multiple.m_nPumps != 3 || multiple.m_nShutdowns != 1 ) {
		fputs( "Native multiwindow lifecycle FAIL\n", stderr );
		return 1;
	}
	SIRNativeWindowHostTestDriver zero;
	if ( sirRunWindowHostsWithDriver( zero, nullptr, 0 ) != 0 || zero.m_nCreates != 0 || zero.m_nShutdowns != 1 ) {
		return 1;
	}
	SIRNativeWindowHostTestDriver partialCreate;
	partialCreate.m_nFailCreateOn = 2;
	if ( sirRunWindowHostsWithDriver( partialCreate, multi, 2 ) == 0 || partialCreate.m_nCreates != 2 ||
	     partialCreate.m_nDestroys != 1 || partialCreate.m_nShutdowns != 1 ) {
		return 1;
	}
	SIRNativeWindowHostTestDriver invalidCount;
	if ( sirRunWindowHostsWithDriver( invalidCount, multi, SIR_WINDOW_MAX_WINDOWS + 1 ) == 0 ||
	     invalidCount.m_nInitializes != 0 ) {
		return 1;
	}

	SIRNativeWindowHostTestDriver initFailure;
	initFailure.m_bFailInit = true;
	if ( sirRunWindowHostWithDriver( initFailure, { "SIREngine Editor", 1280, 720 } ) == 0 || initFailure.m_nCreates != 0 ||
	     initFailure.m_nDestroys != 0 ) {
		fputs( "Editor native window: init failure FAIL\n", stderr );
		return 1;
	}

	SIRNativeWindowHostTestDriver createFailure;
	createFailure.m_bFailCreate = true;
	if ( sirRunWindowHostWithDriver( createFailure, { "SIREngine Editor", 1280, 720 } ) == 0 || createFailure.m_nShutdowns != 1 ||
	     createFailure.m_nDestroys != 0 ) {
		fputs( "Editor native window: create failure FAIL\n", stderr );
		return 1;
	}

	SIRNativeWindowHostTestDriver pumpFailure;
	pumpFailure.m_bFailPump = true;
	if ( sirRunWindowHostWithDriver( pumpFailure, { "SIREngine Editor", 1280, 720 } ) == 0 || pumpFailure.m_nPumps != 1 ||
	     pumpFailure.m_nDestroys != 1 || pumpFailure.m_nShutdowns != 1 ) {
		fputs( "Editor native window: pump failure FAIL\n", stderr );
		return 1;
	}
	puts( "SIRNativeWindowHostTests PASS" );
	return 0;
}
