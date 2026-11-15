#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Core/Platform/SIRThread.h>
#include <Renderer/SIRGraphicsWindowBridge.h>
#include <stdio.h>

class SIRTestWindowDriver final : public SIRWindowDriver
{
public:
	bool initialize( SIRWindowSystem& system ) noexcept override;
	void shutdown( void ) noexcept override;
	bool create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept override;
	void destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept override;
	bool pump( void ) noexcept override;
	bool bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept override;
	SIRWindowSystem *m_pSystem = nullptr;
	uint32_t m_nCreates = 0;
	uint32_t m_nDestroys = 0;
	bool m_bFailInitialize = false;
	bool m_bFailCreate = false;
	bool m_bFailPump = false;
};

bool SIRTestWindowDriver::initialize( SIRWindowSystem& system ) noexcept
{
	m_pSystem = &system;
	return !m_bFailInitialize;
}

void SIRTestWindowDriver::shutdown( void ) noexcept
{
	m_pSystem = nullptr;
}

bool SIRTestWindowDriver::create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept
{
	( void )desc;
	if ( m_bFailCreate ) {
		return false;
	}
	*pView = { SIR_WINDOW_PLATFORM_X11, ( void * )( uintptr_t )( 100 + hWindow.nSlot ), ( void * )( uintptr_t )22, 0 };
	++m_nCreates;
	return true;
}

void SIRTestWindowDriver::destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept
{
	( void )hWindow;
	( void )view;
	++m_nDestroys;
}

bool SIRTestWindowDriver::pump( void ) noexcept
{
	return !m_bFailPump;
}

bool SIRTestWindowDriver::bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept
{
	( void )hWindow;
	( void )input;
	( void )backend;
	return false;
}

typedef struct sirWindowWorkerContext_s
{
	SIRWindowSystem *pWindows;
	sirWindowHandle_t hWindow;
	sirWindowLease_t lease;
	bool bReleased;
	bool bOwnerRejected;
} sirWindowWorkerContext_t;

static void sirWindowWorker( void *pContext )
{
	sirWindowWorkerContext_t *pWorker = ( sirWindowWorkerContext_t * )pContext;
	pWorker->bOwnerRejected = pWorker->pWindows->requestClose( pWorker->hWindow ) == SIR_WINDOW_WRONG_THREAD;
	pWorker->bReleased = pWorker->pWindows->releaseNativeView( pWorker->lease );
}

