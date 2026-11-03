#include <SIR/Core/SIRApplication.h>

int main( void )
{
	SIRApplication application;
	if ( application.getState() != SIRAppState::Uninitialized || application.tick() )
	{
		return 1;
	}
	if ( !application.initialize() || application.getState() != SIRAppState::Ready )
	{
		return 2;
	}
	if ( application.initialize() || !application.tick() )
	{
		return 3;
	}
	if ( application.getTickCount() != 1 )
	{
		return 4;
	}
	application.shutdown();
	if ( application.getState() != SIRAppState::Stopped || application.tick() )
	{
		return 5;
	}
	if ( application.initialize() )
	{
		return 6;
	}
	application.shutdown();
	return application.getTickCount() == 1 ? 0 : 7;
}
