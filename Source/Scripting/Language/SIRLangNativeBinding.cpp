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

#include <Scripting/Language/SIRLangNativeBinding.h>

sirLangVMError_t SIRLangInvokeNativeBinding( const sirLangVMCall_t& call, sirLangVMValue_t *pResult, void *pUser ) noexcept
{
	const sirLangNativeBindingContext_t *pContext = ( const sirLangNativeBindingContext_t * )pUser;
	if ( pContext == nullptr || pContext->pRegistry == nullptr || pContext->nBindingDigest == 0 ) {
		return SIR_LANG_VM_INVALID_CAPABILITY;
	}
	const SIRScriptBindingRegistry *pRegistry = pContext->pRegistry;
	if ( !pRegistry->isPublished() || pRegistry->schemaFingerprint() != pContext->nBindingDigest ||
	     call.nArgumentCount > SIR_LANG_VM_MAX_PARAMETERS ) {
		return SIR_LANG_VM_INVALID_CAPABILITY;
	}

	sirScriptFunctionDescriptor_t descriptor = {};
	if ( !pRegistry->describeFunction( call.nFunctionId, &descriptor ) ) {
		return SIR_LANG_VM_INVALID_CAPABILITY;
	}
	if ( descriptor.nSignatureId != call.nSignatureId || descriptor.nBindingRevision != call.nBindingRevision ||
	     descriptor.nOwnerLane != call.nOwnerLane || descriptor.nOwnerLane != pContext->nOwnerLane ||
	     descriptor.nEffects != call.nEffects ) {
		return SIR_LANG_VM_INVALID_CAPABILITY;
	}
	if ( call.nArgumentCount > 0 && call.pArguments == nullptr ) {
		return SIR_LANG_VM_INVALID_ARGUMENT;
	}
	if ( descriptor.nReturnTypeId != 0 && pResult == nullptr ) {
		return SIR_LANG_VM_INVALID_ARGUMENT;
	}

	const uint32_t nBytes = call.nArgumentCount * sizeof( sirLangVMValue_t );
	const sirScriptReadFrame_t arguments = { nBytes != 0 ? call.pArguments : nullptr, nBytes,
	                                         nBytes != 0 ? alignof( sirLangVMValue_t ) : 1 };
	const sirScriptWriteFrame_t result = { descriptor.nReturnTypeId != 0 ? pResult : nullptr,
	                                       descriptor.nReturnTypeId != 0 ? ( uint32_t )sizeof( sirLangVMValue_t ) : 0,
	                                       descriptor.nReturnTypeId != 0 ? ( uint32_t )alignof( sirLangVMValue_t ) : 1 };
	const sirScriptNativeInvocation_t request = { call.nFunctionId, call.nSignatureId, call.nBindingRevision, arguments, result };
	const sirScriptResult_t eResult = pRegistry->invokeNative( request );
	return eResult == SIR_SCRIPT_OK ? SIR_LANG_VM_OK : SIR_LANG_VM_INVALID_CAPABILITY;
}
