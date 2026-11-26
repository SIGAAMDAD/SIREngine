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

#ifndef __SIR_D3_D9_BARRIERS_H__
#define __SIR_D3_D9_BARRIERS_H__
#include <Core/SIRPCH.h>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Renderer/SIRGraphicsTypes.h>

// Validate serial execution-state transitions before recording native commands.
// Returns whether the transition requires clearing native read bindings.
SIRGraphicsResult sirD3D9ValidateSerialBarrier( const sirGraphicsBarrier_t *pBarrier, uint32_t nMipLevels, uint32_t nArrayLayers,
                                                  bool *pUnbind ) noexcept;

#endif // __SIR_D3_D9_BARRIERS_H__
