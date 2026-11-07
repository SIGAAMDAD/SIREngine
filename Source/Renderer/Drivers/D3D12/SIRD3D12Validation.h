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

#ifndef __SIRENGINE_SIRD3D12VALIDATION_H__
#define __SIRENGINE_SIRD3D12VALIDATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>
#include <float.h>

// D3D12 UPLOAD/GPU_UPLOAD and READBACK resources have immutable native
// states (GENERIC_READ and COPY_DEST, respectively). Reject usage declarations
// that would require an illegal transition before allocating any GPU memory.
static inline sirGraphicsResult_t sirD3D12ValidateBufferHeapUsage( const sirGraphicsBufferDesc_t& desc ) noexcept
{
	if ( desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_READBACK ) {
		return desc.nUsage == SIR_GRAPHICS_USAGE_COPY_DESTINATION ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD || desc.eMemory == SIR_GRAPHICS_MEMORY_GPU_UPLOAD ) {
		if ( ( desc.nUsage & ( SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_STORAGE ) ) != 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

typedef struct sirD3D12CopyLimits_s
{
	uint32_t nMipWidth;
	uint32_t nMipHeight;
	uint32_t nTexelBytes;
	uint64_t nBufferBytes;
} sirD3D12CopyLimits_t;

// Portable validation of the D3D12-specific 2D placed-footprint contract.
// The caller validates format, mip, layer, depth, usage and resource identity.
static inline sirGraphicsResult_t sirD3D12ValidateCopyRegion( const sirGraphicsBufferTextureCopy_t& copy,
                                                              const sirD3D12CopyLimits_t& limits ) noexcept
{
	if ( copy.nZ != 0 || copy.nWidth == 0 || copy.nHeight == 0 || copy.nX > limits.nMipWidth || copy.nY > limits.nMipHeight ||
	     copy.nWidth > limits.nMipWidth - copy.nX || copy.nHeight > limits.nMipHeight - copy.nY || limits.nTexelBytes == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Placed footprints require 256-byte row pitches and 512-byte offsets.
	if ( copy.nBytesPerRow == 0 || copy.nBytesPerRow % 256U != 0 || copy.nRowsPerImage != copy.nHeight || copy.nBufferOffset % 512U != 0 ||
	     ( uint64_t )copy.nWidth * limits.nTexelBytes > copy.nBytesPerRow ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t nNeeded = ( uint64_t )copy.nBytesPerRow * copy.nHeight;
	if ( copy.nBufferOffset > limits.nBufferBytes || nNeeded > limits.nBufferBytes - copy.nBufferOffset ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}

// D3D12 native viewport commands must not receive NaN or infinite coordinates.
// Compare against FLT_MAX to avoid platform-specific math-library intrinsics.
static inline sirGraphicsResult_t sirD3D12ValidateViewport( const sirGraphicsViewport_t& viewport,
                                                            const sirGraphicsRect_t& scissor ) noexcept
{
	if ( !( viewport.fX >= -FLT_MAX && viewport.fX <= FLT_MAX ) || !( viewport.fY >= -FLT_MAX && viewport.fY <= FLT_MAX ) ||
	     !( viewport.fWidth > 0.0f && viewport.fWidth <= FLT_MAX ) || !( viewport.fHeight > 0.0f && viewport.fHeight <= FLT_MAX ) ||
	     !( viewport.fMinimumDepth >= 0.0f && viewport.fMinimumDepth <= 1.0f ) ||
	     !( viewport.fMaximumDepth >= 0.0f && viewport.fMaximumDepth <= 1.0f ) || viewport.fMinimumDepth > viewport.fMaximumDepth ||
	     scissor.nLeft >= scissor.nRight || scissor.nTop >= scissor.nBottom ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}

#endif // __SIRENGINE_SIRD3D12VALIDATION_H__
