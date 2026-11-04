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

#include <assert.h>
#include <string.h>

static sirLangCIRInstruction_t makeInstruction( sirLangOpcode_t eOpcode, sirLangValueKind_t eType, uint32_t iDestination,
                                                 uint32_t iFirstSource ) noexcept
{
	sirLangCIRInstruction_t instruction = {};
	instruction.eOpcode = eOpcode;
	instruction.eType = eType;
	instruction.iDestination = iDestination;
	instruction.iFirstSource = iFirstSource;
	return instruction;
}

static void testBackendLifecycle( void ) noexcept
{
	const sirLangCIRFunction_t function = { 99, 0, 2, 1, 1, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t szInstructions[] = {
	    makeInstruction( SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_U16, 0, 0 ),
	    makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, 0 )
	};
	const sirLangValueKind_t parameter = SIR_LANG_VALUE_U16;
	sirLangCIRProgram_t program = {};
	program.pFunctions = &function;
	program.nFunctionCount = 1;
	program.pInstructions = szInstructions;
	program.nInstructionCount = 2;
	program.pParameterTypes = &parameter;
	program.nParameterTypeCount = 1;
	program.nSchemaId = 201;
	program.nLayoutDigest = 301;
	program.nBindingDigest = 401;
	program.nRevision = SIR_LANG_VM_VERSION;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );

	uint8_t szBytecode[1024] = {};
	uint32_t nBytes = 0;
	assert( SIRLangEncodeGraphVM( &program, szBytecode, sizeof( szBytecode ), &nBytes ) == SIR_LANG_VM_OK );

	sirLangCIRFunction_t szDecodedFunctions[4] = {};
	sirLangCIRInstruction_t szDecodedInstructions[16] = {};
	sirLangValueKind_t szDecodedParameters[8] = {};
	uint32_t szDecodedCallArguments[8] = {};
	sirLangGraphVMProgramSlot_t programs[1] = {};
	programs[0].storage.pFunctions = szDecodedFunctions;
	programs[0].storage.nFunctionCapacity = 4;
	programs[0].storage.pInstructions = szDecodedInstructions;
	programs[0].storage.nInstructionCapacity = 16;
	programs[0].storage.pParameterTypes = szDecodedParameters;
	programs[0].storage.nParameterTypeCapacity = 8;
	programs[0].storage.pCallArguments = szDecodedCallArguments;
	programs[0].storage.nCallArgumentCapacity = 8;

	sirLangGraphVMInstanceSlot_t instances[2] = {};
	sirLangGraphVMBackend_t backend = {};
	backend.pPrograms = programs;
	backend.nProgramCapacity = 1;
	backend.pInstances = instances;
	backend.nInstanceCapacity = 2;
	backend.nFormatId = 77;
	backend.nSchemaId = 201;
	backend.nLayoutDigest = 301;
	backend.nBindingDigest = 401;
	backend.nFuelPerInvoke = 64;
	const sirScriptBackendAPI_t api = SIRLangGraphVMBackendAPI( &backend );
	assert( api.nABIRevision == SIR_SCRIPT_BACKEND_ABI_REVISION );
	assert( api.pStart( api.pUser ) == SIR_SCRIPT_OK );

	sirScriptProgramView_t view = { 100, 1, 77, szBytecode, nBytes, alignof( uint64_t ) };
	uint64_t nProgramToken = 0;
	assert( api.pLoadProgram( api.pUser, view, &nProgramToken ) == SIR_SCRIPT_OK );
	assert( nProgramToken != 0 );
	uint64_t nDuplicate = 0;
	assert( api.pLoadProgram( api.pUser, view, &nDuplicate ) == SIR_SCRIPT_CAPACITY_EXCEEDED );

	uint64_t nInstanceToken = 0;
	assert( api.pCreateInstance( api.pUser, nProgramToken, 202, &nInstanceToken ) == SIR_SCRIPT_OK );
	assert( api.pInitializeInstance( api.pUser, nInstanceToken ) == SIR_SCRIPT_OK );

	const sirLangVMValue_t argument = { 42, SIR_LANG_VALUE_U16 };
	sirLangVMValue_t output = {};
	const sirScriptInvokeRequest_t call = {
	    nInstanceToken, 99,
	    { &argument, sizeof( argument ), alignof( sirLangVMValue_t ) },
	    { &output, sizeof( output ), alignof( sirLangVMValue_t ) }
	};
	assert( api.pInvoke( api.pUser, &call ) == SIR_SCRIPT_OK );
	assert( output.eType == SIR_LANG_VALUE_U16 && output.nBits == 42 );

	sirScriptInvokeRequest_t wrong = call;
	wrong.nMethodId = 1234;
	assert( api.pInvoke( api.pUser, &wrong ) == SIR_SCRIPT_NOT_FOUND );
	wrong = call;
	wrong.arguments.nBytes = 0;
	assert( api.pInvoke( api.pUser, &wrong ) == SIR_SCRIPT_TYPE_MISMATCH );

	api.pShutdownInstance( api.pUser, nInstanceToken );
	api.pDestroyInstance( api.pUser, nInstanceToken );
	assert( api.pInvoke( api.pUser, &call ) == SIR_SCRIPT_INVALID_HANDLE );
	api.pUnloadProgram( api.pUser, nProgramToken );
	szBytecode[nBytes - 1] ^= 1;
	assert( api.pLoadProgram( api.pUser, view, &nDuplicate ) == SIR_SCRIPT_TYPE_MISMATCH );
	api.pStop( api.pUser );
}

int main( void )
{
	testBackendLifecycle();
	return 0;
}
