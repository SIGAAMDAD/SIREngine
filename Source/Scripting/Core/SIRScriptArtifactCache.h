/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_SCRIPTARTIFACTCACHE_H__
#define __SIR_SCRIPTARTIFACTCACHE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptCodeManager.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>

// SHA-256 of a canonical, versioned build recipe: source/dependencies, schema,
// frontend/compiler version, platform ABI, target CPU and optimizer switches.
// The caller owns canonical recipe serialization; no raw pointer values.
typedef struct sirScriptCacheKey_s
{
	uint8_t szDigest[32];
} sirScriptCacheKey_t;

typedef struct sirScriptCachedArtifact_s
{
	sirScriptCodeArtifact_t artifact;
	sirScriptCacheKey_t key;
	uint8_t szPayloadDigest[32];
	uint64_t nBytes;
	uint32_t nAlignment;
} sirScriptCachedArtifact_t;

// Single-owner persistent blob store. The directory must be provisioned by
// the host. Writes commit through a uniquely named temporary file and rename;
// reads validate all header fields and the complete SHA-256 payload digest.
// Bytes remain owned by the caller; this class never allocates executable pages.
class SIRScriptArtifactCache
{
public:
	SIRScriptArtifactCache( void ) noexcept;
	SIRScriptArtifactCache( const SIRScriptArtifactCache& ) = delete;
	SIRScriptArtifactCache& operator=( const SIRScriptArtifactCache& ) = delete;

	// Bound the size of each accepted cache entry (caller-selected budget).
	SIRScriptResult configure( const char *szRoot, uint64_t nMaximumArtifactBytes ) noexcept;
	static SIRScriptResult makeKey( const void *pCanonicalRecipe, uint64_t nRecipeBytes, sirScriptCacheKey_t *pKey ) noexcept;
	SIRScriptResult store( const sirScriptCodeArtifact_t& artifact, sirScriptCacheKey_t key, const void *pBytes, uint64_t nBytes,
	                         uint32_t nAlignment, sirScriptCachedArtifact_t *pDescriptor ) noexcept;
	SIRScriptResult fetch( const sirScriptCachedArtifact_t& descriptor, void *pOutput, uint64_t nCapacity ) const noexcept;
	SIRScriptResult erase( sirScriptCacheKey_t key ) noexcept;

private:
	static constexpr uint32_t MAX_PATH_BYTES = 1024;
	static constexpr uint32_t HEADER_BYTES = 152;

	bool isOwnerThread( void ) const noexcept;
	bool validKey( const sirScriptCacheKey_t& key ) const noexcept;
	bool formatPath( const sirScriptCacheKey_t& key, char szPath[MAX_PATH_BYTES] ) const noexcept;
	bool validDescriptor( const sirScriptCachedArtifact_t& descriptor ) const noexcept;
	static void encodeHeader( const sirScriptCachedArtifact_t& descriptor, uint8_t szHeader[HEADER_BYTES] ) noexcept;
	static bool decodeHeader( const uint8_t szHeader[HEADER_BYTES], sirScriptCachedArtifact_t *pDescriptor ) noexcept;
	static bool sameDescriptor( const sirScriptCachedArtifact_t& a, const sirScriptCachedArtifact_t& b ) noexcept;

	char m_szRoot[MAX_PATH_BYTES];
	uint64_t m_nMaximumArtifactBytes;
	const sirThreadId_t m_OwnerThread;
};

#endif // __SIR_SCRIPTARTIFACTCACHE_H__
