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

#include <Scripting/Core/SIRScriptMemoryRouter.h>
#include <Core/Memory/SIRAllocationRouter.h>

sirScriptPayloadAllocatorAPI_t SIRScriptMemoryRouter::makeAPI( SIRAllocationRouter *pRouter, uint32_t nMaximumBytes,
                                                               uint32_t nMaximumAlignment ) noexcept
{
	if ( pRouter == nullptr ) {
		return {};
	}
	return { SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION, pRouter, nMaximumBytes, nMaximumAlignment, allocate, release };
}

sirScriptResult_t SIRScriptMemoryRouter::allocate( void *pContext, uint32_t nBytes, uint32_t nAlignment,
                                                   sirScriptPayloadAllocation_t *pAllocation ) noexcept
{
	if ( pContext == nullptr || pAllocation == nullptr || nBytes == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pAllocation = {};
	SIRAllocationRouter *pRouter = ( SIRAllocationRouter * )pContext;
	sirRoutedAllocation_t allocation = {};
	if ( !pRouter->alloc( nBytes, nAlignment, &allocation, SIR_LIFETIME_ARBITRARY ) ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	// Preserve the routed backing and exact requested size without a second
	// metadata allocation. Both values are 32 bits in sirRoutedAllocation_t.
	pAllocation->pData = allocation.pMemory;
	pAllocation->nToken = ( ( uint64_t )allocation.nBacking << 32 ) | allocation.nBytes;
	return SIR_SCRIPT_OK;
}

void SIRScriptMemoryRouter::release( void *pContext, sirScriptPayloadAllocation_t allocation ) noexcept
{
	if ( pContext == nullptr || allocation.pData == nullptr || allocation.nToken == 0 ) {
		return;
	}
	SIRAllocationRouter *pRouter = ( SIRAllocationRouter * )pContext;
	sirRoutedAllocation_t routed = { allocation.pData, ( uint32_t )allocation.nToken, ( uint32_t )( allocation.nToken >> 32 ) };
	( void )pRouter->free( &routed );
}
