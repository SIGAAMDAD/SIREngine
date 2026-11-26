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

// Include inside the public section of SIRVulkanDevice.
// Requires m_Device (VkDevice) and m_Procs (sirVulkanProcs_t).
// Optional procedures must have been enabled and loaded before use.

#define SIR_VK_PROC( Name, Proc, ReturnType, Parameters, Arguments ) \
    SIR_FORCEINLINE ReturnType Name Parameters const noexcept \
    { \
        return m_Procs.Proc Arguments; \
    }

#include "SIRVulkanProcs.inl"

#undef SIR_VK_PROC
