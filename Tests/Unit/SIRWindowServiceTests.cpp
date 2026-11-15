#include <Core/Platform/Window/SIRWindowService.h>
#include <Core/SIRApplication.h>
#include <stdio.h>

class SIRWindowServiceTestDriver final : public SIRWindowDriver
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
	bool m_bFailCreate = false;
};

bool SIRWindowServiceTestDriver::initialize( SIRWindowSystem& system ) noexcept
{
	m_pSystem = &system;
	return true;
}
void SIRWindowServiceTestDriver::shutdown( void ) noexcept
{
	m_pSystem = nullptr;
}
bool SIRWindowServiceTestDriver::create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept
{
	( void )hWindow;
	( void )desc;
	if ( m_bFailCreate ) {
		return false;
	}
	++m_nCreates;
	*pView = { SIR_WINDOW_PLATFORM_X11, ( void * )( uintptr_t )100, ( void * )( uintptr_t )11, 0 };
	return true;
}
void SIRWindowServiceTestDriver::destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept
{
	( void )hWindow;
	( void )view;
	++m_nDestroys;
}
bool SIRWindowServiceTestDriver::pump( void ) noexcept
{
	return m_pSystem != nullptr;
}
bool SIRWindowServiceTestDriver::bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept
{
	( void )hWindow;
	( void )input;
	( void )backend;
	return false;
}

class SIRWindowServiceConsumer final : public SIREngineSystem
{
public:
	explicit SIRWindowServiceConsumer( SIRWindowService& service ) noexcept;
	sirSystemResult_t initialize( void ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;
	SIRWindowService& m_Service;
	uint32_t m_nInitialized = 0;
	uint32_t m_nShutdown = 0;
};

SIRWindowServiceConsumer::SIRWindowServiceConsumer( SIRWindowService& service ) noexcept : m_Service( service )
{
}
sirSystemResult_t SIRWindowServiceConsumer::initialize( void ) noexcept
{
	if ( !m_Service.hasLiveWindows() ) {
		return SIR_SYSTEM_ERROR;
	}
	++m_nInitialized;
	return SIR_SYSTEM_OK;
}
sirSystemResult_t SIRWindowServiceConsumer::shutdown( void ) noexcept
{
	if ( !m_Service.hasLiveWindows() ) {
		return SIR_SYSTEM_ERROR;
	}
	++m_nShutdown;
	return SIR_SYSTEM_OK;
}

static bool sirTestWindowServiceDependenciesAndRetry( void )
{
	SIRWindowServiceTestDriver driver;
	SIRWindowSystem windows;
	SIRWindowService service( windows, driver, { "service", 320, 200, false } );
	SIRWindowServiceConsumer consumer( service );
	SIRApplication application;
	if ( !application.registerSystem( &consumer ) || !application.registerSystem( &service ) ||
	     !application.addDependency( consumer, service ) || !application.initialize() || consumer.m_nInitialized != 1 ||
	     !service.hasLiveWindows() || !application.tick() ) {
		return false;
	}
	sirWindowNativeView_t native = {};
	sirWindowLease_t lease = {};
	if ( !windows.acquireNativeView( service.primaryWindow(), &native, &lease ) || native.nWindowGeneration == 0 ||
	     application.shutdown() != SIR_SYSTEM_BUSY || consumer.m_nShutdown != 1 || driver.m_nDestroys != 0 ||
	     application.getState() != SIR_APP_STOPPING || windows.activeWindowCount() != 1 ) {
		return false;
	}
	if ( !windows.releaseNativeView( lease ) || application.shutdown() != SIR_SYSTEM_OK || driver.m_nDestroys != 1 ||
	     windows.activeWindowCount() != 0 || application.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestFailedServiceInitializationRollsBack( void )
{
	SIRWindowServiceTestDriver driver;
	driver.m_bFailCreate = true;
	SIRWindowSystem windows;
	SIRWindowService service( windows, driver, { "fail", 320, 200, true } );
	SIRApplication application;
	if ( !application.registerSystem( &service ) || application.initialize() || application.getFailedSystem() != &service ||
	     application.getInitializationResult() != SIR_SYSTEM_ERROR || windows.activeWindowCount() != 0 ||
	     application.getState() != SIR_APP_STOPPED ) {
		return false;
	}
	return windows.shutdown() == SIR_WINDOW_OK;
}

int main( void )
{
	if ( !sirTestWindowServiceDependenciesAndRetry() || !sirTestFailedServiceInitializationRollsBack() ) {
		fputs( "SIRWindowServiceTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRWindowServiceTests PASS" );
	return 0;
}
