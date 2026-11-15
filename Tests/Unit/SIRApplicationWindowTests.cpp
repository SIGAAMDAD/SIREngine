#include <Core/SIRApplication.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/SIRInputBackend.h>
#include <stdio.h>

class SIRWindowAppTestInput final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override;
	void sample( SIRInputService& input ) noexcept override;
	void resynchronize( SIRInputService& input ) noexcept override;
	bool vibrate( sirInputDeviceId_t device, uint16_t nStrong, uint16_t nWeak ) noexcept override;
	void stop( SIRInputService& input ) noexcept override;
	uint32_t m_nSamples = 0;
};

sirInputBackendId_t SIRWindowAppTestInput::id( void ) const noexcept
{
	return SIR_INPUT_TEST;
}

void SIRWindowAppTestInput::sample( SIRInputService& input ) noexcept
{
	( void )input;
	++m_nSamples;
}

void SIRWindowAppTestInput::resynchronize( SIRInputService& input ) noexcept
{
	( void )input;
}

bool SIRWindowAppTestInput::vibrate( sirInputDeviceId_t device, uint16_t nStrong, uint16_t nWeak ) noexcept
{
	( void )device;
	( void )nStrong;
	( void )nWeak;
	return false;
}

void SIRWindowAppTestInput::stop( SIRInputService& input ) noexcept
{
	( void )input;
}

class SIRWindowAppTestSystem final : public SIREngineSystem
{
public:
	sirSystemResult_t initialize( void ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;
	void update( float fDeltaSeconds ) noexcept override;
	uint32_t m_nUpdates = 0;
};

sirSystemResult_t SIRWindowAppTestSystem::initialize( void ) noexcept
{
	return SIR_SYSTEM_OK;
}

sirSystemResult_t SIRWindowAppTestSystem::shutdown( void ) noexcept
{
	return SIR_SYSTEM_OK;
}

void SIRWindowAppTestSystem::update( float fDeltaSeconds ) noexcept
{
	( void )fDeltaSeconds;
	++m_nUpdates;
}

typedef struct sirWindowPumpTestContext_s
{
	SIRApplication *pApplication;
	SIRInputService *pInput;
	uint32_t nPumps;
	uint32_t nStopAt;
	bool bInputCollecting;
	bool bReentrantRejected;
	bool bShutdownBusy;
	bool bError;
} sirWindowPumpTestContext_t;

static sirAppWindowPumpResult_t sirWindowTestPump( void *pContext )
{
	sirWindowPumpTestContext_t *pPump = ( sirWindowPumpTestContext_t * )pContext;
	++pPump->nPumps;
	if ( pPump->pInput != nullptr ) {
		sirInputDeviceId_t hDevice = {};
		pPump->bInputCollecting = pPump->pInput->connect( { 101, 101, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &hDevice );
		pPump->pInput = nullptr;
	}
	pPump->bReentrantRejected = !pPump->pApplication->tick() && pPump->bReentrantRejected;
	pPump->bShutdownBusy = pPump->pApplication->shutdown() == SIR_SYSTEM_BUSY && pPump->bShutdownBusy;
	if ( pPump->bError ) {
		return SIR_APP_WINDOW_ERROR;
	}
	return pPump->nPumps >= pPump->nStopAt ? SIR_APP_WINDOW_STOP : SIR_APP_WINDOW_CONTINUE;
}

static bool sirTestNoInputWindowLifecycle( void )
{
	SIRApplication application;
	SIRWindowAppTestSystem system;
	sirWindowPumpTestContext_t context = { &application, nullptr, 0, 3, true, true, true, false };
	if ( !application.registerSystem( &system ) || !application.attachWindowPump( sirWindowTestPump, &context ) ||
	     application.attachWindowPump( sirWindowTestPump, &context ) || !application.initialize() ) {
		return false;
	}
	if ( application.run() != SIR_APP_RUN_STOPPED || application.getWindowPumpResult() != SIR_APP_WINDOW_STOP ||
	     application.getTickCount() != 3 || system.m_nUpdates != 2 || context.nPumps != 3 || !context.bReentrantRejected ||
	     !context.bShutdownBusy || application.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestInputWindowOrdering( void )
{
	SIRApplication application;
	SIRInputService input;
	SIRWindowAppTestInput backend;
	sirWindowPumpTestContext_t context = { &application, &input, 0, 2, false, true, true, false };
	if ( !input.attach( backend ) || !application.attachInput( input ) || !application.attachWindowPump( sirWindowTestPump, &context ) ||
	     !application.initialize() ) {
		return false;
	}
	if ( application.run() != SIR_APP_RUN_STOPPED || !context.bInputCollecting || backend.m_nSamples != 2 ||
	     application.getInputFrame() == nullptr || application.getInputFrame()->nTick != 2 || application.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestWindowFailure( void )
{
	SIRApplication application;
	sirWindowPumpTestContext_t context = { &application, nullptr, 0, 100, true, true, true, true };
	if ( application.run( 4 ) != SIR_APP_RUN_ERROR || !application.attachWindowPump( sirWindowTestPump, &context ) ||
	     !application.initialize() ) {
		return false;
	}
	if ( application.run( 4 ) != SIR_APP_RUN_ERROR || application.getWindowPumpResult() != SIR_APP_WINDOW_ERROR ||
	     application.getTickCount() != 1 || application.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	SIRApplication headless;
	return headless.initialize() && headless.run( 0 ) == SIR_APP_RUN_ERROR && headless.run( 4 ) == SIR_APP_RUN_OK &&
	       headless.getTickCount() == 4 && headless.shutdown() == SIR_SYSTEM_OK;
}

int main( void )
{
	if ( !sirTestNoInputWindowLifecycle() || !sirTestInputWindowOrdering() || !sirTestWindowFailure() ) {
		fputs( "SIRApplicationWindowTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRApplicationWindowTests PASS" );
	return 0;
}
