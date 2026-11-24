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

#include <Core/Objects/SIRObjectRegistry.h>

#include <cmath>
#include <string.h>

SIRObjectResult SIRObjectRegistry::initialize( sirObjectTypeDescriptor_t *pTypes, uint32_t nTypeCapacity,
                                                 sirObjectPropertyDescriptor_t *pProperties, uint32_t nPropertyCapacity ) noexcept
{
	if ( m_pTypes != nullptr || pTypes == nullptr || pProperties == nullptr || nTypeCapacity == 0 || nPropertyCapacity == 0 ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	if ( nTypeCapacity > UINT32_MAX / sizeof( *pTypes ) || nPropertyCapacity > UINT32_MAX / sizeof( *pProperties ) ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	m_pTypes = pTypes;
	m_pProperties = pProperties;
	m_nTypeCapacity = nTypeCapacity;
	m_nPropertyCapacity = nPropertyCapacity;
	m_nTypeCount = 0;
	m_nPropertyCount = 0;
	m_bSealed = false;
	return SIRObjectResult::SIR_OBJECT_OK;
}

void SIRObjectRegistry::shutdown( void ) noexcept
{
	m_pTypes = nullptr;
	m_pProperties = nullptr;
	m_nTypeCapacity = 0;
	m_nPropertyCapacity = 0;
	m_nTypeCount = 0;
	m_nPropertyCount = 0;
	m_bSealed = false;
}

const sirObjectTypeDescriptor_t *SIRObjectRegistry::findType( uint32_t nTypeId ) const noexcept
{
	if ( m_pTypes == nullptr || nTypeId == 0 ) {
		return nullptr;
	}
	for ( uint32_t iType = 0; iType < m_nTypeCount; ++iType ) {
		if ( m_pTypes[iType].nTypeId == nTypeId ) {
			return &m_pTypes[iType];
		}
	}
	return nullptr;
}

SIRObjectResult SIRObjectRegistry::registerType( const sirObjectTypeDescriptor_t& type ) noexcept
{
	if ( m_pTypes == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_INITIALIZED;
	}
	if ( m_bSealed ) {
		return SIRObjectResult::SIR_OBJECT_SEALED;
	}
	if ( type.nTypeId == 0 || type.nNameAtom == 0 || type.nTypeId == type.nParentTypeId ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	if ( findType( type.nTypeId ) != nullptr ) {
		return SIRObjectResult::SIR_OBJECT_DUPLICATE_ID;
	}
	if ( type.nParentTypeId != 0 && findType( type.nParentTypeId ) == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_FOUND;
	}
	if ( m_nTypeCount == m_nTypeCapacity ) {
		return SIRObjectResult::SIR_OBJECT_OUT_OF_CAPACITY;
	}
	m_pTypes[m_nTypeCount++] = type;
	return SIRObjectResult::SIR_OBJECT_OK;
}

SIRObjectResult SIRObjectRegistry::registerProperty( const sirObjectPropertyDescriptor_t& property ) noexcept
{
	if ( m_pProperties == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_INITIALIZED;
	}
	if ( m_bSealed ) {
		return SIRObjectResult::SIR_OBJECT_SEALED;
	}
	if ( property.nPropertyId == 0 || property.nNameAtom == 0 || property.eKind == SIRPropertyKind::SIR_PROPERTY_NONE ||
	     property.eKind > SIRPropertyKind::SIR_PROPERTY_RESOURCE_ID ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	if ( findType( property.nOwnerTypeId ) == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_FOUND;
	}
	for ( uint32_t iProperty = 0; iProperty < m_nPropertyCount; ++iProperty ) {
		if ( m_pProperties[iProperty].nPropertyId == property.nPropertyId ) {
			return SIRObjectResult::SIR_OBJECT_DUPLICATE_ID;
		}
	}
	if ( m_nPropertyCount == m_nPropertyCapacity ) {
		return SIRObjectResult::SIR_OBJECT_OUT_OF_CAPACITY;
	}
	m_pProperties[m_nPropertyCount++] = property;
	return SIRObjectResult::SIR_OBJECT_OK;
}

const sirObjectPropertyDescriptor_t *SIRObjectRegistry::findProperty( uint32_t nTypeId, uint32_t nPropertyId ) const noexcept
{
	const sirObjectTypeDescriptor_t *pType = findType( nTypeId );
	while ( pType != nullptr ) {
		for ( uint32_t iProperty = 0; iProperty < m_nPropertyCount; ++iProperty ) {
			const sirObjectPropertyDescriptor_t& property = m_pProperties[iProperty];
			if ( property.nOwnerTypeId == pType->nTypeId && property.nPropertyId == nPropertyId ) {
				return &property;
			}
		}
		pType = findType( pType->nParentTypeId );
	}
	return nullptr;
}

const sirObjectPropertyDescriptor_t *SIRObjectRegistry::getPropertyDescriptor( uint32_t iProperty ) const noexcept
{
	if ( m_pProperties == nullptr || iProperty >= m_nPropertyCount ) {
		return nullptr;
	}
	return &m_pProperties[iProperty];
}

SIRObjectResult SIRObjectRegistry::validateProperty( uint32_t nTypeId, uint32_t nPropertyId,
                                                       const sirPropertyValue_t& value ) const noexcept
{
	const sirObjectPropertyDescriptor_t *pProperty = findProperty( nTypeId, nPropertyId );
	if ( pProperty == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_FOUND;
	}
	if ( value.eKind != pProperty->eKind ) {
		return SIRObjectResult::SIR_OBJECT_TYPE_MISMATCH;
	}
	if ( value.eKind == SIRPropertyKind::SIR_PROPERTY_BOOL && value.nPayload > 1 ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	if ( ( value.eKind == SIRPropertyKind::SIR_PROPERTY_STRING_ATOM || value.eKind == SIRPropertyKind::SIR_PROPERTY_RESOURCE_ID ) && value.nPayload == 0 ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	for ( uint32_t iByte = 0; iByte < sizeof( value.szReserved ); ++iByte ) {
		if ( value.szReserved[iByte] != 0 ) {
			return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
		}
	}
	if ( value.eKind == SIRPropertyKind::SIR_PROPERTY_REAL64 ) {
		double fReal = 0;
		memcpy( &fReal, &value.nPayload, sizeof( fReal ) );
		if ( !std::isfinite( fReal ) ) {
			return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
		}
	}
	return SIRObjectResult::SIR_OBJECT_OK;
}

SIRObjectResult SIRObjectRegistry::seal( void ) noexcept
{
	if ( m_pTypes == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_INITIALIZED;
	}
	m_bSealed = true;
	return SIRObjectResult::SIR_OBJECT_OK;
}

uint32_t SIRObjectRegistry::getTypeCount( void ) const noexcept
{
	return m_nTypeCount;
}

uint32_t SIRObjectRegistry::getPropertyCount( void ) const noexcept
{
	return m_nPropertyCount;
}

SIRObjectResult SIRObjectStore::initialize( sirObjectSlot_t *pSlots, uint32_t nCapacity, const SIRObjectRegistry *pTypes ) noexcept
{
	if ( m_pSlots != nullptr || pSlots == nullptr || nCapacity == 0 || pTypes == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	if ( nCapacity > UINT32_MAX / sizeof( *pSlots ) ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	m_pSlots = pSlots;
	m_nCapacity = nCapacity;
	m_nCount = 0;
	m_pTypes = pTypes;
	memset( pSlots, 0, nCapacity * sizeof( *pSlots ) );
	return SIRObjectResult::SIR_OBJECT_OK;
}

void SIRObjectStore::shutdown( void ) noexcept
{
	m_pSlots = nullptr;
	m_pTypes = nullptr;
	m_nCapacity = 0;
	m_nCount = 0;
}

SIRObjectResult SIRObjectStore::create( uint64_t nStableId, uint32_t nTypeId, sirObjectHandle_t *pOutHandle ) noexcept
{
	if ( m_pSlots == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_INITIALIZED;
	}
	if ( nStableId == 0 || pOutHandle == nullptr || m_pTypes->findType( nTypeId ) == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	uint32_t iCandidate = UINT32_MAX;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		if ( m_pSlots[iSlot].nStableId == nStableId ) {
			return SIRObjectResult::SIR_OBJECT_DUPLICATE_ID;
		}
		if ( m_pSlots[iSlot].nStableId == 0 && m_pSlots[iSlot].nGeneration != UINT32_MAX && iCandidate == UINT32_MAX ) {
			iCandidate = iSlot;
		}
	}
	if ( iCandidate == UINT32_MAX ) {
		return SIRObjectResult::SIR_OBJECT_OUT_OF_CAPACITY;
	}
	sirObjectSlot_t& slot = m_pSlots[iCandidate];
	++slot.nGeneration;
	slot.nStableId = nStableId;
	slot.nTypeId = nTypeId;
	pOutHandle->nStableId = nStableId;
	pOutHandle->nGeneration = slot.nGeneration;
	pOutHandle->iSlot = iCandidate;
	++m_nCount;
	return SIRObjectResult::SIR_OBJECT_OK;
}

SIRObjectResult SIRObjectStore::resolve( const sirObjectHandle_t& handle, sirObjectSlot_t *pOutSlot ) const noexcept
{
	if ( m_pSlots == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_NOT_INITIALIZED;
	}
	if ( handle.nStableId == 0 || handle.iSlot >= m_nCapacity || pOutSlot == nullptr ) {
		return SIRObjectResult::SIR_OBJECT_INVALID_ARGUMENT;
	}
	const sirObjectSlot_t& slot = m_pSlots[handle.iSlot];
	if ( slot.nStableId != handle.nStableId || slot.nGeneration != handle.nGeneration ) {
		return SIRObjectResult::SIR_OBJECT_STALE_HANDLE;
	}
	*pOutSlot = slot;
	return SIRObjectResult::SIR_OBJECT_OK;
}

SIRObjectResult SIRObjectStore::destroy( const sirObjectHandle_t& handle ) noexcept
{
	sirObjectSlot_t slot {};
	const SIRObjectResult eResult = resolve( handle, &slot );
	if ( eResult != SIRObjectResult::SIR_OBJECT_OK ) {
		return eResult;
	}
	sirObjectSlot_t& current = m_pSlots[handle.iSlot];
	current.nStableId = 0;
	current.nTypeId = 0;
	--m_nCount;
	return SIRObjectResult::SIR_OBJECT_OK;
}

uint32_t SIRObjectStore::getCount( void ) const noexcept
{
	return m_nCount;
}
