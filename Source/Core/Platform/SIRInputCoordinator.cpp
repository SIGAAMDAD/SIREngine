/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Platform/SIRInputCoordinator.h>

SIRInputCoordinator::SIRInputCoordinator( SIRInputSystem &input ) noexcept
    : m_Input( input ), m_pBackends{}, m_Routes{}, m_nBackendCount( 0 ), m_nPendingTick( 0 ), m_eLastResult( SIR_INPUT_COORD_OK ),
      m_bCollecting( false ), m_bStopped( false )
{
}

bool SIRInputCoordinator::isDesktopBackend( sirInputBackendKind_t eKind ) noexcept
{
	return eKind == SIR_INPUT_BACKEND_WINDOWS_RAW || eKind == SIR_INPUT_BACKEND_LINUX_X11 || eKind == SIR_INPUT_BACKEND_LINUX_WAYLAND ||
	       eKind == SIR_INPUT_BACKEND_SDL3;
}

SIRInputBackend *SIRInputCoordinator::findBackend( sirInputBackendKind_t eKind ) const noexcept
{
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		if ( m_pBackends[i]->getBackendKind() == eKind ) {
			return m_pBackends[i];
		}
	}
	return nullptr;
}

bool SIRInputCoordinator::attachBackend( SIRInputBackend &backend ) noexcept
{
	if ( m_bStopped || m_bCollecting || m_nBackendCount >= SIR_INPUT_MAX_BACKENDS || findBackend( backend.getBackendKind() ) != nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		if ( m_pBackends[i] == &backend ||
		     ( isDesktopBackend( backend.getBackendKind() ) && isDesktopBackend( m_pBackends[i]->getBackendKind() ) ) ) {
			return false;
		}
	}
	m_pBackends[m_nBackendCount++] = &backend;
	return true;
}

bool SIRInputCoordinator::beginTick( uint64_t nTick ) noexcept
{
	if ( m_bStopped || m_bCollecting || nTick == 0 || nTick <= m_Input.getPublishedTick() ) {
		m_eLastResult = SIR_INPUT_COORD_INVALID_TICK;
		return false;
	}
	m_Input.beginTick();
	m_nPendingTick = nTick;
	m_bCollecting = true;
	m_eLastResult = SIR_INPUT_COORD_OK;
	return true;
}

bool SIRInputCoordinator::reconcileDeviceOwners( void ) noexcept
{
	// Stage route changes until every device has a unique owner and stable
	// physical identity. Failed validation must not mutate the published map.
	sirInputDeviceRoute_t candidate[SIR_INPUT_MAX_DEVICES] = {};
	memcpy( candidate, m_Routes, sizeof( candidate ) );
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		if ( candidate[i].bActive && !m_Input.isDeviceActive( candidate[i].hDevice ) ) {
			candidate[i] = {};
		}
	}
	sirInputDeviceHandle_t handles[SIR_INPUT_MAX_DEVICES] = {};
	const uint32_t nDevices = m_Input.enumerateDevices( handles, SIR_INPUT_MAX_DEVICES );
	for ( uint32_t iDevice = 0; iDevice < nDevices; ++iDevice ) {
		const sirInputDeviceHandle_t hDevice = handles[iDevice];
		SIRInputBackend *pOwner = nullptr;
		for ( uint32_t iBackend = 0; iBackend < m_nBackendCount; ++iBackend ) {
			if ( m_pBackends[iBackend]->ownsDevice( hDevice ) ) {
				if ( pOwner != nullptr ) {
					m_eLastResult = SIR_INPUT_COORD_DUPLICATE_OWNER;
					return false;
				}
				pOwner = m_pBackends[iBackend];
			}
		}
		if ( pOwner == nullptr ) {
			m_eLastResult = SIR_INPUT_COORD_UNOWNED_DEVICE;
			return false;
		}
		const uint64_t nPhysicalKey = pOwner->getPhysicalKey( hDevice );
		for ( uint32_t iOther = 0; iOther < SIR_INPUT_MAX_DEVICES; ++iOther ) {
			if ( nPhysicalKey != 0 && iOther != hDevice.nSlot && candidate[iOther].bActive &&
			     candidate[iOther].nPhysicalKey == nPhysicalKey ) {
				m_eLastResult = SIR_INPUT_COORD_DUPLICATE_PHYSICAL_DEVICE;
				return false;
			}
		}
		sirInputDeviceRoute_t &route = candidate[hDevice.nSlot];
		if ( !route.bActive || route.hDevice.nGeneration != hDevice.nGeneration || route.eBackend != pOwner->getBackendKind() ) {
			route = { hDevice, nPhysicalKey, SIR_INPUT_UNPAIRED_USER, pOwner->getBackendKind(), true };
		} else {
			route.nPhysicalKey = nPhysicalKey;
		}
	}
	memcpy( m_Routes, candidate, sizeof( m_Routes ) );
	return true;
}

