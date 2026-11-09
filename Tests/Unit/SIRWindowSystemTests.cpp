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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <Core/Platform/SIRThread.h>

typedef struct sirWindowWrongLaneCallbackContext_s
{
	sirWindowDriverEventFn_t pCallback;
	void *pUserData;
	sirWindowBackendEvent_t event;
} sirWindowWrongLaneCallbackContext_t;

static void sirWindowInvokeForeignCallback( void *pUserData )
{
	sirWindowWrongLaneCallbackContext_t *pInvocation = ( sirWindowWrongLaneCallbackContext_t * )pUserData;
	pInvocation->pCallback( pInvocation->pUserData, pInvocation->event );
}

class SIRWindowTestDriver final : public SIRWindowDriver
{
public:
	SIRWindowTestDriver( void ) noexcept = default;
	~SIRWindowTestDriver( void ) noexcept override = default;

	sirWindowResult_t initialize( void ) noexcept override
	{
		if ( m_bRunning ) {
			return SIR_WINDOW_BUSY;
		}
		// Simulate a backend that acquires its OS session before setup fails.
		m_bRunning = true;
		if ( m_bFailInitialize ) {
			m_bFailInitialize = false;
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override
	{
		( void )desc;
		if ( !m_bRunning || nSlot >= SIR_WINDOW_MAX_WINDOWS || m_bAlive[nSlot] ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		m_bAlive[nSlot] = true;
		if ( desc.pszTitle != nullptr ) {
			( void )snprintf( m_szCreatedTitle, sizeof( m_szCreatedTitle ), "%s", desc.pszTitle );
		}
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
		++m_nMutations;
		if ( mutation.eKind == SIR_WINDOW_MUTATION_TITLE && mutation.pszTitle != nullptr ) {
			uint32_t n = 0;
			while ( n < sizeof( m_szLastTitle ) - 1 && mutation.pszTitle[n] != '\0' ) {
				m_szLastTitle[n] = mutation.pszTitle[n];
				++n;
			}
			m_szLastTitle[n] = '\0';
		}
		return SIR_WINDOW_OK;
	}

	sirWindowResult_t pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override
	{
		if ( !m_bRunning ) {
			return SIR_WINDOW_STOPPED;
		}
		const uint32_t nCount = m_nPending < nBudget ? m_nPending : nBudget;
		for ( uint32_t i = 0; i < nCount; ++i ) {
			if ( m_bDispatchOnForeignThread ) {
				sirWindowWrongLaneCallbackContext_t callback = { pCallback, pContext, m_Pending[i] };
				SIRThread worker;
				if ( !worker.start( sirWindowInvokeForeignCallback, &callback ) || !worker.join() ) {
					return SIR_WINDOW_NATIVE_FAILURE;
				}
			} else {
				pCallback( pContext, m_Pending[i] );
			}
		}
		m_nPending -= nCount;
		for ( uint32_t i = 0; i < m_nPending; ++i ) {
			m_Pending[i] = m_Pending[i + nCount];
		}
		return SIR_WINDOW_OK;
	}
	void shutdown( void ) noexcept override
	{
		++m_nShutdowns;
		m_bRunning = false;
	}
	void queueEvent( sirWindowBackendEvent_t event ) noexcept
	{
		if ( m_nPending < SIR_WINDOW_EVENT_CAPACITY ) {
			m_Pending[m_nPending++] = event;
		}
	}
	uint32_t m_nMutations = 0;
	uint32_t m_nShutdowns = 0;
	char m_szLastTitle[128] = {};
	char m_szCreatedTitle[SIR_WINDOW_TITLE_CAPACITY] = {};
	bool m_bFailInitialize = false;
	bool m_bDispatchOnForeignThread = false;
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

typedef struct sirQueuedMutationWorkerContext_s
{
	SIRWindowSystem *pSystem;
	sirWindowHandle_t hWindow;
	sirWindowMutation_t mutation;
	uint64_t nTicket;
	sirWindowResult_t eResult;
	bool bClose;
} sirQueuedMutationWorkerContext_t;

static void sirQueueMutationWorker( void *pUserData )
{
	sirQueuedMutationWorkerContext_t *pContext = ( sirQueuedMutationWorkerContext_t * )pUserData;
	pContext->eResult = pContext->bClose ? pContext->pSystem->queueClose( pContext->hWindow, &pContext->nTicket )
	                                     : pContext->pSystem->queueMutation( pContext->hWindow, pContext->mutation, &pContext->nTicket );
}

static bool sirTestQueuedOwnerMutations( void ) noexcept
{
	SIRWindowTestDriver driver;
	SIRWindowSystem windows( driver );
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	sirWindowHandle_t hWindow = {};
	if ( windows.createWindow( { "queued", 640, 480 }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	char szTitle[32] = "owner-owned queued title";
	const sirWindowMutation_t mutation = { SIR_WINDOW_MUTATION_TITLE, szTitle, 0, 0 };
	sirQueuedMutationWorkerContext_t context = { &windows, hWindow, mutation, 0, SIR_WINDOW_STOPPED, false };
	SIRThread worker;
	if ( !worker.start( sirQueueMutationWorker, &context ) || !worker.join() || context.eResult != SIR_WINDOW_OK || context.nTicket == 0 ) {
		return false;
	}
	uint64_t nTicket = context.nTicket;
	sirWindowResult_t eResult = SIR_WINDOW_STOPPED;
	bool bComplete = true;
	if ( windows.queryRequest( nTicket, &eResult, &bComplete ) != SIR_WINDOW_OK || bComplete ) {
		return false;
	}
	szTitle[0] = 'X'; // The platform request must retain its own UTF-8 copy.
	if ( windows.pumpEvents( 16 ) != SIR_WINDOW_OK || driver.m_nMutations != 1 ||
	     strcmp( driver.m_szLastTitle, "owner-owned queued title" ) != 0 ||
	     windows.queryRequest( nTicket, &eResult, &bComplete ) != SIR_WINDOW_OK || !bComplete || eResult != SIR_WINDOW_OK ||
	     windows.queryRequest( nTicket, &eResult, &bComplete ) != SIR_WINDOW_INVALID_HANDLE ) {
		return false;
	}
	// A pending request against a retired slot must fail after recycling.
	uint64_t nStale = 0;
	if ( windows.queueMutation( hWindow, mutation, &nStale ) != SIR_WINDOW_OK || windows.destroyWindow( hWindow ) != SIR_WINDOW_OK ||
	     windows.pumpEvents( 1 ) != SIR_WINDOW_OK || windows.queryRequest( nStale, &eResult, &bComplete ) != SIR_WINDOW_OK || !bComplete ||
	     eResult != SIR_WINDOW_INVALID_HANDLE ) {
		return false;
	}
	if ( windows.createWindow( { "again", 640, 480 }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	uint64_t tickets[SIR_WINDOW_REQUEST_CAPACITY] = {};
	for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
		if ( windows.queueMutation( hWindow, { SIR_WINDOW_MUTATION_RESIZE, nullptr, 640 + i, 480 + i }, &tickets[i] ) != SIR_WINDOW_OK ) {
			return false;
		}
	}
	uint64_t extra = 0;
	if ( windows.queueClose( hWindow, &extra ) != SIR_WINDOW_OUT_OF_CAPACITY || windows.pumpEvents( 32 ) != SIR_WINDOW_OK ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
		if ( windows.queryRequest( tickets[i], &eResult, &bComplete ) != SIR_WINDOW_OK || !bComplete || eResult != SIR_WINDOW_OK ) {
			return false;
		}
	}
	context.hWindow = hWindow;
	context.bClose = true;
	context.nTicket = 0;
	SIRThread closer;
	if ( !closer.start( sirQueueMutationWorker, &context ) || !closer.join() || context.eResult != SIR_WINDOW_OK ||
	     windows.pumpEvents( 16 ) != SIR_WINDOW_OK || windows.queryRequest( context.nTicket, &eResult, &bComplete ) != SIR_WINDOW_OK ||
	     !bComplete || eResult != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t snapshot = {};
	if ( windows.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK || !snapshot.bCloseRequested || snapshot.bReady ||
	     windows.queueMutation( hWindow, mutation, &extra ) != SIR_WINDOW_BUSY || windows.destroyWindow( hWindow ) != SIR_WINDOW_OK ||
	     windows.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestForeignDriverCallbacksCannotPublish( void ) noexcept
{
	SIRWindowTestDriver driver;
	SIRWindowSystem system( driver );
	if ( system.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	sirWindowHandle_t hWindow = {};
	if ( system.createWindow( { "driver-owner", 640, 480 }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowEvent_t events[4] = {};
	sirWindowEventRead_t read = { 0, events, 4, 0, false };
	if ( system.readEvents( &read ) != SIR_WINDOW_OK || read.nCount != 2 ) {
		return false;
	}
	const uint64_t nCursor = read.nAfterSequence;
	driver.m_bDispatchOnForeignThread = true;
	driver.queueEvent( { hWindow.nSlot, hWindow.nSlot + 1, SIR_WINDOW_EVENT_RESIZED, 777, 555 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_WRONG_THREAD ) {
		return false;
	}
	sirWindowSnapshot_t snapshot = {};
	read.nCount = 0;
	if ( system.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK || snapshot.nWidth != 640 || snapshot.nHeight != 480 ||
	     system.readEvents( &read ) != SIR_WINDOW_OK || read.nCount != 0 || read.nAfterSequence != nCursor ) {
		return false;
	}
	assert( snapshot.nWidth == 640 ); // Foreign OS dispatch cannot mutate native window state.
	driver.m_bDispatchOnForeignThread = false;
	driver.queueEvent( { hWindow.nSlot, hWindow.nSlot + 1, SIR_WINDOW_EVENT_RESIZED, 777, 555 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK || system.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK || snapshot.nWidth != 777 ||
	     snapshot.nHeight != 555 || system.destroyWindow( hWindow ) != SIR_WINDOW_OK || system.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestBoundedUtf8WindowTitles( void ) noexcept
{
	SIRWindowTestDriver driver;
	SIRWindowSystem windows( driver );
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	char szNoNul[SIR_WINDOW_TITLE_CAPACITY];
	memset( szNoNul, 'X', sizeof( szNoNul ) );
	sirWindowHandle_t hWindow = {};
	if ( windows.createWindow( { szNoNul, 640, 480, 0 }, &hWindow ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     windows.createWindow( { szNoNul, 640, 480, SIR_WINDOW_TITLE_CAPACITY }, &hWindow ) != SIR_WINDOW_INVALID_ARGUMENT ) {
		return false;
	}
	// Explicit byte lengths must accept a valid UTF-8 buffer with no NUL.
	const char szJapanese[] = { 'E', ( char )0xE6, ( char )0x97, ( char )0xA5 };
	if ( windows.createWindow( { szJapanese, 640, 480, sizeof( szJapanese ) }, &hWindow ) != SIR_WINDOW_OK ||
	     strcmp( driver.m_szCreatedTitle, "E\xE6\x97\xA5" ) != 0 || windows.destroyWindow( hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	const char szOverlong[] = { ( char )0xC0, ( char )0xAF, 0 };
	const char szSurrogate[] = { ( char )0xED, ( char )0xA0, ( char )0x80, 0 };
	const char szTruncated[] = { ( char )0xF0, ( char )0x9F, 0 };
	const char szTooLarge[] = { ( char )0xF4, ( char )0x90, ( char )0x80, ( char )0x80, 0 };
	if ( windows.createWindow( { szOverlong, 100, 100 }, &hWindow ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     windows.createWindow( { szSurrogate, 100, 100 }, &hWindow ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     windows.createWindow( { szTruncated, 100, 100 }, &hWindow ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     windows.createWindow( { szTooLarge, 100, 100 }, &hWindow ) != SIR_WINDOW_INVALID_ARGUMENT ) {
		return false;
	}
	if ( windows.createWindow( { "valid", 100, 100 }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	const sirWindowMutation_t invalid = { SIR_WINDOW_MUTATION_TITLE, szOverlong, 0, 0 };
	const sirWindowMutation_t valid = { SIR_WINDOW_MUTATION_TITLE, "A\xC3\xA9", 0, 0 };
	uint64_t nTicket = 0;
	if ( windows.mutateWindow( hWindow, invalid ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     windows.queueMutation( hWindow, invalid, &nTicket ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     windows.mutateWindow( hWindow, valid ) != SIR_WINDOW_OK || strcmp( driver.m_szLastTitle, "A\xC3\xA9" ) != 0 ||
	     windows.queueMutation( hWindow, valid, &nTicket ) != SIR_WINDOW_OK || windows.pumpEvents( 1 ) != SIR_WINDOW_OK ||
	     strcmp( driver.m_szLastTitle, "A\xC3\xA9" ) != 0 ) {
		return false;
	}
	assert( strcmp( driver.m_szLastTitle, "A\xC3\xA9" ) == 0 );
	return windows.destroyWindow( hWindow ) == SIR_WINDOW_OK && windows.shutdown() == SIR_SYSTEM_OK;
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
	if ( system.initialize() != SIR_SYSTEM_ERROR ) {
		return false;
	}
	assert( driver.m_nShutdowns == 1 ); // Partial native initialization must be rolled back.
	if ( system.initialize() != SIR_SYSTEM_OK ) {
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
	     system.destroyWindow( handles[0] ) != SIR_WINDOW_BUSY || system.requestClose( handles[0] ) != SIR_WINDOW_OK ||
	     system.acquireNativeLease( handles[0], &lease, &native ) != SIR_WINDOW_NOT_READY ||
	     system.mutateWindow( handles[0], resize ) != SIR_WINDOW_BUSY ) {
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
	     system.requestClose( recycled ) != SIR_WINDOW_OK ||
	     system.acquireNativeLease( recycled, &lease, &native ) != SIR_WINDOW_NOT_READY ||
	     system.mutateWindow( recycled, resize ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	// Stale backend geometry/show events must never resurrect a closing window.
	driver.queueEvent( { recycled.nSlot, recycled.nSlot + 1, SIR_WINDOW_EVENT_RESIZED, 800, 600 } );
	driver.queueEvent( { recycled.nSlot, recycled.nSlot + 1, SIR_WINDOW_EVENT_SHOWN, 0, 0 } );
	driver.queueEvent( { recycled.nSlot, recycled.nSlot + 1, SIR_WINDOW_EVENT_RESTORED, 0, 0 } );
	if ( system.pumpEvents( 3 ) != SIR_WINDOW_OK || system.querySnapshot( recycled, &snapshot ) != SIR_WINDOW_OK || snapshot.bReady ||
	     !snapshot.bCloseRequested || system.acquireNativeLease( recycled, &lease, &native ) != SIR_WINDOW_NOT_READY ||
	     system.destroyWindow( recycled ) != SIR_WINDOW_OK || system.shutdown() != SIR_SYSTEM_OK ) {
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
	     read.nAfterSequence != events[1].nSequence || system.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestTerminalNativeLoss( void ) noexcept
{
	SIRWindowTestDriver driver;
	SIRWindowSystem system( driver );
	if ( system.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	sirWindowHandle_t hWindow = {};
	if ( system.createWindow( { "lost-native", 640, 480 }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t before = {};
	sirWindowNativeView_t view = {};
	sirWindowNativeLease_t lease = {};
	if ( system.querySnapshot( hWindow, &before ) != SIR_WINDOW_OK || before.nWindowGeneration == 0 ||
	     system.acquireNativeLease( hWindow, &lease, &view ) != SIR_WINDOW_OK ) {
		return false;
	}
	// Native destruction can arrive before the renderer retires the GPU
	// surface. Previously the published snapshot retained a stale identity.
	driver.queueEvent( { hWindow.nSlot, view.nNativeWindow, SIR_WINDOW_EVENT_LOST, 0, 0 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t after = {};
	if ( system.querySnapshot( hWindow, &after ) != SIR_WINDOW_OK || after.nWindowGeneration != 0 || after.bReady || after.bVisible ||
	     !after.bCloseRequested || after.nMetricsRevision != before.nMetricsRevision + 1 ||
	     system.acquireNativeLease( hWindow, &lease, &view ) != SIR_WINDOW_NOT_READY ||
	     system.destroyWindow( hWindow ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	// A delayed event from the lost OS handle must not restore readiness.
	driver.queueEvent( { hWindow.nSlot, view.nNativeWindow, SIR_WINDOW_EVENT_READY, 640, 480 } );
	if ( system.pumpEvents( 1 ) != SIR_WINDOW_OK || system.querySnapshot( hWindow, &after ) != SIR_WINDOW_OK ||
	     after.nWindowGeneration != 0 || after.bReady ) {
		return false;
	}
	assert( after.nWindowGeneration == 0 ); // Terminal native loss invalidates the renderer-facing identity.
	if ( system.releaseNativeLease( lease ) != SIR_WINDOW_OK || system.destroyWindow( hWindow ) != SIR_WINDOW_OK ||
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
	if ( !sirTestHeadless() || !sirTestHandlesLeasesAndEvents() || !sirTestEventOverflow() || !sirTestWaylandInitialConfigure() ||
	     !sirTestQueuedOwnerMutations() || !sirTestTerminalNativeLoss() || !sirTestForeignDriverCallbacksCannotPublish() ||
	     !sirTestBoundedUtf8WindowTitles() ) {
		fputs( "SIRWindowSystemTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowSystemTests PASS" );
	return 0;
}
