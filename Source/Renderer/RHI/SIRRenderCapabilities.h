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

#ifndef __SIR_RENDER_CAPABILITIES_H__
#define __SIR_RENDER_CAPABILITIES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderTypes.h>

struct SIRRenderCapabilities
{
	uint64_t nFeatureMask{};
	uint32_t nMaximumTextureDimension{};
	uint32_t nMaximumColorAttachments{};
	uint32_t nMaximumFramesInFlight{};
	uint32_t nMaximumBindingSlots{};
	uint32_t nMaximumSampleCount{};
	uint32_t nQueueMask{};
	bool bProgrammableShaders{};
	bool bFixedFunction{};
	bool bCompute{};
	bool bMeshShaders{};
	bool bRayTracing{};
	bool bDeviceAddresses{};
	bool bBindless{};
	bool bSparseResources{};
};

#endif // __SIR_RENDER_CAPABILITIES_H__
