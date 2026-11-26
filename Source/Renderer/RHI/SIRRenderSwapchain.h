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

#ifndef __SIR_RENDER_SWAPCHAIN_H__
#define __SIR_RENDER_SWAPCHAIN_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderQueue.h>
#include <Renderer/RHI/SIRRenderTypes.h>

struct SIRRenderNativeWindow
{
	void *pHandle{};
};

class SIRRenderSwapchain : public SIRRenderResource
{
public:
	explicit SIRRenderSwapchain( const SIRRenderSwapchain& ) = delete;
	explicit SIRRenderSwapchain( SIRRenderSwapchain&& ) = delete;
	virtual ~SIRRenderSwapchain() noexcept = default;
	SIRRenderSwapchain& operator=( const SIRRenderSwapchain& ) = delete;
	SIRRenderSwapchain& operator=( SIRRenderSwapchain&& ) = delete;

	uint32_t getWidth( void ) const noexcept
	{
		return m_nWidth;
	}

	uint32_t getHeight( void ) const noexcept
	{
		return m_nHeight;
	}

	SIRRenderFormat getFormat( void ) const noexcept
	{
		return m_eFormat;
	}

	uint32_t getImageCount( void ) const noexcept
	{
		return m_nImageCount;
	}

	SIRRenderPresentMode getPresentMode( void ) const noexcept
	{
		return m_ePresentMode;
	}

	virtual SIRRenderResult acquire( uint64_t nTimeoutNanoseconds, uint32_t *pImageIndex ) noexcept = 0;
	virtual SIRRenderResult present( const SIRRenderSubmission& submission ) noexcept = 0;
	virtual SIRRenderResult resize( uint32_t nWidth, uint32_t nHeight ) noexcept = 0;

protected:
	explicit SIRRenderSwapchain( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderSwapchainDescription_t& description ) noexcept
	    : SIRRenderResource( device ),
	      m_nWidth( description.nWidth ),
	      m_nHeight( description.nHeight ),
	      m_nImageCount( description.nImageCount ),
	      m_eFormat( description.eFormat ),
	      m_ePresentMode( description.ePresentMode )
	{
	}

	void setPresentationProperties( const sirRenderSwapchainDescription_t& description ) noexcept
	{
		m_nWidth = description.nWidth;
		m_nHeight = description.nHeight;
		m_nImageCount = description.nImageCount;
		m_eFormat = description.eFormat;
		m_ePresentMode = description.ePresentMode;
	}

private:
	uint32_t m_nWidth{};
	uint32_t m_nHeight{};
	uint32_t m_nImageCount{};
	SIRRenderFormat m_eFormat{};
	SIRRenderPresentMode m_ePresentMode{};
};
#endif // __SIR_RENDER_SWAPCHAIN_H__
