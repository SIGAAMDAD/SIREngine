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
#include <Core/Platform/SIRThread.h>

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
		*pView = { m_ePlatform, nSlot + 1, 0x1234, 0 };
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
	sirWindowPlatform_t m_ePlatform = SIR_WINDOW_PLATFORM_X11;

protected:
private:
	bool m_bRunning = false;
	bool m_bAlive[SIR_WINDOW_MAX_WINDOWS] = {};
	sirWindowBackendEvent_t m_Pending[SIR_WINDOW_EVENT_CAPACITY] = {};
	uint32_t m_nPending = 0;
};

typedef struct sirWindowReadWorkerContext_s
{
	SIRWindowSystem *pSystem;
	sirWindowHandle_t hWindow;
	uint32_t nFailures;
} sirWindowReadWorkerContext_t;

static void sirWindowReadWorker( void *pUserData )
{
	sirWindowReadWorkerContext_t *pContext = ( sirWindowReadWorkerContext_t * )pUserData;
	for ( uint32_t i = 0; i < 10000; ++i ) {
		sirWindowSnapshot_t state = {};
		if ( pContext->pSystem->querySnapshot( pContext->hWindow, &state ) != SIR_WINDOW_OK || state.nWindowGeneration == 0 ||
		     state.hWindow.nGeneration != pContext->hWindow.nGeneration ) {
			++pContext->nFailures;
		}
	}
}

typedef struct sirWindowLeaseWorkerContext_s
{
	SIRWindowSystem *pSystem;
	sirWindowHandle_t hWindow;
	sirWindowNativeLease_t hLease;
	sirWindowResult_t eResult;
} sirWindowLeaseWorkerContext_t;

static void sirWindowRequestCloseWorker( void *pUserData )
{
	sirWindowLeaseWorkerContext_t *pContext = ( sirWindowLeaseWorkerContext_t * )pUserData;
	pContext->eResult = pContext->pSystem->requestClose( pContext->hWindow );
}

static void sirWindowReleaseLeaseWorker( void *pUserData )
{
	sirWindowLeaseWorkerContext_t *pContext = ( sirWindowLeaseWorkerContext_t * )pUserData;
	pContext->eResult = pContext->pSystem->releaseNativeLease( pContext->hLease );
}

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
	driver.queueEvent( { handles[1].nSlot, handles[1].nSlot + 1, SIR_WINDOW_EVENT_DPI_CHANGED, 144, 0 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t dpiSnapshot = {};
	if ( system.querySnapshot( handles[1], &dpiSnapshot ) != SIR_WINDOW_OK || dpiSnapshot.nDpi != 144 ) {
		return false;
	}
	driver.queueEvent( { handles[1].nSlot, handles[1].nSlot + 1, SIR_WINDOW_EVENT_HIDDEN, 0, 0 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t hidden = {};
	sirWindowNativeLease_t hiddenLease = {};
	sirWindowNativeView_t hiddenView = {};
	if ( system.querySnapshot( handles[1], &hidden ) != SIR_WINDOW_OK || hidden.bVisible || hidden.bReady ||
	     system.acquireNativeLease( handles[1], &hiddenLease, &hiddenView ) != SIR_WINDOW_NOT_READY ) {
		return false;
	}
	driver.queueEvent( { handles[1].nSlot, handles[1].nSlot + 1, SIR_WINDOW_EVENT_SHOWN, 0, 0 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK || system.querySnapshot( handles[1], &hidden ) != SIR_WINDOW_OK || !hidden.bVisible ||
	     !hidden.bReady ) {
		return false;
	}
	// Renderer-style readers may query while the platform owner changes metrics.
	sirWindowReadWorkerContext_t readContext = { &system, handles[1], 0 };
	SIRThread reader;
	if ( !reader.start( sirWindowReadWorker, &readContext ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < 12; ++i ) {
		driver.queueEvent( { handles[1].nSlot, handles[1].nSlot + 1, SIR_WINDOW_EVENT_RESIZED, 100 + i, 200 + i } );
		if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK ) {
			( void )reader.join();
			return false;
		}
	}
	if ( !reader.join() || readContext.nFailures != 0 ) {
		return false;
	}
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t native = {};
	if ( system.acquireNativeLease( handles[0], &lease, &native ) != SIR_WINDOW_OK || native.nWindowGeneration == 0 ||
	     system.destroyWindow( handles[0] ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	sirWindowLeaseWorkerContext_t threadContext = { &system, handles[0], lease, SIR_WINDOW_OK };
	SIRThread worker;
	if ( !worker.start( sirWindowRequestCloseWorker, &threadContext ) || !worker.join() ||
	     threadContext.eResult != SIR_WINDOW_WRONG_THREAD ) {
		return false;
	}
	SIRThread render;
	if ( !render.start( sirWindowReleaseLeaseWorker, &threadContext ) || !render.join() ) {
		return false;
	}
	if ( threadContext.eResult != SIR_WINDOW_OK || system.releaseNativeLease( lease ) != SIR_WINDOW_INVALID_HANDLE ||
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
	read.nAfterSequence = 0;
	if ( system.readEvents( &read ) != SIR_WINDOW_OK || read.nCount != 0 ) {
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

static bool sirTestWaylandInitialConfigure( void ) noexcept
{
	SIRWindowTestDriver driver;
	driver.m_ePlatform = SIR_WINDOW_PLATFORM_WAYLAND;
	SIRWindowSystem system( driver );
	if ( system.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	const sirWindowCreateDesc_t desc = { "pending configure", 640, 480 };
	sirWindowHandle_t handle = {};
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t native = {};
	sirWindowSnapshot_t state = {};
	if ( system.createWindow( desc, &handle ) != SIR_WINDOW_OK || system.querySnapshot( handle, &state ) != SIR_WINDOW_OK || state.bReady ||
	     state.bVisible || system.acquireNativeLease( handle, &lease, &native ) != SIR_WINDOW_NOT_READY ) {
		return false;
	}
	driver.queueEvent( { handle.nSlot, handle.nSlot + 1, SIR_WINDOW_EVENT_READY, 800, 600 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK || system.querySnapshot( handle, &state ) != SIR_WINDOW_OK || !state.bReady ||
	     !state.bVisible || state.nWidth != 800 || state.nHeight != 600 || state.nMetricsRevision != 2 ||
	     system.acquireNativeLease( handle, &lease, &native ) != SIR_WINDOW_OK ) {
		return false;
	}
	driver.queueEvent( { handle.nSlot, handle.nSlot + 1, SIR_WINDOW_EVENT_HIDDEN, 0, 0 } );
	driver.queueEvent( { handle.nSlot, handle.nSlot + 1, SIR_WINDOW_EVENT_RESIZED, 1024, 768 } );
	if ( system.pumpEvents( 2 ) != SIR_WINDOW_OK || system.querySnapshot( handle, &state ) != SIR_WINDOW_OK || state.bReady ||
	     state.bVisible || system.acquireNativeLease( handle, &lease, &native ) != SIR_WINDOW_NOT_READY || state.nMetricsRevision != 3 ) {
		return false;
	}
	if ( system.destroyWindow( handle ) != SIR_WINDOW_BUSY || system.releaseNativeLease( lease ) != SIR_WINDOW_OK ||
	     system.destroyWindow( handle ) != SIR_WINDOW_OK || system.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestHeadless() || !sirTestHandlesLeasesAndEvents() || !sirTestEventOverflow() || !sirTestWaylandInitialConfigure() ) {
		fputs( "SIRWindowSystemTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowSystemTests PASS" );
	return 0;
}
