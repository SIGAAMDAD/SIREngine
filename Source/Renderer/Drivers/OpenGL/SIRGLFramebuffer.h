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

#ifndef __SIR_GL_FRAMEBUFFER_H__
#define __SIR_GL_FRAMEBUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include "SIRGLShared.h"
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>
#include <Core/Containers/SIRArray.h>
#include <Core/Containers/SIRInlineVector.h>

class SIRGLFramebuffer final : public SIRRenderResource
{
public:
	static constexpr uint32_t MAX_FRAMEBUFFER_COLOR_ATTACHMENTS = 8;

	explicit SIRGLFramebuffer( const SIRSharedPtr<SIRRenderDevice>& device ) noexcept;
	~SIRGLFramebuffer() noexcept;

//	uint32_t getWidth( void ) const noexcept { return m_nWidth; }
//	uint32_t getHeight( void ) const noexcept { return m_nHeight; }
//	uint32_t getLayerCount( void ) const noexcept { return m_nLayerCount; }
//	uint32_t getSampleCount( void ) const noexcept { return m_nSampleCount; }
//	uint32_t getColorAttachmentCount( void ) const noexcept { return static_cast<uint32_t>( m_ColorAttachments.getSize() ); }
//
//	const SIRInlineVector<const SIRRenderResourceView *, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS>& getColorAttachments( void ) const noexcept { return m_ColorAttachments; }
//	const SIRRenderResourceView *getDepthAttachment( void ) const noexcept { return m_pDepthStencil; }
//	const SIRRenderResourceView *getStencilAttachment( void ) const noexcept { return m_pDepthStencil; }

	void clear( void ) noexcept;
	void blit( SIRGLFramebuffer& targetFbo, const SIRRenderViewport& srcExtents, const SIRRenderViewport& dstExtents, SIRRenderFilter eFilter, SIRRenderTextureAspect eMask ) noexcept;
	void setDrawBuffers( const SIRArray<GLenum, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS>& szAttachments ) const noexcept;

	virtual bool isValid( void ) const noexcept override;

private:
	SIRInlineVector<SIRRenderFrameColorAttachment, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> m_ColorAttachments;
	SIRRenderFrameDepthStencilAttachment m_DepthAttachment;
	SIRRenderViewport m_Extents;

	GLuint m_hFramebufferID;
};

#endif // __SIR_GL_FRAMEBUFFER_H__
