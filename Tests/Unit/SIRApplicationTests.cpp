#include <Core/SIRApplication.h>

int main( void )
{
	SIRApplication application;

	if ( application.getState() != SIR_APP_UNINITIALIZED || application.getTickCount() != 0 || application.tick() ) {
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

	application.shutdown();
	if ( application.getState() != SIR_APP_STOPPED || application.tick() ) {
		return 5;
	}

	if ( application.initialize() ) {
		return 6;
	}

	application.shutdown();
	return application.getTickCount() == 32 ? 0 : 7;
}
