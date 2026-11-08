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
#include "SIRAnimationSIMD.h"
#include <cmath>
#if defined( __x86_64__ ) || defined( _M_X64 ) || defined( __i386__ ) || defined( _M_IX86 )
#include <immintrin.h>
#define SIR_ANIMATION_X86_SIMD 1
#elif defined( __ARM_NEON ) || defined( __ARM_NEON__ )
#include <arm_neon.h>
#endif

#if defined( SIR_ANIMATION_X86_SIMD ) && ( defined( __GNUC__ ) || defined( __clang__ ) )
static bool sirAnimationCanUseAVX2( void ) noexcept
{
	return __builtin_cpu_supports( "avx2" );
}

__attribute__( ( target( "avx2" ) ) ) static void sirAnimationBlendAVX2( const sirAnimationSoaBlend_t& options,
                                                                         uint32_t *pProcessed ) noexcept
{
	const __m256 w = _mm256_set1_ps( options.fWeight );
	uint32_t i = 0;
	for ( ; i + 8 <= options.nCount; i += 8 ) {
		const __m256 a = _mm256_loadu_ps( options.pDestination + i );
		const __m256 b = _mm256_loadu_ps( options.pSource + i );
		_mm256_storeu_ps( options.pDestination + i, _mm256_add_ps( a, _mm256_mul_ps( _mm256_sub_ps( b, a ), w ) ) );
	}
	*pProcessed = i;
}
#endif

sirAnimationSIMDPath_t SIRAnimationSIMD::getCompiledPath( void ) noexcept
{
#if defined( SIR_ANIMATION_X86_SIMD ) && ( defined( __GNUC__ ) || defined( __clang__ ) )
	if ( sirAnimationCanUseAVX2() ) {
		return SIR_ANIMATION_SIMD_AVX2;
	}
#endif
#if defined( __SSE2__ ) || defined( _M_X64 )
	return SIR_ANIMATION_SIMD_SSE2;
#elif defined( __ARM_NEON ) || defined( __ARM_NEON__ )
	return SIR_ANIMATION_SIMD_NEON;
#else
	return SIR_ANIMATION_SIMD_SCALAR;
#endif
}

sirAnimationCookResult_t SIRAnimationSIMD::blendChannels( const sirAnimationSoaBlend_t& options ) noexcept
{
	if ( !options.pDestination || !options.pSource || !std::isfinite( options.fWeight ) || options.fWeight < 0.0f ||
	     options.fWeight > 1.0f || !options.nCount || options.nCount > SIR_ANIM_COOK_MAX_BONES * 10 ) {
		return SIR_ANIM_COOK_INVALID;
	}
	const uintptr_t nDest = ( uintptr_t )options.pDestination;
	const uintptr_t nSource = ( uintptr_t )options.pSource;
	const uintptr_t nBytes = ( uintptr_t )options.nCount * sizeof( float );
	if ( nDest > UINTPTR_MAX - nBytes || nSource > UINTPTR_MAX - nBytes ||
	     ( nDest != nSource && nDest < nSource + nBytes && nSource < nDest + nBytes ) ) {
		return SIR_ANIM_COOK_INVALID;
	}
	for ( uint32_t i = 0; i < options.nCount; ++i ) {
		if ( !std::isfinite( options.pSource[i] ) || !std::isfinite( options.pDestination[i] ) ) {
			return SIR_ANIM_COOK_INVALID;
		}
	}
	if ( nDest == nSource ) {
		return SIR_ANIM_COOK_OK;
	}
	uint32_t i = 0;
#if defined( SIR_ANIMATION_X86_SIMD ) && ( defined( __GNUC__ ) || defined( __clang__ ) )
	if ( sirAnimationCanUseAVX2() ) {
		sirAnimationBlendAVX2( options, &i );
	}
#endif
#if defined( __SSE2__ ) || defined( _M_X64 )
	const __m128 w = _mm_set1_ps( options.fWeight );
	for ( ; i + 4 <= options.nCount; i += 4 ) {
		const __m128 a = _mm_loadu_ps( options.pDestination + i );
		const __m128 b = _mm_loadu_ps( options.pSource + i );
		_mm_storeu_ps( options.pDestination + i, _mm_add_ps( a, _mm_mul_ps( _mm_sub_ps( b, a ), w ) ) );
	}
#elif defined( __ARM_NEON ) || defined( __ARM_NEON__ )
	const float32x4_t w = vdupq_n_f32( options.fWeight );
	for ( ; i + 4 <= options.nCount; i += 4 ) {
		const float32x4_t a = vld1q_f32( options.pDestination + i );
		const float32x4_t b = vld1q_f32( options.pSource + i );
		vst1q_f32( options.pDestination + i, vaddq_f32( a, vmulq_f32( vsubq_f32( b, a ), w ) ) );
	}
#endif
	for ( ; i < options.nCount; ++i ) {
		options.pDestination[i] += ( options.pSource[i] - options.pDestination[i] ) * options.fWeight;
	}
	return SIR_ANIM_COOK_OK;
}
