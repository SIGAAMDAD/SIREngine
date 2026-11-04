#include <Core/SIRApplication.h>

SIRApplication::SIRApplication( void ) noexcept
    : m_eState( SIR_APP_UNINITIALIZED ), m_nTicks( 0 ), m_nSystemCount( 0 ), m_nInitializedSystems( 0 ), m_EventRegistry(),
      m_pInput( nullptr ), m_pInputPump( nullptr ), m_pInputPumpContext( nullptr )
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

bool SIRApplication::registerSystem( SIREngineSystem *pSystem ) noexcept
{
	if ( m_eState != SIR_APP_UNINITIALIZED || pSystem == nullptr || m_nSystemCount >= SIR_APPLICATION_MAX_SYSTEMS ) {
		return false;
	}

	for ( uint32_t iSystem = 0; iSystem < m_nSystemCount; ++iSystem ) {
		if ( m_szSystems[iSystem] == pSystem ) {
			return false;
		}
	}

	m_szSystems[m_nSystemCount] = pSystem;
	++m_nSystemCount;
	return true;
}

bool SIRApplication::initialize( void ) noexcept
{
	if ( m_eState != SIR_APP_UNINITIALIZED ) {
		return false;
	}

	m_eState = SIR_APP_STARTING;
	for ( uint32_t iSystem = 0; iSystem < m_nSystemCount; ++iSystem ) {
		if ( m_szSystems[iSystem]->initialize() != SIR_SYSTEM_OK ) {
			m_eState = SIR_APP_STOPPING;
			( void )shutdown();
			return false;
		}
		++m_nInitializedSystems;
	}

	m_eState = SIR_APP_READY;
	return true;
}

bool SIRApplication::tick( float fDeltaSeconds ) noexcept
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
		if ( !bPumped ) {
			m_nTicks = nNextTick;
			return false;
		}
	}

	for ( uint32_t iSystem = 0; iSystem < m_nInitializedSystems; ++iSystem ) {
		m_szSystems[iSystem]->update( fDeltaSeconds );
	}

	m_nTicks = nNextTick;
	return true;
}

bool SIRApplication::fixedTick( float fFixedDeltaSeconds ) noexcept
{
	if ( m_eState != SIR_APP_READY ) {
		return false;
	}

	for ( uint32_t iSystem = 0; iSystem < m_nInitializedSystems; ++iSystem ) {
		m_szSystems[iSystem]->fixedUpdate( fFixedDeltaSeconds );
	}

	return true;
}

sirSystemResult_t SIRApplication::shutdown( void ) noexcept
{
	if ( m_eState == SIR_APP_UNINITIALIZED || m_eState == SIR_APP_STOPPED ) {
		return SIR_SYSTEM_OK;
	}
	if ( m_eState == SIR_APP_STARTING ) {
		return SIR_SYSTEM_BUSY;
	}

	m_eState = SIR_APP_STOPPING;
	while ( m_nInitializedSystems != 0 ) {
		SIREngineSystem *pSystem = m_szSystems[m_nInitializedSystems - 1];
		const sirSystemResult_t eResult = pSystem->shutdown();
		if ( eResult != SIR_SYSTEM_OK ) {
			return eResult;
		}
		--m_nInitializedSystems;
	}

	if ( m_pInput != nullptr ) {
		m_pInput->shutdown();
		m_pInput = nullptr;
	}
	m_pInputPump = nullptr;
	m_pInputPumpContext = nullptr;
	m_EventRegistry.clear();
	m_eState = SIR_APP_STOPPED;
	return SIR_SYSTEM_OK;
}

sirAppState_t SIRApplication::getState( void ) const noexcept
{
	return m_eState;
}

uint64_t SIRApplication::getTickCount( void ) const noexcept
{
	return m_nTicks;
}

uint32_t SIRApplication::getSystemCount( void ) const noexcept
{
	return m_nSystemCount;
}

SIREventRegistry &SIRApplication::getEventRegistry( void ) noexcept
{
	return m_EventRegistry;
}

const sirInputFrame_t *SIRApplication::getInputFrame( void ) const noexcept
{
	return m_pInput == nullptr ? nullptr : m_pInput->frame();
}
