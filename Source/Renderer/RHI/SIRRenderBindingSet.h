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

#ifndef __SIR_RENDER_BINDING_SET_H__
#define __SIR_RENDER_BINDING_SET_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRSpan.h>
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderBindingLayout.h>

class SIRRenderBindingSet : public SIRRenderResource
{
public:
	explicit SIRRenderBindingSet( const SIRRenderBindingSet& ) = delete;
	explicit SIRRenderBindingSet( SIRRenderBindingSet&& ) = delete;
	virtual ~SIRRenderBindingSet() noexcept = default;
	SIRRenderBindingSet& operator=( const SIRRenderBindingSet& ) = delete;
	SIRRenderBindingSet& operator=( SIRRenderBindingSet&& ) = delete;

	const SIRRenderBindingLayout& getLayout( void ) const noexcept
	{
		return *m_pLayout;
	}

	uint32_t getBindingCount( void ) const noexcept
	{
		return m_Bindings.getSize();
	}

	SIRSpan<const SIRRenderBinding> getBindings( void ) const noexcept
	{
		return SIRSpan<const SIRRenderBinding>( m_Bindings.data(), m_Bindings.getSize() );
	}

protected:
	explicit SIRRenderBindingSet( const SIRSharedPtr<SIRRenderDevice>& device, const SIRSharedPtr<SIRRenderBindingLayout>& layout, SIRVector<SIRRenderBinding>&& bindings ) noexcept
	    : SIRRenderResource( device ),
	      m_pLayout( layout ),
	      m_Bindings( static_cast<SIRVector<SIRRenderBinding>&&>( bindings ) )
	{
	}

	SIRSharedPtr<SIRRenderBindingLayout> m_pLayout;
	SIRVector<SIRRenderBinding> m_Bindings;
};
#endif // __SIR_RENDER_BINDING_SET_H__
