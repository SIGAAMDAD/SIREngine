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

#ifndef __SIRENGINE_SIRLANGCOOKRECIPE_H__
#define __SIRENGINE_SIRLANGCOOKRECIPE_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Scripting/Core/SIRScriptArtifactCache.h>
#include <Scripting/Language/SIRLangGraphVM.h>

// Immutable, canonical inputs to one cooked script program. SourceDigest is
// SHA-256 of source bytes and imports are already in stable module-ID order.
// Runtime executable code never derives cache identity from pointer addresses.
typedef struct sirLangCookIdentity_s
{
	uint64_t nModuleId;
	uint64_t nSchemaId;
	uint64_t nLayoutDigest;
	uint64_t nBindingDigest;
	uint64_t nTargetId;
	uint64_t nToolchainId;
	uint64_t nOptimizationFlags;
	uint8_t szSourceDigest[32];
} sirLangCookIdentity_t;

typedef struct sirLangCookImport_s
{
	uint64_t nModuleId;
	uint8_t szDigest[32];
} sirLangCookImport_t;

// Returns a fixed-order content-addressable key over the complete cook input.
// A source importer/cooker must calculate the transitive import digests and
// reject dependency cycles before providing this immutable array.
sirScriptResult_t SIRLangMakeCookKey( const sirLangCookIdentity_t& identity, const sirLangCookImport_t *pImports,
                                      uint32_t nImportCount, sirScriptCacheKey_t *pKey ) noexcept;

#endif // __SIRENGINE_SIRLANGCOOKRECIPE_H__
