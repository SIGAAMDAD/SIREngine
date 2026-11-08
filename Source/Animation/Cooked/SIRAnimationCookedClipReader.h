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
#ifndef __SIRENGINE_SIRANIMATIONCOOKEDCLIPREADER_H__
#define __SIRENGINE_SIRANIMATIONCOOKEDCLIPREADER_H__

#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationCooked.h"

// Borrowed, validated cooked clip reader. initialize() verifies the entire
// archive once; later reads require the same immutable GRDG-pinned bytes.
// The reader neither owns nor extends the source lifetime.
class SIRAnimationCookedClipReader
{
public:
	SIRAnimationCookedClipReader( void ) noexcept = default;

	sirAnimationCookResult_t initialize( sirAnimationCookSpan_t source, sirAnimationCookAssetInfo_t *pInfo ) noexcept;
	sirAnimationCookResult_t getTrackCount( uint32_t nBoneIndex, bool bPreferFine, uint32_t *pCount ) const noexcept;
	sirAnimationCookResult_t readTrack( uint32_t nBoneIndex, bool bPreferFine, sirAnimationCookTrackOutput_t *pOutput ) const noexcept;

protected:
private:
	sirAnimationCookSpan_t m_Source = {};
	uint32_t m_nTrackCount = 0;
	uint64_t m_nTrackOffset = 0;
};

#endif // __SIRENGINE_SIRANIMATIONCOOKEDCLIPREADER_H__
