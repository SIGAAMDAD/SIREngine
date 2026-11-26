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

#ifndef __SIR_RENDERING_SYSTEM_H__
#define __SIR_RENDERING_SYSTEM_H__
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Systems/SIREngineSystem.h>
#include <Core/Memory/Pointers/SIRSharedPtr.h>
#include <Renderer/RHI/SIRRenderDevice.h>

struct sirRenderingSystemConfig_t
{
	SIRRenderBackend eBackend{ SIRRenderBackend::SIR_RENDER_AUTO };
	uint32_t nAdapterIndex{};
};

class SIRRenderingSystem : public SIREngineSystem
{
public:
	explicit SIRRenderingSystem( const sirRenderingSystemConfig_t& config ) noexcept
	    : m_Config( config )
	{
	}

	SIRRenderingSystem( const SIRRenderingSystem& ) = delete;
	SIRRenderingSystem( SIRRenderingSystem&& ) = delete;
	SIRRenderingSystem& operator=( const SIRRenderingSystem& ) = delete;
	SIRRenderingSystem& operator=( SIRRenderingSystem&& ) = delete;
	SIRSystemResult initialize( void ) noexcept override;
	void update( float fDeltaSeconds ) noexcept override;
	SIRSystemResult shutdown( void ) noexcept override;

	const SIRSharedPtr<SIRRenderDevice>& getDevice( void ) const noexcept
	{
		return m_pDevice;
	}

protected:
	~SIRRenderingSystem( void ) noexcept override = default;

private:
	sirRenderingSystemConfig_t m_Config;
	SIRSharedPtr<SIRRenderDevice> m_pDevice;
};
#endif
