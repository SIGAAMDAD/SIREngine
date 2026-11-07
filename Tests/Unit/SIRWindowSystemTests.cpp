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
#include <Core/Platform/Window/Headless/SIRWindowDriverHeadless.h>
#include <Core/Platform/Window/SIRWindowSystem.h>
#include <stdio.h>
#include <thread>

class SIRWindowTestDriver final : public SIRWindowDriver
{
public:
	SIRWindowTestDriver( void ) noexcept = default;
	~SIRWindowTestDriver( void ) noexcept override = default;

	sirWindowResult_t initialize( void ) noexcept override
	{
		if ( m_bFailInitialize ) {
			m_bFailInitialize = false;
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		m_bRunning = true;
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override
	{
		( void )desc;
		if ( !m_bRunning || nSlot >= SIR_WINDOW_MAX_WINDOWS || m_bAlive[nSlot] ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		m_bAlive[nSlot] = true;
		*pView = { SIR_WINDOW_PLATFORM_X11, nSlot + 1, 0x1234, 0 };
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t destroyWindow( uint32_t nSlot ) noexcept override
	{
		if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || !m_bAlive[nSlot] ) {
			return SIR_WINDOW_INVALID_HANDLE;
		}
		m_bAlive[nSlot] = false;
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override
	{
		( void )nSlot;
		( void )mutation;
		return SIR_WINDOW_OK;
	}

	sirWindowResult_t pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override
	{
		if ( !m_bRunning ) {
			return SIR_WINDOW_STOPPED;
		}
		const uint32_t nCount = m_nPending < nBudget ? m_nPending : nBudget;
		for ( uint32_t i = 0; i < nCount; ++i ) {
			pCallback( pContext, m_Pending[i] );
		}
		m_nPending -= nCount;
		for ( uint32_t i = 0; i < m_nPending; ++i ) {
			m_Pending[i] = m_Pending[i + nCount];
		}
		return SIR_WINDOW_OK;
	}
	void shutdown( void ) noexcept override
	{
		m_bRunning = false;
	}
	void queueEvent( sirWindowBackendEvent_t event ) noexcept
	{
		if ( m_nPending < SIR_WINDOW_EVENT_CAPACITY ) {
			m_Pending[m_nPending++] = event;
		}
	}
	bool m_bFailInitialize = false;

protected:
private:
	bool m_bRunning = false;
	bool m_bAlive[SIR_WINDOW_MAX_WINDOWS] = {};
	sirWindowBackendEvent_t m_Pending[SIR_WINDOW_EVENT_CAPACITY] = {};
	uint32_t m_nPending = 0;
};

static bool sirTestHeadless( void ) noexcept
{
	SIRWindowDriverHeadless driver;
	SIRWindowSystem system( driver );
	sirWindowHandle_t handle = {};
	const sirWindowCreateDesc_t desc = { "headless", 800, 450 };
	if ( system.createWindow( desc, &handle ) != SIR_WINDOW_STOPPED || system.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK || system.createWindow( desc, &handle ) != SIR_WINDOW_UNSUPPORTED ||
	     system.shutdown() != SIR_SYSTEM_OK || system.pumpEvents( 1 ) != SIR_WINDOW_STOPPED ) {
		return false;
	}
	return true;
}

static bool sirTestHandlesLeasesAndEvents( void ) noexcept
{
	SIRWindowTestDriver driver;
	driver.m_bFailInitialize = true;
	SIRWindowSystem system( driver );
	if ( system.initialize() != SIR_SYSTEM_ERROR || system.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	const sirWindowCreateDesc_t desc = { "window", 800, 450 };
	sirWindowHandle_t handles[SIR_WINDOW_MAX_WINDOWS] = {};
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( system.createWindow( desc, &handles[i] ) != SIR_WINDOW_OK ) {
			return false;
		}
	}
	sirWindowHandle_t invalid = {};
	if ( system.createWindow( desc, &invalid ) != SIR_WINDOW_OUT_OF_CAPACITY || system.shutdown() != SIR_SYSTEM_BUSY ||
	     system.pumpEvents( 0 ) != SIR_WINDOW_INVALID_ARGUMENT ) {
		return false;
	}
	const sirWindowMutation_t resize = { SIR_WINDOW_MUTATION_RESIZE, nullptr, 1024, 768 };
	const sirWindowMutation_t badResize = { SIR_WINDOW_MUTATION_RESIZE, nullptr, 0, 768 };
	const sirWindowMutation_t badTitle = { SIR_WINDOW_MUTATION_TITLE, nullptr, 0, 0 };
	if ( system.mutateWindow( handles[0], resize ) != SIR_WINDOW_OK ||
	     system.mutateWindow( handles[0], badResize ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     system.mutateWindow( handles[0], badTitle ) != SIR_WINDOW_INVALID_ARGUMENT ) {
		return false;
	}
	// Renderer-style readers may query while the platform owner changes metrics.
	uint32_t nReadFailures = 0;
	std::thread reader( [&]() {
		for ( uint32_t i = 0; i < 10000; ++i ) {
			sirWindowSnapshot_t state = {};
			if ( system.querySnapshot( handles[1], &state ) != SIR_WINDOW_OK || state.nWindowGeneration == 0 ||
			     state.hWindow.nGeneration != handles[1].nGeneration ) {
				++nReadFailures;
			}
		}
	} );
	for ( uint32_t i = 0; i < 12; ++i ) {
		driver.queueEvent( { handles[1].nSlot, handles[1].nSlot + 1, SIR_WINDOW_EVENT_RESIZED, 100 + i, 200 + i } );
		if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK ) {
			reader.join();
			return false;
		}
	}
	reader.join();
	if ( nReadFailures != 0 ) {
		return false;
	}
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t native = {};
	if ( system.acquireNativeLease( handles[0], &lease, &native ) != SIR_WINDOW_OK || native.nWindowGeneration == 0 ||
	     system.destroyWindow( handles[0] ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	sirWindowResult_t eThreadResult = SIR_WINDOW_OK;
	std::thread worker( [&]() { eThreadResult = system.requestClose( handles[0] ); } );
	worker.join();
	if ( eThreadResult != SIR_WINDOW_WRONG_THREAD ) {
		return false;
	}
	std::thread render( [&]() { eThreadResult = system.releaseNativeLease( lease ); } );
	render.join();
	if ( eThreadResult != SIR_WINDOW_OK || system.releaseNativeLease( lease ) != SIR_WINDOW_INVALID_HANDLE ||
	     system.destroyWindow( handles[0] ) != SIR_WINDOW_OK ||
	     system.querySnapshot( handles[0], nullptr ) != SIR_WINDOW_INVALID_ARGUMENT ) {
		return false;
	}
	sirWindowHandle_t recycled = {};
	if ( system.createWindow( desc, &recycled ) != SIR_WINDOW_OK || recycled.nSlot != handles[0].nSlot ||
	     recycled.nGeneration == handles[0].nGeneration || system.requestClose( handles[0] ) != SIR_WINDOW_INVALID_HANDLE ) {
		return false;
	}
	driver.queueEvent( { recycled.nSlot, recycled.nSlot + 1, SIR_WINDOW_EVENT_RESIZED, 0, 0 } );
	driver.queueEvent( { recycled.nSlot, recycled.nSlot + 1, SIR_WINDOW_EVENT_CLOSE_REQUESTED, 0, 0 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t snapshot = {};
	if ( system.querySnapshot( recycled, &snapshot ) != SIR_WINDOW_OK || snapshot.bReady || snapshot.nMetricsRevision != 2 ||
	     system.acquireNativeLease( recycled, &lease, &native ) != SIR_WINDOW_NOT_READY ) {
		return false;
	}
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK || system.querySnapshot( recycled, &snapshot ) != SIR_WINDOW_OK ||
	     !snapshot.bCloseRequested ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		const sirWindowHandle_t h = i == 0 ? recycled : handles[i];
		if ( system.destroyWindow( h ) != SIR_WINDOW_OK ) {
			return false;
		}
	}
	sirWindowEvent_t events[SIR_WINDOW_EVENT_CAPACITY] = {};
	sirWindowEventRead_t read = { 0, events, SIR_WINDOW_EVENT_CAPACITY, 0, false };
	if ( system.readEvents( &read ) != SIR_WINDOW_OK || read.nCount == 0 || read.bLost || system.shutdown() != SIR_SYSTEM_OK ||
	     system.initialize() != SIR_SYSTEM_OK || system.requestClose( recycled ) != SIR_WINDOW_INVALID_HANDLE ) {
		return false;
	}
	if ( system.createWindow( desc, &recycled ) != SIR_WINDOW_OK || system.requestClose( recycled ) != SIR_WINDOW_OK ||
	     system.requestClose( recycled ) != SIR_WINDOW_OK || system.destroyWindow( recycled ) != SIR_WINDOW_OK ||
	     system.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestEventOverflow( void ) noexcept
{
	SIRWindowTestDriver driver;
	SIRWindowSystem system( driver );
	if ( system.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	const sirWindowCreateDesc_t desc = { "many", 100, 100 };
	sirWindowHandle_t handle = {};
	for ( uint32_t i = 0; i < SIR_WINDOW_EVENT_CAPACITY; ++i ) {
		if ( system.createWindow( desc, &handle ) != SIR_WINDOW_OK || system.destroyWindow( handle ) != SIR_WINDOW_OK ) {
			return false;
		}
	}
	sirWindowEvent_t events[2] = {};
	sirWindowEventRead_t read = { 0, events, 2, 0, false };
	if ( system.readEvents( &read ) != SIR_WINDOW_EVENTS_LOST || !read.bLost || read.nCount != 2 || events[0].nSequence <= 1 ||
	     system.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestHeadless() || !sirTestHandlesLeasesAndEvents() || !sirTestEventOverflow() ) {
		fputs( "SIRWindowSystemTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowSystemTests PASS" );
	return 0;
}
