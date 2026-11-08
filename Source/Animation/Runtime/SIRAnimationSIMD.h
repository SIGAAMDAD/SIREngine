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
#ifndef __SIRENGINE_SIRANIMATIONSIMD_H__
#define __SIRENGINE_SIRANIMATIONSIMD_H__

#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Animation/Cooked/SIRAnimationCooked.h>

typedef enum sirAnimationSIMDPath_e : uint8_t
{
	SIR_ANIMATION_SIMD_SCALAR = 0,
	SIR_ANIMATION_SIMD_SSE2 = 1,
	SIR_ANIMATION_SIMD_AVX2 = 2,
	SIR_ANIMATION_SIMD_NEON = 3
} sirAnimationSIMDPath_t;

typedef struct sirAnimationSoaBlend_s
{
	float *pDestination;
	const float *pSource;
	uint32_t nCount;
	float fWeight;
} sirAnimationSoaBlend_t;

class SIRAnimationSIMD
{
public:
	SIRAnimationSIMD( void ) noexcept = delete;

	static sirAnimationSIMDPath_t getCompiledPath( void ) noexcept;
	static sirAnimationCookResult_t blendChannels( const sirAnimationSoaBlend_t& options ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONSIMD_H__
