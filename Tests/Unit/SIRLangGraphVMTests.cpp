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

#include <Scripting/Language/SIRLangGraphVM.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct sirLangTestOperands_s
{
	uint32_t iFirstSource;
	uint32_t iSecondSource;
	uint32_t nAuxiliary;
	uint64_t nImmediate;
} sirLangTestOperands_t;

static sirLangCIRInstruction_t makeInstruction( sirLangOpcode_t eOpcode, sirLangValueKind_t eType, uint32_t iDestination,
                                                const sirLangTestOperands_t& operands ) noexcept
{
	return { eOpcode, eType, 0, iDestination, operands.iFirstSource, operands.iSecondSource, operands.nAuxiliary, operands.nImmediate, 0 };
}

static sirLangCIRProgram_t makeProgram( const sirLangCIRFunction_t *pFunctions, uint32_t nFunctions,
                                        const sirLangCIRInstruction_t *pInstructions, uint32_t nInstructions ) noexcept
{
	static const sirLangValueKind_t s_szTypes[] = { SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16 };
	sirLangCIRProgram_t program = {};
	program.pFunctions = pFunctions;
	program.nFunctionCount = nFunctions;
	program.pInstructions = pInstructions;
	program.nInstructionCount = nInstructions;
	program.pParameterTypes = s_szTypes;
	program.nParameterTypeCount = 2;
	program.nSchemaId = 1;
	program.nLayoutDigest = 2;
	program.nBindingDigest = 3;
	program.nRevision = SIR_LANG_VM_VERSION;
	return program;
}

static sirLangVMInvocation_t makeInvocation( uint64_t nFunctionId, uint64_t nFuel ) noexcept
{
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = nFunctionId;
	invocation.nFuel = nFuel;
	invocation.nExpectedLayoutDigest = 2;
	invocation.nExpectedBindingDigest = 3;
	return invocation;
}

static void testBasicCall( void ) noexcept
{
	const sirLangCIRFunction_t szFunctions[] = { { 100, 0, 4, 3, 2, 0, SIR_LANG_VALUE_U16 }, { 101, 4, 4, 3, 0, 0, SIR_LANG_VALUE_U16 } };
	const sirLangCIRInstruction_t szCode[] = { makeInstruction( SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_U16, 0, { 0, 0, 0, 0 } ),
	                                           makeInstruction( SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_U16, 1, { 1, 0, 0, 0 } ),
	                                           makeInstruction( SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, 2, { 0, 1, 0, 0 } ),
	                                           makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, { 2, 0, 0, 0 } ),
	                                           makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, { 0, 0, 0, 7 } ),
	                                           makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 1, { 0, 0, 0, 11 } ),
	                                           makeInstruction( SIR_LANG_OP_CALL, SIR_LANG_VALUE_U16, 2, { 0, 2, 0, 0 } ),
	                                           makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, { 2, 0, 0, 0 } ) };
	const uint32_t szCallArguments[] = { 0, 1 };
	sirLangCIRProgram_t program = makeProgram( szFunctions, 2, szCode, 8 );
	program.pCallArguments = szCallArguments;
	program.nCallArgumentCount = 2;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, makeInvocation( 101, 128 ) );
	assert( result.eError == SIR_LANG_VM_OK );
	assert( result.result.eType == SIR_LANG_VALUE_U16 && result.result.nBits == 18 );
	assert( result.nFuelConsumed == 8 );
	assert( SIRLangRunGraphVM( &program, makeInvocation( 101, 3 ) ).eError == SIR_LANG_VM_FUEL_EXHAUSTED );
	program.nLayoutDigest = 999;
	assert( SIRLangRunGraphVM( &program, makeInvocation( 101, 10 ) ).eError == SIR_LANG_VM_INVALID_FORMAT );
}

static void testBranchAndDataFlow( void ) noexcept
{
	const sirLangCIRFunction_t function = { 4, 0, 5, 3, 0, 0, SIR_LANG_VALUE_U16 };
	sirLangCIRInstruction_t szCode[] = { makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_BOOL, 0, { 0, 0, 0, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_JUMP_FALSE, SIR_LANG_VALUE_VOID, UINT32_MAX, { 0, 0, 3, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 1, { 0, 0, 0, 17 } ),
	                                     makeInstruction( SIR_LANG_OP_MOVE, SIR_LANG_VALUE_U16, 2, { 1, 0, 0, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, { 2, 0, 0, 0 } ) };
	sirLangCIRProgram_t program = makeProgram( &function, 1, szCode, 5 );
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_UNINITIALIZED );
	szCode[1].nAuxiliary = 4;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_UNINITIALIZED );
	szCode[1].nAuxiliary = 2;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	szCode[1].nAuxiliary = 99;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CONTROL_FLOW );
}

static void testCheckedArithmetic( void ) noexcept
{
	const sirLangCIRFunction_t function = { 7, 0, 4, 3, 0, 0, SIR_LANG_VALUE_U8 };
	sirLangCIRInstruction_t szCode[] = { makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U8, 0, { 0, 0, 0, 250 } ),
	                                     makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U8, 1, { 0, 0, 0, 8 } ),
	                                     makeInstruction( SIR_LANG_OP_ADD, SIR_LANG_VALUE_U8, 2, { 0, 1, 0, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U8, UINT32_MAX, { 2, 0, 0, 0 } ) };
	sirLangCIRProgram_t program = makeProgram( &function, 1, szCode, 4 );
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	assert( SIRLangRunGraphVM( &program, makeInvocation( 7, 20 ) ).eError == SIR_LANG_VM_ARITHMETIC_OVERFLOW );
	szCode[2].eOpcode = SIR_LANG_OP_SUBTRACT;
	assert( SIRLangRunGraphVM( &program, makeInvocation( 7, 20 ) ).result.nBits == 242 );
	szCode[2].eOpcode = SIR_LANG_OP_DIVIDE;
	szCode[1].nImmediate = 0;
	assert( SIRLangRunGraphVM( &program, makeInvocation( 7, 20 ) ).eError == SIR_LANG_VM_DIVIDE_ZERO );
	szCode[0].nImmediate = 256;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
}

