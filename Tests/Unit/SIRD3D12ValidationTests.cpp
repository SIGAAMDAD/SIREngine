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

static int sirTestBindingLayouts( void ) noexcept
{
	if ( sirD3D12ValidateBindingLayout( nullptr, 0, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_SUCCESS ||
	     sirD3D12ValidateBindingLayout( nullptr, 1, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 24;
	}
	sirGraphicsBindingDesc_t bindings[33] = {};
	bindings[0].nBinding = 0;
	bindings[0].nArrayCount = 4;
	bindings[0].nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	bindings[0].eType = SIR_GRAPHICS_BINDING_STORAGE_BUFFER;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_SUCCESS ) {
		return 25;
	}
	bindings[0].nArrayCount = 0;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 26;
	}
	bindings[0].nArrayCount = 129;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 27;
	}
	bindings[0].nArrayCount = 4;
	bindings[0].nBinding = UINT32_MAX - 2;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 28;
	}
	bindings[0].nBinding = 2;
	bindings[1] = bindings[0];
	bindings[1].nBinding = 4;
	bindings[1].eType = SIR_GRAPHICS_BINDING_CONSTANT_BUFFER;
	if ( sirD3D12ValidateBindingLayout( bindings, 2, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 29;
	}
	bindings[1].nBinding = 6;
	if ( sirD3D12ValidateBindingLayout( bindings, 2, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_SUCCESS ) {
		return 30;
	}
	bindings[1].nSpace = 1;
	bindings[1].nBinding = 2;
	if ( sirD3D12ValidateBindingLayout( bindings, 2, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_SUCCESS ) {
		return 31;
	}
	bindings[1].nSpace = 0;
	bindings[1].nBinding = 6;
	bindings[0].nArrayCount = 120;
	bindings[1].nArrayCount = 16;
	bindings[1].nBinding = 122;
	if ( sirD3D12ValidateBindingLayout( bindings, 2, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ) {
		return 32;
	}
	bindings[0].nArrayCount = 4;
	bindings[1].nArrayCount = 4;
	if ( sirD3D12ValidateBindingLayout( bindings, 33, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 33;
	}
	bindings[0].eType = ( sirGraphicsBindingType_t )99;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_COMPUTE ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 34;
	}
	bindings[0].eType = SIR_GRAPHICS_BINDING_STORAGE_BUFFER;
	bindings[0].nShaderStageMask = 0;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_GRAPHICS ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 35;
	}
	bindings[0].nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_GRAPHICS ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 36;
	}
	bindings[0].nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_GRAPHICS ) != SIR_GRAPHICS_SUCCESS ) {
		return 37;
	}
	if ( sirD3D12ValidateBindingLayout( bindings, 1, SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 38;
	}
	return 0;
}

static int sirTestExternalAdmission( void ) noexcept
{
	if ( sirD3D12CanReserveExternal( 0, 0, 4096, true ) || !sirD3D12CanReserveExternal( 0, 0, 4096, false ) ||
	     sirD3D12CanReserveExternal( 0, 4096, 0, false ) ) {
		return 39;
	}
	if ( !sirD3D12CanReserveExternal( 20, 30, 10, true ) || sirD3D12CanReserveExternal( 21, 30, 10, true ) ||
	     sirD3D12CanReserveExternal( 31, 30, 1, true ) ) {
		return 40;
	}
	if ( sirD3D12CanReserveExternal( UINT64_MAX - 1, 0, 3, false ) || !sirD3D12CanReserveExternal( UINT64_MAX - 4, 0, 4, false ) ) {
		return 41;
	}
	return 0;
}

static int sirTestOptionalFeatures( void ) noexcept
{
	if ( sirD3D12NegotiatedOptionalFeatures( false, false ) != 0 ||
	     sirD3D12NegotiatedOptionalFeatures( true, false ) != SIR_GRAPHICS_FEATURE_ENHANCED_BARRIERS ||
	     sirD3D12NegotiatedOptionalFeatures( false, true ) != SIR_GRAPHICS_FEATURE_MULTISAMPLE ||
	     sirD3D12NegotiatedOptionalFeatures( true, true ) !=
	         ( SIR_GRAPHICS_FEATURE_ENHANCED_BARRIERS | SIR_GRAPHICS_FEATURE_MULTISAMPLE ) ) {
		return 42;
	}
	return 0;
}

