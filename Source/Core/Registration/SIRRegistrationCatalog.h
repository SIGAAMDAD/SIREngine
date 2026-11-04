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

#ifndef __SIRENGINE_SIRREGISTRATIONCATALOG_H__
#define __SIRENGINE_SIRREGISTRATIONCATALOG_H__

#include <SIRPCH.hpp>
#include <Core/Registration/SIRRegistrationStringBridge.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirRegistrationKind_e : uint8_t
{
	SIR_REGISTRATION_COMMAND = 1,
	SIR_REGISTRATION_CVAR = 2
} sirRegistrationKind_t;

// Callbacks are module-owned and must be kept loaded through their execution leases.
typedef void ( *sirRegistrationCallback_t )( void *pContext, const void *pInvocation, const void *pArguments ) noexcept;

typedef struct sirRegistrationBinding_s
{
	uint64_t nBindingId;
	uint32_t nOwnerId;
	sirRegistrationCallback_t pfnCallback;
	void *pContext;
} sirRegistrationBinding_t;

typedef struct sirRegistrationHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirRegistrationHandle_t;

typedef struct sirRegistrationEntry_s
{
	SIRStringAtom atom;
	uint64_t nBindingId;
	uint32_t nSchemaId;
	uint32_t nOwnerId;
	uint32_t nFlags;
	uint32_t nCapabilities;
	uint32_t nType;
	uint32_t nDefaultOffset;
	uint32_t nGeneration;
	sirRegistrationCallback_t pfnCallback;
	void *pContext;
	sirRegistrationKind_t eKind;
	bool bActive;
	bool bCooked;
} sirRegistrationEntry_t;

// All scratch/storage is provided by the caller's SIRMemory-owned span.
// nEntryCapacity must cover cooked entries plus desired dynamic overlay slots.
typedef struct sirRegistrationCatalogInit_s
{
	const uint8_t *pBytes;
	size_t nBytes;
	uint64_t nStringCatalogHash;
	sirRegistrationEntry_t *pEntries;
	uint32_t nEntryCapacity;
	SIRStringAtom *pNames;
	uint32_t *pNameIds;
	uint32_t nNameCapacity;
	uint64_t *pBindingIds;
	uint32_t nBindingCapacity;
	const sirRegistrationBinding_t *pBindings;
	uint32_t nBindingCount;
} sirRegistrationCatalogInit_t;

typedef struct sirRegistrationDynamicDesc_s
{
	sirRegistrationNameRequest_t name;
	sirRegistrationKind_t eKind;
	uint64_t nBindingId;
	uint32_t nSchemaId;
	uint32_t nFlags;
	uint32_t nCapabilities;
	sirCVarType_t eType;
	sirRegistrationCallback_t pfnCallback;
	void *pContext;
} sirRegistrationDynamicDesc_t;

static_assert( sizeof( sirRegistrationHandle_t ) == 8, "registration handle must remain eight bytes" );

class SIRRegistrationCatalog
{
public:
	SIRRegistrationCatalog( void ) noexcept;
	SIRRegistrationCatalog( const SIRRegistrationCatalog& ) = delete;
	~SIRRegistrationCatalog( void ) noexcept;

	SIRRegistrationCatalog& operator=( const SIRRegistrationCatalog& ) = delete;

	bool isInitialized( void ) const noexcept;
	uint32_t getCount( void ) const noexcept;
	uint32_t getCookedCount( void ) const noexcept;
	sirRegistrationResult_t getEntry( sirRegistrationHandle_t handle, const sirRegistrationEntry_t **ppEntry ) const noexcept;

	sirRegistrationResult_t initialize( SIRStringPool& pool, const sirRegistrationCatalogInit_t& init ) noexcept;
	sirRegistrationResult_t find( const char *pszName, size_t nNameBytes, sirRegistrationHandle_t *pHandle ) const noexcept;
	sirRegistrationResult_t registerDynamic( const sirRegistrationDynamicDesc_t& desc, sirRegistrationHandle_t *pHandle ) noexcept;
	sirRegistrationResult_t unregisterDynamic( sirRegistrationHandle_t handle ) noexcept;
	uint32_t unregisterModule( uint32_t nOwnerId ) noexcept;
	void shutdown( void ) noexcept;

protected:
private:
	sirRegistrationResult_t loadNames( const sirRegistrationCatalogInit_t& init, SIRStringPool& pool, uint32_t nNames ) noexcept;
	sirRegistrationResult_t loadDescriptors( const sirRegistrationCatalogInit_t& init, uint32_t nNames ) noexcept;
	bool isNameOccupied( SIRStringAtom atom ) const noexcept;

	SIRStringPool *m_pPool;
	sirRegistrationEntry_t *m_pEntries;
	SIRStringAtom *m_pNames;
	uint32_t m_nCapacity;
	uint32_t m_nCookedCount;
	uint32_t m_nLiveCount;
	uint32_t m_nGenerationSeed;
	bool m_bInitialized;
};

#endif // __SIRENGINE_SIRREGISTRATIONCATALOG_H__
