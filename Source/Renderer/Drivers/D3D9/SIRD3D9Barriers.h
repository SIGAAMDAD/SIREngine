/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRD3D9BARRIERS_H__
#define __SIRENGINE_SIRD3D9BARRIERS_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Renderer/SIRGraphicsTypes.h>

// Validate serial execution-state transitions before recording native commands.
// Returns whether the transition requires clearing native read bindings.
sirGraphicsResult_t sirD3D9ValidateSerialBarrier( const sirGraphicsBarrier_t *pBarrier, uint32_t nMipLevels, uint32_t nArrayLayers,
                                                  bool *pUnbind ) noexcept;

#endif // __SIRENGINE_SIRD3D9BARRIERS_H__