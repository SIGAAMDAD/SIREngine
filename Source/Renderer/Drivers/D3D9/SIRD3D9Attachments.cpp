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

sirGraphicsResult_t sirD3D9ValidateColorAttachmentView( const sirGraphicsTextureDesc_t *pTexture,
                                                        const sirGraphicsViewDesc_t *pView ) noexcept
{
	if ( pTexture == nullptr || pView == nullptr || pView->hResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE ||
	     pView->nUsage != SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT || pView->eFormat != pTexture->eFormat ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pTexture->eDimension != SIR_GRAPHICS_TEXTURE_2D || pTexture->nSampleCount != 1 ||
	     ( pTexture->nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pView->nBaseMip != 0 || pView->nMipLevels != 1 || pView->nBaseLayer != 0 || pView->nLayers != 1 || pTexture->nMipLevels == 0 ||
	     pTexture->nArrayLayers != 1 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}