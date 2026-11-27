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

#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#include <Core/Input/SIRInputService.h>
#include <stdio.h>

class SIRNativeWindowHostTestDriver final : public SIRWindowDriver
{
public:
	SIRNativeWindowHostTestDriver( void ) noexcept = default;
	~SIRNativeWindowHostTestDriver( void ) noexcept override = default;

	sirWindowResult initialize( void ) noexcept override
	{
		++m_nInitializes;
		return m_bFailInit ? sirWindowResult::SIR_WINDOW_NATIVE_FAILURE : sirWindowResult::SIR_WINDOW_OK;
	}
	sirWindowResult createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override
	{
		++m_nCreates;
		if ( m_bFailCreate || ( m_nFailCreateOn != 0 && m_nCreates == m_nFailCreateOn ) || desc.nWidth == 0 || desc.nHeight == 0 ||
		     pView == nullptr || nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
			return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
		}
		m_bAlive[nSlot] = true;
		*pView = { sirWindowPlatform::SIR_WINDOW_PLATFORM_X11, ( uintptr_t )( nSlot + 1 ), 1, 0 };
		return sirWindowResult::SIR_WINDOW_OK;
	}
	sirWindowResult destroyWindow( uint32_t nSlot ) noexcept override
	{
		if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || !m_bAlive[nSlot] ) {
			return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
		}
		m_bAlive[nSlot] = false;
		++m_nDestroys;
		return sirWindowResult::SIR_WINDOW_OK;
	}
	sirWindowResult mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override
	{
		( void )nSlot;
		( void )mutation;
		return sirWindowResult::SIR_WINDOW_UNSUPPORTED;
	}
	sirWindowResult pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override
	{
		if ( nBudget == 0 || pCallback == nullptr ) {
			return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
		}
		++m_nPumps;
		if ( m_bFailPump || m_nPumps == m_nFailPumpOn ) {
			return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
		}
		if ( m_bMinimizeBeforeClose && m_nPumps == 2 ) {
			pCallback( pContext, { 0, 1, sirWindowEventKind::SIR_WINDOW_EVENT_MINIMIZED, 0, 0 } );
			return sirWindowResult::SIR_WINDOW_OK;
		}
		if ( m_nPumps == 2 || m_nPumps == 3 ) {
			for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
				if ( m_bAlive[i] && ( m_nPumps == 3 || i == 0 ) ) {
					const sirWindowBackendEvent_t close = { i, ( uintptr_t )( i + 1 ), sirWindowEventKind::SIR_WINDOW_EVENT_CLOSE_REQUESTED, 0, 0 };
					pCallback( pContext, close );
				}
			}
		}
		return sirWindowResult::SIR_WINDOW_OK;
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
	bool m_bMinimizeBeforeClose = false;
	uint32_t m_nFailPumpOn = 0;

protected:
private:
};

static sirNativeWindowFrameResult sirNativeWindowHostStopFrame( const sirNativeWindowFrame_t&, void * ) noexcept
{
	return sirNativeWindowFrameResult::STOP;
}

struct sirHostInputTest_t
{
	SIRNativeWindowHostTestDriver *pDriver;
	uint32_t nFrames = 0;
	uint32_t nReleases = 0;
	bool bValid = true;
};

static bool sirHostBindInput( SIRInputService& input, const sirWindowNativeView_t& view, void *pContext ) noexcept
{
	auto& test = *static_cast<sirHostInputTest_t *>(pContext);
	test.bValid &= view.nNativeWindow == 1 && test.pDriver->m_nPumps == 0;
	input.text().setEnabled( true );
	input.text().setFocused( true );
	return true;
}

static bool sirHostCollectInput( SIRInputService& input, void * ) noexcept
{
	return input.text().appendCodepoint( 0x00e9 );
}

static sirNativeWindowFrameResult sirHostReadInput( const sirNativeWindowFrame_t& frame, void *pContext ) noexcept
{
	auto& test = *static_cast<sirHostInputTest_t *>(pContext);
	++test.nFrames;
	const sirInputFrame_t *pInputFrame = frame.pInput == nullptr ? nullptr : frame.pInput->frame();
	test.bValid &= pInputFrame != nullptr && pInputFrame->nTick == test.nFrames && frame.nDeltaSeconds > 0.0f;
	test.bValid &= frame.snapshot.bMinimized == ( test.nFrames == 2 ) && frame.native.nNativeWindow == 1;
	if ( frame.pInput != nullptr ) {
		const auto& text = frame.pInput->text().snapshot();
		test.bValid &= text.nTick == test.nFrames && text.nCount == 1 && text.szCodepoints[0] == 0x00e9;
	}
	return sirNativeWindowFrameResult::CONTINUE;
}

static void sirHostReleaseFrame( void *pContext ) noexcept
{
	auto& test = *static_cast<sirHostInputTest_t *>(pContext);
	test.bValid &= test.pDriver->m_bAlive[0] && test.pDriver->m_nDestroys == 0;
	++test.nReleases;
}

