/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/D3D9/SIRD3D9Copies.h>

#include <stdio.h>

static int s_nChecks = 0;

static int sirD3D9Require( bool bCondition, int nCode )
{
	++s_nChecks;
	if ( !bCondition ) {
		fprintf( stderr, "SIRD3D9CopyTests: check %d failed\n", s_nChecks );
		return nCode;
	}
	return 0;
}

static int sirD3D9TestBufferCopy( void )
{
	sirD3D9BufferCopy_t copy = {};
	copy.nSourceBytes = 1024;
	copy.nDestinationBytes = 1024;
	copy.nBytes = 256;

	int nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_SUCCESS, 10 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A copy that spans the whole source is valid.
	copy.nSourceOffset = 768;
	copy.nDestinationOffset = 0;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_SUCCESS, 11 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A null descriptor is rejected.
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, 12 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A zero-length copy has no work to perform and is rejected.
	copy.nSourceOffset = 0;
	copy.nDestinationOffset = 0;
	copy.nBytes = 0;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_INVALID_ARGUMENT, 13 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A copy from a buffer onto itself is rejected: D3D9 cannot hold a read
	// lock and a write lock on the same resource.
	copy.nBytes = 256;
	copy.bSameResource = true;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_INVALID_ARGUMENT, 14 );
	if ( nResult != 0 ) {
		return nResult;
	}
	copy.bSameResource = false;

	// A source range beyond the source buffer is rejected.
	copy.nSourceOffset = 900;
	copy.nBytes = 256;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_INVALID_ARGUMENT, 15 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A source offset beyond the end of the source buffer is rejected.
	copy.nSourceOffset = 1025;
	copy.nBytes = 256;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_INVALID_ARGUMENT, 16 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A destination range beyond the destination buffer is rejected.
	copy.nSourceOffset = 0;
	copy.nDestinationOffset = 800;
	copy.nBytes = 256;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_INVALID_ARGUMENT, 17 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A destination offset beyond the end of the destination buffer is
	// rejected.
	copy.nDestinationOffset = 1025;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_INVALID_ARGUMENT, 18 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A copy that exactly fills the destination is valid.
	copy.nDestinationOffset = 768;
	copy.nBytes = 256;
	nResult = sirD3D9Require( sirD3D9ValidateBufferCopy( &copy ) == SIR_GRAPHICS_SUCCESS, 19 );
	if ( nResult != 0 ) {
		return nResult;
	}

	return 0;
}

static int sirD3D9TestTextureTransfer( void )
{
	sirD3D9TextureTransfer_t transfer = {};
	transfer.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	transfer.nTextureWidth = 64;
	transfer.nTextureHeight = 32;
	transfer.nTextureDepth = 1;
	transfer.nArrayLayers = 1;
	transfer.nMipLevels = 6;
	transfer.nWidth = 16;
	transfer.nHeight = 8;
	transfer.nDepth = 1;
	transfer.nBytesPerRow = 16 * 4;
	transfer.nRowsPerImage = 8;
	transfer.nBufferBytes = 16 * 4 * 8;

	int nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_SUCCESS, 30 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// The RGBA8 format is equally transferable.
	transfer.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_SUCCESS, 31 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A null descriptor is rejected.
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, 32 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A zero-sized region is rejected.
	transfer.nWidth = 0;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 33 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nWidth = 16;

	// A mip level outside the texture is rejected.
	transfer.nMip = 6;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 34 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A layer outside the array is rejected.
	transfer.nMip = 0;
	transfer.nLayer = 1;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 35 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nLayer = 0;

	// A region that does not fit the addressed mip level is rejected. Mip 4
	// of a 64x32 texture is 4x2, so a 16x8 region does not fit.
	transfer.nMip = 4;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 36 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// The same region does fit mip 3, whose level is 8x4.
	transfer.nMip = 3;
	transfer.nWidth = 8;
	transfer.nHeight = 4;
	transfer.nBytesPerRow = 8 * 4;
	transfer.nRowsPerImage = 4;
	transfer.nBufferBytes = 8 * 4 * 4;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_SUCCESS, 37 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A region offset beyond the addressed level is rejected.
	transfer.nX = 5;
	transfer.nBytesPerRow = 8 * 4;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 38 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nX = 0;

	// A row pitch below the minimum row size is rejected.
	transfer.nBytesPerRow = 8 * 4 - 1;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 39 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A rows-per-image below the addressed height is rejected.
	transfer.nBytesPerRow = 8 * 4;
	transfer.nRowsPerImage = 3;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 40 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nRowsPerImage = 4;

	// A buffer that is too small for the addressed region is rejected.
	transfer.nBufferBytes = 8 * 4 * 4 - 1;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 41 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nBufferBytes = 8 * 4 * 4;

	// A buffer offset beyond the end of the buffer is rejected.
	transfer.nBufferOffset = transfer.nBufferBytes + 1;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 42 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nBufferOffset = 0;

	// A block-compressed format cannot be moved with UpdateSurface.
	transfer.eFormat = SIR_GRAPHICS_FORMAT_BC1_UNORM;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_UNSUPPORTED, 43 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A float format cannot be moved with UpdateSurface either.
	transfer.eFormat = SIR_GRAPHICS_FORMAT_RGBA16_FLOAT;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_UNSUPPORTED, 44 );
	if ( nResult != 0 ) {
		return nResult;
	}

	return 0;
}

