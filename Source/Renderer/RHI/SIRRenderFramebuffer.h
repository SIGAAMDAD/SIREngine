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

#ifndef __SIR_RENDERFRAMEBUFFER_H__
#define __SIR_RENDERFRAMEBUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>
#include <Renderer/RHI/SIRRenderResource.h>

// hView and hResolveView are borrowed views. A framebuffer owns no texture
// memory. Load/store operations are per attachment, not per texture.
typedef struct sirRenderAttachmentDesc_s
{
	sirGraphicsHandle_t hView;
	sirGraphicsHandle_t hResolveView;
	SIRRenderLoadOp eLoad;
	SIRRenderStoreOp eStore;
	float szClearColor[4];
	uint32_t nAspectMask;
} sirRenderAttachmentDesc_t;

// Depth and stencil have distinct load/store semantics on modern APIs.
typedef struct sirRenderDepthStencilAttachmentDesc_s
{
	sirGraphicsHandle_t hView;
	SIRRenderLoadOp eDepthLoad;
	SIRRenderStoreOp eDepthStore;
	SIRRenderLoadOp eStencilLoad;
	SIRRenderStoreOp eStencilStore;
	float fClearDepth;
	uint32_t nClearStencil;
} sirRenderDepthStencilAttachmentDesc_t;

typedef struct sirRenderFramebufferDesc_s
{
	const sirRenderAttachmentDesc_t *pColors;
	uint32_t nColorCount;
	sirRenderDepthStencilAttachmentDesc_t depthStencil;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nLayers;
	uint32_t nSampleCount;
} sirRenderFramebufferDesc_t;

// Logical render-target attachment collection. It does not allocate images.
// Vulkan may use dynamic rendering, D3D12 RTV/DSV and OpenGL a cached FBO.
// The descriptor borrows attachment arrays for the duration of command record.
class SIRRenderFramebuffer : public SIRRenderResource
{
public:
	explicit SIRRenderFramebuffer( void ) noexcept = default;
	explicit SIRRenderFramebuffer( const sirRenderFramebufferDesc_t& rDesc ) noexcept;

	const sirRenderFramebufferDesc_t& getDesc( void ) const noexcept;

	bool isValid( void ) const noexcept
	{
		return m_Desc.nWidth != 0 && m_Desc.nHeight != 0 && m_Desc.nLayers != 0 && m_Desc.nSampleCount != 0 && m_Desc.nColorCount <= 8 &&
		       ( m_Desc.nColorCount == 0 || m_Desc.pColors != nullptr ) &&
		       ( m_Desc.nColorCount != 0 || m_Desc.depthStencil.hView.nGeneration != 0 );
	}

private:
	sirRenderFramebufferDesc_t m_Desc = {};
};

#endif // __SIR_RENDERFRAMEBUFFER_H__
