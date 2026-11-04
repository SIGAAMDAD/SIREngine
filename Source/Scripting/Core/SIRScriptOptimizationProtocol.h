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

#ifndef __SIRENGINE_SIRSCRIPTOPTIMIZATIONPROTOCOL_H__
#define __SIRENGINE_SIRSCRIPTOPTIMIZATIONPROTOCOL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptArtifactCache.h>

// Scripting-language-independent record sent to an external optimizer.
// A caller computes the recipe key from canonical source/dependency/toolchain
// inputs. It is never constructed from a host pointer or process-local ID.
typedef enum sirScriptOptimizationTier_e : uint32_t
{
	SIR_SCRIPT_OPT_BASELINE = 0,
	SIR_SCRIPT_OPT_TARGET_CPU = 1,
	SIR_SCRIPT_OPT_PROFILE_GUIDED = 2
} sirScriptOptimizationTier_t;

typedef struct sirScriptOptimizationRequest_s
{
	uint64_t nJobId;
	sirScriptCodeArtifact_t artifact;
	sirScriptCacheKey_t key;
	uint64_t nInputBytes;
	sirScriptOptimizationTier_t eTier;
	uint32_t nFlags;
} sirScriptOptimizationRequest_t;

typedef struct sirScriptOptimizationReply_s
{
	uint64_t nJobId;
	sirScriptResult_t eResult;
	sirScriptCachedArtifact_t cached;
} sirScriptOptimizationReply_t;

// Canonical versioned IPC records: no C++ padding, pointers or executable
// bytes on the wire. The SHA-256 trailer detects transport corruption;
// completion still requires a trusted worker and cache payload verification.
class SIRScriptOptimizationProtocol
{
public:
	static constexpr uint32_t REQUEST_BYTES = 160;
	static constexpr uint32_t REPLY_BYTES = 200;

	static sirScriptResult_t encodeRequest( const sirScriptOptimizationRequest_t &request, uint8_t szOutput[REQUEST_BYTES] ) noexcept;
	static sirScriptResult_t decodeRequest( const uint8_t *pInput, uint32_t nBytes, sirScriptOptimizationRequest_t *pRequest ) noexcept;
	static sirScriptResult_t encodeReply( const sirScriptOptimizationReply_t &reply, uint8_t szOutput[REPLY_BYTES] ) noexcept;
	static sirScriptResult_t decodeReply( const uint8_t *pInput, uint32_t nBytes, sirScriptOptimizationReply_t *pReply ) noexcept;
	static sirScriptResult_t validateCompletion( const sirScriptOptimizationRequest_t &request,
	                                             const sirScriptOptimizationReply_t &reply ) noexcept;

private:
	static bool validRequest( const sirScriptOptimizationRequest_t &request ) noexcept;
	static bool validReply( const sirScriptOptimizationReply_t &reply ) noexcept;
};

#endif // __SIRENGINE_SIRSCRIPTOPTIMIZATIONPROTOCOL_H__
