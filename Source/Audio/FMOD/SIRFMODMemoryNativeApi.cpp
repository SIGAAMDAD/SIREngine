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

#include <Audio/FMOD/SIRFMODRuntime.h>
#include <Core/Containers/SIRRawStorage.h>
#include <fmod.hpp>
#include <stdint.h>
#include <string.h>

// FMOD's callbacks are process-global and may run on streaming/mixer threads.
// Once installed they remain installed for process lifetime; never install
// them against an ephemeral per-runtime allocator or replace them mid-flight.
typedef struct sirFMODNativeAllocationHeader_s
{
	size_t nPayloadBytes;
	uint64_t nSignature;
} sirFMODNativeAllocationHeader_t;

static constexpr uint64_t SIR_FMOD_NATIVE_ALLOCATION_SIGNATURE = 0x534952464D4F4441ULL;
static_assert( sizeof( sirFMODNativeAllocationHeader_t ) == 16, "Native FMOD allocation alignment contract" );
static sirContainerAllocator_t s_FMODNativeUpstream = {};
static SIRFMODMemoryBudget s_FMODNativeBudget;
static SIRAtomic<uint32_t> s_nFMODMemoryInstalled( 0 );
static uint64_t s_nFMODNativeBudgetBytes = 0;

static void *fmodNativeAllocate( unsigned int nBytes, FMOD_MEMORY_TYPE, const char * ) noexcept
{
	const sirContainerAllocator_t *pAllocator = s_FMODNativeBudget.getAllocator();
	if ( pAllocator == nullptr ) {
		return nullptr;
	}
	size_t nTotal = 0;
	if ( !SIRRawStorage::checkedAdd( ( size_t )nBytes, sizeof( sirFMODNativeAllocationHeader_t ), &nTotal ) ) {
		return nullptr;
	}
	void *pBase = pAllocator->pfnAllocate( pAllocator->pContext, nTotal, 16 );
	if ( pBase == nullptr ) {
		return nullptr;
	}
	sirFMODNativeAllocationHeader_t *pHeader = ( sirFMODNativeAllocationHeader_t * )pBase;
	pHeader->nPayloadBytes = nBytes;
	pHeader->nSignature = SIR_FMOD_NATIVE_ALLOCATION_SIGNATURE;
	return pHeader + 1;
}

static void fmodNativeFree( void *pMemory, FMOD_MEMORY_TYPE, const char * ) noexcept
{
	if ( pMemory == nullptr ) {
		return;
	}
	sirFMODNativeAllocationHeader_t *pHeader = ( ( sirFMODNativeAllocationHeader_t * )pMemory ) - 1;
	if ( pHeader->nSignature != SIR_FMOD_NATIVE_ALLOCATION_SIGNATURE ) {
		abort();
	}
	const sirContainerAllocator_t *pAllocator = s_FMODNativeBudget.getAllocator();
	if ( pAllocator == nullptr ) {
		abort();
	}
	pAllocator->pfnDeallocate( pAllocator->pContext, pHeader, pHeader->nPayloadBytes + sizeof( *pHeader ), 16 );
}

static void *fmodNativeReallocate( void *pMemory, unsigned int nBytes, FMOD_MEMORY_TYPE eType, const char *pszSource ) noexcept
{
	if ( pMemory == nullptr ) {
		return fmodNativeAllocate( nBytes, eType, pszSource );
	}
	if ( nBytes == 0 ) {
		fmodNativeFree( pMemory, eType, pszSource );
		return nullptr;
	}
	const sirFMODNativeAllocationHeader_t *pHeader = ( ( const sirFMODNativeAllocationHeader_t * )pMemory ) - 1;
	if ( pHeader->nSignature != SIR_FMOD_NATIVE_ALLOCATION_SIGNATURE ) {
		abort();
	}
	const size_t nOldBytes = pHeader->nPayloadBytes;
	void *pNew = fmodNativeAllocate( nBytes, eType, pszSource );
	if ( pNew == nullptr ) {
		return nullptr;
	}
	memcpy( pNew, pMemory, nOldBytes < nBytes ? nOldBytes : nBytes );
	fmodNativeFree( pMemory, eType, pszSource );
	return pNew;
}

// F_CALL is required by FMOD's C ABI on platforms using non-default
// callback calling conventions. Wrappers keep private storage format out
// of all portable SIREngine headers.
static void *F_CALL fmodNativeAllocateCallback( unsigned int nBytes, FMOD_MEMORY_TYPE eType, const char *pszSource )
{
	return fmodNativeAllocate( nBytes, eType, pszSource );
}

static void *F_CALL fmodNativeReallocateCallback( void *pMemory, unsigned int nBytes, FMOD_MEMORY_TYPE eType, const char *pszSource )
{
	return fmodNativeReallocate( pMemory, nBytes, eType, pszSource );
}

static void F_CALL fmodNativeFreeCallback( void *pMemory, FMOD_MEMORY_TYPE eType, const char *pszSource )
{
	fmodNativeFree( pMemory, eType, pszSource );
}

static sirAudioResult_t sirInstallFMODNativeMemory( const sirContainerAllocator_t *pAllocator, uint64_t nBudgetBytes ) noexcept
{
	if ( !SIRRawStorage::isValidAllocator( pAllocator ) || nBudgetBytes == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	uint32_t nExpected = 0;
	if ( !s_nFMODMemoryInstalled.compareExchangeStrong( nExpected, 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) ) {
		if ( nExpected == 2 && s_nFMODNativeBudgetBytes == nBudgetBytes && s_FMODNativeUpstream.pContext == pAllocator->pContext &&
		     s_FMODNativeUpstream.pfnAllocate == pAllocator->pfnAllocate &&
		     s_FMODNativeUpstream.pfnDeallocate == pAllocator->pfnDeallocate ) {
			return SIR_AUDIO_OK;
		}
		return SIR_AUDIO_BUSY;
	}
	s_FMODNativeUpstream = *pAllocator;
	s_nFMODNativeBudgetBytes = nBudgetBytes;
	if ( s_FMODNativeBudget.configure( &s_FMODNativeUpstream, nBudgetBytes ) != SIR_AUDIO_OK ) {
		s_nFMODMemoryInstalled.store( 0, SIR_MEMORY_ORDER_RELEASE );
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	// Must run before any Studio or Core System object exists.
	const FMOD_RESULT eResult = FMOD::Memory_Initialize( nullptr, 0, fmodNativeAllocateCallback, fmodNativeReallocateCallback,
	                                                     fmodNativeFreeCallback, FMOD_MEMORY_ALL );
	if ( eResult != FMOD_OK ) {
		s_FMODNativeBudget.shutdown();
		s_nFMODMemoryInstalled.store( 0, SIR_MEMORY_ORDER_RELEASE );
		return SIR_AUDIO_BACKEND_ERROR;
	}
	s_nFMODMemoryInstalled.store( 2, SIR_MEMORY_ORDER_RELEASE );
	return SIR_AUDIO_OK;
}

static sirFMODMemoryBudgetStats_t sirGetFMODNativeMemoryStats( void ) noexcept
{
	return s_nFMODMemoryInstalled.load( SIR_MEMORY_ORDER_ACQUIRE ) == 2 ? s_FMODNativeBudget.getStats() : sirFMODMemoryBudgetStats_t{};
}

sirFMODRuntimeMemoryNativeOps_t sirGetFMODNativeMemoryOps( void ) noexcept
{
	return { 1, sirInstallFMODNativeMemory, sirGetFMODNativeMemoryStats };
}
