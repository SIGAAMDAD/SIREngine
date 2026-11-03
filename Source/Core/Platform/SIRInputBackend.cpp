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

#include <Core/Platform/SIRInputBackend.h>

uint64_t SIRInputBackend::getPhysicalKey( sirInputDeviceHandle_t hDevice ) const noexcept
{
	( void )hDevice;
	return 0;
}

bool SIRInputBackend::setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept
{
	( void )hDevice;
	( void )nStrong;
	( void )nWeak;
	( void )nDurationMs;
	return false;
}
