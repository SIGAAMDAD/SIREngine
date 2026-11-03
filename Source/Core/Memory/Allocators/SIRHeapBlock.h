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

#ifndef __SIRENGINE_SIRHEAPBLOCK_H__
#define __SIRENGINE_SIRHEAPBLOCK_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

// Raw 16-byte in-band boundary-tag header shared by the two concrete heaps.
// Offsets always address 16-byte aligned blocks, and 0xffffffff is invalid.
typedef struct sirHeapBlock_s
{
    uint32_t nPreviousPhysicalBytes;
    uint32_t nSizeAndFlags;
    uint32_t nPreviousFreeOffset;
    uint32_t nNextFreeOffset;
} sirHeapBlock_t;
static_assert( sizeof( sirHeapBlock_t ) == 16, "16-byte heap block header" );

class SIRHeapBlock
{
public:
    static constexpr uint32_t NONE = UINT32_MAX;
    static constexpr uint32_t FREE = 1;
    static constexpr uint32_t HEADER = 16;
    static constexpr uint32_t MINIMUM = 32;

    static uint32_t size( const sirHeapBlock_t *pBlock ) noexcept;
    static bool isFree( const sirHeapBlock_t *pBlock ) noexcept;
};

#endif // __SIRENGINE_SIRHEAPBLOCK_H__
