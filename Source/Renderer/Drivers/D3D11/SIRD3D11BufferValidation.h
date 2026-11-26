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

#ifndef __SIR_D3_D11_BUFFERVALIDATION_H__
#define __SIR_D3_D11_BUFFERVALIDATION_H__

#include <Core/SIRPCH.h>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/SIRGraphicsTypes.h>

enum class SIRD3D11BufferStorage : uint32_t {
	SIR_D3D11_BUFFER_DEFAULT = 0,
	SIR_D3D11_BUFFER_DYNAMIC = 1,
	SIR_D3D11_BUFFER_STAGING = 2
};

typedef struct sirD3D11BufferPlan_s
{
	SIRD3D11BufferStorage eStorage;
	uint32_t nNativeBindUsage;
	uint32_t nSizeBytes;
	uint32_t nMappedAccess;
} sirD3D11BufferPlan_t;

// Validates shared ABI descriptors without requiring any Windows SDK headers.
// Zero budget denotes a caller-owned unlimited budget (e.g. editor mode).
SIRGraphicsResult sirD3D11PlanBuffer( const sirGraphicsBufferDesc_t *pDesc, uint64_t nCommittedBytes, uint64_t nResidentBudgetBytes,
                                        sirD3D11BufferPlan_t *pPlan ) noexcept;

#endif // __SIR_D3_D11_BUFFERVALIDATION_H__
