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

#ifndef __SIRENGINE_SIRD3D9UPLOADRING_H__
#define __SIRENGINE_SIRD3D9UPLOADRING_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/SIRGraphicsTypes.h>

typedef struct sirD3D9UploadRange_s
{
	uint32_t nOffset;
	uint32_t nBytes;
	uint32_t nLockFlags;
	uint32_t nGeneration;
} sirD3D9UploadRange_t;

typedef struct sirD3D9UploadRing_s
{
	uint32_t nCapacity;
	uint32_t nCursor;
	uint32_t nGeneration;
	uint32_t nRetiredGeneration;
	uint32_t nPendingWrites;
	uint64_t nLatestSubmission;
	uint64_t nCompletedSubmission;
} sirD3D9UploadRing_t;

// Lock flag numeric values are driver-local and mapped to D3DLOCK_* in the Windows implementation.
static constexpr uint32_t SIR_D3D9_RING_DISCARD = 1;
static constexpr uint32_t SIR_D3D9_RING_NOOVERWRITE = 2;

sirGraphicsResult_t sirD3D9RingInitialize( sirD3D9UploadRing_t *pRing, uint32_t nCapacity ) noexcept;
sirGraphicsResult_t sirD3D9RingReserve( sirD3D9UploadRing_t *pRing, uint32_t nBytes, uint32_t nAlignment,
                                        sirD3D9UploadRange_t *pRange ) noexcept;
sirGraphicsResult_t sirD3D9RingSubmit( sirD3D9UploadRing_t *pRing, uint64_t nSubmission ) noexcept;
sirGraphicsResult_t sirD3D9RingComplete( sirD3D9UploadRing_t *pRing, uint64_t nSubmission ) noexcept;
void sirD3D9RingReset( sirD3D9UploadRing_t *pRing ) noexcept;

#endif // __SIRENGINE_SIRD3D9UPLOADRING_H__
