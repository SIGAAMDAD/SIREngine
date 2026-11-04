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

#ifndef __SIRENGINE_SIROBJECTREGISTRY_H__
#define __SIRENGINE_SIROBJECTREGISTRY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirObjectResult_e : uint8_t
{
	SIR_OBJECT_OK = 0,
	SIR_OBJECT_INVALID_ARGUMENT = 1,
	SIR_OBJECT_NOT_INITIALIZED = 2,
	SIR_OBJECT_DUPLICATE_ID = 3,
	SIR_OBJECT_NOT_FOUND = 4,
	SIR_OBJECT_OUT_OF_CAPACITY = 5,
	SIR_OBJECT_SEALED = 6,
	SIR_OBJECT_TYPE_MISMATCH = 7,
	SIR_OBJECT_STALE_HANDLE = 8
} sirObjectResult_t;

typedef enum sirPropertyKind_e : uint8_t
{
	SIR_PROPERTY_NONE = 0,
	SIR_PROPERTY_BOOL = 1,
	SIR_PROPERTY_INT64 = 2,
	SIR_PROPERTY_UINT64 = 3,
	SIR_PROPERTY_REAL64 = 4,
	SIR_PROPERTY_STRING_ATOM = 5,
	SIR_PROPERTY_RESOURCE_ID = 6
} sirPropertyKind_t;

typedef struct sirPropertyValue_s
{
	sirPropertyKind_t eKind;
	uint8_t szReserved[7];
	uint64_t nPayload;
} sirPropertyValue_t;

typedef struct sirObjectTypeDescriptor_s
{
	uint32_t nTypeId;
	uint32_t nParentTypeId;
	uint64_t nNameAtom;
} sirObjectTypeDescriptor_t;

typedef struct sirObjectPropertyDescriptor_s
{
	uint32_t nPropertyId;
	uint32_t nOwnerTypeId;
	uint64_t nNameAtom;
	sirPropertyKind_t eKind;
	uint8_t szReserved[7];
} sirObjectPropertyDescriptor_t;

// The registry owns metadata only through caller-supplied arrays. Names
// reference the engine string atom pool rather than creating a second pool.
class SIRObjectRegistry
{
private:
	sirObjectTypeDescriptor_t *m_pTypes = nullptr;
	sirObjectPropertyDescriptor_t *m_pProperties = nullptr;
	uint32_t m_nTypeCapacity = 0;
	uint32_t m_nPropertyCapacity = 0;
	uint32_t m_nTypeCount = 0;
	uint32_t m_nPropertyCount = 0;
	bool m_bSealed = false;

public:
	sirObjectResult_t initialize( sirObjectTypeDescriptor_t *pTypes, uint32_t nTypeCapacity, sirObjectPropertyDescriptor_t *pProperties,
	                              uint32_t nPropertyCapacity ) noexcept;
	void shutdown( void ) noexcept;
	sirObjectResult_t registerType( const sirObjectTypeDescriptor_t &type ) noexcept;
	sirObjectResult_t registerProperty( const sirObjectPropertyDescriptor_t &property ) noexcept;
	sirObjectResult_t seal( void ) noexcept;
	const sirObjectTypeDescriptor_t *findType( uint32_t nTypeId ) const noexcept;
	const sirObjectPropertyDescriptor_t *findProperty( uint32_t nTypeId, uint32_t nPropertyId ) const noexcept;
	sirObjectResult_t validateProperty( uint32_t nTypeId, uint32_t nPropertyId, const sirPropertyValue_t &value ) const noexcept;
	uint32_t getTypeCount( void ) const noexcept;
	uint32_t getPropertyCount( void ) const noexcept;
};

typedef struct sirObjectSlot_s
{
	uint64_t nStableId;
	uint32_t nTypeId;
	uint32_t nGeneration;
} sirObjectSlot_t;

typedef struct sirObjectHandle_s
{
	uint64_t nStableId;
	uint32_t iSlot;
	uint32_t nGeneration;
} sirObjectHandle_t;

// Runtime object lifetime store. Authored scene mutation remains the sole
// responsibility of SIREditorSceneDocument, not this runtime pool.
class SIRObjectStore
{
private:
	sirObjectSlot_t *m_pSlots = nullptr;
	const SIRObjectRegistry *m_pTypes = nullptr;
	uint32_t m_nCapacity = 0;
	uint32_t m_nCount = 0;

public:
	sirObjectResult_t initialize( sirObjectSlot_t *pSlots, uint32_t nCapacity, const SIRObjectRegistry *pTypes ) noexcept;
	void shutdown( void ) noexcept;
	sirObjectResult_t create( uint64_t nStableId, uint32_t nTypeId, sirObjectHandle_t *pOutHandle ) noexcept;
	sirObjectResult_t resolve( const sirObjectHandle_t &handle, sirObjectSlot_t *pOutSlot ) const noexcept;
	sirObjectResult_t destroy( const sirObjectHandle_t &handle ) noexcept;
	uint32_t getCount( void ) const noexcept;
};

#endif // __SIRENGINE_SIROBJECTREGISTRY_H__
