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

#ifndef __SIR_VULKAN_SHARED_H__
#define __SIR_VULKAN_SHARED_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

// Generated procedure inventory: Vulkan SDK 1.4.363.1.
// Compile all users with the same Vulkan platform/beta macros.
typedef struct sirVulkanProcs_s
{
#define SIR_VK_PROC( Name, Proc, ReturnType, Parameters, Arguments ) PFN_##Proc Proc = nullptr;

#include "SIRVulkanProcs.inl"

#undef SIR_VK_PROC
} sirVulkanProcs_t;

// Load in global -> instance -> device order. Rebuild the table on instance replacement.
// The Vulkan runtime must remain loaded for the lifetime of the table.
// Success validates Vulkan 1.0 core commands only; optional commands can be null.
bool SIR_Vulkan_LoadGlobalProcs(sirVulkanProcs_t& procs, PFN_vkGetInstanceProcAddr getProc) noexcept;
bool SIR_Vulkan_LoadInstanceProcs(sirVulkanProcs_t& procs, VkInstance instance) noexcept;
bool SIR_Vulkan_LoadDeviceProcs(sirVulkanProcs_t& procs, VkDevice device) noexcept;

#endif // __SIR_VULKAN_SHARED_H__
