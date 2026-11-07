/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIREDITORRESOURCEKEYS_H__
#define __SIRENGINE_SIREDITORRESOURCEKEYS_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on
class SIRResourceCatalog;

typedef struct sirEditorResourceRequestKey_s
{
	uint64_t nCanonicalId;
	uint32_t nTypeId;
} sirEditorResourceRequestKey_t;

typedef struct sirEditorResourceKeyBuffer_s
{
	uint64_t *pOrderScratch;
	sirEditorResourceRequestKey_t *pKeys;
	uint32_t nCapacity;
	uint32_t nCount;
} sirEditorResourceKeyBuffer_t;

bool sirEditorCollectCatalogKeys( SIRResourceCatalog& catalog, sirEditorResourceKeyBuffer_t *pBuffer ) noexcept;
#endif // __SIRENGINE_SIREDITORRESOURCEKEYS_H__
