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

#ifndef __SIR_RENDER_QUERY_POOL_H__
#define __SIR_RENDER_QUERY_POOL_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderQueryPool : public SIRRenderResource
{
public:
	explicit SIRRenderQueryPool( const SIRRenderQueryPool& ) = delete;
	explicit SIRRenderQueryPool( SIRRenderQueryPool&& ) = delete;
	virtual ~SIRRenderQueryPool() noexcept = default;
	SIRRenderQueryPool& operator=( const SIRRenderQueryPool& ) = delete;
	SIRRenderQueryPool& operator=( SIRRenderQueryPool&& ) = delete;

	SIRRenderQueryType getType( void ) const noexcept
	{
		return m_eType;
	}

	uint32_t getCount( void ) const noexcept
	{
		return m_nQueryCount;
	}

	uint32_t getStatisticsMask( void ) const noexcept
	{
		return m_nStatisticsMask;
	}

protected:
	explicit SIRRenderQueryPool( const SIRSharedPtr<SIRRenderDevice>& device, SIRRenderQueryType eType, uint32_t nQueryCount, uint32_t nStatisticsMask ) noexcept
	    : SIRRenderResource( device ),
	      m_eType( eType ),
	      m_nQueryCount( nQueryCount ),
	      m_nStatisticsMask( nStatisticsMask )
	{
	}

private:
	SIRRenderQueryType m_eType{};
	uint32_t m_nQueryCount{};
	uint32_t m_nStatisticsMask{};
};
#endif // __SIR_RENDER_QUERY_POOL_H__
