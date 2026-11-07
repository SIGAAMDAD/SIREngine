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

#ifndef __SIRENGINE_SIRD3D11BUFFERVALIDATION_H__
#define __SIRENGINE_SIRD3D11BUFFERVALIDATION_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/SIRGraphicsTypes.h>

typedef enum sirD3D11BufferStorage_e : uint32_t
{
	SIR_D3D11_BUFFER_DEFAULT = 0,
	SIR_D3D11_BUFFER_DYNAMIC = 1,
	SIR_D3D11_BUFFER_STAGING = 2
} sirD3D11BufferStorage_t;

typedef struct sirD3D11BufferPlan_s
{
	sirD3D11BufferStorage_t eStorage;
	uint32_t nNativeBindUsage;
	uint32_t nSizeBytes;
	uint32_t nMappedAccess;
} sirD3D11BufferPlan_t;

// Validates shared ABI descriptors without requiring any Windows SDK headers.
// Zero budget denotes a caller-owned unlimited budget (e.g. editor mode).
sirGraphicsResult_t sirD3D11PlanBuffer( const sirGraphicsBufferDesc_t *pDesc, uint64_t nCommittedBytes, uint64_t nResidentBudgetBytes,
                                        sirD3D11BufferPlan_t *pPlan ) noexcept;

#endif // __SIRENGINE_SIRD3D11BUFFERVALIDATION_H__
