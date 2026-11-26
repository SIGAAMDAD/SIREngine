/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/SIRApplication.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>

SIRApplication::SIRApplication( void ) noexcept
    : m_eState( SIRAppState::SIR_APP_UNINITIALIZED ), m_nTicks( 0 ), m_nLastFrameTimestampNs( 0 ), m_fFrameDeltaSeconds( 0.0f ),
      m_dElapsedSeconds( 0.0 ), m_nSystemCount( 0 ), m_nInitializedSystems( 0 ), m_eInitializationResult( SIRSystemResult::SIR_SYSTEM_OK ),
      m_pFailedSystem( nullptr ), m_bShutdownInProgress( false ), m_bFrameDispatchInProgress( false ), m_EventRegistry(),
      m_pInput( nullptr ), m_pInputPump( nullptr ), m_pInputPumpContext( nullptr ), m_pWindowPump( nullptr ),
      m_pWindowPumpContext( nullptr ), m_eWindowPumpResult( SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE ), m_pTimeline( nullptr ), m_pSchedule( nullptr ),
      m_fScheduledDeltaSeconds( 0.0f ), m_bScheduledFixed( false ), m_bScheduledActive( false )
{
}

uint32_t SIRApplication::findSystemIndex( const SIREngineSystem *pSystem ) const noexcept
{
	for ( uint32_t iSystem = 0; iSystem < m_nSystemCount; ++iSystem ) {
		if ( m_szSystems[iSystem] == pSystem ) {
			return iSystem;
		}
	}
	return m_nSystemCount;
}

bool SIRApplication::resolveInitializationOrder( void ) noexcept
{
	uint32_t nReadyMask = 0;
	for ( uint32_t iOrder = 0; iOrder < m_nSystemCount; ++iOrder ) {
		bool bResolved = false;
		for ( uint32_t iSystem = 0; iSystem < m_nSystemCount; ++iSystem ) {
			const uint32_t nSystemBit = 1U << iSystem;
			if ( ( nReadyMask & nSystemBit ) != 0 || ( m_szDependencyMasks[iSystem] & ~nReadyMask ) != 0 ) {
				continue;
			}

			m_szInitializationOrder[iOrder] = iSystem;
			nReadyMask |= nSystemBit;
			bResolved = true;
			break;
		}
		if ( !bResolved ) {
			return false;
		}
	}
	return true;
}

bool SIRApplication::attachInput( SIRInputService& input, sirAppInputPump_t pPump, void *pContext ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || m_pInput != nullptr || ( pPump != nullptr && m_pWindowPump != nullptr ) ) {
		return false;
	}

	m_pInput = &input;
	m_pInputPump = pPump;
	m_pInputPumpContext = pContext;
	return true;
}

bool SIRApplication::attachWindowPump( sirAppWindowPump_t pPump, void *pContext ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || pPump == nullptr || m_pWindowPump != nullptr || m_pInputPump != nullptr ) {
		return false;
	}
	m_pWindowPump = pPump;
	m_pWindowPumpContext = pContext;
	return true;
}

SIRAppWindowPumpResult SIRApplication::getWindowPumpResult( void ) const noexcept
{
	return m_eWindowPumpResult;
}

bool SIRApplication::registerSystem( SIREngineSystem *pSystem ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || pSystem == nullptr || m_nSystemCount >= SIR_APPLICATION_MAX_SYSTEMS ) {
		return false;
	}

	for ( uint32_t iSystem = 0; iSystem < m_nSystemCount; ++iSystem ) {
		if ( m_szSystems[iSystem] == pSystem ) {
			return false;
		}
	}

	m_szSystems[m_nSystemCount] = pSystem;
	m_szSystemPriorities[m_nSystemCount] = SIRTaskPriority::SIR_TASK_PRIORITY_NORMAL;
	++m_nSystemCount;
	return true;
}

bool SIRApplication::addDependency( SIREngineSystem& consumer, SIREngineSystem& provider ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED || &consumer == &provider ) {
		return false;
	}

	const uint32_t iConsumer = findSystemIndex( &consumer );
	const uint32_t iProvider = findSystemIndex( &provider );
	if ( iConsumer >= m_nSystemCount || iProvider >= m_nSystemCount ) {
		return false;
	}

	const uint32_t nDependencyBit = 1U << iProvider;
	if ( ( m_szDependencyMasks[iConsumer] & nDependencyBit ) != 0 ) {
		return false;
	}
	m_szDependencyMasks[iConsumer] |= nDependencyBit;
	return true;
}