static sirLangVMError_t nativeDouble( const sirLangVMCall_t& call, sirLangVMValue_t *pResult, void *pUser ) noexcept
{
	if ( call.nFunctionId != 999 || call.nSignatureId != 123 || call.nArgumentCount != 1 || pUser == nullptr ) {
		return SIR_LANG_VM_INVALID_CAPABILITY;
	}
	++*( uint32_t * )pUser;
	*pResult = { call.pArguments[0].nBits * 2, SIR_LANG_VALUE_U16 };
	return SIR_LANG_VM_OK;
}

static void testNativeCapability( void ) noexcept
{
	const sirLangCIRFunction_t function = { 81, 0, 3, 2, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t szCode[] = { makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, { 0, 0, 0, 21 } ),
	                                           makeInstruction( SIR_LANG_OP_CALL_NATIVE, SIR_LANG_VALUE_U16, 1, { 0, 1, 0, 0 } ),
	                                           makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, { 1, 0, 0, 0 } ) };
	const uint32_t szArgs[] = { 0 };
	const sirLangCIRCapability_t capability = { 999, 123, 1, 0, SIR_LANG_EFFECT_STATE_READ, 0, 1, SIR_LANG_VALUE_U16 };
	sirLangCIRProgram_t program = makeProgram( &function, 1, szCode, 3 );
	program.pCallArguments = szArgs;
	program.nCallArgumentCount = 1;
	program.pCapabilities = &capability;
	program.nCapabilityCount = 1;
	uint32_t nCalls = 0;
	sirLangVMInvocation_t invocation = makeInvocation( 81, 30 );
	invocation.pInvokeNative = nativeDouble;
	invocation.pUser = &nCalls;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, invocation );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 42 && nCalls == 1 );
	invocation.pInvokeNative = nullptr;
	assert( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_CAPABILITY );
}

static sirLangVMError_t nativeInvalidWidth( const sirLangVMCall_t& call, sirLangVMValue_t *pResult, void *pUser ) noexcept
{
	( void )call;
	( void )pUser;
	*pResult = { 65536, SIR_LANG_VALUE_U16 };
	return SIR_LANG_VM_OK;
}

static void testExternalValueCanonicalization( void ) noexcept
{
	const sirLangCIRFunction_t function = { 500, 0, 2, 1, 1, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t szCode[] = { makeInstruction( SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_U16, 0, { 0, 0, 0, 0 } ),
	                                           makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, { 0, 0, 0, 0 } ) };
	sirLangCIRProgram_t program = makeProgram( &function, 1, szCode, 2 );
	const sirLangVMValue_t arguments[] = { { 65536, SIR_LANG_VALUE_U16 } };
	sirLangVMInvocation_t invocation = makeInvocation( 500, 32 );
	invocation.pArguments = arguments;
	invocation.nArgumentCount = 1;
	assert( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_TYPE );

	const sirLangCIRFunction_t native = { 501, 0, 2, 1, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t szNativeCode[] = {
	    makeInstruction( SIR_LANG_OP_CALL_NATIVE, SIR_LANG_VALUE_U16, 0, { 0, 0, 0, 0 } ),
	    makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, { 0, 0, 0, 0 } ) };
	const sirLangCIRCapability_t capability = { 1234, 4321, 1, 0, SIR_LANG_EFFECT_PURE, 0, 0, SIR_LANG_VALUE_U16 };
	program = makeProgram( &native, 1, szNativeCode, 2 );
	program.pCapabilities = &capability;
	program.nCapabilityCount = 1;
	invocation = makeInvocation( 501, 32 );
	invocation.pInvokeNative = nativeInvalidWidth;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	assert( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_TYPE );
}

static void testPHIControlFlow( void ) noexcept
{
	const sirLangCIRFunction_t function = { 90, 0, 7, 4, 0, 0, SIR_LANG_VALUE_U16 };
	sirLangCIRInstruction_t szCode[] = { makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_BOOL, 0, { 0, 0, 0, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_JUMP_FALSE, SIR_LANG_VALUE_VOID, UINT32_MAX, { 0, 0, 4, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 1, { 0, 0, 0, 11 } ),
	                                     makeInstruction( SIR_LANG_OP_JUMP, SIR_LANG_VALUE_VOID, UINT32_MAX, { 0, 0, 5, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 2, { 0, 0, 0, 22 } ),
	                                     makeInstruction( SIR_LANG_OP_PHI, SIR_LANG_VALUE_U16, 3, { 1, 2, 3, 0 } ),
	                                     makeInstruction( SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, UINT32_MAX, { 3, 0, 0, 0 } ) };
	sirLangCIRProgram_t program = makeProgram( &function, 1, szCode, 7 );
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, makeInvocation( 90, 128 ) );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 22 );
	szCode[0].nImmediate = 1;
	result = SIRLangRunGraphVM( &program, makeInvocation( 90, 128 ) );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 11 );
	szCode[5].iSecondSource = 1;
	assert( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_UNINITIALIZED );
}

int main( void )
{
	testBasicCall();
	testBranchAndDataFlow();
	testCheckedArithmetic();
	testNativeCapability();
	testPHIControlFlow();
	testExternalValueCanonicalization();
	puts( "SIR-Lang GraphVM tests passed" );
	return 0;
}