static bool sirTestWindowStateAndLeases( void )
{
	SIRTestWindowDriver driver;
	SIRWindowSystem windows;
	if ( windows.initialize( driver ) != SIR_WINDOW_OK || windows.initialize( driver ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	sirWindowHandle_t hWindow = {};
	const sirWindowDesc_t desc = { "test", 640, 480, false };
	if ( windows.create( { nullptr, 640, 480, false }, &hWindow ) != SIR_WINDOW_INVALID ||
	     windows.create( desc, &hWindow ) != SIR_WINDOW_OK || hWindow.nGeneration == 0 || windows.activeWindowCount() != 1 ) {
		return false;
	}
	sirWindowSnapshot_t snapshot = {};
	if ( !windows.snapshot( hWindow, &snapshot ) || snapshot.nRevision == 0 || snapshot.nWidth != 640 ||
	     snapshot.eState != SIR_WINDOW_READY || snapshot.bVisible || windows.notifyResize( hWindow, 320, 240 ) == false ||
	     windows.notifyFocus( hWindow, true ) == false ) {
		return false;
	}
	sirGraphicsWindowBorrow_t borrow = {};
	if ( !sirGraphicsAcquireWindowSurface( windows, hWindow, &borrow ) || borrow.surface.ePlatform != SIR_GRAPHICS_SURFACE_X11 ||
	     borrow.surface.nWindowGeneration != hWindow.nGeneration || borrow.surface.pDisplayOrInstance != ( void * )( uintptr_t )22 ) {
		return false;
	}
	if ( windows.requestClose( hWindow ) != SIR_WINDOW_OK || windows.pump() != SIR_WINDOW_BUSY ||
	     windows.acquireNativeView( hWindow, nullptr, nullptr ) || driver.m_nDestroys != 0 || windows.notifyResize( hWindow, 10, 10 ) ) {
		return false;
	}
	if ( windows.shutdown() != SIR_WINDOW_BUSY || windows.activeWindowCount() != 1 ) {
		return false;
	}
	sirWindowWorkerContext_t workerContext = { &windows, hWindow, borrow.lease, false, false };
	SIRThread worker;
	if ( !worker.start( sirWindowWorker, &workerContext ) || !worker.join() || !workerContext.bReleased || !workerContext.bOwnerRejected ||
	     windows.releaseNativeView( borrow.lease ) || windows.pump() != SIR_WINDOW_OK || driver.m_nDestroys != 1 ||
	     windows.activeWindowCount() != 0 || windows.snapshot( hWindow, &snapshot ) ) {
		return false;
	}
	// The borrowed view is invalid after release, even if a slot is reused.
	sirWindowHandle_t hReused = {};
	if ( windows.create( desc, &hReused ) != SIR_WINDOW_OK || hReused.nGeneration == hWindow.nGeneration ||
	     windows.acquireNativeView( hWindow, nullptr, nullptr ) || windows.releaseNativeView( workerContext.lease ) ) {
		return false;
	}
	if ( windows.shutdown() != SIR_WINDOW_OK || driver.m_nDestroys != 2 ) {
		return false;
	}
	return true;
}

static bool sirTestCapacityEventsAndRecovery( void )
{
	SIRTestWindowDriver driver;
	SIRWindowSystem windows;
	if ( windows.initialize( driver ) != SIR_WINDOW_OK ) {
		return false;
	}
	const sirWindowDesc_t desc = { "multi", 100, 100, true };
	sirWindowHandle_t szWindows[SIR_WINDOW_MAX_WINDOWS] = {};
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( windows.create( desc, &szWindows[i] ) != SIR_WINDOW_OK ) {
			return false;
		}
	}
	sirWindowHandle_t unchanged = { 999, 999 };
	if ( windows.create( desc, &unchanged ) != SIR_WINDOW_NO_CAPACITY || unchanged.nSlot != 999 ||
	     windows.activeWindowCount() != SIR_WINDOW_MAX_WINDOWS ) {
		return false;
	}
	for ( uint32_t i = 0; i < 160; ++i ) {
		if ( !windows.notifyFocus( szWindows[0], ( i & 1U ) != 0 ) ) {
			return false;
		}
	}
	if ( !windows.eventOverflowed() || windows.requestClose( szWindows[0] ) != SIR_WINDOW_OK || !windows.notifyLost( szWindows[1] ) ||
	     windows.pump() != SIR_WINDOW_OK || windows.activeWindowCount() != 6 ) {
		return false;
	}
	sirWindowEvent_t event = {};
	uint32_t nEvents = 0;
	while ( windows.popEvent( &event ) ) {
		++nEvents;
	}
	if ( nEvents != SIR_WINDOW_EVENT_CAPACITY || windows.popEvent( &event ) ) {
		return false;
	}
	return windows.shutdown() == SIR_WINDOW_OK && windows.activeWindowCount() == 0;
}

static bool sirTestIndependentWindowRetirement( void )
{
	SIRTestWindowDriver driver;
	SIRWindowSystem windows;
	if ( windows.initialize( driver ) != SIR_WINDOW_OK ) {
		return false;
	}

	const sirWindowDesc_t desc = { "lease-retire", 100, 100, false };
	sirWindowHandle_t hPinned = {};
	sirWindowHandle_t hIndependent = {};
	if ( windows.create( desc, &hPinned ) != SIR_WINDOW_OK || windows.create( desc, &hIndependent ) != SIR_WINDOW_OK ) {
		return false;
	}

	sirWindowNativeView_t view = {};
	sirWindowLease_t lease = {};
	if ( !windows.acquireNativeView( hPinned, &view, &lease ) || windows.requestClose( hPinned ) != SIR_WINDOW_OK ||
	     windows.requestClose( hIndependent ) != SIR_WINDOW_OK || windows.pump() != SIR_WINDOW_BUSY || driver.m_nDestroys != 1 ||
	     windows.activeWindowCount() != 1 ) {
		return false;
	}

	return windows.releaseNativeView( lease ) && windows.pump() == SIR_WINDOW_OK && driver.m_nDestroys == 2 &&
	       windows.shutdown() == SIR_WINDOW_OK;
}

static bool sirTestFailureAndNullBackend( void )
{
	SIRTestWindowDriver driver;
	SIRWindowSystem windows;
	driver.m_bFailInitialize = true;
	if ( windows.initialize( driver ) != SIR_WINDOW_PLATFORM_ERROR ) {
		return false;
	}
	driver.m_bFailInitialize = false;
	if ( windows.initialize( driver ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowHandle_t hWindow = { 100, 100 };
	driver.m_bFailCreate = true;
	if ( windows.create( { "failure", 100, 100, true }, &hWindow ) != SIR_WINDOW_PLATFORM_ERROR || hWindow.nSlot != 100 ||
	     windows.activeWindowCount() != 0 ) {
		return false;
	}
	driver.m_bFailCreate = false;
	if ( windows.create( { "success", 100, 100, true }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	driver.m_bFailPump = true;
	if ( windows.pump() != SIR_WINDOW_PLATFORM_ERROR ) {
		return false;
	}
	driver.m_bFailPump = false;
	if ( windows.shutdown() != SIR_WINDOW_OK ) {
		return false;
	}
	SIRNullWindowDriver nullDriver;
	if ( windows.initialize( nullDriver ) != SIR_WINDOW_OK || windows.create( { "null", 1, 1, false }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirGraphicsWindowBorrow_t borrow = {};
	if ( sirGraphicsAcquireWindowSurface( windows, hWindow, &borrow ) || borrow.lease.nToken != 0 ||
	     windows.requestClose( hWindow ) != SIR_WINDOW_OK || windows.pump() != SIR_WINDOW_OK || windows.shutdown() != SIR_WINDOW_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestWindowStateAndLeases() || !sirTestCapacityEventsAndRecovery() || !sirTestIndependentWindowRetirement() ||
	     !sirTestFailureAndNullBackend() ) {
		fputs( "SIRWindowSystemTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRWindowSystemTests PASS" );
	return 0;
}
