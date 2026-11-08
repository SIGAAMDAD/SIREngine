/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/D3D9/SIRD3D9Mapping.h>

int main( void )
{
	sirGraphicsABIBufferRange_t range = {};
	range.nSize = sizeof( range );
	range.hBuffer = { 3, 1, 1, SIR_GRAPHICS_HANDLE_BUFFER };
	range.nOffsetBytes = 16;
	range.nBytes = 32;
	if ( sirD3D9ValidateBufferMap( &range, 48, SIR_GRAPHICS_MEMORY_CPU_UPLOAD, false ) != SIR_GRAPHICS_SUCCESS ) {
		return 1;
	}
	if ( sirD3D9ValidateBufferMap( &range, 48, SIR_GRAPHICS_MEMORY_CPU_UPLOAD, true ) != SIR_GRAPHICS_BUSY ) {
		return 2;
	}
	if ( sirD3D9ValidateBufferMap( &range, 48, SIR_GRAPHICS_MEMORY_GPU_LOCAL, false ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 3;
	}
	range.nOffsetBytes = 17;
	if ( sirD3D9ValidateBufferMap( &range, 48, SIR_GRAPHICS_MEMORY_CPU_UPLOAD, false ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 4;
	}
	range.nOffsetBytes = 0;
	range.nBytes = 0;
	if ( sirD3D9ValidateBufferMap( &range, 48, SIR_GRAPHICS_MEMORY_CPU_UPLOAD, false ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 5;
	}
	range.nBytes = UINT64_MAX;
	if ( sirD3D9ValidateBufferMap( &range, UINT64_MAX, SIR_GRAPHICS_MEMORY_CPU_UPLOAD, false ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 6;
	}
	range.nBytes = 8;
	range.nReserved = 1;
	if ( sirD3D9ValidateBufferMap( &range, 48, SIR_GRAPHICS_MEMORY_CPU_UPLOAD, false ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 7;
	}
	range.nReserved = 0;
	range.hBuffer.nDeviceGeneration = 0;
	if ( sirD3D9ValidateBufferMap( &range, 48, SIR_GRAPHICS_MEMORY_CPU_UPLOAD, false ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 8;
	}
	return 0;
}