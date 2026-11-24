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

#ifndef __SIR_GRAPHICSPIPELINEBRIDGE_H__
#define __SIR_GRAPHICSPIPELINEBRIDGE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsABIOperations.h>

// Converts the source-facing C++ descriptor to the exact graphics ABI v1
// layout. All recipe, shader, binding, and attribute pointers remain borrowed
// for the duration of the native pCreatePipeline call. No allocation occurs.
// On failure, pEncoded is never modified.
SIRGraphicsResult sirGraphicsEncodeABIPipeline( const sirGraphicsPipelineDesc_t& source, sirGraphicsABIPipeline_t *pEncoded ) noexcept;

#endif // __SIR_GRAPHICSPIPELINEBRIDGE_H__
