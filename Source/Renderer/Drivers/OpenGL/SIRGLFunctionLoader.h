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

#ifndef __SIRENGINE_SIRGLFUNCTIONLOADER_H__
#define __SIRENGINE_SIRGLFUNCTIONLOADER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef void *( *sirGLProcedureLookup_t )( void *pContext, const char *pszName ) noexcept;

bool sirGLIsUsableProcedure( const void *pAddress ) noexcept;
bool sirGLValidateRequired46Functions( sirGLProcedureLookup_t pLookup, void *pContext ) noexcept;

#endif // __SIRENGINE_SIRGLFUNCTIONLOADER_H__
