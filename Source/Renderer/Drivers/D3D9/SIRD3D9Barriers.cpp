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

#include <Renderer/Drivers/D3D9/SIRD3D9Barriers.h>

static bool sirD3D9StateAllowed( SIRGraphicsHandleType eType, SIRGraphicsState eState ) noexcept
{
	if ( eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return eState == SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COMMON || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_VERTEX_INDEX ||
		       eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION;
	}
	return eState == SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COMMON || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ ||
	       eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION ||
	       eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT ||
	       eState == SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE;
}

SIRGraphicsResult sirD3D9ValidateSerialBarrier( const sirGraphicsBarrier_t *pBarrier, uint32_t nMipLevels, uint32_t nArrayLayers,
                                                  bool *pUnbind ) noexcept
{
	if ( pBarrier == nullptr || pUnbind == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pUnbind = false;
	if ( pBarrier->eType != SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION ||
	     ( pBarrier->hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER && pBarrier->hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) ||
	     pBarrier->hResource.nGeneration == 0 || pBarrier->hResource.nDeviceGeneration == 0 ||
	     pBarrier->hAliasedResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_NONE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !sirD3D9StateAllowed( pBarrier->hResource.eType, pBarrier->eBefore ) ||
	     !sirD3D9StateAllowed( pBarrier->hResource.eType, pBarrier->eAfter ) || pBarrier->eAfter == SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pBarrier->hResource.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		if ( pBarrier->nBaseMip != 0 || pBarrier->nMipCount != 0 || pBarrier->nBaseLayer != 0 || pBarrier->nLayerCount != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( !sirGraphicsTextureBarrierRangeValid( *pBarrier, nMipLevels, nArrayLayers ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const SIRGraphicsState eAfter = pBarrier->eAfter;
	*pUnbind = eAfter == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION || eAfter == SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT ||
	           eAfter == SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE || eAfter == SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}