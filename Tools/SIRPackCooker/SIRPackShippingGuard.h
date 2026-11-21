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
#ifndef __SIRENGINE_SIRPACKSHIPPINGGUARD_H__
#define __SIRENGINE_SIRPACKSHIPPINGGUARD_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Tools/SIRPackCooker/SIRPackCooker.h>

// Caller-owned authoring-only source identities and physical byte signatures.
// Enumerate displacement input resource keys (including transitive sources),
// original source-image bytes, source paths and authoring sampler/recipe tokens.
// Runtime material textures that are not enumerated remain valid dependencies.
typedef struct sirPackShippingPattern_s
{
	const uint8_t *pBytes;
	uint64_t nBytes;
} sirPackShippingPattern_t;

typedef struct sirPackShippingPolicy_s
{
	const SIRResourceKey *pForbiddenKeys;
	uint32_t nForbiddenKeys;
	const uint32_t *pForbiddenTypeIds;
	uint32_t nForbiddenTypeIds;
	const sirPackShippingPattern_t *pForbiddenPatterns;
	uint32_t nForbiddenPatterns;
} sirPackShippingPolicy_t;

// Enforces an immutable shipping boundary before the offline writer can
// change the caller's catalog or shard output. No heap or global registry.
class SIRPackShippingGuard
{
public:
	static sirPackResult_t validate( const sirPackCookDesc_t& desc, const sirPackShippingPolicy_t& policy ) noexcept;
	static sirPackResult_t build( const sirPackCookDesc_t& desc, const sirPackShippingPolicy_t& policy, uint8_t *pCatalog,
	                              uint64_t nCatalogCapacity ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRPACKSHIPPINGGUARD_H__
