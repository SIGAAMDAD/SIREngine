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

#ifndef __SIR_VULKAN_PROGRAM_H__
#define __SIR_VULKAN_PROGRAM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <Renderer/RHI/SIRRenderProgram.h>
#include "SIRVulkanShader.h"
#include "SIRVulkanShared.h"

class SIRVulkanProgram final : public SIRRenderProgram
{
public:
	SIRVulkanProgram( const SIRSharedPtr<SIRRenderDevice>& device, SIRSpan<const SIRSharedPtr<SIRVulkanShader>> shaders,
	                  const sirContainerAllocator_t *pAllocator = nullptr ) noexcept;
	~SIRVulkanProgram() noexcept override = default;
	const SIRVulkanShader *getShader( SIRRenderShaderStage eStage ) const noexcept;
	bool isValid( void ) const noexcept;

private:
	bool m_bValid{};
};

#endif // __SIR_VULKAN_PROGRAM_H__
