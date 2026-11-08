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

#include <Core/Input/SIRInputBackend.h>

uint32_t SIRInputBackend::hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept
{
	( void )hDevice;
	return SIR_INPUT_HAPTIC_CAP_NONE;
}

bool SIRInputBackend::vibrateTriggers( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	( void )hDevice;
	( void )nLeft;
	( void )nRight;
	return false;
}
