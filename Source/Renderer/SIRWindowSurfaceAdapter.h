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
#ifndef __SIRENGINE_SIRWINDOWSURFACEADAPTER_H__
#define __SIRENGINE_SIRWINDOWSURFACEADAPTER_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Renderer/SIRGraphicsTypes.h>

// Renderer retains the acquired native lease until GPU surface retirement.
// The window owner must call this from its owner thread; render workers hand
// the request to that thread, never dereference borrowed native pointers.
sirGraphicsResult_t sirGraphicsAcquireWindowSurface( SIRWindowSystem& windows, sirWindowHandle_t hWindow,
                                                     sirGraphicsNativeSurface_t *pNative, sirWindowNativeLease_t *pLease ) noexcept;
#endif // __SIRENGINE_SIRWINDOWSURFACEADAPTER_H__
