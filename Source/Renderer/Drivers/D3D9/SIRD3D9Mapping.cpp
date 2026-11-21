/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/D3D9/SIRD3D9Mapping.h>

sirGraphicsResult_t sirD3D9ValidateBufferMap( const sirGraphicsABIBufferRange_t *pRange, uint64_t nAllocationBytes,
                                              sirGraphicsMemoryClass_t eMemory, bool bAlreadyMapped ) noexcept
{
	if ( pRange == nullptr || pRange->nSize < sizeof( *pRange ) || pRange->nReserved != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pRange->hBuffer.eType != SIR_GRAPHICS_HANDLE_BUFFER || pRange->hBuffer.nGeneration == 0 ||
	     pRange->hBuffer.nDeviceGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( bAlreadyMapped ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( eMemory != SIR_GRAPHICS_MEMORY_CPU_UPLOAD ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pRange->nBytes == 0 || pRange->nBytes > UINT32_MAX || pRange->nOffsetBytes > UINT32_MAX ||
	     pRange->nOffsetBytes > nAllocationBytes || pRange->nBytes > nAllocationBytes - pRange->nOffsetBytes ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}