/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRRENDERALL_H__
#define __SIRENGINE_SIRRENDERALL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderStorageBuffer.h>
#include <Renderer/RHI/SIRRenderTexture.h>
#include <Renderer/RHI/SIRRenderResourceView.h>
#include <Renderer/RHI/SIRRenderShader.h>
#include <Renderer/RHI/SIRRenderSampler.h>
#include <Renderer/RHI/SIRRenderPipeline.h>
#include <Renderer/RHI/SIRRenderFramebuffer.h>
#include <Renderer/RHI/SIRRenderBindingLayout.h>
#include <Renderer/RHI/SIRRenderBindingSet.h>
#include <Renderer/RHI/SIRRenderCommandList.h>
#include <Renderer/RHI/SIRRenderQueue.h>
#include <Renderer/RHI/SIRRenderSynchronization.h>
#include <Renderer/RHI/SIRRenderSwapchain.h>
#include <Renderer/RHI/SIRRenderDevice.h>
#include <Renderer/RHI/SIRRenderMemoryAllocator.h>
#include <Renderer/RHI/SIRRenderQueryPool.h>
#include <Renderer/RHI/SIRRenderAccelerationStructure.h>
#include <Renderer/RHI/SIRRenderShaderBindingTable.h>

// Optional convenience include for standalone syntax validation.

#endif // __SIRENGINE_SIRRENDERALL_H__
