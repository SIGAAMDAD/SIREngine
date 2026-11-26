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

#ifndef __SIR_MATH_H__
#define __SIR_MATH_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#define SIR_IS_POWER_OF_TWO( nValue ) \
	( ( nValue ) != 0 && ( ( ( nValue ) & ( ( nValue ) - 1 ) ) == 0 ) )

#define SIR_ALIGN_DOWN( nValue, nAlignment ) \
	( static_cast<uintptr_t>( nValue ) & ~( static_cast<uintptr_t>( nAlignment ) - 1u ) )

#define SIR_ALIGN_UP( nValue, nAlignment ) \
	( ( static_cast<uintptr_t>( nValue ) + static_cast<uintptr_t>( nAlignment ) - 1u ) & ~( static_cast<uintptr_t>( nAlignment ) - 1u ) )

#define SIR_ROUND_UP_MULTIPLE( nValue, nMultiple ) \
	( ( ( nValue ) / ( nMultiple ) + ( ( nValue ) % ( nMultiple ) != 0 ) ) * ( nMultiple ) )

#endif // __SIR_MATH_H__
