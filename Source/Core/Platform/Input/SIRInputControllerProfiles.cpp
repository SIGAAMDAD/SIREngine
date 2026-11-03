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

#include <Core/Platform/Input/SIRInputControllerProfiles.h>

SIRInputControllerProfiles::SIRInputControllerProfiles( void ) noexcept : m_Profiles{}, m_nCount( 0 )
{
}

bool SIRInputControllerProfiles::add( const sirInputControllerProfile_t &profile ) noexcept
{
	if ( m_nCount >= SIR_INPUT_CONTROLLER_PROFILE_CAPACITY || profile.nVendor == 0 || profile.nProduct == 0 ||
	     profile.nButtonCount > SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY || profile.nAxisCount > SIR_INPUT_AXIS_COUNT ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		const sirInputControllerProfile_t &registered = m_Profiles[i];
		if ( registered.nBus == profile.nBus && registered.nVendor == profile.nVendor && registered.nProduct == profile.nProduct ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < profile.nButtonCount; ++i ) {
		if ( profile.buttons[i].nLogicalCode >= SIR_INPUT_PAD_BUTTON_COUNT ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( profile.buttons[i].nNativeCode == profile.buttons[j].nNativeCode ||
			     profile.buttons[i].nLogicalCode == profile.buttons[j].nLogicalCode ) {
				return false;
			}
		}
	}
	for ( uint32_t i = 0; i < profile.nAxisCount; ++i ) {
		if ( profile.axes[i].nLogicalCode >= SIR_INPUT_AXIS_COUNT ||
		     ( profile.axes[i].bInvert && profile.axes[i].nLogicalCode >= SIR_INPUT_AXIS_LT ) ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( profile.axes[i].nNativeCode == profile.axes[j].nNativeCode ||
			     profile.axes[i].nLogicalCode == profile.axes[j].nLogicalCode ) {
				return false;
			}
		}
	}
	m_Profiles[m_nCount++] = profile;
	return true;
}

const sirInputControllerProfile_t *SIRInputControllerProfiles::find( uint16_t nBus, uint16_t nVendor, uint16_t nProduct ) const noexcept
{
	const sirInputControllerProfile_t *pGeneric = nullptr;
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		const sirInputControllerProfile_t &profile = m_Profiles[i];
		if ( profile.nVendor == nVendor && profile.nProduct == nProduct ) {
			if ( profile.nBus == nBus ) {
				return &profile;
			}
			if ( profile.nBus == 0 ) {
				pGeneric = &profile;
			}
		}
	}
	return pGeneric;
}

int16_t SIRInputControllerProfiles::button( const sirInputControllerProfile_t &profile, uint16_t nNativeCode ) noexcept
{
	for ( uint32_t i = 0; i < profile.nButtonCount; ++i ) {
		if ( profile.buttons[i].nNativeCode == nNativeCode ) {
			return ( int16_t )profile.buttons[i].nLogicalCode;
		}
	}
	return -1;
}

int16_t SIRInputControllerProfiles::axis( const sirInputControllerProfile_t &profile, uint16_t nNativeCode, bool *pInvert ) noexcept
{
	for ( uint32_t i = 0; i < profile.nAxisCount; ++i ) {
		if ( profile.axes[i].nNativeCode == nNativeCode ) {
			if ( pInvert != nullptr ) {
				*pInvert = profile.axes[i].bInvert;
			}
			return ( int16_t )profile.axes[i].nLogicalCode;
		}
	}
	return -1;
}

uint32_t SIRInputControllerProfiles::count( void ) const noexcept
{
	return m_nCount;
}
