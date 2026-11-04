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

#include <Core/Platform/Input/SIRInputHaptics.h>
#include <string.h>

SIRInputHaptics::SIRInputHaptics( void ) noexcept : m_Effects{}, m_Motors{}, m_nGeneration( 1 )
{
}

bool SIRInputHaptics::schedule( const sirInputHapticRequest_t &request, uint64_t nNowNs,
                               sirInputHapticHandle_t *pHandle ) noexcept
{
	const uint64_t nDuration = ( uint64_t )request.nAttackMs + request.nSustainMs + request.nReleaseMs;
	if ( request.hDevice.nSlot >= SIR_INPUT_DEVICE_CAPACITY || request.hDevice.nGeneration == 0 || nDuration == 0 ||
	     nDuration > SIR_INPUT_HAPTIC_MAX_DURATION_MS || ( request.nLeft == 0 && request.nRight == 0 ) ||
	     pHandle == nullptr ) {
		return false;
	}

	for ( uint32_t i = 0; i < SIR_INPUT_HAPTIC_EFFECT_CAPACITY; ++i ) {
		if ( m_Effects[i].bActive ) {
			continue;
		}
		if ( ++m_nGeneration == 0 ) {
			++m_nGeneration;
		}
		m_Effects[i] = { request, nNowNs, m_nGeneration, true };
		*pHandle = { i, m_nGeneration };
		return true;
	}
	return false;
}

bool SIRInputHaptics::cancel( sirInputHapticHandle_t hEffect ) noexcept
{
	if ( hEffect.nSlot >= SIR_INPUT_HAPTIC_EFFECT_CAPACITY ||
	     !m_Effects[hEffect.nSlot].bActive || m_Effects[hEffect.nSlot].nGeneration != hEffect.nGeneration ) {
		return false;
	}
	m_Effects[hEffect.nSlot].bActive = false;
	return true;
}

uint32_t SIRInputHaptics::gain( const sirInputHapticEffect_t &effect, uint64_t nNowNs ) noexcept
{
	const sirInputHapticRequest_t &request = effect.request;
	const uint64_t nElapsed = nNowNs < effect.nStartNs ? 0 : ( nNowNs - effect.nStartNs ) / 1000000;
	const uint64_t nAttackEnd = request.nAttackMs;
	const uint64_t nSustainEnd = nAttackEnd + request.nSustainMs;
	const uint64_t nEnd = nSustainEnd + request.nReleaseMs;
	if ( nElapsed >= nEnd ) {
		return 0;
	}
	if ( request.nAttackMs != 0 && nElapsed < nAttackEnd ) {
		return ( uint32_t )( nElapsed * 65535 / request.nAttackMs );
	}
	if ( request.nReleaseMs != 0 && nElapsed >= nSustainEnd ) {
		return ( uint32_t )( ( nEnd - nElapsed ) * 65535 / request.nReleaseMs );
	}
	return 65535;
}

bool SIRInputHaptics::advance( uint64_t nNowNs, bool bPaused, sirInputRumbleSink_t pSink, void *pContext ) noexcept
{
	if ( pSink == nullptr ) {
		return false;
	}

	int32_t szPriority[SIR_INPUT_DEVICE_CAPACITY] = {};
	uint16_t szLeft[SIR_INPUT_DEVICE_CAPACITY] = {};
	uint16_t szRight[SIR_INPUT_DEVICE_CAPACITY] = {};
	sirInputDeviceId_t szDevices[SIR_INPUT_DEVICE_CAPACITY] = {};
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		szPriority[i] = -1;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_HAPTIC_EFFECT_CAPACITY; ++i ) {
		sirInputHapticEffect_t &effect = m_Effects[i];
		if ( !effect.bActive ) {
			continue;
		}
		const uint32_t nGain = gain( effect, nNowNs );
		const uint64_t nEnd = ( uint64_t )effect.request.nAttackMs + effect.request.nSustainMs +
		                     effect.request.nReleaseMs;
		if ( nNowNs >= effect.nStartNs && ( nNowNs - effect.nStartNs ) / 1000000 >= nEnd ) {
			effect.bActive = false;
			continue;
		}
		if ( bPaused ) {
			continue;
		}

		const uint32_t iSlot = effect.request.hDevice.nSlot;
		const int32_t nPriority = effect.request.nPriority;
		if ( nPriority < szPriority[iSlot] ) {
			continue;
		}
		const uint16_t nLeft = ( uint16_t )( ( uint32_t )effect.request.nLeft * nGain / 65535 );
		const uint16_t nRight = ( uint16_t )( ( uint32_t )effect.request.nRight * nGain / 65535 );
		if ( nPriority > szPriority[iSlot] ) {
			szPriority[iSlot] = nPriority;
			szLeft[iSlot] = nLeft;
			szRight[iSlot] = nRight;
			szDevices[iSlot] = effect.request.hDevice;
		} else if ( szDevices[iSlot].nGeneration == effect.request.hDevice.nGeneration ) {
			if ( nLeft > szLeft[iSlot] ) {
				szLeft[iSlot] = nLeft;
			}
			if ( nRight > szRight[iSlot] ) {
				szRight[iSlot] = nRight;
			}
		}
	}

	bool bOkay = true;
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirInputHapticMotorState_t &motor = m_Motors[i];
		if ( motor.bActive && ( szPriority[i] < 0 || motor.hDevice.nGeneration != szDevices[i].nGeneration ) ) {
			if ( !pSink( pContext, motor.hDevice, 0, 0 ) ) {
				bOkay = false;
				continue;
			}
			motor = {};
		}
		if ( szPriority[i] < 0 ) {
			continue;
		}
		if ( !motor.bActive || motor.nLeft != szLeft[i] || motor.nRight != szRight[i] ) {
			if ( !pSink( pContext, szDevices[i], szLeft[i], szRight[i] ) ) {
				bOkay = false;
				continue;
			}
			motor = { szDevices[i], szLeft[i], szRight[i], true };
		}
	}
	return bOkay;
}

void SIRInputHaptics::forgetDevice( sirInputDeviceId_t hDevice, sirInputRumbleSink_t pSink, void *pContext ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_HAPTIC_EFFECT_CAPACITY; ++i ) {
		if ( m_Effects[i].bActive && m_Effects[i].request.hDevice.nSlot == hDevice.nSlot &&
		     m_Effects[i].request.hDevice.nGeneration == hDevice.nGeneration ) {
			m_Effects[i].bActive = false;
		}
	}
	if ( hDevice.nSlot < SIR_INPUT_DEVICE_CAPACITY ) {
		sirInputHapticMotorState_t &motor = m_Motors[hDevice.nSlot];
		if ( motor.bActive && motor.hDevice.nGeneration == hDevice.nGeneration ) {
			if ( pSink != nullptr ) {
				( void )pSink( pContext, hDevice, 0, 0 );
			}
			motor = {};
		}
	}
}

void SIRInputHaptics::stopAll( sirInputRumbleSink_t pSink, void *pContext ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		if ( m_Motors[i].bActive && pSink != nullptr ) {
			( void )pSink( pContext, m_Motors[i].hDevice, 0, 0 );
		}
	}
	memset( m_Motors, 0, sizeof( m_Motors ) );
	memset( m_Effects, 0, sizeof( m_Effects ) );
}

uint32_t SIRInputHaptics::activeCount( void ) const noexcept
{
	uint32_t nActive = 0;
	for ( uint32_t i = 0; i < SIR_INPUT_HAPTIC_EFFECT_CAPACITY; ++i ) {
		nActive += m_Effects[i].bActive ? 1U : 0U;
	}
	return nActive;
}
