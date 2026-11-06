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

#include <Core/Platform/Input/SIRInputDeviceRegistry.h>

SIRInputDeviceRegistry::SIRInputDeviceRegistry( void ) noexcept : m_Devices{}, m_nCount( 0 )
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		m_Devices[i].id = { i, 1 };
		m_Devices[i].nUserId = SIR_INPUT_NO_USER;
	}
}

bool SIRInputDeviceRegistry::valid( sirInputDeviceId_t id ) const noexcept
{
	return id.nSlot < SIR_INPUT_DEVICE_CAPACITY && m_Devices[id.nSlot].bConnected && m_Devices[id.nSlot].id.nGeneration == id.nGeneration;
}

bool SIRInputDeviceRegistry::resolve( sirInputBackendId_t eBackend, uint64_t nNativeId, sirInputDeviceId_t *pResult ) const noexcept
{
	if ( pResult == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		const sirInputDeviceDescriptor_t& record = m_Devices[i];
		if ( record.bConnected && record.key.eBackend == eBackend && record.key.nNativeId == nNativeId ) {
			*pResult = record.id;
			return true;
		}
	}
	return false;
}

bool SIRInputDeviceRegistry::connect( const sirInputHardwareKey_t& key, sirInputDeviceId_t *pResult ) noexcept
{
	if ( pResult == nullptr || key.nNativeId == 0 || key.eBackend == 0 || key.eClass > SIR_INPUT_CONTROLLER ) {
		return false;
	}
	sirInputDeviceId_t unused = {};
	if ( resolve( key.eBackend, key.nNativeId, &unused ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		const sirInputDeviceDescriptor_t& record = m_Devices[i];
		if ( key.nPhysicalId != 0 && record.bConnected && record.key.nPhysicalId == key.nPhysicalId && record.key.eClass == key.eClass ) {
			return false; // GameInput/XInput/HID duplicates must be arbitrated before publication.
		}
	}
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirInputDeviceDescriptor_t& record = m_Devices[i];
		if ( !record.bConnected ) {
			record.key = key;
			record.nUserId = SIR_INPUT_NO_USER;
			record.bConnected = true;
			*pResult = record.id;
			++m_nCount;
			return true;
		}
	}
	return false;
}

bool SIRInputDeviceRegistry::describe( sirInputDeviceId_t id, sirInputDeviceDescriptor_t *pOut ) const noexcept
{
	if ( !valid( id ) || pOut == nullptr ) {
		return false;
	}
	*pOut = m_Devices[id.nSlot];
	return true;
}

bool SIRInputDeviceRegistry::authorize( sirInputBackendId_t eSource, sirInputDeviceId_t id, sirInputDeviceClass_t *pClass ) const noexcept
{
	if ( pClass == nullptr || id.nSlot >= SIR_INPUT_DEVICE_CAPACITY ) {
		return false;
	}
	const sirInputDeviceDescriptor_t& record = m_Devices[id.nSlot];
	if ( !record.bConnected || record.id.nGeneration != id.nGeneration || record.key.eBackend != eSource ) {
		return false;
	}
	*pClass = record.key.eClass;
	return true;
}

bool SIRInputDeviceRegistry::inspectSlot( uint32_t nSlot, sirInputDeviceDescriptor_t *pOut ) const noexcept
{
	if ( nSlot >= SIR_INPUT_DEVICE_CAPACITY || pOut == nullptr ) {
		return false;
	}
	*pOut = m_Devices[nSlot];
	return true;
}

void SIRInputDeviceRegistry::snapshot( sirInputFrame_t& frame ) const noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		const sirInputDeviceDescriptor_t& record = m_Devices[i];
		frame.devices[i] = record.id;
		frame.nConnected[i] = record.bConnected ? 1 : 0;
		frame.nUsers[i] = record.bConnected ? record.nUserId : SIR_INPUT_NO_USER;
	}
}

bool SIRInputDeviceRegistry::pairUser( sirInputDeviceId_t id, uint32_t nUser ) noexcept
{
	if ( !valid( id ) || ( nUser != SIR_INPUT_NO_USER && nUser >= SIR_INPUT_USER_CAPACITY ) ) {
		return false;
	}
	m_Devices[id.nSlot].nUserId = nUser;
	return true;
}

bool SIRInputDeviceRegistry::disconnect( sirInputDeviceId_t id ) noexcept
{
	if ( !valid( id ) ) {
		return false;
	}
	sirInputDeviceDescriptor_t& record = m_Devices[id.nSlot];
	record.bConnected = false;
	record.nUserId = SIR_INPUT_NO_USER;
	++record.id.nGeneration;
	if ( record.id.nGeneration == 0 ) {
		record.id.nGeneration = 1;
	}
	--m_nCount;
	return true;
}

uint32_t SIRInputDeviceRegistry::count( void ) const noexcept
{
	return m_nCount;
}

void SIRInputDeviceRegistry::reset( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_DEVICE_CAPACITY; ++i ) {
		sirInputDeviceDescriptor_t& record = m_Devices[i];
		record.bConnected = false;
		record.nUserId = SIR_INPUT_NO_USER;
		++record.id.nGeneration;
		if ( record.id.nGeneration == 0 ) {
			record.id.nGeneration = 1;
		}
	}
	m_nCount = 0;
}
