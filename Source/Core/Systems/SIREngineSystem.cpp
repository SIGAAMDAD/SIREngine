#include <Core/Systems/SIREngineSystem.h>

SIREngineSystem::~SIREngineSystem( void ) noexcept = default;

void SIREngineSystem::update( float fDeltaSeconds ) noexcept
{
	static_cast<void>(fDeltaSeconds);
}

void SIREngineSystem::fixedUpdate( float fFixedDeltaSeconds ) noexcept
{
	static_cast<void>(fFixedDeltaSeconds);
}
