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

#ifndef __SIR_RENDER_SAMPLER_H__
#define __SIR_RENDER_SAMPLER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderResource.h>

struct SIRRenderSamplerDescription
{
	uint8_t nMinFilter{};
	uint8_t nMagFilter{};
	uint8_t nMipmapFilter{};
	uint8_t nAddressU{};
	uint8_t nAddressV{};
	uint8_t nAddressW{};
	float fAnisotropy{};
	float fMinLod{};
	float fMaxLod{};
	float fLodBias{};
	bool bCompare{};
};

class SIRRenderSampler : public SIRRenderResource
{
public:
	explicit SIRRenderSampler( const SIRRenderSampler& ) = delete;
	explicit SIRRenderSampler( SIRRenderSampler&& ) = delete;
	virtual ~SIRRenderSampler() noexcept = default;
	SIRRenderSampler& operator=( const SIRRenderSampler& ) = delete;
	SIRRenderSampler& operator=( SIRRenderSampler&& ) = delete;

	const SIRRenderSamplerDescription& getDescription( void ) const noexcept
	{
		return m_Description;
	}

protected:
	explicit SIRRenderSampler( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderSamplerDescription& description ) noexcept
	    : SIRRenderResource( device ),
	      m_Description( description )
	{
	}

	SIRRenderSamplerDescription m_Description{};
};
#endif // __SIR_RENDER_SAMPLER_H__
