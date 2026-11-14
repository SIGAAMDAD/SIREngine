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

#ifndef __SIRENGINE_SIRRENDERBINDINGLAYOUT_H__
#define __SIRENGINE_SIRRENDERBINDINGLAYOUT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// Pipeline binding signature, not an independent physical GPU allocation.
// Borrowed descriptor must stay alive through native pipeline creation.
class SIRRenderBindingLayout final
{
public:
	SIRRenderBindingLayout( void ) noexcept = default;
	explicit SIRRenderBindingLayout( const sirRenderBindingLayoutDesc_t& desc ) noexcept : m_Desc( desc )
	{
	}

	const sirRenderBindingLayoutDesc_t& getDesc( void ) const noexcept
	{
		return m_Desc;
	}
	bool isValid( void ) const noexcept
	{
		return ( m_Desc.nBindingCount == 0 || m_Desc.pBindings != nullptr ) && m_Desc.nSignatureHash != 0;
	}

private:
	sirRenderBindingLayoutDesc_t m_Desc = {};
};

#endif // __SIRENGINE_SIRRENDERBINDINGLAYOUT_H__
