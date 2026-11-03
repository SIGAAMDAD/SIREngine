#include <SIR/Core/SIRApplication.h>

namespace sir
{
SIRApplication::SIRApplication( void ) noexcept : m_eState( SIRAppState::Uninitialized ), m_nTicks( 0 )
{
}

bool SIRApplication::initialize( void ) noexcept
{
	if ( m_eState != SIRAppState::Uninitialized ) {
		return false;
	}

	m_eState = SIRAppState::Ready;
	return true;
}

bool SIRApplication::tick( void ) noexcept
{
	if ( m_eState != SIRAppState::Ready ) {
		return false;
	}

	++m_nTicks;
	return true;
}

void SIRApplication::shutdown( void ) noexcept
{
	if ( m_eState == SIRAppState::Ready ) {
		m_eState = SIRAppState::Stopped;
	}
}

SIRAppState SIRApplication::getState( void ) const noexcept
{
	return m_eState;
}

uint64_t SIRApplication::getTickCount( void ) const noexcept
{
	return m_nTicks;
}
}