bool SIRApplication::initialize( void ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_UNINITIALIZED ) {
		return false;
	}

	m_eInitializationResult = SIRSystemResult::SIR_SYSTEM_OK;
	m_pFailedSystem = nullptr;
	m_eWindowPumpResult = SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE;
	if ( !resolveInitializationOrder() ) {
		m_eInitializationResult = SIRSystemResult::SIR_SYSTEM_ERROR;
		m_eState = SIRAppState::SIR_APP_STOPPING;
		static_cast<void>(shutdown());
		return false;
	}

	m_eState = SIRAppState::SIR_APP_STARTING;
	for ( uint32_t iOrder = 0; iOrder < m_nSystemCount; ++iOrder ) {
		const uint32_t iSystem = m_szInitializationOrder[iOrder];
		const SIRSystemResult eResult = m_szSystems[iSystem]->initialize();
		if ( eResult != SIRSystemResult::SIR_SYSTEM_OK ) {
			m_eInitializationResult = eResult;
			m_pFailedSystem = m_szSystems[iSystem];
			m_eState = SIRAppState::SIR_APP_STOPPING;
			static_cast<void>(shutdown());
			return false;
		}
		++m_nInitializedSystems;
	}

	m_nLastFrameTimestampNs = SIRPlatformTime::getMonotonicNanoseconds();
	m_eState = SIRAppState::SIR_APP_READY;
	return true;
}

bool SIRApplication::tick( float fDeltaSeconds ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_READY || m_bFrameDispatchInProgress || m_bScheduledActive ) {
		return false;
	}
	m_bFrameDispatchInProgress = true;
	const bool bResult = tickFrame( fDeltaSeconds );
	m_bFrameDispatchInProgress = false;
	return bResult;
}

bool SIRApplication::tickFrame( float fDeltaSeconds ) noexcept
{
	const uint64_t nNextTick = m_nTicks + 1;
	const bool bInputBegun = m_pInput == nullptr || m_pInput->begin( nNextTick );

	// The window OS event pump runs exactly once even without InputService.
	// When input exists, its native events are collected between begin/publish.
	m_eWindowPumpResult = SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE;
	if ( m_pWindowPump != nullptr ) {
		m_eWindowPumpResult = m_pWindowPump( m_pWindowPumpContext );
	} else if ( m_pInput != nullptr && m_pInputPump != nullptr && !m_pInputPump( *m_pInput, m_pInputPumpContext ) ) {
		m_eWindowPumpResult = SIRAppWindowPumpResult::SIR_APP_WINDOW_STOP;
	}

	if ( m_eState != SIRAppState::SIR_APP_READY ) {
		return false;
	}
	if ( !bInputBegun ) {
		return false;
	}
	if ( m_pInput != nullptr ) {
		m_pInput->sample();
		if ( !m_pInput->publish() && ( !m_pInput->recover() || !m_pInput->publish() ) ) {
			return false;
		}
	}
	if ( m_eWindowPumpResult != SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE ) {
		m_nTicks = nNextTick;
		return false;
	}

	for ( uint32_t iOrder = 0; iOrder < m_nInitializedSystems; ++iOrder ) {
		m_szSystems[m_szInitializationOrder[iOrder]]->update( fDeltaSeconds );
		if ( m_eState != SIRAppState::SIR_APP_READY ) {
			return false;
		}
	}

	m_nTicks = nNextTick;
	m_fFrameDeltaSeconds = fDeltaSeconds;
	m_dElapsedSeconds += static_cast<double>(fDeltaSeconds);
	return true;
}

bool SIRApplication::tickTimed( void ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_READY || m_nLastFrameTimestampNs == 0 ) {
		return false;
	}

	const uint64_t nNowNs = SIRPlatformTime::getMonotonicNanoseconds();
	if ( nNowNs == 0 || nNowNs < m_nLastFrameTimestampNs ) {
		return false;
	}

	const float fDeltaSeconds = ( float )( static_cast<double>( nNowNs - m_nLastFrameTimestampNs ) * 0.000000001 );
	if ( !tick( fDeltaSeconds ) ) {
		return false;
	}

	m_nLastFrameTimestampNs = nNowNs;
	return true;
}

