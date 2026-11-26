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

#ifndef __SIR_RENDER_BINDING_LAYOUT_H__
#define __SIR_RENDER_BINDING_LAYOUT_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRSpan.h>
#include <Renderer/RHI/SIRRenderResource.h>

struct SIRRenderBinding
{
	uint32_t nBinding{};
	uint32_t nType{};
	uint32_t nArrayLength{ 1 };
	uint32_t nStageMask{};
	uint32_t nConstantBytes{};
};

class SIRRenderBindingLayout : public SIRRenderResource
{
public:
	explicit SIRRenderBindingLayout( const SIRRenderBindingLayout& ) = delete;
	explicit SIRRenderBindingLayout( SIRRenderBindingLayout&& ) = delete;
	virtual ~SIRRenderBindingLayout() noexcept = default;
	SIRRenderBindingLayout& operator=( const SIRRenderBindingLayout& ) = delete;
	SIRRenderBindingLayout& operator=( SIRRenderBindingLayout&& ) = delete;

	uint32_t getBindingCount( void ) const noexcept
	{
		return m_Bindings.getSize();
	}

	SIRSpan<const SIRRenderBinding> getBindings( void ) const noexcept
	{
		return SIRSpan<const SIRRenderBinding>( m_Bindings.data(), m_Bindings.getSize() );
	}

	uint64_t getCompatibilityHash( void ) const noexcept
	{
		return m_nSignatureHash;
	}

protected:
	explicit SIRRenderBindingLayout( const SIRSharedPtr<SIRRenderDevice>& device, SIRVector<SIRRenderBinding>&& bindings, uint64_t nSignatureHash ) noexcept
	    : SIRRenderResource( device ),
	      m_Bindings( static_cast<SIRVector<SIRRenderBinding>&&>( bindings ) ),
	      m_nSignatureHash( nSignatureHash )
	{
	}

	SIRVector<SIRRenderBinding> m_Bindings;
	uint64_t m_nSignatureHash{};
};
#endif // __SIR_RENDER_BINDING_LAYOUT_H__