static int sirD3D9TestTransferExtents( void )
{
	// A transfer that addresses a slice other than the first is rejected:
	// D3D9 exposes a surface only for the two-dimensional levels.
	sirD3D9TextureTransfer_t transfer = {};
	transfer.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	transfer.nTextureWidth = 64;
	transfer.nTextureHeight = 64;
	transfer.nTextureDepth = 1;
	transfer.nArrayLayers = 1;
	transfer.nMipLevels = 1;
	transfer.nWidth = 8;
	transfer.nHeight = 4;
	transfer.nDepth = 1;
	transfer.nBytesPerRow = 8 * 4;
	transfer.nRowsPerImage = 4;
	transfer.nBufferBytes = 8 * 4 * 4;

	int nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_SUCCESS, 50 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// A multi-slice transfer is rejected.
	transfer.nDepth = 3;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 51 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nDepth = 1;

	// A zero-slice transfer is rejected.
	transfer.nDepth = 0;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 52 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nDepth = 1;

	// A non-zero slice origin is rejected because only the first slice of a
	// surface is addressable.
	transfer.nZ = 1;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 57 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nZ = 0;

	// A buffer that cannot hold the addressed region is rejected.
	transfer.nBufferBytes = 8 * 4 * 4 - 1;
	nResult = sirD3D9Require( sirD3D9ValidateTextureTransfer( &transfer ) == SIR_GRAPHICS_INVALID_ARGUMENT, 58 );
	if ( nResult != 0 ) {
		return nResult;
	}
	transfer.nBufferBytes = 8 * 4 * 4;

	// The row and row-count helpers report zero for a format D3D9 cannot
	// transfer.
	nResult = sirD3D9Require( sirD3D9TransferRowBytes( SIR_GRAPHICS_FORMAT_BC3_UNORM, 16 ) == 0, 53 );
	if ( nResult != 0 ) {
		return nResult;
	}
	nResult = sirD3D9Require( sirD3D9TransferLevelRows( SIR_GRAPHICS_FORMAT_BC3_UNORM, 16 ) == 0, 54 );
	if ( nResult != 0 ) {
		return nResult;
	}

	// An unorm eight bit format reports four bytes per pixel.
	nResult = sirD3D9Require( sirD3D9TransferRowBytes( SIR_GRAPHICS_FORMAT_BGRA8_UNORM, 16 ) == 64, 55 );
	if ( nResult != 0 ) {
		return nResult;
	}
	nResult = sirD3D9Require( sirD3D9TransferLevelRows( SIR_GRAPHICS_FORMAT_BGRA8_UNORM, 16 ) == 16, 56 );
	if ( nResult != 0 ) {
		return nResult;
	}

	return 0;
}

int main( void )
{
	int nResult = sirD3D9TestBufferCopy();
	if ( nResult != 0 ) {
		return nResult;
	}
	nResult = sirD3D9TestTextureTransfer();
	if ( nResult != 0 ) {
		return nResult;
	}
	nResult = sirD3D9TestTransferExtents();
	if ( nResult != 0 ) {
		return nResult;
	}
	printf( "SIRD3D9CopyTests: %d checks passed\n", s_nChecks );
	return 0;
}
