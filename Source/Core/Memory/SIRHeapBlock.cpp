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

#include <Core/Memory/Allocators/SIRHeapBlock.h>

uint32_t SIRHeapBlock::size( const sirHeapBlock_t *pBlock ) noexcept
{
    return pBlock->nSizeAndFlags & ~15u;
}

bool SIRHeapBlock::isFree( const sirHeapBlock_t *pBlock ) noexcept
{
    return ( pBlock->nSizeAndFlags & FREE ) != 0;
}
