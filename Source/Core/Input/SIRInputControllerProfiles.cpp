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

#include <Core/Input/SIRInputControllerProfiles.h>
#include <string.h>

static void sirInputCookWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

static void sirInputCookWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pBytes[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static uint16_t sirInputCookRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )( pBytes[0] | ( ( uint16_t )pBytes[1] << 8 ) );
}

static uint32_t sirInputCookRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

static uint32_t sirInputCookHash( const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	uint32_t nHash = UINT32_C( 2166136261 );
	for ( uint32_t i = 0; i < nBytes; ++i ) {
		nHash = ( nHash ^ pBytes[i] ) * UINT32_C( 16777619 );
	}
	return nHash;
}

SIRInputControllerProfiles::SIRInputControllerProfiles( void ) noexcept : m_Profiles{}, m_nCount( 0 )
{
}

bool SIRInputControllerProfiles::add( const sirInputControllerProfile_t& profile ) noexcept
{
	if ( m_nCount >= SIR_INPUT_CONTROLLER_PROFILE_CAPACITY || profile.nVendor == 0 || profile.nProduct == 0 ||
	     profile.nButtonCount > SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY || profile.nAxisCount > SIR_INPUT_AXIS_COUNT ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		const sirInputControllerProfile_t& registered = m_Profiles[i];
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
		const sirInputControllerProfile_t& profile = m_Profiles[i];
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

int16_t SIRInputControllerProfiles::button( const sirInputControllerProfile_t& profile, uint16_t nNativeCode ) noexcept
{
	for ( uint32_t i = 0; i < profile.nButtonCount; ++i ) {
		if ( profile.buttons[i].nNativeCode == nNativeCode ) {
			return ( int16_t )profile.buttons[i].nLogicalCode;
		}
	}
	return -1;
}

int16_t SIRInputControllerProfiles::axis( const sirInputControllerProfile_t& profile, uint16_t nNativeCode, bool *pInvert ) noexcept
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

const sirInputControllerProfile_t *SIRInputControllerProfiles::profileAt( uint32_t iProfile ) const noexcept
{
	return iProfile < m_nCount ? &m_Profiles[iProfile] : nullptr;
}

bool SIRInputControllerProfiles::saveCooked( uint8_t *pBytes, uint32_t nCapacity, uint32_t *pWritten ) const noexcept
{
	uint32_t nRequired = SIR_INPUT_COOKED_PROFILE_HEADER_BYTES;
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		const sirInputControllerProfile_t& profile = m_Profiles[i];
		nRequired += SIR_INPUT_COOKED_PROFILE_RECORD_BYTES + ( uint32_t )profile.nButtonCount * 4 +
		             ( uint32_t )profile.nAxisCount * SIR_INPUT_COOKED_PROFILE_AXIS_BYTES;
	}
	if ( pWritten != nullptr ) {
		*pWritten = nRequired;
	}
	if ( pBytes == nullptr || nRequired > nCapacity ) {
		return false;
	}

	memset( pBytes, 0, nRequired );
	sirInputCookWrite32( pBytes, UINT32_C( 0x50434953 ) ); // SICP
	sirInputCookWrite16( pBytes + 4, 1 );
	sirInputCookWrite16( pBytes + 6, SIR_INPUT_COOKED_PROFILE_HEADER_BYTES );
	sirInputCookWrite32( pBytes + 8, m_nCount );
	sirInputCookWrite32( pBytes + 12, nRequired );
	uint32_t nOffset = SIR_INPUT_COOKED_PROFILE_HEADER_BYTES;
	for ( uint32_t i = 0; i < m_nCount; ++i ) {
		const sirInputControllerProfile_t& profile = m_Profiles[i];
		sirInputCookWrite16( pBytes + nOffset, profile.nBus );
		sirInputCookWrite16( pBytes + nOffset + 2, profile.nVendor );
		sirInputCookWrite16( pBytes + nOffset + 4, profile.nProduct );
		sirInputCookWrite16( pBytes + nOffset + 6, ( uint16_t )( ( profile.bReplaceButtons ? 1 : 0 ) | ( profile.bReplaceAxes ? 2 : 0 ) ) );
		pBytes[nOffset + 8] = profile.nButtonCount;
		pBytes[nOffset + 9] = profile.nAxisCount;
		nOffset += SIR_INPUT_COOKED_PROFILE_RECORD_BYTES;
		for ( uint32_t j = 0; j < profile.nButtonCount; ++j ) {
			sirInputCookWrite16( pBytes + nOffset, profile.buttons[j].nNativeCode );
			sirInputCookWrite16( pBytes + nOffset + 2, profile.buttons[j].nLogicalCode );
			nOffset += 4;
		}
		for ( uint32_t j = 0; j < profile.nAxisCount; ++j ) {
			sirInputCookWrite16( pBytes + nOffset, profile.axes[j].nNativeCode );
			sirInputCookWrite16( pBytes + nOffset + 2, profile.axes[j].nLogicalCode );
			pBytes[nOffset + 4] = profile.axes[j].bInvert ? 1 : 0;
			nOffset += SIR_INPUT_COOKED_PROFILE_AXIS_BYTES;
		}
	}
	sirInputCookWrite32( pBytes + 16, sirInputCookHash( pBytes + SIR_INPUT_COOKED_PROFILE_HEADER_BYTES,
	                                                    nRequired - SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) );
	return true;
}

bool SIRInputControllerProfiles::loadCooked( const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	if ( pBytes == nullptr || nBytes < SIR_INPUT_COOKED_PROFILE_HEADER_BYTES || nBytes > SIR_INPUT_COOKED_PROFILE_MAX_BYTES ||
	     sirInputCookRead32( pBytes ) != UINT32_C( 0x50434953 ) || sirInputCookRead16( pBytes + 4 ) != 1 ||
	     sirInputCookRead16( pBytes + 6 ) != SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) {
		return false;
	}
	const uint32_t nCount = sirInputCookRead32( pBytes + 8 );
	if ( nCount > SIR_INPUT_CONTROLLER_PROFILE_CAPACITY || sirInputCookRead32( pBytes + 12 ) != nBytes ||
	     sirInputCookRead32( pBytes + 16 ) !=
	         sirInputCookHash( pBytes + SIR_INPUT_COOKED_PROFILE_HEADER_BYTES, nBytes - SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) ) {
		return false;
	}

	SIRInputControllerProfiles decoded;
	uint32_t nOffset = SIR_INPUT_COOKED_PROFILE_HEADER_BYTES;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( nBytes - nOffset < SIR_INPUT_COOKED_PROFILE_RECORD_BYTES ) {
			return false;
		}
		sirInputControllerProfile_t profile = {};
		profile.nBus = sirInputCookRead16( pBytes + nOffset );
		profile.nVendor = sirInputCookRead16( pBytes + nOffset + 2 );
		profile.nProduct = sirInputCookRead16( pBytes + nOffset + 4 );
		const uint16_t nFlags = sirInputCookRead16( pBytes + nOffset + 6 );
		profile.nButtonCount = pBytes[nOffset + 8];
		profile.nAxisCount = pBytes[nOffset + 9];
		if ( ( nFlags & ~3U ) != 0 || pBytes[nOffset + 10] != 0 || pBytes[nOffset + 11] != 0 ||
		     profile.nButtonCount > SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY || profile.nAxisCount > SIR_INPUT_AXIS_COUNT ) {
			return false;
		}
		profile.bReplaceButtons = ( nFlags & 1 ) != 0;
		profile.bReplaceAxes = ( nFlags & 2 ) != 0;
		nOffset += SIR_INPUT_COOKED_PROFILE_RECORD_BYTES;
		const uint32_t nRecordBytes =
		    ( uint32_t )profile.nButtonCount * 4 + ( uint32_t )profile.nAxisCount * SIR_INPUT_COOKED_PROFILE_AXIS_BYTES;
		if ( nRecordBytes > nBytes - nOffset ) {
			return false;
		}
		for ( uint32_t j = 0; j < profile.nButtonCount; ++j ) {
			profile.buttons[j].nNativeCode = sirInputCookRead16( pBytes + nOffset );
			profile.buttons[j].nLogicalCode = sirInputCookRead16( pBytes + nOffset + 2 );
			nOffset += 4;
		}
		for ( uint32_t j = 0; j < profile.nAxisCount; ++j ) {
			profile.axes[j].nNativeCode = sirInputCookRead16( pBytes + nOffset );
			profile.axes[j].nLogicalCode = sirInputCookRead16( pBytes + nOffset + 2 );
			const uint8_t nInvert = pBytes[nOffset + 4];
			if ( nInvert > 1 || pBytes[nOffset + 5] != 0 ) {
				return false;
			}
			profile.axes[j].bInvert = nInvert != 0;
			nOffset += SIR_INPUT_COOKED_PROFILE_AXIS_BYTES;
		}
		if ( !decoded.add( profile ) ) {
			return false;
		}
	}
	if ( nOffset != nBytes ) {
		return false;
	}
	*this = decoded;
	return true;
}
