/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_RENDER_FRAMEBUFFER_H__
#define __SIR_RENDER_FRAMEBUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRInlineVector.h>
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRSpan.h>
#include "SIRRenderTypes.h"
#include "SIRRenderResource.h"
#include "SIRRenderResourceView.h"

class SIRRenderFramebuffer : public SIRRenderResource
{
public:
	static constexpr const size_t MAX_FRAMEBUFFER_COLOR_ATTACHMENTS = 16;

public:
	explicit SIRRenderFramebuffer( const SIRRenderFramebuffer& ) = delete;
	explicit SIRRenderFramebuffer( SIRRenderFramebuffer&& ) = delete;
	virtual ~SIRRenderFramebuffer() noexcept = default;

	SIRRenderFramebuffer& operator=( const SIRRenderFramebuffer& ) = delete;
	SIRRenderFramebuffer& operator=( SIRRenderFramebuffer&& ) = delete;

	uint32_t getWidth( void ) const noexcept;
	uint32_t getHeight( void ) const noexcept;
	uint32_t getLayerCount( void ) const noexcept;
	uint32_t getSampleCount( void ) const noexcept;
	uint32_t getColorAttachmentCount( void ) const noexcept;

	const SIRInlineVector<const SIRRenderResourceView *, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS>& getColorAttachments( void ) const noexcept;
	const SIRRenderResourceView *getDepthAttachment( void ) const noexcept;
	const SIRRenderResourceView *getStencilAttachment( void ) const noexcept;

protected:
	explicit SIRRenderFramebuffer( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderFramebufferDescription_t& description ) noexcept
	    : SIRRenderResource( device ),
	      m_ColorAttachments(),
	      m_pDepthStencil( nullptr ),
	      m_nWidth( description.nWidth ),
	      m_nHeight( description.nHeight ),
	      m_nLayerCount( description.nLayerCount ),
	      m_nSampleCount( description.nSampleCount )
	{
	}

	SIRInlineVector<const SIRRenderResourceView *, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> m_ColorAttachments{};
	const SIRRenderResourceView *m_pDepthStencil{};

	uint32_t m_nWidth{};
	uint32_t m_nHeight{};
	uint32_t m_nLayerCount{};
	uint32_t m_nSampleCount{};
};

#endif // __SIR_RENDER_FRAMEBUFFER_H__
