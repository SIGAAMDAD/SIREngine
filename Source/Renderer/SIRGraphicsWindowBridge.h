/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRGRAPHICSWINDOWBRIDGE_H__
#define __SIRENGINE_SIRGRAPHICSWINDOWBRIDGE_H__

#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Renderer/SIRGraphicsTypes.h>

// A graphics surface must retain this borrowing token until its backend has
// retired every swapchain/native-surface reference to the OS window.
typedef struct sirGraphicsWindowBorrow_s
{
	sirGraphicsNativeSurface_t surface;
	sirWindowLease_t lease;
} sirGraphicsWindowBorrow_t;

bool sirGraphicsAcquireWindowSurface( SIRWindowSystem& windows, sirWindowHandle_t hWindow, sirGraphicsWindowBorrow_t *pBorrow ) noexcept;
bool sirGraphicsReleaseWindowSurface( SIRWindowSystem& windows, sirGraphicsWindowBorrow_t *pBorrow ) noexcept;

#endif // __SIRENGINE_SIRGRAPHICSWINDOWBRIDGE_H__