static bool sirHostChecksInputAndTeardown( const sirWindowCreateDesc_t& desc, bool bPumpError )
{
	SIRNativeWindowHostTestDriver driver;
	driver.m_bMinimizeBeforeClose = true;
	driver.m_nFailPumpOn = bPumpError ? 3 : 0;
	sirHostInputTest_t test{ &driver };
	const sirNativeWindowFrameCallbacks_t callbacks{ sirHostReadInput, sirHostReleaseFrame, &test };
	const sirNativeWindowInputHooks_t input{ sirHostBindInput, sirHostCollectInput, nullptr, nullptr, &test };
	return SIRNativeWindowHost::runWithDriverFrame( driver, &desc, 1, callbacks, &input ) == ( bPumpError ? 1 : 0 ) &&
	       test.bValid && test.nFrames == 2 && test.nReleases == 1 && driver.m_nDestroys == 1 && driver.m_nShutdowns == 1;
}

int main( void )
{
	SIRNativeWindowHostTestDriver successful;
	const sirWindowCreateDesc_t single{ "SIREngine Editor", 1280, 720 };
	if ( SIRNativeWindowHost::runWithDriver( successful, &single, 1 ) != 0 || successful.m_nInitializes != 1 ||
	     successful.m_nCreates != 1 || successful.m_nDestroys != 1 || successful.m_nPumps != 2 || successful.m_nShutdowns != 1 ) {
		fputs( "Editor native window: graceful close FAIL\n", stderr );
		return 1;
	}

	SIRNativeWindowHostTestDriver callbackStop;
	if ( SIRNativeWindowHost::runWithDriverFrame( callbackStop, &single, 1, { sirNativeWindowHostStopFrame } ) != 0 ||
	     callbackStop.m_nDestroys != 1 || callbackStop.m_nShutdowns != 1 ) {
		fputs( "Native frame callback cleanup FAIL\n", stderr );
		return 1;
	}
	if ( !sirHostChecksInputAndTeardown( single, false ) || !sirHostChecksInputAndTeardown( single, true ) ) {
		fputs( "Native input publication, minimized frame or terminal ordering FAIL\n", stderr );
		return 1;
	}

	sirWindowCreateDesc_t multi[] = { { "primary", 800, 450 }, { "auxiliary", 640, 360 } };
	SIRNativeWindowHostTestDriver multiple;
	if ( SIRNativeWindowHost::runWithDriver( multiple, multi, 2 ) != 0 || multiple.m_nCreates != 2 || multiple.m_nDestroys != 2 ||
	     multiple.m_nPumps != 3 || multiple.m_nShutdowns != 1 ) {
		fputs( "Native multiwindow lifecycle FAIL\n", stderr );
		return 1;
	}
	SIRNativeWindowHostTestDriver zero;
	if ( SIRNativeWindowHost::runWithDriver( zero, nullptr, 0 ) != 0 || zero.m_nCreates != 0 || zero.m_nShutdowns != 1 ) {
		return 1;
	}
	SIRNativeWindowHostTestDriver partialCreate;
	partialCreate.m_nFailCreateOn = 2;
	if ( SIRNativeWindowHost::runWithDriver( partialCreate, multi, 2 ) == 0 || partialCreate.m_nCreates != 2 || partialCreate.m_nDestroys != 1 ||
	     partialCreate.m_nShutdowns != 1 ) {
		return 1;
	}
	SIRNativeWindowHostTestDriver invalidCount;
	if ( SIRNativeWindowHost::runWithDriver( invalidCount, multi, SIR_WINDOW_MAX_WINDOWS + 1 ) == 0 || invalidCount.m_nInitializes != 0 ) {
		return 1;
	}

	SIRNativeWindowHostTestDriver initFailure;
	initFailure.m_bFailInit = true;
	if ( SIRNativeWindowHost::runWithDriver( initFailure, &single, 1 ) == 0 || initFailure.m_nCreates != 0 ||
	     initFailure.m_nDestroys != 0 ) {
		fputs( "Editor native window: init failure FAIL\n", stderr );
		return 1;
	}

	SIRNativeWindowHostTestDriver createFailure;
	createFailure.m_bFailCreate = true;
	if ( SIRNativeWindowHost::runWithDriver( createFailure, &single, 1 ) == 0 || createFailure.m_nShutdowns != 1 ||
	     createFailure.m_nDestroys != 0 ) {
		fputs( "Editor native window: create failure FAIL\n", stderr );
		return 1;
	}

	SIRNativeWindowHostTestDriver pumpFailure;
	pumpFailure.m_bFailPump = true;
	if ( SIRNativeWindowHost::runWithDriver( pumpFailure, &single, 1 ) == 0 || pumpFailure.m_nPumps != 1 ||
	     pumpFailure.m_nDestroys != 1 || pumpFailure.m_nShutdowns != 1 ) {
		fputs( "Editor native window: pump failure FAIL\n", stderr );
		return 1;
	}
	puts( "SIRNativeWindowHostTests PASS" );
	return 0;
}
