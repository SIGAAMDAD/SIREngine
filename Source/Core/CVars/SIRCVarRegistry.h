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

#ifndef __SIRENGINE_SIRCVARREGISTRY_H__
#define __SIRENGINE_SIRCVARREGISTRY_H__

#include <Core/Registration/SIRRegistrationCatalog.h>
#include <SIRPCH.hpp>
#include <thread>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Caller owns every backing array and its lifetime. No runtime allocations.
typedef struct sirCVarValue_s
{
	uint64_t nBits;
	const char *pszText;
	uint32_t nTextBytes;
} sirCVarValue_t;

typedef struct sirCVarOrigin_s
{
	sirCVarSource_t eSource;
	uint32_t nIssuerId;
	uint32_t nCapabilities;
	bool bInitializing;
	bool bEngineInternal;
	bool bDevelopmentAllowed;
	bool bCheatsAllowed;
} sirCVarOrigin_t;

typedef struct sirCVarWrite_s
{
	sirCVarHandle_t handle;
	sirCVarOrigin_t origin;
	sirCVarValue_t value;
	bool bRemove;
} sirCVarWrite_t;

typedef struct sirCVarOverride_s
{
	sirCVarValue_t value;
	sirCVarValue_t pending;
	uint64_t nSerial;
	uint64_t nPendingSerial;
	uint32_t nSlot;
	uint32_t nIssuerId;
	sirCVarSource_t eSource;
	bool bActive;
	bool bApplied;
	bool bPending;
} sirCVarOverride_t;

typedef struct sirCVarSlot_s
{
	sirRegistrationHandle_t registration;
	sirCVarValue_t baseValue;
	sirCVarValue_t effective;
	uint32_t nGeneration;
	uint32_t nFlags;
	uint32_t nSinkGroup;
	uint32_t nBankIndex;
	sirCVarType_t eType;
	sirCVarApplyPhase_t ePhase;
	bool bActive;
} sirCVarSlot_t;

typedef struct sirCVarEvent_s
{
	sirCVarHandle_t handle;
	sirCVarValue_t oldValue;
	sirCVarValue_t newValue;
	uint64_t nRevision;
	uint32_t nSinkGroup;
} sirCVarEvent_t;

typedef struct sirCVarStorage_s
{
	sirCVarSlot_t *pSlots;
	uint32_t nSlots;
	uint32_t *pBank32;
	uint32_t nBank32;
	uint64_t *pBank64;
	uint32_t nBank64;
	sirCVarOverride_t *pOverrides;
	sirCVarOverride_t *pShadow;
	uint32_t nOverrides;
	sirCVarEvent_t *pEvents;
	sirCVarEvent_t *pEventScratch;
	uint32_t nEvents;
	sirCVarWrite_t *pStagedWrites;
	uint32_t nStagedWrites;
	char *pszActiveText;
	char *pszPendingText;
	char *pszStagedText;
	char *pszEventText;
	uint32_t nStringStride;
	size_t nActiveTextBytes;
	size_t nPendingTextBytes;
	size_t nStagedTextBytes;
	size_t nEventTextBytes;
} sirCVarStorage_t;

typedef struct sirCVarDefinition_s
{
	sirRegistrationHandle_t registration;
	sirCVarValue_t defaultValue;
	uint32_t nSinkGroup;
	sirCVarApplyPhase_t ePhase;
} sirCVarDefinition_t;

typedef struct sirCVarCookedSource_s
{
	const uint8_t *pBytes;
	size_t nBytes;
	uint32_t nSinkGroup;
	sirCVarApplyPhase_t ePhase;
} sirCVarCookedSource_t;

// Validated SIRC v1 default payload is borrowed until the registry unbinds.
sirRegistrationResult_t sirCVarDefinitionFromCooked( const SIRRegistrationCatalog& registration, sirRegistrationHandle_t handle,
                                                     const sirCVarCookedSource_t& source, sirCVarDefinition_t *pOutput ) noexcept;

typedef void ( *sirCVarGroupSink_t )( void *pContext, uint32_t nGroup, uint64_t nRevision ) noexcept;

class SIRCVarRegistry
{
public:
	SIRCVarRegistry( void ) noexcept;
	SIRCVarRegistry( const SIRCVarRegistry& ) = delete;
	~SIRCVarRegistry( void ) noexcept;

	SIRCVarRegistry& operator=( const SIRCVarRegistry& ) = delete;

	bool isInitialized( void ) const noexcept;
	uint32_t getCount( void ) const noexcept;
	uint32_t getPendingEvents( void ) const noexcept;
	uint64_t getRevision( void ) const noexcept;
	sirRegistrationResult_t getValue( sirCVarHandle_t handle, uint32_t nCapabilities, sirCVarValue_t *pValue ) const noexcept;

	sirRegistrationResult_t initialize( SIRRegistrationCatalog& registration, const sirCVarStorage_t& storage ) noexcept;
	sirRegistrationResult_t bind( const sirCVarDefinition_t& definition, sirCVarHandle_t *pHandle ) noexcept;
	sirRegistrationResult_t unbind( sirCVarHandle_t handle ) noexcept;
	sirRegistrationResult_t prepareBatch( const sirCVarWrite_t *pWrites, uint32_t nCount, uint64_t *pToken ) noexcept;
	sirRegistrationResult_t commitBatch( uint64_t nToken ) noexcept;
	sirRegistrationResult_t cancelBatch( uint64_t nToken ) noexcept;
	sirRegistrationResult_t applyLatched( sirCVarApplyPhase_t ePhase ) noexcept;
	sirRegistrationResult_t peekEvent( const sirCVarEvent_t **ppEvent ) const noexcept;
	sirRegistrationResult_t popEvent( void ) noexcept;
	uint32_t pumpGroups( sirCVarGroupSink_t pfnSink, void *pContext ) noexcept;
	void shutdown( void ) noexcept;

protected:
private:
	sirRegistrationResult_t validateHandle( sirCVarHandle_t handle, const sirCVarSlot_t **ppSlot ) const noexcept;
	sirRegistrationResult_t validateWrite( const sirCVarWrite_t& write, const sirCVarSlot_t **ppSlot ) const noexcept;
	sirRegistrationResult_t applyStagedWrite( const sirCVarWrite_t& write, uint32_t iWrite ) noexcept;
	sirRegistrationResult_t reserveEvents( void ) noexcept;
	int32_t selectWinner( uint32_t iSlot, const sirCVarOverride_t *pOverrides ) const noexcept;
	sirCVarValue_t selectValue( uint32_t iSlot, const sirCVarOverride_t *pOverrides ) const noexcept;
	void copyEvents( void ) noexcept;
	void publishOverrides( void ) noexcept;
	void publishBanks( void ) noexcept;
	bool sameValue( const sirCVarValue_t& a, const sirCVarValue_t& b, sirCVarType_t eType ) const noexcept;
	bool ownerThread( void ) const noexcept;

	SIRRegistrationCatalog *m_pRegistration;
	sirCVarStorage_t m_Storage;
	std::thread::id m_OwnerThread;
	uint64_t m_nRevision;
	uint64_t m_nSerial;
	uint64_t m_nTokenSeed;
	uint64_t m_nPreparedToken;
	uint32_t m_nPreparedWrites;
	uint32_t m_nPreparedEvents;
	uint32_t m_nEventHead;
	uint32_t m_nEventTail;
	uint32_t m_nEventCount;
	uint32_t m_nActiveSlots;
	uint32_t m_nHandleSeed;
	bool m_bPrepared;
	bool m_bPumping;
};

#endif // __SIRENGINE_SIRCVARREGISTRY_H__
