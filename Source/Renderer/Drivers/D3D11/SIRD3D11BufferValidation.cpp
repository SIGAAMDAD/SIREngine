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

#include <Renderer/Drivers/D3D11/SIRD3D11BufferValidation.h>
#include <stdint.h>

sirGraphicsResult_t sirD3D11PlanBuffer( const sirGraphicsBufferDesc_t *pDesc, uint64_t nCommittedBytes, uint64_t nResidentBudgetBytes,
                                        sirD3D11BufferPlan_t *pPlan ) noexcept
{
	if ( pDesc == nullptr || pPlan == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pPlan = {};
	if ( pDesc->nSizeBytes == 0 || pDesc->nSizeBytes > UINT32_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t nAlignment = pDesc->nAlignmentBytes;
	if ( nAlignment > 16 || ( nAlignment != 0 && ( nAlignment & ( nAlignment - 1 ) ) != 0 ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nBindableUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_INDEX | SIR_GRAPHICS_USAGE_CONSTANT;
	const uint32_t nCopyUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	const uint32_t nSupportedUsage = nBindableUsage | nCopyUsage;
	if ( ( pDesc->nUsage & ~nSupportedUsage ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( UINT64_MAX - nCommittedBytes < pDesc->nSizeBytes ||
	     ( nResidentBudgetBytes != 0 &&
	       ( nCommittedBytes > nResidentBudgetBytes || pDesc->nSizeBytes > nResidentBudgetBytes - nCommittedBytes ) ) ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	const uint32_t nBind = pDesc->nUsage & nBindableUsage;
	if ( ( nBind & SIR_GRAPHICS_USAGE_CONSTANT ) != 0 ) {
		if ( nBind != SIR_GRAPHICS_USAGE_CONSTANT || pDesc->nSizeBytes > 65536 || ( pDesc->nSizeBytes & 15 ) != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	sirD3D11BufferPlan_t plan = {};
	plan.nSizeBytes = ( uint32_t )pDesc->nSizeBytes;
	plan.nNativeBindUsage = nBind;
	switch ( pDesc->eMemory ) {
	case SIR_GRAPHICS_MEMORY_GPU_LOCAL:
		if ( nBind == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		plan.eStorage = SIR_D3D11_BUFFER_DEFAULT;
		break;
	case SIR_GRAPHICS_MEMORY_CPU_UPLOAD:
		if ( nBind == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		plan.eStorage = SIR_D3D11_BUFFER_DYNAMIC;
		plan.nMappedAccess = 1;
		break;
	case SIR_GRAPHICS_MEMORY_CPU_READBACK:
		if ( nBind != 0 || ( pDesc->nUsage & nCopyUsage ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		plan.eStorage = SIR_D3D11_BUFFER_STAGING;
		plan.nMappedAccess = 2;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*pPlan = plan;
	return SIR_GRAPHICS_SUCCESS;
}
