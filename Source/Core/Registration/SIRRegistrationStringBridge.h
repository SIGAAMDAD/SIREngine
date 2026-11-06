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

#ifndef __SIRENGINE_SIRREGISTRATIONSTRINGBRIDGE_H__
#define __SIRENGINE_SIRREGISTRATIONSTRINGBRIDGE_H__

#include <SIRPCH.hpp>
#include <Core/Registration/SIRRegistrationTypes.h>
#include <Core/Strings/SIRStringPool.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Owner zero is reserved for engine registration. Modules must register qualified names.
typedef struct sirRegistrationNameRequest_s
{
	const char *pszName;
	uint32_t nNameBytes;
	uint32_t nOwnerId;
} sirRegistrationNameRequest_t;

// Cold registration only: normalize names once, never intern submitted argument text.
sirRegistrationResult_t sirRegistrationInternName( SIRStringPool& pool, const sirRegistrationNameRequest_t& request,
                                                   SIRStringAtom *pAtom ) noexcept;
// Read-only lookup is allocation-free and never creates a name on a miss.
sirRegistrationResult_t sirRegistrationFindName( const SIRStringPool& pool, const char *pszName, size_t nLength,
                                                 SIRStringAtom *pAtom ) noexcept;
// Cooked ordinals are accepted only for this pool's matching SIRS content hash.
sirRegistrationResult_t sirRegistrationCookedName( const SIRStringPool& pool, uint32_t nOrdinal, uint64_t nCatalogHash,
                                                   SIRStringAtom *pAtom ) noexcept;

#endif // __SIRENGINE_SIRREGISTRATIONSTRINGBRIDGE_H__
