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
#include <Renderer/Drivers/D3D9/SIRD3D9UploadRing.h>
#include <limits.h>

sirGraphicsResult_t sirD3D9RingInitialize( sirD3D9UploadRing_t *pRing, uint32_t nCapacity ) noexcept
{
	if ( pRing == nullptr || nCapacity == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pRing = {};
	pRing->nCapacity = nCapacity;
	pRing->nGeneration = 1;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9RingReserve( sirD3D9UploadRing_t *pRing, uint32_t nBytes, uint32_t nAlignment,
                                        sirD3D9UploadRange_t *pRange ) noexcept
{
	if ( pRing == nullptr || pRange == nullptr || pRing->nCapacity == 0 || pRing->nGeneration == 0 || nBytes == 0 || nAlignment == 0 ||
	     ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nBytes > pRing->nCapacity ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pRing->nCursor > UINT32_MAX - ( nAlignment - 1 ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const uint32_t nAligned = ( pRing->nCursor + nAlignment - 1 ) & ~( nAlignment - 1 );
	if ( nAligned <= pRing->nCapacity && nBytes <= pRing->nCapacity - nAligned ) {
		*pRange = { nAligned, nBytes, pRing->nCursor == 0 ? SIR_D3D9_RING_DISCARD : SIR_D3D9_RING_NOOVERWRITE, pRing->nGeneration };
		pRing->nCursor = nAligned + nBytes;
		++pRing->nPendingWrites;
		return SIR_GRAPHICS_SUCCESS;
	}
	// Every reservation in the previous cycle must be idle before DISCARD.
	if ( pRing->nPendingWrites != 0 || pRing->nCompletedSubmission < pRing->nLatestSubmission ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pRing->nGeneration == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	++pRing->nGeneration;
	pRing->nRetiredGeneration = pRing->nGeneration - 1;
	*pRange = { 0, nBytes, SIR_D3D9_RING_DISCARD, pRing->nGeneration };
	pRing->nCursor = nBytes;
	pRing->nPendingWrites = 1;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9RingSubmit( sirD3D9UploadRing_t *pRing, uint64_t nSubmission ) noexcept
{
	if ( pRing == nullptr || pRing->nCapacity == 0 || nSubmission == 0 || nSubmission <= pRing->nLatestSubmission ||
	     pRing->nPendingWrites == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pRing->nLatestSubmission = nSubmission;
	pRing->nPendingWrites = 0;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9RingComplete( sirD3D9UploadRing_t *pRing, uint64_t nSubmission ) noexcept
{
	if ( pRing == nullptr || pRing->nCapacity == 0 || nSubmission > pRing->nLatestSubmission ||
	     nSubmission < pRing->nCompletedSubmission ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pRing->nCompletedSubmission = nSubmission;
	return SIR_GRAPHICS_SUCCESS;
}

void sirD3D9RingReset( sirD3D9UploadRing_t *pRing ) noexcept
{
	if ( pRing == nullptr ) {
		return;
	}
	const uint32_t nCapacity = pRing->nCapacity;
	const uint32_t nGeneration = pRing->nGeneration;
	*pRing = {};
	pRing->nCapacity = nCapacity;
	pRing->nGeneration = nGeneration == UINT32_MAX ? 0 : nGeneration + 1;
}
