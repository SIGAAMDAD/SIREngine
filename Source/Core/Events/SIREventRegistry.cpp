/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/
#include <Core/Events/SIREventRegistry.h>
#include <string.h>

SIREventRegistry::SIREventRegistry( void ) noexcept : m_Slots{}, m_nCount( 0 )
{
	for ( uint32_t i = 0; i < MAX_EVENTS; ++i ) {
		m_Slots[i].descriptor.nGeneration = 1;
	}
}

bool SIREventRegistry::registerEvent( uint32_t nTypeId, const char *pszName, uint32_t nBytes, uint32_t nAlignment,
                                      sirEventHandle_t &handle ) noexcept
{
	handle = {};
	if ( nTypeId == 0 || pszName == nullptr || nBytes == 0 || nAlignment == 0 || ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
		return false;
	}
	const size_t nLength = strlen( pszName );
	if ( nLength == 0 || nLength >= sizeof( m_Slots[0].descriptor.szName ) ) {
		return false;
	}
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	uint32_t nFree = MAX_EVENTS;
	for ( uint32_t i = 0; i < MAX_EVENTS; ++i ) {
		if ( m_Slots[i].bUsed ) {
			if ( m_Slots[i].descriptor.nTypeId == nTypeId || strcmp( m_Slots[i].descriptor.szName, pszName ) == 0 ) {
				return false;
			}
		} else if ( nFree == MAX_EVENTS ) {
			nFree = i;
		}
	}
	if ( nFree == MAX_EVENTS ) {
		return false;
	}
	sirEventRegistrationSlot_t &slot = m_Slots[nFree];
	slot.descriptor.nTypeId = nTypeId;
	slot.descriptor.nPayloadBytes = nBytes;
	slot.descriptor.nPayloadAlignment = nAlignment;
	memcpy( slot.descriptor.szName, pszName, nLength + 1 );
	slot.bUsed = true;
	handle = { nFree + 1, slot.descriptor.nGeneration };
	++m_nCount;
	return true;
}

bool SIREventRegistry::unregisterEvent( sirEventHandle_t handle ) noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_EVENTS ) {
		return false;
	}
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	sirEventRegistrationSlot_t &slot = m_Slots[handle.nSlot - 1];
	if ( !slot.bUsed || slot.descriptor.nGeneration != handle.nGeneration ) {
		return false;
	}
	slot.bUsed = false;
	if ( ++slot.descriptor.nGeneration == 0 ) {
		++slot.descriptor.nGeneration;
	}
	--m_nCount;
	return true;
}

bool SIREventRegistry::findByType( uint32_t nTypeId, sirEventHandle_t &handle ) const noexcept
{
	handle = {};
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	for ( uint32_t i = 0; i < MAX_EVENTS; ++i ) {
		if ( m_Slots[i].bUsed && m_Slots[i].descriptor.nTypeId == nTypeId ) {
			handle = { i + 1, m_Slots[i].descriptor.nGeneration };
			return true;
		}
	}
	return false;
}

bool SIREventRegistry::getDescriptor( sirEventHandle_t handle, sirRegisteredEvent_t &descriptor ) const noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > MAX_EVENTS ) {
		return false;
	}
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	const sirEventRegistrationSlot_t &slot = m_Slots[handle.nSlot - 1];
	if ( !slot.bUsed || slot.descriptor.nGeneration != handle.nGeneration ) {
		return false;
	}
	descriptor = slot.descriptor;
	return true;
}

void SIREventRegistry::clear( void ) noexcept
{
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	for ( uint32_t i = 0; i < MAX_EVENTS; ++i ) {
		m_Slots[i].bUsed = false;
		if ( ++m_Slots[i].descriptor.nGeneration == 0 ) {
			++m_Slots[i].descriptor.nGeneration;
		}
	}
	m_nCount = 0;
}

uint32_t SIREventRegistry::count( void ) const noexcept
{
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	return m_nCount;
}
