/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRMEMORYUTILS_H__
#define __SIRENGINE_SIRMEMORYUTILS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <stdint.h>
#include <stddef.h>

#define SIR_IS_POWER_OF_TWO( nValue ) ( (nValue) != 0 && ( ( (nValue) & ( (nValue) - 1 ) ) == 0 ) )
#define SIR_ALIGN_DOWN( nValue, nAlignment ) ( (uintptr_t)( nValue ) & ~( (uintptr_t)( nAlignment ) - 1u ) )
#define SIR_ALIGN_UP( nValue, nAlignment ) ( ( (uintptr_t)( nValue ) + (uintptr_t)( nAlignment ) - 1u ) & ~( (uintptr_t)( nAlignment ) - 1u ) )
#define SIR_ROUND_UP_MULTIPLE( nValue, nMultiple ) ( ( (nValue) / (nMultiple) + ( (nValue) % (nMultiple) != 0 ) ) * (nMultiple) )

#endif // __SIRENGINE_SIRMEMORYUTILS_H__
