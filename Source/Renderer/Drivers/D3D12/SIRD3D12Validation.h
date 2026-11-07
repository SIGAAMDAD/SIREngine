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

// DXC emits DXIL inside a DXBC container. Validate offsets before handing the
// untrusted cooked byte stream to the native PSO compiler.
static inline uint32_t sirD3D12ReadDXILU32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) |
	       ( ( uint32_t )pBytes[3] << 24 );
}

static inline bool sirD3D12ValidateDXILContainer( const void *pData, uint64_t nBytes ) noexcept
{
	if ( pData == nullptr || nBytes < 40 || nBytes > 16ULL * 1024ULL * 1024ULL ) {
		return false;
	}
	const uint8_t *pBytes = static_cast<const uint8_t *>( pData );
	if ( pBytes[0] != 'D' || pBytes[1] != 'X' || pBytes[2] != 'B' || pBytes[3] != 'C' ||
	     sirD3D12ReadDXILU32( pBytes + 20 ) != 1 || sirD3D12ReadDXILU32( pBytes + 24 ) != nBytes ) {
		return false;
	}
	const uint32_t nPartCount = sirD3D12ReadDXILU32( pBytes + 28 );
	if ( nPartCount == 0 || nPartCount > ( nBytes - 32 ) / 4 ) {
		return false;
	}
	uint32_t nDXILParts = 0;
	for ( uint32_t iPart = 0; iPart < nPartCount; ++iPart ) {
		const uint32_t nOffset = sirD3D12ReadDXILU32( pBytes + 32 + iPart * 4 );
		if ( nOffset < 32ULL + 4ULL * nPartCount || nOffset > nBytes || nBytes - nOffset < 8 ) {
			return false;
		}
		const uint32_t nPayload = sirD3D12ReadDXILU32( pBytes + nOffset + 4 );
		if ( nPayload > nBytes - nOffset - 8 ) {
			return false;
		}
		if ( pBytes[nOffset] == 'D' && pBytes[nOffset + 1] == 'X' && pBytes[nOffset + 2] == 'I' &&
		     pBytes[nOffset + 3] == 'L' ) {
			if ( nPayload < 24 || ++nDXILParts > 1 ) {
				return false;
			}
		}
	}
	return nDXILParts == 1;
}

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

// The public descriptor update address is (register, array index), without a
// descriptor-type discriminator. Overlap inside one register space is ambiguous.
// Each descriptor table consumes one root DWORD; the native root is capped at
// 32 tables and each set at 128 shader-visible descriptors.
static inline sirGraphicsResult_t sirD3D12ValidateBindingLayout( const sirGraphicsBindingDesc_t *pBindings, uint32_t nCount,
                                                                 sirGraphicsPipelineType_t ePipeline ) noexcept
{
	if ( nCount > 32 || ( nCount != 0 && pBindings == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirGraphicsBindingDesc_t& binding = pBindings[i];
		if ( binding.nArrayCount == 0 || binding.nArrayCount > 128 || binding.nBinding > UINT32_MAX - ( binding.nArrayCount - 1 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( binding.eType > SIR_GRAPHICS_BINDING_SAMPLER ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint32_t nStages = binding.nShaderStageMask;
		if ( ePipeline == SIR_GRAPHICS_PIPELINE_COMPUTE ) {
			if ( nStages != SIR_GRAPHICS_STAGE_COMPUTE ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		} else if ( ePipeline != SIR_GRAPHICS_PIPELINE_GRAPHICS || nStages == 0 ||
		            ( nStages & ~( SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT ) ) != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		uint32_t nSetDescriptors = binding.nArrayCount;
		for ( uint32_t j = 0; j < i; ++j ) {
			const sirGraphicsBindingDesc_t& prior = pBindings[j];
			if ( binding.nSpace != prior.nSpace ) {
				continue;
			}
			const uint64_t nEnd = ( uint64_t )binding.nBinding + binding.nArrayCount;
			const uint64_t nPriorEnd = ( uint64_t )prior.nBinding + prior.nArrayCount;
			if ( binding.nBinding < nPriorEnd && prior.nBinding < nEnd ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( nSetDescriptors > 128 - prior.nArrayCount ) {
				return SIR_GRAPHICS_CAPACITY_EXCEEDED;
			}
			nSetDescriptors += prior.nArrayCount;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

// An external DXGI buffer is distinct from the preallocated D3D12MA pools.
// Sealed windowed operation therefore requires an explicit *total* resident
// budget: fixed blocks plus all external backbuffers, without double billing.
static inline bool sirD3D12CanReserveExternal( uint64_t nCommitted, uint64_t nBudget, uint64_t nBytes, bool bSealed ) noexcept
{
	if ( nBytes == 0 || nCommitted > UINT64_MAX - nBytes || ( bSealed && nBudget == 0 ) ) {
		return false;
	}
	return nBudget == 0 || ( nCommitted <= nBudget && nBytes <= nBudget - nCommitted );
}

// Hardware probes are recorded only for operations the native backend
// actually implements. The bit set must be identical at adapter and device
// admission when given the same physical device.
static inline uint64_t sirD3D12NegotiatedOptionalFeatures( bool bEnhancedBarriers, bool bMultisample ) noexcept
{
	uint64_t nMask = 0;
	if ( bEnhancedBarriers ) {
		nMask |= SIR_GRAPHICS_FEATURE_ENHANCED_BARRIERS;
	}
	if ( bMultisample ) {
		nMask |= SIR_GRAPHICS_FEATURE_MULTISAMPLE;
	}
	return nMask;
}


/**
 * Validate all queue dependencies before calling ID3D12CommandQueue::Wait.
 * The submitted values must come from one device-generation snapshot.
 * The destination queue must have room for the next fence signal, so a
 * rejected submission cannot enqueue waits without a matching signal.
 */
static inline sirGraphicsResult_t sirD3D12ValidateSubmissionDependencies( sirGraphicsQueue_t eQueue,
                                                                            const sirGraphicsCompletion_t *pWaits,
                                                                            uint32_t nWaitCount,
                                                                            const sirGraphicsCompletionSet_t& submitted ) noexcept
{
	if ( eQueue >= SIR_GRAPHICS_QUEUE_COUNT || nWaitCount > 64 || ( nWaitCount != 0 && pWaits == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( submitted.nDeviceGeneration == 0 ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( submitted.szQueueValues[eQueue] >= UINT64_MAX - 1 ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	for ( uint32_t iWait = 0; iWait < nWaitCount; ++iWait ) {
		const sirGraphicsCompletion_t& wait = pWaits[iWait];
		if ( wait.nDeviceGeneration != submitted.nDeviceGeneration || wait.eQueue >= SIR_GRAPHICS_QUEUE_COUNT ||
		     wait.nValue == 0 || wait.nValue > submitted.szQueueValues[wait.eQueue] ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

#endif // __SIRENGINE_SIRD3D12VALIDATION_H__
