/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_INPUTDEVICEREGISTRY_H__
#define __SIR_INPUTDEVICEREGISTRY_H__

#include <Core/SIRPCH.h>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/Input/SIRInputTypes.h>

typedef struct sirInputDeviceDescriptor_s
{
	sirInputHardwareKey_t key;
	sirInputDeviceId_t id;
	uint32_t nUserId;
	bool bConnected;
} sirInputDeviceDescriptor_t;

// UE ApplicationCore/InputDeviceMapper influence: one authoritative device
// owner and a separate user pairing table. The sole input thread mutates it.
class SIRInputDeviceRegistry
{
public:
	SIRInputDeviceRegistry( void ) noexcept;
	bool connect( const sirInputHardwareKey_t& key, sirInputDeviceId_t *pResult ) noexcept;
	bool disconnect( sirInputDeviceId_t id ) noexcept;
	bool resolve( SIRInputBackendId eBackend, uint64_t nNativeId, sirInputDeviceId_t *pResult ) const noexcept;
	bool describe( sirInputDeviceId_t id, sirInputDeviceDescriptor_t *pOut ) const noexcept;
	bool authorize( SIRInputBackendId eSource, sirInputDeviceId_t id, SIRInputDeviceClass *pClass ) const noexcept;
	bool pairUser( sirInputDeviceId_t id, uint32_t nUser ) noexcept;
	bool valid( sirInputDeviceId_t id ) const noexcept;
	bool inspectSlot( uint32_t nSlot, sirInputDeviceDescriptor_t *pOut ) const noexcept;
	void snapshot( sirInputFrame_t& frame ) const noexcept;
	uint32_t count( void ) const noexcept;
	void reset( void ) noexcept;

private:
	sirInputDeviceDescriptor_t m_Devices[SIR_INPUT_DEVICE_CAPACITY];
	uint32_t m_nCount;
};

#endif // __SIR_INPUTDEVICEREGISTRY_H__
