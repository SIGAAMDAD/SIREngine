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

#ifndef __SIRENGINE_SIRREGISTRATIONCOOKER_H__
#define __SIRENGINE_SIRREGISTRATIONCOOKER_H__

#include <SIRPCH.hpp>
#include <Core/Registration/SIRRegistrationCatalog.h>
#include <string>
#include <vector>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Offline metadata is owned by the cook tool, never by a runtime registry.
typedef struct sirRegistrationCookDefinition_s
{
	std::string sName;
	sirRegistrationKind_t eKind;
	uint64_t nBindingId;
	uint32_t nSchemaId;
	uint32_t nOwnerId;
	uint32_t nFlags;
	uint32_t nCapabilities;
	sirCVarType_t eType;
	uint64_t nDefaultBits;
	std::string sDefaultString;
} sirRegistrationCookDefinition_t;

// Sorts and validates caller-owned definitions; emits deterministic SIRS v1 and SIRC v1.
// Failure preserves both caller output buffers.
bool sirCookRegistrationCatalog( const std::vector<sirRegistrationCookDefinition_t>& definitions,
                                 std::vector<uint8_t>& strings, std::vector<uint8_t>& registrations );

#endif // __SIRENGINE_SIRREGISTRATIONCOOKER_H__
