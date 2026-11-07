#include <Core/SIRApplication.h>

int main( void )
{
	SIRApplication application;

	if ( application.getState() != SIR_APP_UNINITIALIZED || application.getTickCount() != 0 || application.getFrameDeltaSeconds() != 0.0f ||
	     application.getElapsedSeconds() != 0.0 || application.tick() ) {
		return 1;
	}

	if ( !application.initialize() || application.getState() != SIR_APP_READY ) {
		return 2;
	}

	if ( application.initialize() ) {
		return 3;
	}

	for ( uint32_t iTick = 1; iTick <= 32; ++iTick ) {
		if ( !application.tick() || application.getTickCount() != iTick ) {
			return 4;
		}
	}

	if ( !application.tick( 0.125f ) || application.getFrameDeltaSeconds() != 0.125f || application.getElapsedSeconds() != 0.125 ) {
		return 8;
	}
	const double dElapsedBeforeTimedTick = application.getElapsedSeconds();
	if ( !application.tickTimed() || application.getFrameDeltaSeconds() < 0.0f ||
	     application.getElapsedSeconds() < dElapsedBeforeTimedTick || application.getTickCount() != 34 ) {
		return 9;
	}
	application.shutdown();
	if ( application.getState() != SIR_APP_STOPPED || application.tick() ) {
		return 5;
	}

	if ( application.initialize() ) {
		return 6;
	}

	application.shutdown();
	return application.getTickCount() == 34 && !application.tickTimed() ? 0 : 7;
}