bool SIRInputCoordinator::publishTick( void ) noexcept
{
	if ( m_bStopped || !m_bCollecting ) {
		m_eLastResult = SIR_INPUT_COORD_NOT_COLLECTING;
		return false;
	}
	for ( uint32_t i = 0; i < m_nBackendCount; ++i ) {
		m_pBackends[i]->pump();
	}
	if ( m_Input.isOverflowed() ) {
		m_eLastResult = SIR_INPUT_COORD_QUEUE_OVERFLOW;
		return false;
	}
	if ( !reconcileDeviceOwners() ) {
		return false;
	}
	if ( !m_Input.publishTick( m_nPendingTick ) ) {
		m_eLastResult = SIR_INPUT_COORD_INVALID_TICK;
		return false;
	}
	m_bCollecting = false;
	m_eLastResult = SIR_INPUT_COORD_OK;
	return true;
}

bool SIRInputCoordinator::getDeviceRoute( sirInputDeviceHandle_t hDevice, sirInputDeviceRoute_t *pRoute ) const noexcept
{
	if ( pRoute == nullptr || hDevice.nSlot >= SIR_INPUT_MAX_DEVICES || !m_Input.isDeviceActive( hDevice ) ) {
		return false;
	}
	const sirInputDeviceRoute_t &route = m_Routes[hDevice.nSlot];
	if ( !route.bActive || route.hDevice.nGeneration != hDevice.nGeneration ) {
		return false;
	}
	*pRoute = route;
	return true;
}

bool SIRInputCoordinator::assignUser( sirInputDeviceHandle_t hDevice, uint32_t nUserId ) noexcept
{
	if ( m_bCollecting || ( nUserId >= SIR_INPUT_MAX_USERS && nUserId != SIR_INPUT_UNPAIRED_USER ) ||
	     hDevice.nSlot >= SIR_INPUT_MAX_DEVICES || !m_Input.isDeviceActive( hDevice ) ) {
		return false;
	}
	sirInputDeviceRoute_t &route = m_Routes[hDevice.nSlot];
	if ( !route.bActive || route.hDevice.nGeneration != hDevice.nGeneration ) {
		return false;
	}
	route.nUserId = nUserId;
	return true;
}

bool SIRInputCoordinator::setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept
{
	sirInputDeviceRoute_t route = {};
	if ( m_bStopped || !getDeviceRoute( hDevice, &route ) ) {
		return false;
	}
	SIRInputBackend *pBackend = findBackend( route.eBackend );
	return pBackend != nullptr && pBackend->setVibration( hDevice, nStrong, nWeak, nDurationMs );
}

sirInputCoordinatorResult_t SIRInputCoordinator::getLastResult( void ) const noexcept
{
	return m_eLastResult;
}

uint32_t SIRInputCoordinator::getBackendCount( void ) const noexcept
{
	return m_nBackendCount;
}

void SIRInputCoordinator::shutdown( void ) noexcept
{
	if ( m_bStopped ) {
		return;
	}
	for ( uint32_t i = m_nBackendCount; i > 0; --i ) {
		m_pBackends[i - 1]->shutdown();
		m_pBackends[i - 1] = nullptr;
	}
	m_nBackendCount = 0;
	m_Input.shutdown();
	memset( m_Routes, 0, sizeof( m_Routes ) );
	m_bCollecting = false;
	m_bStopped = true;
}