static int sirTestValidatorBranches( void ) noexcept
{
	sirGraphicsBufferTextureCopy_t copy = {};
	copy.nWidth = 16;
	copy.nHeight = 16;
	copy.nDepth = 1;
	copy.nRowsPerImage = 16;
	copy.nBytesPerRow = 256;
	sirD3D12CopyLimits_t limits = { 16, 16, 4, 4096 };
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_SUCCESS ) {
		return 43;
	}
	copy.nHeight = 0;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 44;
	}
	copy.nHeight = 16;
	copy.nX = 17;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 45;
	}
	copy.nX = 0;
	copy.nY = 17;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 46;
	}
	copy.nY = 0;
	copy.nWidth = 17;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 47;
	}
	copy.nWidth = 16;
	copy.nHeight = 17;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 48;
	}
	copy.nHeight = 16;
	limits.nTexelBytes = 0;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 49;
	}
	limits.nTexelBytes = 4;
	copy.nBytesPerRow = 0;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 50;
	}
	copy.nBytesPerRow = 256;
	copy.nWidth = 16;
	limits.nTexelBytes = 17;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 51;
	}
	limits.nTexelBytes = 4;
	copy.nBufferOffset = 4608;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 52;
	}
	copy.nBufferOffset = 512;
	if ( sirD3D12ValidateCopyRegion( copy, limits ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 53;
	}
	copy.nBufferOffset = 0;
	sirGraphicsViewport_t viewport = {};
	viewport.fWidth = 16.0f;
	viewport.fHeight = 16.0f;
	viewport.fMaximumDepth = 1.0f;
	sirGraphicsRect_t scissor = { 0, 0, 16, 16 };
	viewport.fY = HUGE_VALF;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 54;
	}
	viewport.fY = 0.0f;
	viewport.fWidth = 0.0f;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 55;
	}
	viewport.fWidth = 16.0f;
	viewport.fHeight = NAN;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 56;
	}
	viewport.fHeight = 16.0f;
	viewport.fMaximumDepth = 2.0f;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 57;
	}
	viewport.fMaximumDepth = 1.0f;
	viewport.fMinimumDepth = 1.0f;
	viewport.fMaximumDepth = 0.0f;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 58;
	}
	viewport.fMinimumDepth = 0.0f;
	viewport.fMaximumDepth = 1.0f;
	scissor.nBottom = scissor.nTop;
	if ( sirD3D12ValidateViewport( viewport, scissor ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 59;
	}
	return 0;
}

static int sirTestDXILContainer( void ) noexcept
{
	uint8_t szDXIL[72] = {};
	szDXIL[0] = 'D';
	szDXIL[1] = 'X';
	szDXIL[2] = 'B';
	szDXIL[3] = 'C';
	szDXIL[20] = 1;
	szDXIL[24] = sizeof( szDXIL );
	szDXIL[28] = 1;
	szDXIL[32] = 36;
	szDXIL[36] = 'D';
	szDXIL[37] = 'X';
	szDXIL[38] = 'I';
	szDXIL[39] = 'L';
	szDXIL[40] = 28;
	if ( !sirD3D12ValidateDXILContainer( szDXIL, sizeof( szDXIL ) ) ) {
		return 60;
	}
	if ( sirD3D12ValidateDXILContainer( nullptr, sizeof( szDXIL ) ) ||
	     sirD3D12ValidateDXILContainer( szDXIL, 39 ) ) {
		return 61;
	}
	szDXIL[24] = 71;
	if ( sirD3D12ValidateDXILContainer( szDXIL, sizeof( szDXIL ) ) ) {
		return 62;
	}
	szDXIL[24] = sizeof( szDXIL );
	szDXIL[28] = 16;
	if ( sirD3D12ValidateDXILContainer( szDXIL, sizeof( szDXIL ) ) ) {
		return 63;
	}
	szDXIL[28] = 1;
	szDXIL[32] = 71;
	if ( sirD3D12ValidateDXILContainer( szDXIL, sizeof( szDXIL ) ) ) {
		return 64;
	}
	szDXIL[32] = 36;
	szDXIL[40] = 40;
	if ( sirD3D12ValidateDXILContainer( szDXIL, sizeof( szDXIL ) ) ) {
		return 65;
	}
	szDXIL[40] = 28;
	szDXIL[36] = 'A';
	if ( sirD3D12ValidateDXILContainer( szDXIL, sizeof( szDXIL ) ) ) {
		return 66;
	}
	return 0;
}

int main( void )
{
	const int nHeap = sirTestHeapUsage();
	const int nCopy = sirTestTextureCopy();
	const int nViewport = sirTestViewport();
	const int nBindings = sirTestBindingLayouts();
	const int nExternal = sirTestExternalAdmission();
	const int nFeatures = sirTestOptionalFeatures();
	const int nExtra = sirTestValidatorBranches();
	const int nDXIL = sirTestDXILContainer();
	if ( nHeap != 0 || nCopy != 0 || nViewport != 0 || nBindings != 0 || nExternal != 0 || nFeatures != 0 || nExtra != 0 || nDXIL != 0 ) {
		printf( "D3D12 portable validation failed: heap=%d copy=%d viewport=%d bindings=%d external=%d features=%d extra=%d\n", nHeap,
		        nCopy, nViewport, nBindings, nExternal, nFeatures, nExtra );
		return 1;
	}
	if ( nDXIL != 0 ) {
		printf( "D3D12 DXIL container validation failed: %d\n", nDXIL );
		return 1;
	}
	printf( "D3D12 portable validation suites PASS\n" );
	return 0;
}
