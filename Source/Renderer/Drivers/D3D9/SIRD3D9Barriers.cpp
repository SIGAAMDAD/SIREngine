/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/D3D9/SIRD3D9Barriers.h>

static bool sirD3D9StateAllowed( sirGraphicsHandleType_t eType, sirGraphicsState_t eState ) noexcept
{
	if ( eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
		return eState == SIR_GRAPHICS_STATE_UNDEFINED || eState == SIR_GRAPHICS_STATE_COMMON || eState == SIR_GRAPHICS_STATE_VERTEX_INDEX ||
		       eState == SIR_GRAPHICS_STATE_COPY_SOURCE || eState == SIR_GRAPHICS_STATE_COPY_DESTINATION;
	}
	return eState == SIR_GRAPHICS_STATE_UNDEFINED || eState == SIR_GRAPHICS_STATE_COMMON || eState == SIR_GRAPHICS_STATE_SHADER_READ ||
	       eState == SIR_GRAPHICS_STATE_COPY_SOURCE || eState == SIR_GRAPHICS_STATE_COPY_DESTINATION ||
	       eState == SIR_GRAPHICS_STATE_COLOR_ATTACHMENT || eState == SIR_GRAPHICS_STATE_PRESENT ||
	       eState == SIR_GRAPHICS_STATE_DEPTH_READ || eState == SIR_GRAPHICS_STATE_DEPTH_WRITE;
}

sirGraphicsResult_t sirD3D9ValidateSerialBarrier( const sirGraphicsBarrier_t *pBarrier, uint32_t nMipLevels, uint32_t nArrayLayers,
                                                  bool *pUnbind ) noexcept
{
	if ( pBarrier == nullptr || pUnbind == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pUnbind = false;
	if ( pBarrier->eType != SIR_GRAPHICS_BARRIER_TRANSITION ||
	     ( pBarrier->hResource.eType != SIR_GRAPHICS_HANDLE_BUFFER && pBarrier->hResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE ) ||
	     pBarrier->hResource.nGeneration == 0 || pBarrier->hResource.nDeviceGeneration == 0 ||
	     pBarrier->hAliasedResource.eType != SIR_GRAPHICS_HANDLE_NONE ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !sirD3D9StateAllowed( pBarrier->hResource.eType, pBarrier->eBefore ) ||
	     !sirD3D9StateAllowed( pBarrier->hResource.eType, pBarrier->eAfter ) || pBarrier->eAfter == SIR_GRAPHICS_STATE_UNDEFINED ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pBarrier->hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
		if ( pBarrier->nBaseMip != 0 || pBarrier->nMipCount != 0 || pBarrier->nBaseLayer != 0 || pBarrier->nLayerCount != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( !sirGraphicsTextureBarrierRangeValid( *pBarrier, nMipLevels, nArrayLayers ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsState_t eAfter = pBarrier->eAfter;
	*pUnbind = eAfter == SIR_GRAPHICS_STATE_COPY_DESTINATION || eAfter == SIR_GRAPHICS_STATE_COLOR_ATTACHMENT ||
	           eAfter == SIR_GRAPHICS_STATE_DEPTH_WRITE || eAfter == SIR_GRAPHICS_STATE_PRESENT;
	return SIR_GRAPHICS_SUCCESS;
}