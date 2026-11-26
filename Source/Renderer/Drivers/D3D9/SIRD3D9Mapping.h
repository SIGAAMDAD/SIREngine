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

#ifndef __SIR_D3_D9_MAPPING_H__
#define __SIR_D3_D9_MAPPING_H__
#include <Core/SIRPCH.h>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Renderer/SIRGraphicsDriverABI.h>

// Validate a CPU-upload lock against the actual D3D9 allocation extent.
// The D3D9 backend only admits one outstanding CPU mapping per buffer.
SIRGraphicsResult sirD3D9ValidateBufferMap( const sirGraphicsABIBufferRange_t *pRange, uint64_t nAllocationBytes,
                                              SIRGraphicsMemoryClass eMemory, bool bAlreadyMapped ) noexcept;

#endif // __SIR_D3_D9_MAPPING_H__
