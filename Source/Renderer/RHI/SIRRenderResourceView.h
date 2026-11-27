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

#ifndef __SIR_RENDER_RESOURCE_VIEW_H__
#define __SIR_RENDER_RESOURCE_VIEW_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderResourceView : public SIRRenderResource
{
public:
	explicit SIRRenderResourceView( const SIRRenderResourceView& ) = delete;
	explicit SIRRenderResourceView( SIRRenderResourceView&& ) = delete;
	virtual ~SIRRenderResourceView() noexcept = default;
	SIRRenderResourceView& operator=( const SIRRenderResourceView& ) = delete;
	SIRRenderResourceView& operator=( SIRRenderResourceView&& ) = delete;

	SIRRenderFormat getFormat( void ) const noexcept
	{
		return m_eFormat;
	}

	virtual const SIRRenderResource& getResource( void ) const noexcept = 0;

protected:
	explicit SIRRenderResourceView( const SIRSharedPtr<SIRRenderDevice>& device, SIRRenderFormat eFormat ) noexcept
	    : SIRRenderResource( device ),
	      m_eFormat( eFormat )
	{
	}

	SIRRenderFormat m_eFormat{};
};
#endif // __SIR_RENDER_RESOURCE_VIEW_H__