SIRAppRunResult SIRApplication::run( uint64_t nMaxFrames ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_READY || m_bFrameDispatchInProgress || ( nMaxFrames == 0 && m_pWindowPump == nullptr ) ) {
		return SIRAppRunResult::SIR_APP_RUN_ERROR;
	}
	uint64_t nFrame = 0;
	while ( nMaxFrames == 0 || nFrame < nMaxFrames ) {
		if ( !tickTimed() ) {
			return m_eWindowPumpResult == SIRAppWindowPumpResult::SIR_APP_WINDOW_STOP ? SIRAppRunResult::SIR_APP_RUN_STOPPED : SIRAppRunResult::SIR_APP_RUN_ERROR;
		}
		++nFrame;
	}
	return SIRAppRunResult::SIR_APP_RUN_OK;
}

bool SIRApplication::fixedTick( float fFixedDeltaSeconds ) noexcept
{
	if ( m_eState != SIRAppState::SIR_APP_READY || m_bFrameDispatchInProgress || m_bScheduledActive ) {
		return false;
	}

	m_bFrameDispatchInProgress = true;
	for ( uint32_t iOrder = 0; iOrder < m_nInitializedSystems; ++iOrder ) {
		m_szSystems[m_szInitializationOrder[iOrder]]->fixedUpdate( fFixedDeltaSeconds );
		if ( m_eState != SIRAppState::SIR_APP_READY ) {
			m_bFrameDispatchInProgress = false;
			return false;
		}
	}

	m_bFrameDispatchInProgress = false;
	return true;
}

SIRSystemResult SIRApplication::shutdown( void ) noexcept
{
	if ( m_bShutdownInProgress || m_bFrameDispatchInProgress || m_bScheduledActive ) {
		return SIRSystemResult::SIR_SYSTEM_BUSY;
	}
	if ( m_eState == SIRAppState::SIR_APP_UNINITIALIZED || m_eState == SIRAppState::SIR_APP_STOPPED ) {
		return SIRSystemResult::SIR_SYSTEM_OK;
	}
	if ( m_eState == SIRAppState::SIR_APP_STARTING ) {
		return SIRSystemResult::SIR_SYSTEM_BUSY;
	}

	m_eState = SIRAppState::SIR_APP_STOPPING;
	m_bShutdownInProgress = true;
	while ( m_nInitializedSystems != 0 ) {
		const uint32_t iSystem = m_szInitializationOrder[m_nInitializedSystems - 1];
		SIREngineSystem *pSystem = m_szSystems[iSystem];
		const SIRSystemResult eResult = pSystem->shutdown();
		if ( eResult != SIRSystemResult::SIR_SYSTEM_OK ) {
			m_bShutdownInProgress = false;
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
	m_pWindowPump = nullptr;
	m_pWindowPumpContext = nullptr;
	m_EventRegistry.clear();
	m_eState = SIRAppState::SIR_APP_STOPPED;
	m_bShutdownInProgress = false;
	return SIRSystemResult::SIR_SYSTEM_OK;
}

SIRSystemResult SIRApplication::getInitializationResult( void ) const noexcept
{
	return m_eInitializationResult;
}

const SIREngineSystem *SIRApplication::getFailedSystem( void ) const noexcept
{
	return m_pFailedSystem;
}

SIRAppState SIRApplication::getState( void ) const noexcept
{
	return m_eState;
}

uint64_t SIRApplication::getTickCount( void ) const noexcept
{
	return m_nTicks;
}

float SIRApplication::getFrameDeltaSeconds( void ) const noexcept
{
	return m_fFrameDeltaSeconds;
}

double SIRApplication::getElapsedSeconds( void ) const noexcept
{
	return m_dElapsedSeconds;
}

uint32_t SIRApplication::getSystemCount( void ) const noexcept
{
	return m_nSystemCount;
}

SIREventRegistry& SIRApplication::getEventRegistry( void ) noexcept
{
	return m_EventRegistry;
}

const sirInputFrame_t *SIRApplication::getInputFrame( void ) const noexcept
{
	return m_pInput == nullptr ? nullptr : m_pInput->frame();
}
