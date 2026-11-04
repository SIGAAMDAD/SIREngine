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

#include <Scripting/Language/SIRLangGraphVMBackend.h>

#include <string.h>

static sirLangGraphVMProgramSlot_t *sirLangBackendProgram( sirLangGraphVMBackend_t *pBackend, uint64_t nToken ) noexcept
{
	if ( pBackend == nullptr || pBackend->pPrograms == nullptr || nToken == 0 ) {
		return nullptr;
	}
	for ( uint32_t iProgram = 0; iProgram < pBackend->nProgramCapacity; ++iProgram ) {
		sirLangGraphVMProgramSlot_t *pProgram = &pBackend->pPrograms[iProgram];
		if ( pProgram->bLoaded && pProgram->nToken == nToken ) {
			return pProgram;
		}
	}
	return nullptr;
}

static sirLangGraphVMInstanceSlot_t *sirLangBackendInstance( sirLangGraphVMBackend_t *pBackend, uint64_t nToken ) noexcept
{
	if ( pBackend == nullptr || pBackend->pInstances == nullptr || nToken == 0 ) {
		return nullptr;
	}
	for ( uint32_t iInstance = 0; iInstance < pBackend->nInstanceCapacity; ++iInstance ) {
		sirLangGraphVMInstanceSlot_t *pInstance = &pBackend->pInstances[iInstance];
		if ( pInstance->bActive && pInstance->nToken == nToken ) {
			return pInstance;
		}
	}
	return nullptr;
}

static bool sirLangBackendNextToken( sirLangGraphVMBackend_t *pBackend, uint64_t *pToken ) noexcept
{
	if ( pBackend->nTokenSequence == UINT64_MAX ) {
		return false;
	}
	*pToken = ++pBackend->nTokenSequence;
	return true;
}

