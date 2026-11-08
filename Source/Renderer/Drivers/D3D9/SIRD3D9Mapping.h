/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRD3D9MAPPING_H__
#define __SIRENGINE_SIRD3D9MAPPING_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Renderer/SIRGraphicsDriverABI.h>

// Validate a CPU-upload lock against the actual D3D9 allocation extent.
// The D3D9 backend only admits one outstanding CPU mapping per buffer.
sirGraphicsResult_t sirD3D9ValidateBufferMap( const sirGraphicsABIBufferRange_t *pRange, uint64_t nAllocationBytes,
                                              sirGraphicsMemoryClass_t eMemory, bool bAlreadyMapped ) noexcept;

#endif // __SIRENGINE_SIRD3D9MAPPING_H__