#include <Core/SIRApplication.h>

SIRApplication::SIRApplication( void ) noexcept
    : m_eState( SIR_APP_UNINITIALIZED ), m_nTicks( 0 ), m_EventRegistry(), m_pInput( nullptr ), m_pInputPump( nullptr ),
      m_pInputPumpContext( nullptr )
{
}

bool SIRApplication::attachInput( SIRInputService &input, sirAppInputPump_t pPump, void *pContext ) noexcept
{
	if ( m_eState != SIR_APP_UNINITIALIZED || m_pInput != nullptr ) {
		return false;
	}

	m_pInput = &input;
	m_pInputPump = pPump;
	m_pInputPumpContext = pContext;
	return true;
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

	const uint64_t nNextTick = m_nTicks + 1;
	if ( m_pInput != nullptr ) {
		if ( !m_pInput->begin( nNextTick ) ) {
			return false;
		}

		const bool bPumped = m_pInputPump == nullptr || m_pInputPump( *m_pInput, m_pInputPumpContext );
		m_pInput->sample();
		if ( !m_pInput->publish() ) {
			if ( !m_pInput->recover() || !m_pInput->publish() ) {
				return false;
			}
		}

		m_nTicks = nNextTick;
		return bPumped;
	}

	m_nTicks = nNextTick;
	return true;
}

void SIRApplication::shutdown( void ) noexcept
{
	if ( m_eState == SIR_APP_READY ) {
		if ( m_pInput != nullptr ) {
			m_pInput->shutdown();
			m_pInput = nullptr;
		}
		m_pInputPump = nullptr;
		m_pInputPumpContext = nullptr;
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

const sirInputFrame_t *SIRApplication::getInputFrame( void ) const noexcept
{
	return m_pInput == nullptr ? nullptr : m_pInput->frame();
}
