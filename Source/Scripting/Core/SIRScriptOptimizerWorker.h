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

#ifndef __SIRENGINE_SIRSCRIPTOPTIMIZERWORKER_H__
#define __SIRENGINE_SIRSCRIPTOPTIMIZERWORKER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptOptimizerInputStore.h>

static constexpr uint32_t SIR_SCRIPT_OPT_BACKEND_ABI_REVISION = 1;
static constexpr char SIR_SCRIPT_OPT_BACKEND_ENTRYPOINT[] = "SIRScriptOptimizerBackendGetAPI";

typedef struct sirScriptOptimizerWorkView_s
{
	const sirScriptOptimizationRequest_t *pRequest;
	const void *pRecipe;
	uint64_t nRecipeBytes;
	const void *pInput;
	uint64_t nInputBytes;
} sirScriptOptimizerWorkView_t;

typedef struct sirScriptOptimizerOutput_s
{
	void *pBytes;
	uint64_t nBytes;
	uint32_t nAlignment;
} sirScriptOptimizerOutput_t;

typedef struct sirScriptOptimizerBackendAPI_s
{
	uint32_t nABIRevision;
	void *pUser;
	sirScriptResult_t ( *pCompile )( void *pUser, const sirScriptOptimizerWorkView_t *pView, sirScriptOptimizerOutput_t *pOutput ) noexcept;
	void ( *pRelease )( void *pUser, sirScriptOptimizerOutput_t *pOutput ) noexcept;
} sirScriptOptimizerBackendAPI_t;

// Export this symbol from an optional compiler backend DLL/SO. The backend
// owns its output buffers and MUST release them through pRelease. No backend
// code is linked into the host engine or into the transport layer.
extern "C" {
typedef const sirScriptOptimizerBackendAPI_t *( *sirScriptOptimizerGetBackendAPI_t )( void );
}

class SIRScriptOptimizerWorker
{
public:
	SIRScriptOptimizerWorker( void ) noexcept;
	SIRScriptOptimizerWorker( const SIRScriptOptimizerWorker& ) = delete;
	SIRScriptOptimizerWorker& operator=( const SIRScriptOptimizerWorker& ) = delete;

	sirScriptResult_t configure( SIRScriptOptimizerInputStore *pInputs, SIRScriptArtifactCache *pCache,
	                             const sirScriptOptimizerBackendAPI_t& backend, uint64_t nMaximumOutputBytes ) noexcept;
	sirScriptResult_t process( const sirScriptOptimizationRequest_t& request, sirScriptOptimizationReply_t *pReply ) noexcept;

protected:
	// Lives only in the external optimizer process; no host allocations.

private:
	SIRScriptOptimizerInputStore *m_pInputs;
	SIRScriptArtifactCache *m_pCache;
	sirScriptOptimizerBackendAPI_t m_Backend;
	uint64_t m_nMaximumOutputBytes;
	bool m_bProcessing;
};

#endif // __SIRENGINE_SIRSCRIPTOPTIMIZERWORKER_H__