static sirScriptResult_t sirLangBackendStart( void *pContext ) noexcept
{
	sirLangGraphVMBackend_t *pBackend = ( sirLangGraphVMBackend_t * )pContext;
	if ( pBackend == nullptr || pBackend->bStarted ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pBackend->pPrograms == nullptr || pBackend->pInstances == nullptr ||
	     pBackend->nProgramCapacity == 0 || pBackend->nInstanceCapacity == 0 ||
	     pBackend->nFormatId == 0 || pBackend->nSchemaId == 0 ||
	     pBackend->nLayoutDigest == 0 || pBackend->nBindingDigest == 0 || pBackend->nFuelPerInvoke == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	for ( uint32_t iProgram = 0; iProgram < pBackend->nProgramCapacity; ++iProgram ) {
		if ( pBackend->pPrograms[iProgram].bLoaded ) {
			return SIR_SCRIPT_BUSY;
		}
	}
	for ( uint32_t iInstance = 0; iInstance < pBackend->nInstanceCapacity; ++iInstance ) {
		if ( pBackend->pInstances[iInstance].bActive ) {
			return SIR_SCRIPT_BUSY;
		}
	}
	pBackend->bStarted = true;
	return SIR_SCRIPT_OK;
}

static void sirLangBackendStop( void *pContext ) noexcept
{
	sirLangGraphVMBackend_t *pBackend = ( sirLangGraphVMBackend_t * )pContext;
	if ( pBackend != nullptr ) {
		pBackend->bStarted = false;
	}
}

static sirScriptResult_t sirLangBackendLoad( void *pContext, sirScriptProgramView_t view, uint64_t *pToken ) noexcept
{
	sirLangGraphVMBackend_t *pBackend = ( sirLangGraphVMBackend_t * )pContext;
	if ( pBackend == nullptr || !pBackend->bStarted ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pToken == nullptr || view.nFormatId != pBackend->nFormatId ||
	     view.pData == nullptr || view.nBytes > UINT32_MAX || view.nBytes < SIR_LANG_BYTECODE_HEADER_BYTES ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	*pToken = 0;
	for ( uint32_t iProgram = 0; iProgram < pBackend->nProgramCapacity; ++iProgram ) {
		sirLangGraphVMProgramSlot_t& slot = pBackend->pPrograms[iProgram];
		if ( slot.bLoaded ) {
			continue;
		}
		const sirLangVMError_t eDecode = SIRLangDecodeGraphVM( ( const uint8_t * )view.pData, ( uint32_t )view.nBytes,
		                                                       &slot.storage );
		if ( eDecode != SIR_LANG_VM_OK ) {
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
		const sirLangCIRProgram_t& program = slot.storage.program;
		if ( program.nSchemaId != pBackend->nSchemaId || program.nLayoutDigest != pBackend->nLayoutDigest ||
		     program.nBindingDigest != pBackend->nBindingDigest ) {
			slot.storage.program = {};
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
		if ( !sirLangBackendNextToken( pBackend, pToken ) ) {
			slot.storage.program = {};
			return SIR_SCRIPT_CAPACITY_EXCEEDED;
		}
		slot.nToken = *pToken;
		slot.nInstances = 0;
		slot.bLoaded = true;
		return SIR_SCRIPT_OK;
	}
	return SIR_SCRIPT_CAPACITY_EXCEEDED;
}

static void sirLangBackendUnload( void *pContext, uint64_t nToken ) noexcept
{
	sirLangGraphVMBackend_t *pBackend = ( sirLangGraphVMBackend_t * )pContext;
	sirLangGraphVMProgramSlot_t *pProgram = sirLangBackendProgram( pBackend, nToken );
	if ( pProgram == nullptr || pProgram->nInstances != 0 ) {
		return;
	}
	pProgram->bLoaded = false;
	pProgram->nToken = 0;
	pProgram->storage.program = {};
}

static sirScriptResult_t sirLangBackendCreate( void *pContext, uint64_t nProgramToken, uint64_t nOwnerId,
                                               uint64_t *pInstanceToken ) noexcept
{
	sirLangGraphVMBackend_t *pBackend = ( sirLangGraphVMBackend_t * )pContext;
	if ( pBackend == nullptr || !pBackend->bStarted || pInstanceToken == nullptr || nOwnerId == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	*pInstanceToken = 0;
	sirLangGraphVMProgramSlot_t *pProgram = sirLangBackendProgram( pBackend, nProgramToken );
	if ( pProgram == nullptr ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	for ( uint32_t iInstance = 0; iInstance < pBackend->nInstanceCapacity; ++iInstance ) {
		sirLangGraphVMInstanceSlot_t& slot = pBackend->pInstances[iInstance];
		if ( slot.bActive ) {
			continue;
		}
		if ( !sirLangBackendNextToken( pBackend, pInstanceToken ) ) {
			return SIR_SCRIPT_CAPACITY_EXCEEDED;
		}
		slot.nToken = *pInstanceToken;
		slot.nOwnerId = nOwnerId;
		slot.iProgram = ( uint32_t )( pProgram - pBackend->pPrograms );
		slot.bInitialized = false;
		slot.bActive = true;
		++pProgram->nInstances;
		return SIR_SCRIPT_OK;
	}
	return SIR_SCRIPT_CAPACITY_EXCEEDED;
}

static sirScriptResult_t sirLangBackendInitialize( void *pContext, uint64_t nToken ) noexcept
{
	sirLangGraphVMInstanceSlot_t *pInstance = sirLangBackendInstance( ( sirLangGraphVMBackend_t * )pContext, nToken );
	if ( pInstance == nullptr ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	if ( pInstance->bInitialized ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	pInstance->bInitialized = true;
	return SIR_SCRIPT_OK;
}

static void sirLangBackendShutdown( void *pContext, uint64_t nToken ) noexcept
{
	sirLangGraphVMInstanceSlot_t *pInstance = sirLangBackendInstance( ( sirLangGraphVMBackend_t * )pContext, nToken );
	if ( pInstance != nullptr ) {
		pInstance->bInitialized = false;
	}
}

static void sirLangBackendDestroy( void *pContext, uint64_t nToken ) noexcept
{
	sirLangGraphVMBackend_t *pBackend = ( sirLangGraphVMBackend_t * )pContext;
	sirLangGraphVMInstanceSlot_t *pInstance = sirLangBackendInstance( pBackend, nToken );
	if ( pInstance == nullptr ) {
		return;
	}
	sirLangGraphVMProgramSlot_t& program = pBackend->pPrograms[pInstance->iProgram];
	if ( program.nInstances != 0 ) {
		--program.nInstances;
	}
	pInstance->bInitialized = false;
	pInstance->bActive = false;
	pInstance->nToken = 0;
	pInstance->nOwnerId = 0;
}

static sirScriptResult_t sirLangBackendInvoke( void *pContext, const sirScriptInvokeRequest_t *pRequest ) noexcept
{
	sirLangGraphVMBackend_t *pBackend = ( sirLangGraphVMBackend_t * )pContext;
	if ( pBackend == nullptr || !pBackend->bStarted || pRequest == nullptr || pRequest->nMethodId == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	sirLangGraphVMInstanceSlot_t *pInstance = sirLangBackendInstance( pBackend, pRequest->nInstanceToken );
	if ( pInstance == nullptr || !pInstance->bInitialized ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	const sirLangCIRProgram_t& program = pBackend->pPrograms[pInstance->iProgram].storage.program;
	const sirLangCIRFunction_t *pFunction = nullptr;
	for ( uint32_t iFunction = 0; iFunction < program.nFunctionCount; ++iFunction ) {
		if ( program.pFunctions[iFunction].nFunctionId == pRequest->nMethodId ) {
			pFunction = &program.pFunctions[iFunction];
			break;
		}
	}
	if ( pFunction == nullptr ) {
		return SIR_SCRIPT_NOT_FOUND;
	}

	const uint32_t nExpectedBytes = pFunction->nParameterCount * sizeof( sirLangVMValue_t );
	if ( pRequest->arguments.nBytes != nExpectedBytes ||
	     ( nExpectedBytes > 0 && pRequest->arguments.pData == nullptr ) ||
	     ( pFunction->eReturnType == SIR_LANG_VALUE_VOID && pRequest->result.nBytes != 0 ) ||
	     ( pFunction->eReturnType != SIR_LANG_VALUE_VOID &&
	       ( pRequest->result.nBytes != sizeof( sirLangVMValue_t ) || pRequest->result.pData == nullptr ) ) ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	sirLangVMValue_t szArguments[SIR_LANG_VM_MAX_PARAMETERS] = {};
	if ( nExpectedBytes > 0 ) {
		memcpy( szArguments, pRequest->arguments.pData, nExpectedBytes );
	}

	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = pRequest->nMethodId;
	invocation.pArguments = szArguments;
	invocation.nArgumentCount = pFunction->nParameterCount;
	invocation.nFuel = pBackend->nFuelPerInvoke;
	invocation.pInvokeNative = pBackend->pNativeCall;
	invocation.pUser = pBackend->pNativeUser;
	invocation.nExpectedLayoutDigest = pBackend->nLayoutDigest;
	invocation.nExpectedBindingDigest = pBackend->nBindingDigest;
	invocation.nOwnerLane = pBackend->nOwnerLane;
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, invocation );
	if ( result.eError != SIR_LANG_VM_OK ) {
		return result.eError == SIR_LANG_VM_FUEL_EXHAUSTED ? SIR_SCRIPT_CAPACITY_EXCEEDED : SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( pFunction->eReturnType != SIR_LANG_VALUE_VOID ) {
		memcpy( pRequest->result.pData, &result.result, sizeof( result.result ) );
	}
	return SIR_SCRIPT_OK;
}

sirScriptBackendAPI_t SIRLangGraphVMBackendAPI( sirLangGraphVMBackend_t *pBackend ) noexcept
{
	sirScriptBackendAPI_t api = {};
	api.nABIRevision = SIR_SCRIPT_BACKEND_ABI_REVISION;
	api.pUser = pBackend;
	api.pStart = sirLangBackendStart;
	api.pStop = sirLangBackendStop;
	api.pLoadProgram = sirLangBackendLoad;
	api.pUnloadProgram = sirLangBackendUnload;
	api.pCreateInstance = sirLangBackendCreate;
	api.pInitializeInstance = sirLangBackendInitialize;
	api.pShutdownInstance = sirLangBackendShutdown;
	api.pDestroyInstance = sirLangBackendDestroy;
	api.pInvoke = sirLangBackendInvoke;
	return api;
}
