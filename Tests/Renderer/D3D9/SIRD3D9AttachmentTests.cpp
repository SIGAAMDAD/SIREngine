/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/D3D9/SIRD3D9Attachments.h>

int main( void )
{
	sirGraphicsTextureDesc_t texture = {};
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.nSampleCount = 1;
	texture.nMipLevels = 3;
	texture.nArrayLayers = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	texture.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	sirGraphicsViewDesc_t view = {};
	view.hResource = { 2, 1, 1, SIR_GRAPHICS_HANDLE_TEXTURE };
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.eFormat = texture.eFormat;
	if ( sirD3D9ValidateColorAttachmentView( &texture, &view ) != SIR_GRAPHICS_SUCCESS ) {
		return 1;
	}
	view.nBaseMip = 1;
	if ( sirD3D9ValidateColorAttachmentView( &texture, &view ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 2;
	}
	view.nBaseMip = 0;
	view.nLayers = 2;
	if ( sirD3D9ValidateColorAttachmentView( &texture, &view ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 3;
	}
	view.nLayers = 1;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	if ( sirD3D9ValidateColorAttachmentView( &texture, &view ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 4;
	}
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	if ( sirD3D9ValidateColorAttachmentView( &texture, &view ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 5;
	}
	texture.nUsage |= SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	if ( sirD3D9ValidateColorAttachmentView( &texture, &view ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 6;
	}
	return 0;
}