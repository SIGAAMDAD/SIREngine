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

#ifndef __SIRENGINE_SIRINPUTCONTROLLERPROFILES_H__
#define __SIRENGINE_SIRINPUTCONTROLLERPROFILES_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Core/Platform/SIRInput.h>

constexpr uint32_t SIR_INPUT_MAX_CONTROLLER_PROFILES = 32;
constexpr uint32_t SIR_INPUT_MAX_PROFILE_BUTTONS = 32;

typedef struct sirInputControllerButtonBinding_s
{
	uint16_t nNativeCode;
	uint16_t nLogicalCode;
} sirInputControllerButtonBinding_t;

typedef struct sirInputControllerAxisBinding_s
{
	uint16_t nNativeCode;
	uint16_t nLogicalCode;
	bool bInvert;
} sirInputControllerAxisBinding_t;

// Native report-code mapping, selected once at device open. Bus 0 matches any bus.
// A device-specific profile fully replaces the standard axis layout; unknown
// buttons fall through to the generic map unless bReplaceButtons is true.
typedef struct sirInputControllerProfile_s
{
	uint16_t nBus;
	uint16_t nVendor;
	uint16_t nProduct;
	uint8_t nButtonCount;
	uint8_t nAxisCount;
	bool bReplaceButtons;
	sirInputControllerButtonBinding_t buttons[SIR_INPUT_MAX_PROFILE_BUTTONS];
	sirInputControllerAxisBinding_t axes[SIR_GAMEPAD_AXIS_COUNT];
} sirInputControllerProfile_t;

class SIRInputControllerProfiles
{
public:
	SIRInputControllerProfiles( void ) noexcept;
	bool addProfile( const sirInputControllerProfile_t &profile ) noexcept;
	const sirInputControllerProfile_t *findProfile( uint16_t nBus, uint16_t nVendor, uint16_t nProduct ) const noexcept;
	static int16_t findButton( const sirInputControllerProfile_t &profile, uint16_t nNativeCode ) noexcept;
	static int16_t findAxis( const sirInputControllerProfile_t &profile, uint16_t nNativeCode ) noexcept;
	uint32_t getCount( void ) const noexcept;

private:
	sirInputControllerProfile_t m_Profiles[SIR_INPUT_MAX_CONTROLLER_PROFILES];
	uint32_t m_nCount;
};

#endif // __SIRENGINE_SIRINPUTCONTROLLERPROFILES_H__
