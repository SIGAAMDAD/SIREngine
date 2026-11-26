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

// A vulkan specific cached program
class SIRVulkanProgram final : public SIRRenderProgram
{
public:
	explicit SIRVulkanProgram( const SIRSharedPtr<SIRRenderDevice>& device, const sirContainerAllocator_t *pAllocator ) noexcept;
	~SIRVulkanProgram() noexcept override = default;

private:
	uint64_t m_nProgramHash;
	uint32_t m_nStageMask;

	SIRVector<SIRSharedPtr<SIRVulkanShader>> m_Shaders;
};

#endif // __SIR_VULKAN_PROGRAM_H__
