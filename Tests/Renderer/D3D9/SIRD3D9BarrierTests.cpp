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

int main( void )
{
	sirGraphicsBarrier_t barrier = {};
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.hResource = { 2, 3, 1, SIR_GRAPHICS_HANDLE_TEXTURE };
	barrier.eBefore = SIR_GRAPHICS_STATE_SHADER_READ;
	barrier.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	barrier.nMipCount = 1;
	barrier.nLayerCount = 1;
	bool bUnbind = false;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 4, 1, &bUnbind ) != SIR_GRAPHICS_SUCCESS || !bUnbind ) {
		return 1;
	}
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 4, 1, &bUnbind ) != SIR_GRAPHICS_SUCCESS || bUnbind ) {
		return 2;
	}
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 4, 1, &bUnbind ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 3;
	}
	barrier.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	barrier.nBaseMip = 4;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 4, 1, &bUnbind ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 4;
	}
	barrier.nBaseMip = 0;
	barrier.eType = SIR_GRAPHICS_BARRIER_ALIAS;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 4, 1, &bUnbind ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 5;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.hResource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	barrier.eBefore = SIR_GRAPHICS_STATE_VERTEX_INDEX;
	barrier.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 0, 0, &bUnbind ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 6;
	}
	barrier.nMipCount = 0;
	barrier.nLayerCount = 0;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 0, 0, &bUnbind ) != SIR_GRAPHICS_SUCCESS || !bUnbind ) {
		return 7;
	}
	barrier.hResource.nDeviceGeneration = 0;
	if ( sirD3D9ValidateSerialBarrier( &barrier, 0, 0, &bUnbind ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 8;
	}
	return 0;
}