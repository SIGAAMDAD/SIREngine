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

#ifndef __SIRENGINE_SIRSCRIPTSHA256_H__
#define __SIRENGINE_SIRSCRIPTSHA256_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

// Stable platform-independent artifact integrity digest. Not a signature.
class SIRScriptSHA256
{
public:
	static void digest( const void *pData, uint64_t nBytes, uint8_t szDigest[32] ) noexcept;
};

#endif // __SIRENGINE_SIRSCRIPTSHA256_H__
