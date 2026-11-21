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

#include <Core/Platform/Windows/SIRWindowsGameInputCodec.h>

#include <limits.h>

typedef struct sirGameInputButtonMap_s
{
	uint32_t nNative;
	uint16_t nControl;
} sirGameInputButtonMap_t;

// Native GameInputGamepadButtons mask values (Microsoft GameInput v1+).
static constexpr sirGameInputButtonMap_t s_ButtonMap[] = {
    { 0x00000004U, SIR_INPUT_PAD_SOUTH },     { 0x00000008U, SIR_INPUT_PAD_EAST },      { 0x00000010U, SIR_INPUT_PAD_WEST },
    { 0x00000020U, SIR_INPUT_PAD_NORTH },     { 0x00000400U, SIR_INPUT_PAD_LB },        { 0x00000800U, SIR_INPUT_PAD_RB },
    { 0x00000002U, SIR_INPUT_PAD_BACK },      { 0x00000001U, SIR_INPUT_PAD_START },     { 0x00001000U, SIR_INPUT_PAD_L3 },
    { 0x00002000U, SIR_INPUT_PAD_R3 },        { 0x00000040U, SIR_INPUT_PAD_DPAD_UP },   { 0x00000080U, SIR_INPUT_PAD_DPAD_DOWN },
    { 0x00000100U, SIR_INPUT_PAD_DPAD_LEFT }, { 0x00000200U, SIR_INPUT_PAD_DPAD_RIGHT } };

int32_t SIRWindowsGameInputCodec::normalizeAxis( float fValue, bool bTrigger ) noexcept
{
	if ( !( fValue >= -1.0f && fValue <= 1.0f ) ) {
		if ( fValue > 1.0f ) {
			fValue = 1.0f;
		} else if ( fValue < -1.0f ) {
			fValue = -1.0f;
		} else {
			fValue = 0.0f;
		}
	}
	if ( bTrigger && fValue < 0.0f ) {
		fValue = 0.0f;
	}
	const float fScaled = fValue * 32767.0f;
	return ( int32_t )( fScaled + ( fScaled >= 0.0f ? 0.5f : -0.5f ) );
}

uint64_t SIRWindowsGameInputCodec::timestampNanoseconds( uint64_t nMicroseconds ) noexcept
{
	if ( nMicroseconds > UINT64_MAX / 1000U ) {
		return 0;
	}
	return nMicroseconds * 1000U;
}

bool SIRWindowsGameInputCodec::decode( const sirGameInputRawGamepad_t& raw, const sirGameInputDecodedGamepad_t& previous,
                                       sirGameInputDecodedGamepad_t *pNext, sirInputControlChange_t *pChanges, uint32_t nCapacity,
                                       uint32_t *pCount ) noexcept
{
	if ( pNext == nullptr || pChanges == nullptr || pCount == nullptr || nCapacity < MAX_CHANGES ) {
		return false;
	}

	sirGameInputDecodedGamepad_t next = {};
	next.nButtons = raw.nButtons;
	next.nAxes[SIR_INPUT_AXIS_LX] = normalizeAxis( raw.fLX, false );
	next.nAxes[SIR_INPUT_AXIS_LY] = normalizeAxis( raw.fLY, false );
	next.nAxes[SIR_INPUT_AXIS_RX] = normalizeAxis( raw.fRX, false );
	next.nAxes[SIR_INPUT_AXIS_RY] = normalizeAxis( raw.fRY, false );
	next.nAxes[SIR_INPUT_AXIS_LT] = normalizeAxis( raw.fLT, true );
	next.nAxes[SIR_INPUT_AXIS_RT] = normalizeAxis( raw.fRT, true );
	next.bValid = true;

	uint32_t nChanged = 0;
	for ( uint32_t i = 0; i < sizeof( s_ButtonMap ) / sizeof( s_ButtonMap[0] ); ++i ) {
		const uint32_t nMask = s_ButtonMap[i].nNative;
		const bool bDown = ( next.nButtons & nMask ) != 0;
		if ( !previous.bValid || bDown != ( ( previous.nButtons & nMask ) != 0 ) ) {
			pChanges[nChanged++] = { SIR_INPUT_BUTTON, s_ButtonMap[i].nControl, bDown ? 1 : 0 };
		}
	}
	for ( uint16_t i = 0; i < SIR_INPUT_AXIS_COUNT; ++i ) {
		if ( !previous.bValid || next.nAxes[i] != previous.nAxes[i] ) {
			pChanges[nChanged++] = { SIR_INPUT_AXIS, i, next.nAxes[i] };
		}
	}

	*pNext = next;
	*pCount = nChanged;
	return true;
}
