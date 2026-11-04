#include <Core/Systems/SIREngineSystem.h>

SIREngineSystem::~SIREngineSystem( void ) noexcept = default;

void SIREngineSystem::update( float fDeltaSeconds ) noexcept
{
	( void )fDeltaSeconds;
}

void SIREngineSystem::fixedUpdate( float fFixedDeltaSeconds ) noexcept
{
	( void )fFixedDeltaSeconds;
}
