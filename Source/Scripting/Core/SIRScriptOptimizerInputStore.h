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

#ifndef __SIRENGINE_SIRSCRIPTOPTIMIZERINPUTSTORE_H__
#define __SIRENGINE_SIRSCRIPTOPTIMIZERINPUTSTORE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptOptimizationProtocol.h>
#include <thread>

typedef struct sirScriptOptimizerInputBuffers_s
{
	void *pRecipe;
	uint64_t nRecipeCapacity;
	void *pInput;
	uint64_t nInputCapacity;
	uint64_t nRecipeBytes;
} sirScriptOptimizerInputBuffers_t;

// Stores an immutable build recipe and compiler input under the recipe hash.
// The job ID is deliberately excluded; identical recipe inputs can be reused
// across child-process starts. The frontend owns canonical recipe semantics.
class SIRScriptOptimizerInputStore
{
public:
	SIRScriptOptimizerInputStore( void ) noexcept;
	SIRScriptOptimizerInputStore( const SIRScriptOptimizerInputStore& ) = delete;
	SIRScriptOptimizerInputStore& operator=( const SIRScriptOptimizerInputStore& ) = delete;

	sirScriptResult_t configure( const char *pszRoot, uint64_t nMaximumInputBytes, uint64_t nMaximumRecipeBytes ) noexcept;
	sirScriptResult_t stage( const sirScriptOptimizationRequest_t& request, const void *pRecipe, uint64_t nRecipeBytes,
	                         const void *pInput ) noexcept;
	sirScriptResult_t inspect( const sirScriptOptimizationRequest_t& request, uint64_t *pRecipeBytes ) const noexcept;
	sirScriptResult_t fetch( const sirScriptOptimizationRequest_t& request, sirScriptOptimizerInputBuffers_t *pBuffers ) const noexcept;
	sirScriptResult_t erase( sirScriptCacheKey_t key ) noexcept;

protected:
	// The caller provisions and owns the input directory.

private:
	static constexpr uint32_t MAX_PATH_BYTES = 1024;
	static constexpr uint32_t HEADER_BYTES = 224;

	bool isOwnerThread( void ) const noexcept;
	bool formatPath( const sirScriptCacheKey_t& key, char szPath[MAX_PATH_BYTES] ) const noexcept;
	sirScriptResult_t readHeader( const sirScriptOptimizationRequest_t& request, uint8_t szHeader[HEADER_BYTES],
	                              uint64_t *pRecipeBytes ) const noexcept;
	static void encodeHeader( const sirScriptOptimizationRequest_t& request, uint64_t nRecipeBytes, const uint8_t szInputDigest[32],
	                          uint8_t szHeader[HEADER_BYTES] ) noexcept;
	static bool validHeader( const sirScriptOptimizationRequest_t& request, const uint8_t szHeader[HEADER_BYTES],
	                         uint64_t nMaximumRecipeBytes, uint64_t *pRecipeBytes ) noexcept;

	char m_szRoot[MAX_PATH_BYTES];
	uint64_t m_nMaximumInputBytes;
	uint64_t m_nMaximumRecipeBytes;
	const std::thread::id m_OwnerThread;
};

#endif // __SIRENGINE_SIRSCRIPTOPTIMIZERINPUTSTORE_H__
