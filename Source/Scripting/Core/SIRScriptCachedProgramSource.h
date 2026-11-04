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

#ifndef __SIRENGINE_SIRSCRIPTCACHEDPROGRAMSOURCE_H__
#define __SIRENGINE_SIRSCRIPTCACHEDPROGRAMSOURCE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptArtifactCache.h>
#include <Scripting/Core/SIRScriptScheduler.h>
#include <thread>

typedef struct sirScriptCachedEntryHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirScriptCachedEntryHandle_t;

typedef struct sirScriptCachedSourceStats_s
{
	uint64_t nResidentBytes;
	uint64_t nPeakResidentBytes;
	uint32_t nRegisteredArtifacts;
	uint32_t nResidentArtifacts;
	uint32_t nActiveLeases;
} sirScriptCachedSourceStats_t;

typedef struct sirScriptCachedSourceEntry_s
{
	sirScriptCachedArtifact_t descriptor;
	sirScriptPayloadAllocation_t allocation;
	uint32_t nGeneration;
	uint32_t nPins;
	bool bOccupied;
} sirScriptCachedSourceEntry_t;

typedef struct sirScriptCachedSourceLease_s
{
	uint32_t nGeneration;
	uint32_t iEntry;
	bool bOccupied;
} sirScriptCachedSourceLease_t;

// Owner-thread bridge between verified on-disk artifacts and immutable program
// source leases. The caller supplies an engine-owned, individually reclaimable
// allocator. No implicit heap fallback, executable mapping, or JIT machinery.
// Keep this provider and its cache/allocator alive until code-manager shutdown.
class SIRScriptCachedProgramSource
{
public:
	SIRScriptCachedProgramSource( void ) noexcept;
	~SIRScriptCachedProgramSource( void ) noexcept;
	SIRScriptCachedProgramSource( const SIRScriptCachedProgramSource& ) = delete;
	SIRScriptCachedProgramSource& operator=( const SIRScriptCachedProgramSource& ) = delete;

	sirScriptResult_t configure( SIRScriptArtifactCache *pCache, const sirScriptPayloadAllocatorAPI_t& allocator,
	                             uint64_t nMaximumResidentBytes ) noexcept;
	sirScriptResult_t registerArtifact( const sirScriptCachedArtifact_t& descriptor, sirScriptCachedEntryHandle_t *pHandle ) noexcept;
	sirScriptResult_t unregisterArtifact( sirScriptCachedEntryHandle_t handle ) noexcept;
	sirScriptResult_t shutdown( void ) noexcept;
	sirScriptCachedSourceStats_t stats( void ) const noexcept;
	sirScriptProgramSourceAPI_t makeProgramSourceAPI( void ) noexcept;

private:
	static constexpr uint32_t MAX_ENTRIES = 32;
	static constexpr uint32_t MAX_LEASES = 128;

	static uint32_t nextGeneration( uint32_t nGeneration ) noexcept;
	static bool validDescriptor( const sirScriptCachedArtifact_t& descriptor ) noexcept;
	static bool sameDescriptor( const sirScriptCachedArtifact_t& a, const sirScriptCachedArtifact_t& b ) noexcept;
	static sirScriptResult_t acquireThunk( void *pUser, uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
	                                       sirScriptProgramView_t *pView, uint64_t *pToken ) noexcept;
	static void releaseThunk( void *pUser, uint64_t nToken ) noexcept;
	bool isOwnerThread( void ) const noexcept;
	sirScriptResult_t acquire( uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision, sirScriptProgramView_t *pView,
	                           uint64_t *pToken ) noexcept;
	void release( uint64_t nToken ) noexcept;
	void releaseEntry( uint32_t iEntry ) noexcept;

	SIRScriptArtifactCache *m_pCache;
	sirScriptPayloadAllocatorAPI_t m_Allocator;
	const std::thread::id m_OwnerThread;
	uint64_t m_nMaximumResidentBytes;
	uint64_t m_nResidentBytes;
	uint64_t m_nPeakResidentBytes;
	sirScriptCachedSourceEntry_t m_szEntries[MAX_ENTRIES];
	sirScriptCachedSourceLease_t m_szLeases[MAX_LEASES];
	bool m_bInAllocator;
};

#endif // __SIRENGINE_SIRSCRIPTCACHEDPROGRAMSOURCE_H__
