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
#include <stdio.h>
#include <string.h>

static void sirProfileWrite16( uint8_t *pData, uint16_t nValue ) noexcept
{
	pData[0] = ( uint8_t )nValue;
	pData[1] = ( uint8_t )( nValue >> 8 );
}

static void sirProfileWrite32( uint8_t *pData, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pData[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static uint32_t sirProfileChecksum( const uint8_t *pData, uint32_t nSize ) noexcept
{
	uint32_t nHash = UINT32_C( 2166136261 );
	for ( uint32_t i = 0; i < nSize; ++i ) {
		nHash = ( nHash ^ pData[i] ) * UINT32_C( 16777619 );
	}
	return nHash;
}

static bool sirProfileInvalidRegistrations( void ) noexcept
{
	SIRInputControllerProfiles profiles;
	sirInputControllerProfile_t profile = {};
	profile.nVendor = 100;
	profile.nProduct = 200;
	if ( profiles.add( {} ) || profiles.profileAt( 0 ) != nullptr ) {
		return false;
	}
	sirInputControllerProfile_t invalid = profile;
	invalid.nProduct = 0;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = profile;
	invalid.nButtonCount = SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY + 1;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = profile;
	invalid.nAxisCount = SIR_INPUT_AXIS_COUNT + 1;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = profile;
	invalid.nButtonCount = 1;
	invalid.buttons[0] = { 33, SIR_INPUT_PAD_BUTTON_COUNT };
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid.buttons[0].nLogicalCode = SIR_INPUT_PAD_SOUTH;
	invalid.nButtonCount = 2;
	invalid.buttons[1] = { 33, SIR_INPUT_PAD_EAST };
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid.buttons[1] = { 34, SIR_INPUT_PAD_SOUTH };
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = profile;
	invalid.nAxisCount = 1;
	invalid.axes[0] = { 22, SIR_INPUT_AXIS_COUNT, false };
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid.axes[0] = { 22, SIR_INPUT_AXIS_LT, true };
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid.axes[0] = { 22, SIR_INPUT_AXIS_LX, false };
	invalid.nAxisCount = 2;
	invalid.axes[1] = { 22, SIR_INPUT_AXIS_LY, false };
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid.axes[1] = { 23, SIR_INPUT_AXIS_LX, false };
	if ( profiles.add( invalid ) ) {
		return false;
	}
	profile.nButtonCount = 1;
	profile.buttons[0] = { 33, SIR_INPUT_PAD_NORTH };
	profile.nAxisCount = 1;
	profile.axes[0] = { 22, SIR_INPUT_AXIS_LY, true };
	if ( !profiles.add( profile ) || profiles.add( profile ) || profiles.count() != 1 || profiles.profileAt( 1 ) != nullptr ||
	     profiles.find( 5, 999, 200 ) != nullptr ) {
		return false;
	}
	const sirInputControllerProfile_t *pFound = profiles.find( 5, 100, 200 );
	bool bInverted = false;
	if ( pFound == nullptr || SIRInputControllerProfiles::button( *pFound, 33 ) != SIR_INPUT_PAD_NORTH ||
	     SIRInputControllerProfiles::button( *pFound, 999 ) != -1 ||
	     SIRInputControllerProfiles::axis( *pFound, 22, &bInverted ) != SIR_INPUT_AXIS_LY || !bInverted ||
	     SIRInputControllerProfiles::axis( *pFound, 22, nullptr ) != SIR_INPUT_AXIS_LY ||
	     SIRInputControllerProfiles::axis( *pFound, 999, &bInverted ) != -1 ) {
		return false;
	}
	profile.nBus = 5;
	if ( !profiles.add( profile ) || profiles.find( 5, 100, 200 )->nBus != 5 || profiles.find( 6, 100, 200 )->nBus != 0 ) {
		return false;
	}
	return profiles.profileAt( 1 ) != nullptr;
}

static bool sirProfileRejectedCookedData( void ) noexcept
{
	SIRInputControllerProfiles profiles;
	sirInputControllerProfile_t profile = {};
	profile.nVendor = 0x1234;
	profile.nProduct = 0xabcd;
	profile.nButtonCount = 1;
	profile.nAxisCount = 1;
	profile.buttons[0] = { 304, SIR_INPUT_PAD_SOUTH };
	profile.axes[0] = { 3, SIR_INPUT_AXIS_LX, true };
	if ( !profiles.add( profile ) ) {
		return false;
	}
	uint8_t szValid[128] = {};
	uint32_t nSize = 0;
	if ( profiles.saveCooked( nullptr, sizeof( szValid ), &nSize ) || nSize != 42 || profiles.saveCooked( szValid, nSize - 1, &nSize ) ||
	     !profiles.saveCooked( szValid, sizeof( szValid ), &nSize ) ) {
		return false;
	}
	SIRInputControllerProfiles restored;
	if ( !restored.loadCooked( szValid, nSize ) || restored.count() != 1 || restored.loadCooked( nullptr, nSize ) ||
	     restored.loadCooked( szValid, nSize - 1 ) || restored.loadCooked( szValid, SIR_INPUT_COOKED_PROFILE_MAX_BYTES + 1 ) ) {
		return false;
	}
	const uint32_t szHeaderOffsets[] = { 0, 4, 6, 8, 12, 16 };
	for ( uint32_t i = 0; i < sizeof( szHeaderOffsets ) / sizeof( szHeaderOffsets[0] ); ++i ) {
		uint8_t szCorrupt[128] = {};
		memcpy( szCorrupt, szValid, nSize );
		szCorrupt[szHeaderOffsets[i]] ^= 0xff;
		if ( restored.loadCooked( szCorrupt, nSize ) || restored.count() != 1 ) {
			return false;
		}
	}
	const uint32_t szBodyOffsets[] = { 20 + 2,  20 + 6,      20 + 8,          20 + 9,          20 + 10,
	                                   20 + 11, 20 + 12 + 2, 20 + 12 + 4 + 2, 20 + 12 + 4 + 4, 20 + 12 + 4 + 5 };
	const uint8_t szBadValues[] = { 0, 0x80, 32, 6, 1, 1, 0xff, 0xff, 2, 1 };
	for ( uint32_t i = 0; i < sizeof( szBodyOffsets ) / sizeof( szBodyOffsets[0] ); ++i ) {
		uint8_t szCorrupt[128] = {};
		memcpy( szCorrupt, szValid, nSize );
		szCorrupt[szBodyOffsets[i]] = szBadValues[i];
		if ( i == 0 ) {
			sirProfileWrite16( szCorrupt + 20 + 2, 0 );
		}
		sirProfileWrite32( szCorrupt + 16, sirProfileChecksum( szCorrupt + SIR_INPUT_COOKED_PROFILE_HEADER_BYTES,
		                                                       nSize - SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) );
		if ( restored.loadCooked( szCorrupt, nSize ) || restored.count() != 1 ) {
			return false;
		}
	}
	uint8_t szTrailing[128] = {};
	memcpy( szTrailing, szValid, nSize );
	szTrailing[nSize] = 0x56;
	sirProfileWrite32( szTrailing + 12, nSize + 1 );
	sirProfileWrite32( szTrailing + 16, sirProfileChecksum( szTrailing + SIR_INPUT_COOKED_PROFILE_HEADER_BYTES,
	                                                        nSize + 1 - SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) );
	if ( restored.loadCooked( szTrailing, nSize + 1 ) || restored.count() != 1 ) {
		return false;
	}
	return restored.profileAt( 0 )->nVendor == profile.nVendor;
}

static bool sirProfileSerializationBoundaries( void ) noexcept
{
	SIRInputControllerProfiles empty;
	uint8_t szBytes[256] = {};
	uint32_t nWritten = 0;
	if ( !empty.saveCooked( szBytes, sizeof( szBytes ), nullptr ) || !empty.saveCooked( szBytes, sizeof( szBytes ), &nWritten ) ||
	     nWritten != SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) {
		return false;
	}
	SIRInputControllerProfiles restored;
	if ( !restored.loadCooked( szBytes, nWritten ) || restored.count() != 0 ||
	     restored.loadCooked( szBytes, SIR_INPUT_COOKED_PROFILE_HEADER_BYTES - 1 ) ) {
		return false;
	}
	for ( uint32_t nFlags = 0; nFlags < 4; ++nFlags ) {
		SIRInputControllerProfiles profiles;
		sirInputControllerProfile_t profile = {};
		profile.nBus = 7;
		profile.nVendor = 0xaabb;
		profile.nProduct = 0xccdd;
		profile.bReplaceButtons = ( nFlags & 1 ) != 0;
		profile.bReplaceAxes = ( nFlags & 2 ) != 0;
		profile.nButtonCount = 1;
		profile.nAxisCount = 1;
		profile.buttons[0] = { 20, SIR_INPUT_PAD_EAST };
		profile.axes[0] = { 30, SIR_INPUT_AXIS_LX, false };
		if ( !profiles.add( profile ) || profiles.find( 7, profile.nVendor, profile.nProduct ) == nullptr ||
		     profiles.find( 7, profile.nVendor, profile.nProduct + 1 ) != nullptr ||
		     profiles.find( 7, profile.nVendor + 1, profile.nProduct ) != nullptr ) {
			return false;
		}
		if ( !profiles.saveCooked( szBytes, sizeof( szBytes ), nullptr ) || !profiles.saveCooked( szBytes, sizeof( szBytes ), &nWritten ) ||
		     nWritten != 42 || !restored.loadCooked( szBytes, nWritten ) ) {
			return false;
		}
		const sirInputControllerProfile_t *pResult = restored.find( 7, 0xaabb, 0xccdd );
		bool bInvert = true;
		if ( pResult == nullptr || pResult->bReplaceButtons != profile.bReplaceButtons || pResult->bReplaceAxes != profile.bReplaceAxes ||
		     SIRInputControllerProfiles::axis( *pResult, 30, &bInvert ) != SIR_INPUT_AXIS_LX || bInvert ) {
			return false;
		}
		uint8_t szCorrupted[256] = {};
		memcpy( szCorrupted, szBytes, nWritten );
		sirProfileWrite32( szCorrupted + 12, SIR_INPUT_COOKED_PROFILE_HEADER_BYTES );
		sirProfileWrite32( szCorrupted + 16, sirProfileChecksum( szCorrupted + SIR_INPUT_COOKED_PROFILE_HEADER_BYTES, 0 ) );
		if ( restored.loadCooked( szCorrupted, SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) ) {
			return false;
		}
		memcpy( szCorrupted, szBytes, nWritten );
		szCorrupted[SIR_INPUT_COOKED_PROFILE_HEADER_BYTES + 8] = SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY + 1;
		sirProfileWrite32( szCorrupted + 16, sirProfileChecksum( szCorrupted + SIR_INPUT_COOKED_PROFILE_HEADER_BYTES,
		                                                         nWritten - SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) );
		if ( restored.loadCooked( szCorrupted, nWritten ) ) {
			return false;
		}
		memcpy( szCorrupted, szBytes, nWritten );
		szCorrupted[SIR_INPUT_COOKED_PROFILE_HEADER_BYTES + 9] = SIR_INPUT_AXIS_COUNT + 1;
		sirProfileWrite32( szCorrupted + 16, sirProfileChecksum( szCorrupted + SIR_INPUT_COOKED_PROFILE_HEADER_BYTES,
		                                                         nWritten - SIR_INPUT_COOKED_PROFILE_HEADER_BYTES ) );
		if ( restored.loadCooked( szCorrupted, nWritten ) ) {
			return false;
		}
		if ( restored.count() != 1 || restored.profileAt( 0 )->nVendor != profile.nVendor ) {
			return false;
		}
	}
	sirInputControllerProfile_t first = {};
	first.nBus = 5;
	first.nVendor = 91;
	first.nProduct = 101;
	sirInputControllerProfile_t second = first;
	second.nProduct = 102;
	SIRInputControllerProfiles distinct;
	if ( !distinct.add( first ) || !distinct.add( second ) || distinct.count() != 2 || distinct.find( 5, 91, 101 ) == nullptr ||
	     distinct.find( 5, 91, 102 ) == nullptr ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirProfileInvalidRegistrations() ) {
		fputs( "input profiles: invalid registration FAILED\n", stderr );
		return 1;
	}
	if ( !sirProfileRejectedCookedData() ) {
		fputs( "input profiles: corrupt cooked data FAILED\n", stderr );
		return 1;
	}
	if ( !sirProfileSerializationBoundaries() ) {
		fputs( "input profiles: serialization boundaries FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputProfileNegativeTests PASS" );
	return 0;
}
