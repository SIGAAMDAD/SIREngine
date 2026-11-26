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

#include <Renderer/Drivers/D3D9/SIRD3D9Mapping.h>

SIRGraphicsResult sirD3D9ValidateBufferMap( const sirGraphicsABIBufferRange_t *pRange, uint64_t nAllocationBytes,
                                              SIRGraphicsMemoryClass eMemory, bool bAlreadyMapped ) noexcept
{
	if ( pRange == nullptr || pRange->nSize < sizeof( *pRange ) || pRange->nReserved != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pRange->hBuffer.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER || pRange->hBuffer.nGeneration == 0 ||
	     pRange->hBuffer.nDeviceGeneration == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( bAlreadyMapped ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_UPLOAD ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pRange->nBytes == 0 || pRange->nBytes > UINT32_MAX || pRange->nOffsetBytes > UINT32_MAX ||
	     pRange->nOffsetBytes > nAllocationBytes || pRange->nBytes > nAllocationBytes - pRange->nOffsetBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}