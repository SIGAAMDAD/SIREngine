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

#include <Core/Platform/SIRInputControllerProfiles.h>

SIRInputControllerProfiles::SIRInputControllerProfiles( void ) noexcept : m_Profiles{}, m_nCount( 0 )
{
}

bool SIRInputControllerProfiles::addProfile( const sirInputControllerProfile_t &profile ) noexcept
{
	if ( m_nCount >= SIR_INPUT_MAX_CONTROLLER_PROFILES || profile.nVendor == 0 || profile.nProduct == 0 ||
	     profile.nButtonCount > SIR_INPUT_MAX_PROFILE_BUTTONS || profile.nAxisCount != SIR_GAMEPAD_AXIS_COUNT ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		const sirInputControllerProfile_t &stored = m_Profiles[i];
		if ( stored.nBus == profile.nBus && stored.nVendor == profile.nVendor && stored.nProduct == profile.nProduct ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < profile.nButtonCount; ++i ) {
		const sirInputControllerButtonBinding_t &binding = profile.buttons[i];
		if ( binding.nLogicalCode >= SIR_GAMEPAD_BUTTON_COUNT ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( profile.buttons[j].nNativeCode == binding.nNativeCode || profile.buttons[j].nLogicalCode == binding.nLogicalCode ) {
				return false;
			}
		}
	}
	uint32_t nAxisMask = 0;
	for ( uint32_t i = 0; i < profile.nAxisCount; ++i ) {
		const sirInputControllerAxisBinding_t &binding = profile.axes[i];
		if ( binding.nLogicalCode >= SIR_GAMEPAD_AXIS_COUNT ||
		     ( binding.nLogicalCode >= SIR_GAMEPAD_AXIS_LEFT_TRIGGER && binding.bInvert ) ||
		     ( nAxisMask & ( UINT32_C( 1 ) << binding.nLogicalCode ) ) != 0 ) {
			return false;
		}
		nAxisMask |= UINT32_C( 1 ) << binding.nLogicalCode;
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( profile.axes[j].nNativeCode == binding.nNativeCode ) {
				return false;
			}
		}
	}
	if ( nAxisMask != ( UINT32_C( 1 ) << SIR_GAMEPAD_AXIS_COUNT ) - 1 ) {
		return false;
	}
	m_Profiles[m_nCount++] = profile;
	return true;
}

const sirInputControllerProfile_t *SIRInputControllerProfiles::findProfile( uint16_t nBus, uint16_t nVendor,
                                                                            uint16_t nProduct ) const noexcept
{
	const sirInputControllerProfile_t *pWildcard = nullptr;
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		const sirInputControllerProfile_t &profile = m_Profiles[i];
		if ( profile.nVendor != nVendor || profile.nProduct != nProduct ) {
			continue;
		}
		if ( profile.nBus == nBus && nBus != 0 ) {
			return &profile;
		}
		if ( profile.nBus == 0 ) {
			pWildcard = &profile;
		}
	}
	return pWildcard;
}

int16_t SIRInputControllerProfiles::findButton( const sirInputControllerProfile_t &profile, uint16_t nNativeCode ) noexcept
{
	for ( uint32_t i = 0; i < profile.nButtonCount; ++i ) {
		if ( profile.buttons[i].nNativeCode == nNativeCode ) {
			return ( int16_t )profile.buttons[i].nLogicalCode;
		}
	}
	return -1;
}

int16_t SIRInputControllerProfiles::findAxis( const sirInputControllerProfile_t &profile, uint16_t nNativeCode ) noexcept
{
	for ( uint32_t i = 0; i < profile.nAxisCount; ++i ) {
		if ( profile.axes[i].nNativeCode == nNativeCode ) {
			return ( int16_t )profile.axes[i].nLogicalCode;
		}
	}
	return -1;
}

uint32_t SIRInputControllerProfiles::getCount( void ) const noexcept
{
	return m_nCount;
}
