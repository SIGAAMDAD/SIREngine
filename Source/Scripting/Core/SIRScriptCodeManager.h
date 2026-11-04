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

#ifndef __SIRENGINE_SIRSCRIPTCODEMANAGER_H__
#define __SIRENGINE_SIRSCRIPTCODEMANAGER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptBindingRegistry.h>
#include <thread>

// An externally produced artifact. The build fingerprint is an opaque cache
// identity, NOT an integrity proof; the resource provider validates its bytes.
typedef struct sirScriptCodeArtifact_s
{
	uint64_t nBackendId;
	uint64_t nProgramId;
	uint64_t nRevision;
	uint64_t nFormatId;
	uint64_t nSchemaFingerprint;
	uint64_t nTargetId;
	uint64_t nBuildFingerprint;
} sirScriptCodeArtifact_t;

typedef struct sirScriptCodeHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirScriptCodeHandle_t;

typedef struct sirScriptCodeStats_s
{
	uint64_t nPinnedArtifactBytes;
	uint32_t nArtifacts;
	uint32_t nRetiredArtifacts;
	uint32_t nActiveRuntimeLeases;
} sirScriptCodeStats_t;

typedef struct sirScriptCodeSlot_s
{
	sirScriptCodeArtifact_t artifact;
	sirScriptProgramView_t view;
	uint64_t nProviderLease;
	uint32_t nGeneration;
	uint32_t nPins;
	bool bOccupied;
	bool bActive;
	bool bRetired;
} sirScriptCodeSlot_t;

typedef struct sirScriptCodeLeaseSlot_s
{
	uint32_t nGeneration;
	uint32_t iArtifact;
	bool bOccupied;
} sirScriptCodeLeaseSlot_t;

// Owner-thread staging/activation. The artifact source keeps immutable bytes
// resident; a separate host-side lease is issued for every runtime load.
// Retired versions remain resident while any runtime code lease is live.
class SIRScriptCodeManager
{
public:
	explicit SIRScriptCodeManager( const SIRScriptBindingRegistry *pRegistry ) noexcept;
	~SIRScriptCodeManager( void ) noexcept;
	SIRScriptCodeManager( const SIRScriptCodeManager& ) = delete;
	SIRScriptCodeManager& operator=( const SIRScriptCodeManager& ) = delete;

	sirScriptResult_t configure( uint64_t nTargetId, const sirScriptProgramSourceAPI_t& provider ) noexcept;
	sirScriptResult_t stage( const sirScriptCodeArtifact_t& artifact, sirScriptCodeHandle_t *pHandle ) noexcept;
	sirScriptResult_t activate( sirScriptCodeHandle_t handle ) noexcept;
	sirScriptResult_t discard( sirScriptCodeHandle_t handle ) noexcept;
	sirScriptResult_t retire( sirScriptCodeHandle_t handle ) noexcept;
	sirScriptResult_t activeRevision( uint64_t nBackendId, uint64_t nProgramId, uint64_t *pRevision ) const noexcept;
	sirScriptResult_t shutdown( void ) noexcept;
	sirScriptCodeStats_t stats( void ) const noexcept;
	sirScriptProgramSourceAPI_t makeProgramSourceAPI( void ) noexcept;

private:
	static constexpr uint32_t MAX_ARTIFACTS = 32;
	static constexpr uint32_t MAX_LEASES = 128;

	static uint32_t nextGeneration( uint32_t nGeneration ) noexcept;
	static bool validView( const sirScriptProgramView_t& view, const sirScriptCodeArtifact_t& artifact ) noexcept;
	static sirScriptResult_t acquireThunk( void *pUser, uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
	                                       sirScriptProgramView_t *pView, uint64_t *pToken ) noexcept;
	static void releaseThunk( void *pUser, uint64_t nToken ) noexcept;
	bool isOwnerThread( void ) const noexcept;
	int32_t resolve( sirScriptCodeHandle_t handle ) const noexcept;
	sirScriptResult_t acquire( uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision, sirScriptProgramView_t *pView,
	                           uint64_t *pToken ) noexcept;
	void release( uint64_t nToken ) noexcept;
	void destroySlot( uint32_t iSlot ) noexcept;

	const SIRScriptBindingRegistry *m_pRegistry;
	const std::thread::id m_OwnerThread;
	sirScriptProgramSourceAPI_t m_Provider;
	uint64_t m_nTargetId;
	sirScriptCodeSlot_t m_szArtifacts[MAX_ARTIFACTS];
	sirScriptCodeLeaseSlot_t m_szLeases[MAX_LEASES];
	bool m_bInProvider;
};

#endif // __SIRENGINE_SIRSCRIPTCODEMANAGER_H__
