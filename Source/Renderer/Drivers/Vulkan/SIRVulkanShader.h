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

#ifndef __SIR_VULKAN_SHADER_H__
#define __SIR_VULKAN_SHADER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderTypes.h>
#include <Renderer/RHI/SIRRenderShader.h>
#include "SIRVulkanShared.h"

class SIRVulkanShader final : public SIRRenderShader
{
public:
	explicit SIRVulkanShader( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderShaderDescription_t& description );
	~SIRVulkanShader() noexcept override;
	VkShaderModule getNativeModule( void ) const noexcept { return m_hModule; }
	bool isValid( void ) const noexcept { return m_hModule != VK_NULL_HANDLE; }


private:
	VkShaderModule m_hModule{ VK_NULL_HANDLE };
};

#endif // __SIR_VULKAN_SHADER_H__
