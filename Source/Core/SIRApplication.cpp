#include <Core/SIRApplication.h>

SIRApplication::SIRApplication( void ) noexcept : m_eState( SIR_APP_UNINITIALIZED ), m_nTicks( 0 ), m_EventRegistry()
{
}

bool SIRApplication::initialize( void ) noexcept
{
	if ( m_eState != SIR_APP_UNINITIALIZED ) {
		return false;
	}

	m_eState = SIR_APP_READY;
	return true;
}

bool SIRApplication::tick( void ) noexcept
{
	if ( m_eState != SIR_APP_READY ) {
		return false;
	}

	++m_nTicks;
	return true;
}

void SIRApplication::shutdown( void ) noexcept
{
	if ( m_eState == SIR_APP_READY ) {
		m_EventRegistry.clear();
		m_eState = SIR_APP_STOPPED;
	}
}

sirAppState_t SIRApplication::getState( void ) const noexcept
{
	return m_eState;
}

uint64_t SIRApplication::getTickCount( void ) const noexcept
{
	return m_nTicks;
}

SIREventRegistry &SIRApplication::getEventRegistry( void ) noexcept
{
	return m_EventRegistry;
}
