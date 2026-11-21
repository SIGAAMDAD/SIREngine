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

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputTypes.h>

constexpr uint32_t SIR_INPUT_CONTROLLER_PROFILE_CAPACITY = 32;
constexpr uint32_t SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY = 32;
constexpr uint32_t SIR_INPUT_COOKED_PROFILE_HEADER_BYTES = 20;
constexpr uint32_t SIR_INPUT_COOKED_PROFILE_RECORD_BYTES = 12;
constexpr uint32_t SIR_INPUT_COOKED_PROFILE_AXIS_BYTES = 6;
constexpr uint32_t SIR_INPUT_COOKED_PROFILE_MAX_BYTES =
    SIR_INPUT_COOKED_PROFILE_HEADER_BYTES +
    SIR_INPUT_CONTROLLER_PROFILE_CAPACITY * ( SIR_INPUT_COOKED_PROFILE_RECORD_BYTES + SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY * 4 +
	                                          SIR_INPUT_AXIS_COUNT * SIR_INPUT_COOKED_PROFILE_AXIS_BYTES );

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

// The device transport/bus is 0 for a transport-independent profile.
// Replace flags disable generic mappings not explicitly declared here.
typedef struct sirInputControllerProfile_s
{
	uint16_t nBus;
	uint16_t nVendor;
	uint16_t nProduct;
	uint8_t nButtonCount;
	uint8_t nAxisCount;
	bool bReplaceButtons;
	bool bReplaceAxes;
	sirInputControllerButtonBinding_t buttons[SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY];
	sirInputControllerAxisBinding_t axes[SIR_INPUT_AXIS_COUNT];
} sirInputControllerProfile_t;

// Registration occurs before discovery. Profiles are copied into each device
// at open; the registry retains no borrowed mapping asset pointers.
class SIRInputControllerProfiles
{
public:
	SIRInputControllerProfiles( void ) noexcept;
	bool add( const sirInputControllerProfile_t& profile ) noexcept;
	const sirInputControllerProfile_t *find( uint16_t nBus, uint16_t nVendor, uint16_t nProduct ) const noexcept;
	static int16_t button( const sirInputControllerProfile_t& profile, uint16_t nNativeCode ) noexcept;
	static int16_t axis( const sirInputControllerProfile_t& profile, uint16_t nNativeCode, bool *pInvert ) noexcept;
	uint32_t count( void ) const noexcept;
	const sirInputControllerProfile_t *profileAt( uint32_t iProfile ) const noexcept;
	bool saveCooked( uint8_t *pBytes, uint32_t nCapacity, uint32_t *pWritten ) const noexcept;
	bool loadCooked( const uint8_t *pBytes, uint32_t nBytes ) noexcept;

private:
	sirInputControllerProfile_t m_Profiles[SIR_INPUT_CONTROLLER_PROFILE_CAPACITY];
	uint32_t m_nCount;
};

#endif // __SIRENGINE_SIRINPUTCONTROLLERPROFILES_H__
