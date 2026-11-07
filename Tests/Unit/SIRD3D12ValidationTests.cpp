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

#include <Renderer/Drivers/D3D12/SIRD3D12Validation.h>
#include <math.h>
#include <stdio.h>

static int sirTestHeapUsage( void ) noexcept
{
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = 4096;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	if ( sirD3D12ValidateBufferHeapUsage( desc ) != SIR_GRAPHICS_SUCCESS ) {
		return 1;
	}
	desc.nUsage |= SIR_GRAPHICS_USAGE_COPY_SOURCE;
	if ( sirD3D12ValidateBufferHeapUsage( desc ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 2;
	}
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	if ( sirD3D12ValidateBufferHeapUsage( desc ) != SIR_GRAPHICS_SUCCESS ) {
		return 3;
	}
	desc.nUsage |= SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	if ( sirD3D12ValidateBufferHeapUsage( desc ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 4;
	}
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_UPLOAD;
	if ( sirD3D12ValidateBufferHeapUsage( desc ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 5;
	}
	desc.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	if ( sirD3D12ValidateBufferHeapUsage( desc ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 6;
	}
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_STORAGE;
	if ( sirD3D12ValidateBufferHeapUsage( desc ) != SIR_GRAPHICS_SUCCESS ) {
		return 7;
	}
	return 0;
}

static int sirTestTextureCopy( void ) noexcept
{
	sirGraphicsBufferTextureCopy_t copy = {};
	copy.nWidth = 16;
	copy.nHeight = 16;
	copy.nBytesPerRow = 256;
	copy.nRowsPerImage = 16;
	copy.nDepth = 1;
	sirD3D12CopyLimits_t limits = { 16, 16, 4, 4096 };
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_SUCCESS ) {
		return 8;
	}
	copy.nZ = 1;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 9;
	}
	copy.nZ = 0;
	copy.nBufferOffset = 512;
	limits.nBufferBytes = 4608;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_SUCCESS ) {
		return 10;
	}
	copy.nBufferOffset = 256;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 11;
	}
	copy.nBufferOffset = 512;
	limits.nBufferBytes = 4607;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 12;
	}
	copy.nBufferOffset = 0;
	limits.nBufferBytes = 4096;
	copy.nBytesPerRow = 255;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 13;
	}
	copy.nBytesPerRow = 256;
	copy.nRowsPerImage = 15;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 14;
	}
	copy.nRowsPerImage = 16;
	copy.nX = 1;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 15;
	}
	copy.nX = 0;
	copy.nWidth = 0;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 16;
	}
	copy.nWidth = 16;
	copy.nBytesPerRow = 256;
	limits.nBufferBytes = UINT64_MAX;
	copy.nBufferOffset = UINT64_MAX - 511;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 17;
	}
	return 0;
}

static int sirTestViewport( void ) noexcept
{
	sirGraphicsViewport_t viewport = {};
	viewport.fWidth = 16.0f;
	viewport.fHeight = 16.0f;
	viewport.fMaximumDepth = 1.0f;
	sirGraphicsRect_t scissor = { 0, 0, 16, 16 };
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_SUCCESS ) {
		return 18;
	}
	viewport.fX = HUGE_VALF;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 19;
	}
	viewport.fX = 0.0f;
	viewport.fMinimumDepth = NAN;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 20;
	}
	viewport.fMinimumDepth = 0.0f;
	viewport.fHeight = -1.0f;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 21;
	}
	viewport.fHeight = 16.0f;
	viewport.fMaximumDepth = -1.0f;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 22;
	}
	viewport.fMaximumDepth = 1.0f;
	scissor.nRight = scissor.nLeft;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 23;
	}
	return 0;
}

int main( void )
{
	const int nHeap = sirTestHeapUsage();
	const int nCopy = sirTestTextureCopy();
	const int nViewport = sirTestViewport();
	if ( nHeap != 0 || nCopy != 0 || nViewport != 0 ) {
		printf( "D3D12 portable validation failed: heap=%d copy=%d viewport=%d\n", nHeap, nCopy, nViewport );
		return 1;
	}
	printf( "D3D12 portable validation: 23 assertions PASS\n" );
	return 0;
}
