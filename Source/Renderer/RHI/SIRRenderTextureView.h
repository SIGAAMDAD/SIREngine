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

#ifndef __SIR_RENDER_TEXTURE_VIEW_H__
#define __SIR_RENDER_TEXTURE_VIEW_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderResourceView.h>
#include <Renderer/RHI/SIRRenderTexture.h>
class SIRRenderTexture;

class SIRRenderTextureView : public SIRRenderResourceView
{
public:
	explicit SIRRenderTextureView( const SIRRenderTextureView& ) = delete;
	explicit SIRRenderTextureView( SIRRenderTextureView&& ) = delete;
	virtual ~SIRRenderTextureView() noexcept = default;
	SIRRenderTextureView& operator=( const SIRRenderTextureView& ) = delete;
	SIRRenderTextureView& operator=( SIRRenderTextureView&& ) = delete;

	const SIRRenderTexture& getTexture( void ) const noexcept
	{
		return *m_pTexture;
	}

	uint32_t getBaseMip( void ) const noexcept
	{
		return m_nBaseMip;
	}

	uint32_t getMipCount( void ) const noexcept
	{
		return m_nMipCount;
	}

	uint32_t getBaseLayer( void ) const noexcept
	{
		return m_nBaseLayer;
	}

	uint32_t getLayerCount( void ) const noexcept
	{
		return m_nLayerCount;
	}

	uint32_t getAspectMask( void ) const noexcept
	{
		return m_nAspectMask;
	}

	uint64_t getUsage( void ) const noexcept
	{
		return m_nUsage;
	}

	SIRRenderTextureDimension getDimension( void ) const noexcept
	{
		return m_eDimension;
	}

	const SIRRenderResource& getResource( void ) const noexcept override
	{
		return *m_pTexture;
	}

protected:
	explicit SIRRenderTextureView( const SIRSharedPtr<SIRRenderDevice>& device,
	                               const SIRSharedPtr<SIRRenderTexture>& texture,
	                               const SIRRenderViewDesc& description ) noexcept
	    : SIRRenderResourceView( device, description.eFormat ),
	      m_pTexture( texture ),
	      m_nBaseMip( description.subresources.nBaseMip ),
	      m_nMipCount( description.subresources.nMipCount ),
	      m_nBaseLayer( description.subresources.nBaseLayer ),
	      m_nLayerCount( description.subresources.nLayerCount ),
	      m_nAspectMask( description.subresources.nAspectMask ),
	      m_nUsage( description.nUsageMask ),
	      m_eDimension( description.eDimension )
	{
	}

	SIRSharedPtr<SIRRenderTexture> m_pTexture;
	uint32_t m_nBaseMip{};
	uint32_t m_nMipCount{};
	uint32_t m_nBaseLayer{};
	uint32_t m_nLayerCount{};
	uint32_t m_nAspectMask{};
	uint64_t m_nUsage{};
	SIRRenderTextureDimension m_eDimension{};
};
#endif // __SIR_RENDER_TEXTURE_VIEW_H__
