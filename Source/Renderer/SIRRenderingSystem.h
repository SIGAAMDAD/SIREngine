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

struct SIRRenderingSystemConfig
{
	SIRRenderBackend eBackend{};
	SIRPipelineEra ePipelineEra{};
};

class SIRRenderingSystem : public SIREngineSystem
{
public:
	explicit SIRRenderingSystem( const SIRRenderingSystemConfig& config ) noexcept
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

	const SIRSharedPtr<SIRRenderDevice>& getDevice( void ) const noexcept;

protected:
	~SIRRenderingSystem() noexcept override = default;

private:
	const SIRRenderingSystemConfig m_Config;
	SIRSharedPtr<SIRRenderDevice> m_pDevice;
};

#endif // __SIR_RENDERING_SYSTEM_H__
