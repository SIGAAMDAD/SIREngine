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

#include <Renderer/Drivers/D3D11/SIRD3D11TextureValidation.h>
#include <stdio.h>
#include <stdint.h>

static uint32_t s_nChecks = 0;
static uint32_t s_nFailures = 0;

static void sirD3D11TextureCheck( bool bExpected, const char *pszCase ) noexcept
{
	++s_nChecks;
	if ( !bExpected ) {
		++s_nFailures;
		fprintf( stderr, "D3D11 texture validation: %s\n", pszCase );
	}
}

int main( void )
{
	sirD3D11FormatSupport_t support = {};
	support.nSize = sizeof( support );
	support.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	support.nDimensionMask = 0xF;
	support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_STORAGE |
	                           SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	support.nSampleCountMask = 0x7;
	sirGraphicsTextureDesc_t desc = {};
	desc.nWidth = 16;
	desc.nHeight = 8;
	desc.nDepth = 1;
	desc.nArrayLayers = 1;
	desc.nMipLevels = 1;
	desc.nSampleCount = 1;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirD3D11TexturePlan_t plan = {};
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "basic color allocation" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 512, "exact 16x8 rgba8" );
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 8, 520, &plan ) == SIR_GRAPHICS_SUCCESS, "bounded remaining capacity" );
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 9, 520, &plan ) == SIR_GRAPHICS_OUT_OF_MEMORY, "budget exhausted" );
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, UINT64_MAX, 0, &plan ) == SIR_GRAPHICS_OUT_OF_MEMORY,
	                      "committed arithmetic overflow" );
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "missing output" );
	sirD3D11TextureCheck( sirD3D11PlanTexture( nullptr, &support, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT, "missing descriptor" );
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, nullptr, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT, "missing format probe" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 0, "cleared failed output" );
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED,
	                      "upload needs native map policy" );
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED,
	                      "readback needs staging policy" );
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_UPLOAD;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED,
	                      "GPU_UPLOAD not a D3D11 resource type" );
	desc.eMemory = SIR_GRAPHICS_MEMORY_TRANSIENT;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED, "unadmitted transient backing" );
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	desc.nMipLevels = 5;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "complete mip chain" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 684, "exact mip chain bytes" );
	desc.nMipLevels = 1;
	desc.nSampleCount = 4;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "native multisample texture plan" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 2048, "sample count capacity charge" );
	desc.nSampleCount = 1;
	desc.nArrayLayers = 6;
	desc.nHeight = 16;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "cube texture allocation" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 6144, "cube logical capacity" );
	desc.nSampleCount = 2;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "multisample cube forbidden" );
	desc.nSampleCount = 1;
	desc.nArrayLayers = 1;
	desc.nWidth = 8;
	desc.nHeight = 8;
	desc.nDepth = 4;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "3D texture allocation" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 1024, "3D depth accounted" );
	desc.nDepth = 1;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nWidth = 16;
	desc.nHeight = 8;
	desc.eFormat = SIR_GRAPHICS_FORMAT_BC1_UNORM;
	support.eFormat = desc.eFormat;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT, "BC cannot render target" );
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "BC1 texture allocation" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 64, "BC1 block bytes" );
	desc.nWidth = 5;
	desc.nHeight = 5;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "BC1 partial blocks" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 32, "BC1 5x5 rounds to four 4x4 blocks" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_BC3_UNORM;
	support.eFormat = desc.eFormat;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "BC3 texture allocation" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 64, "BC3 blocks are 16 bytes" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA32_FLOAT;
	support.eFormat = desc.eFormat;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "RGBA32 float storage" );
	sirD3D11TextureCheck( plan.nLogicalBytes == 400, "RGBA32 float byte count" );
	desc.nWidth = UINT32_MAX;
	desc.nHeight = UINT32_MAX;
	desc.nDepth = 1;
	desc.nArrayLayers = UINT32_MAX;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_OUT_OF_MEMORY,
	                      "oversized texture arithmetic" );
	desc.nWidth = 16;
	desc.nHeight = 8;
	desc.nArrayLayers = 1;
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	support.eFormat = desc.eFormat;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirGraphicsViewDesc_t view = {};
	view.eFormat = desc.eFormat;
	view.nBaseMip = 0;
	view.nMipLevels = 1;
	view.nBaseLayer = 0;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "basic SRV" );
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "basic RTV" );
	view.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_UNSUPPORTED, "unadvertised storage" );
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "multi-usage view rejected" );
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	view.eFormat = SIR_GRAPHICS_FORMAT_R8_UNORM;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_UNSUPPORTED, "wrong view format" );
	view.eFormat = desc.eFormat;
	view.nMipLevels = 0;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero mip range" );
	view.nMipLevels = 1;
	view.nLayers = 0;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero layer range" );
	view.nLayers = 1;
	view.nBaseMip = 1;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "mip out of range" );
	view.nBaseMip = 0;
	view.nBaseLayer = 1;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "layer out of range" );
	view.nBaseLayer = 0;
	view.nBufferOffsetBytes = 32;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "buffer offsets disallowed on textures" );
	view.nBufferOffsetBytes = 0;
	view.nBufferSizeBytes = 16;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "buffer ranges disallowed on textures" );
	view.nBufferSizeBytes = 0;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( nullptr, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "missing view" );
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "missing parent" );
	desc.nArrayLayers = 12;
	desc.nWidth = 16;
	desc.nHeight = 16;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
	view.nLayers = 6;
	view.nBaseLayer = 1;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "unaligned cube face group" );
	view.nBaseLayer = 0;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "first cube array view" );
	view.nBaseLayer = 6;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "second cube array view" );
	view.nLayers = 5;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "partial cube rejected" );
	view.nLayers = 1;
	view.nBaseLayer = 0;
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "cube render target face" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	desc.nArrayLayers = 1;
	desc.nDepth = 4;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "3D sampled view" );
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_UNSUPPORTED,
	                      "3D render target slice not yet represented" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nDepth = 1;
	desc.nSampleCount = 2;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "MSAA SRV" );
	view.nMipLevels = 2;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "MSAA mip range" );
	// Exhaust the native format byte-width switch through the complete shared
	// D3D11 format set; depth and block-compressed variants use legal usages.
	const sirGraphicsFormat_t szFormats[] = {
	    SIR_GRAPHICS_FORMAT_R8_UNORM,          SIR_GRAPHICS_FORMAT_RG8_UNORM,    SIR_GRAPHICS_FORMAT_R16_FLOAT,
	    SIR_GRAPHICS_FORMAT_D16_UNORM,         SIR_GRAPHICS_FORMAT_RGBA8_UNORM,  SIR_GRAPHICS_FORMAT_BGRA8_UNORM,
	    SIR_GRAPHICS_FORMAT_RGBA8_SRGB,        SIR_GRAPHICS_FORMAT_R32_FLOAT,    SIR_GRAPHICS_FORMAT_R32_UINT,
	    SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT, SIR_GRAPHICS_FORMAT_D32_FLOAT,    SIR_GRAPHICS_FORMAT_RGBA16_FLOAT,
	    SIR_GRAPHICS_FORMAT_RG32_FLOAT,        SIR_GRAPHICS_FORMAT_RGBA32_FLOAT, SIR_GRAPHICS_FORMAT_BC1_UNORM,
	    SIR_GRAPHICS_FORMAT_BC3_UNORM,         SIR_GRAPHICS_FORMAT_BC5_UNORM,    SIR_GRAPHICS_FORMAT_BC7_UNORM };
	for ( const sirGraphicsFormat_t eFormat : szFormats ) {
		desc = {};
		desc.nWidth = 16;
		desc.nHeight = 16;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1;
		desc.nSampleCount = 1;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
		desc.eFormat = eFormat;
		const bool bDepth = eFormat == SIR_GRAPHICS_FORMAT_D16_UNORM || eFormat == SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ||
		                    eFormat == SIR_GRAPHICS_FORMAT_D32_FLOAT;
		desc.nUsage = bDepth ? SIR_GRAPHICS_USAGE_DEPTH_STENCIL : SIR_GRAPHICS_USAGE_SAMPLED;
		support.eFormat = eFormat;
		support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_DEPTH_STENCIL | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
		sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS, "per-format logical capacity" );
		sirD3D11TextureCheck( plan.nLogicalBytes > 0, "per-format positive capacity" );
	}
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	support.eFormat = desc.eFormat;
	desc.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "non-depth texture cannot claim depth" );
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.nMipLevels = 2;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 5, 4, &plan ) == SIR_GRAPHICS_OUT_OF_MEMORY,
	                      "existing over-budget accounting" );
	desc.nMipLevels = 1;
	desc.eFormat = SIR_GRAPHICS_FORMAT_BC5_UNORM;
	support.eFormat = desc.eFormat;
	desc.nSampleCount = 2;
	sirD3D11TextureCheck( sirD3D11PlanTexture( &desc, &support, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	                      "BC sampled MSAA rejected" );
	desc.nSampleCount = 1;
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	support.eFormat = desc.eFormat;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	view = {};
	view.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	view.eFormat = desc.eFormat;
	view.nMipLevels = 1;
	view.nLayers = 1;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "UAV view admission" );
	desc.nMipLevels = 2;
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	view.nMipLevels = 2;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "render target is one mip" );
	view.nMipLevels = 1;
	view.nBaseLayer = 1;
	desc.nArrayLayers = 2;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "array view subset" );
	view.nLayers = 2;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "array view range overflow" );
	desc.nArrayLayers = 1;
	view.nLayers = 1;
	view.nBaseLayer = 0;
	desc.nSampleCount = 2;
	view.nBaseMip = 1;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "MSAA base mip" );
	view.nBaseMip = 0;
	view.nMipLevels = 2;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "MSAA many mips" );
	view.nMipLevels = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "MSAA UAV rejected" );
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.nSampleCount = 1;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
	desc.nArrayLayers = 12;
	desc.nHeight = desc.nWidth;
	view.nLayers = 6;
	view.nBaseLayer = 0;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "cube valid face subset" );
	view.nLayers = 7;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "cube partial face subset" );
	view.nLayers = 6;
	view.nBaseLayer = 7;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_INVALID_ARGUMENT, "cube face subset outside array" );
	view.nBaseLayer = 0;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nArrayLayers = 1;
	view.nLayers = 1;
	desc.nSampleCount = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	desc.nUsage |= SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	sirD3D11TextureCheck( sirD3D11ValidateTextureView( &view, &desc ) == SIR_GRAPHICS_SUCCESS, "depth view range on admitted texture" );
	printf( "D3D11 texture validation: %u assertions, %u failures\n", s_nChecks, s_nFailures );
	return s_nFailures == 0 ? 0 : 1;
}
