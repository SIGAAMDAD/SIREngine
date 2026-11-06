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

#include <limits.h>
#include <string.h>
#include <stdio.h>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirLangNumericCase_s
{
	sirLangOpcode_t eOpcode;
	uint64_t nFirst;
	uint64_t nSecond;
	uint64_t nExpected;
	sirLangVMError_t eExpected;
} sirLangNumericCase_t;

static sirLangCIRProgram_t makeProgram( const sirLangCIRFunction_t *pFunction, const sirLangCIRInstruction_t *pCode,
                                        uint32_t nCode ) noexcept
{
	static constexpr sirLangValueKind_t s_szTypes[] = { SIR_LANG_VALUE_U16 };
	sirLangCIRProgram_t program = {};
	program.pFunctions = pFunction;
	program.nFunctionCount = 1;
	program.pInstructions = pCode;
	program.nInstructionCount = nCode;
	program.pParameterTypes = s_szTypes;
	program.nParameterTypeCount = 1;
	program.nSchemaId = 1;
	program.nLayoutDigest = 2;
	program.nBindingDigest = 3;
	program.nRevision = SIR_LANG_VM_VERSION;
	return program;
}

static sirLangVMInvocation_t makeInvocation( uint64_t nFunctionId ) noexcept
{
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = nFunctionId;
	invocation.nFuel = 32;
	invocation.nExpectedLayoutDigest = 2;
	invocation.nExpectedBindingDigest = 3;
	return invocation;
}

static bool testNumericCase( sirLangValueKind_t eType, const sirLangNumericCase_t& test ) noexcept
{
	const sirLangCIRFunction_t function = { 5300, 0, 4, 3, 0, 0, eType };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, eType, 0, 0, 0, 0, 0, test.nFirst, 1 },
	                                         { SIR_LANG_OP_CONST, eType, 0, 1, 0, 0, 0, test.nSecond, 2 },
	                                         { test.eOpcode, eType, 0, 2, 0, 1, 0, 0, 3 },
	                                         { SIR_LANG_OP_RETURN, eType, 0, UINT32_MAX, 2, 0, 0, 0, 4 } };
	const sirLangCIRProgram_t program = makeProgram( &function, code, 4 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, makeInvocation( 5300 ) );
	SIR_TEST_CHECK( result.eError == test.eExpected );
	if ( result.eError == SIR_LANG_VM_OK ) {
		SIR_TEST_CHECK( result.result.eType == eType && result.result.nBits == test.nExpected && result.nFuelConsumed == 4 );
	}
	return true;
}

static bool testIntegerWidths( void ) noexcept
{
	static constexpr sirLangValueKind_t s_szSignedTypes[] = { SIR_LANG_VALUE_I8, SIR_LANG_VALUE_I16, SIR_LANG_VALUE_I32,
	                                                          SIR_LANG_VALUE_I64 };
	static constexpr sirLangValueKind_t s_szUnsignedTypes[] = { SIR_LANG_VALUE_U8, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U32,
	                                                            SIR_LANG_VALUE_U64 };
	static constexpr uint32_t s_szWidths[] = { 8, 16, 32, 64 };
	for ( uint32_t iWidth = 0; iWidth < 4; ++iWidth ) {
		const uint32_t nWidth = s_szWidths[iWidth];
		const uint64_t nSignedMaximum = nWidth == 64 ? ( uint64_t )INT64_MAX : ( UINT64_C( 1 ) << ( nWidth - 1 ) ) - 1;
		const uint64_t nSignedMinimum = ~nSignedMaximum;
		const uint64_t nUnsignedMaximum = nWidth == 64 ? UINT64_MAX : ( UINT64_C( 1 ) << nWidth ) - 1;
		const sirLangNumericCase_t signedCases[] = {
		    { SIR_LANG_OP_ADD, 7, 3, 10, SIR_LANG_VM_OK },
		    { SIR_LANG_OP_ADD, UINT64_MAX, 2, 1, SIR_LANG_VM_OK },
		    { SIR_LANG_OP_ADD, nSignedMaximum, 1, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
		    { SIR_LANG_OP_SUBTRACT, 8, 3, 5, SIR_LANG_VM_OK },
		    { SIR_LANG_OP_SUBTRACT, nSignedMinimum, 1, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
		    { SIR_LANG_OP_MULTIPLY, 6, 3, 18, SIR_LANG_VM_OK },
		    { SIR_LANG_OP_MULTIPLY, UINT64_MAX - 2, 2, UINT64_MAX - 5, SIR_LANG_VM_OK },
		    { SIR_LANG_OP_MULTIPLY, nSignedMaximum, 2, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
		    { SIR_LANG_OP_DIVIDE, 18, 3, 6, SIR_LANG_VM_OK },
		    { SIR_LANG_OP_DIVIDE, UINT64_MAX - 5, 2, UINT64_MAX - 2, SIR_LANG_VM_OK },
		    { SIR_LANG_OP_DIVIDE, 10, 0, 0, SIR_LANG_VM_DIVIDE_ZERO },
		    { SIR_LANG_OP_DIVIDE, nSignedMinimum, UINT64_MAX, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW } };
		for ( const sirLangNumericCase_t& test : signedCases ) {
			SIR_TEST_CHECK( testNumericCase( s_szSignedTypes[iWidth], test ) );
		}
		const sirLangNumericCase_t unsignedCases[] = { { SIR_LANG_OP_ADD, 7, 3, 10, SIR_LANG_VM_OK },
		                                               { SIR_LANG_OP_ADD, nUnsignedMaximum, 1, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
		                                               { SIR_LANG_OP_SUBTRACT, 8, 3, 5, SIR_LANG_VM_OK },
		                                               { SIR_LANG_OP_SUBTRACT, 0, 1, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
		                                               { SIR_LANG_OP_MULTIPLY, 6, 3, 18, SIR_LANG_VM_OK },
		                                               { SIR_LANG_OP_MULTIPLY, nUnsignedMaximum, 2, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
		                                               { SIR_LANG_OP_DIVIDE, 18, 3, 6, SIR_LANG_VM_OK },
		                                               { SIR_LANG_OP_DIVIDE, 10, 0, 0, SIR_LANG_VM_DIVIDE_ZERO } };
		for ( const sirLangNumericCase_t& test : unsignedCases ) {
			SIR_TEST_CHECK( testNumericCase( s_szUnsignedTypes[iWidth], test ) );
		}
	}
	return true;
}

static bool testFloatingPointCanonicalConstants( void ) noexcept
{
	const sirLangCIRFunction_t function = { 5400, 0, 2, 1, 0, 0, SIR_LANG_VALUE_F32 };
	sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_F32, 0, 0, 0, 0, 0, UINT64_C( 0x3f800000 ), 1 },
	                                   { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_F32, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	sirLangCIRProgram_t program = makeProgram( &function, code, 2 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, makeInvocation( 5400 ) ).result.nBits == UINT64_C( 0x3f800000 ) );
	code[0].nImmediate = UINT64_C( 0x100000000 ) | UINT64_C( 0x3f800000 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	return true;
}

typedef struct sirLangComparisonCase_s
{
	sirLangValueKind_t eInputType;
	sirLangOpcode_t eOpcode;
	uint64_t nFirst;
	uint64_t nSecond;
	uint64_t nExpected;
} sirLangComparisonCase_t;

static bool testComparisonCase( const sirLangComparisonCase_t& test ) noexcept
{
	const sirLangCIRFunction_t function = { 5500, 0, 4, 3, 0, 0, SIR_LANG_VALUE_BOOL };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, test.eInputType, 0, 0, 0, 0, 0, test.nFirst, 1 },
	                                         { SIR_LANG_OP_CONST, test.eInputType, 0, 1, 0, 0, 0, test.nSecond, 2 },
	                                         { test.eOpcode, SIR_LANG_VALUE_BOOL, 0, 2, 0, 1, 0, 0, 3 },
	                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_BOOL, 0, UINT32_MAX, 2, 0, 0, 0, 4 } };
	const sirLangCIRProgram_t program = makeProgram( &function, code, 4 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, makeInvocation( 5500 ) );
	SIR_TEST_CHECK( result.eError == SIR_LANG_VM_OK && result.result.eType == SIR_LANG_VALUE_BOOL &&
	                result.result.nBits == test.nExpected );
	return true;
}

static bool testLogicAndComparisons( void ) noexcept
{
	static constexpr sirLangComparisonCase_t s_szComparisons[] = {
	    { SIR_LANG_VALUE_I8, SIR_LANG_OP_LESS, UINT64_MAX, 1, 1 },
	    { SIR_LANG_VALUE_I16, SIR_LANG_OP_LESS_EQUAL, 1, UINT64_MAX, 0 },
	    { SIR_LANG_VALUE_I32, SIR_LANG_OP_EQUAL, 1, 1, 1 },
	    { SIR_LANG_VALUE_I64, SIR_LANG_OP_LESS_EQUAL, UINT64_MAX - 2, UINT64_MAX - 2, 1 },
	    { SIR_LANG_VALUE_U8, SIR_LANG_OP_LESS, 255, 1, 0 },
	    { SIR_LANG_VALUE_U16, SIR_LANG_OP_LESS_EQUAL, 2, 2, 1 },
	    { SIR_LANG_VALUE_U32, SIR_LANG_OP_LESS, 2, 3, 1 },
	    { SIR_LANG_VALUE_U64, SIR_LANG_OP_LESS_EQUAL, 3, 2, 0 },
	    { SIR_LANG_VALUE_BOOL, SIR_LANG_OP_EQUAL, 1, 0, 0 },
	    { SIR_LANG_VALUE_BOOL, SIR_LANG_OP_LESS, 0, 1, 1 },
	    { SIR_LANG_VALUE_BOOL, SIR_LANG_OP_AND, 1, 1, 1 },
	    { SIR_LANG_VALUE_BOOL, SIR_LANG_OP_AND, 1, 0, 0 },
	    { SIR_LANG_VALUE_BOOL, SIR_LANG_OP_OR, 0, 1, 1 },
	    { SIR_LANG_VALUE_BOOL, SIR_LANG_OP_OR, 0, 0, 0 },
	    { SIR_LANG_VALUE_BOOL, SIR_LANG_OP_OR, 1, 1, 1 } };
	for ( const sirLangComparisonCase_t& test : s_szComparisons ) {
		SIR_TEST_CHECK( testComparisonCase( test ) );
	}
	for ( uint64_t nValue = 0; nValue < 2; ++nValue ) {
		const sirLangCIRFunction_t function = { 5510, 0, 3, 2, 0, 0, SIR_LANG_VALUE_BOOL };
		const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_BOOL, 0, 0, 0, 0, 0, nValue, 1 },
		                                         { SIR_LANG_OP_NOT, SIR_LANG_VALUE_BOOL, 0, 1, 0, 0, 0, 0, 2 },
		                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_BOOL, 0, UINT32_MAX, 1, 0, 0, 0, 3 } };
		const sirLangCIRProgram_t program = makeProgram( &function, code, 3 );
		SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
		const sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, makeInvocation( 5510 ) );
		SIR_TEST_CHECK( result.eError == SIR_LANG_VM_OK && result.result.nBits == 1 - nValue );
	}
	const sirLangCIRFunction_t function = { 5520, 0, 3, 2, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 991, 1 },
	                                         { SIR_LANG_OP_MOVE, SIR_LANG_VALUE_U16, 0, 1, 0, 0, 0, 0, 2 },
	                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 1, 0, 0, 0, 3 } };
	const sirLangCIRProgram_t program = makeProgram( &function, code, 3 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, makeInvocation( 5520 ) ).result.nBits == 991 );
	return true;
}

static bool testRecursiveCallLimit( void ) noexcept
{
	const sirLangCIRFunction_t function = { 5600, 0, 2, 1, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CALL, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 0, 1 },
	                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	const sirLangCIRProgram_t program = makeProgram( &function, code, 2 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	sirLangVMInvocation_t invocation = makeInvocation( 5600 );
	invocation.nFuel = 128;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_STACK_OVERFLOW );
	invocation.nFuel = 2;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_FUEL_EXHAUSTED );
	return true;
}

static sirLangVMError_t invokeNative( const sirLangVMCall_t& call, sirLangVMValue_t *pResult, void *pUser ) noexcept
{
	if ( pUser == nullptr || call.nFunctionId != 5730 || call.nSignatureId != 44 || call.nArgumentCount != 1 || call.nOwnerLane != 8 ) {
		return SIR_LANG_VM_INVALID_CAPABILITY;
	}
	++*( uint32_t * )pUser;
	*pResult = { call.pArguments[0].nBits + 1, SIR_LANG_VALUE_U16 };
	return SIR_LANG_VM_OK;
}

static bool testNativeDispatchContracts( void ) noexcept
{
	const sirLangCIRFunction_t function = { 5700, 0, 3, 2, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 20, 1 },
	                                         { SIR_LANG_OP_CALL_NATIVE, SIR_LANG_VALUE_U16, 0, 1, 0, 1, 0, 0, 2 },
	                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 1, 0, 0, 0, 3 } };
	static constexpr uint32_t s_szArguments[] = { 0 };
	sirLangCIRCapability_t capability = { 5730, 44, 1, 8, SIR_LANG_EFFECT_STATE_READ, 0, 1, SIR_LANG_VALUE_U16 };
	sirLangCIRProgram_t program = makeProgram( &function, code, 3 );
	program.pCapabilities = &capability;
	program.nCapabilityCount = 1;
	program.pCallArguments = s_szArguments;
	program.nCallArgumentCount = 1;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	sirLangVMInvocation_t invocation = makeInvocation( 5700 );
	uint32_t nInvocations = 0;
	invocation.pInvokeNative = invokeNative;
	invocation.pUser = &nInvocations;
	invocation.nOwnerLane = 8;
	const sirLangVMRunResult_t passed = SIRLangRunGraphVM( &program, invocation );
	SIR_TEST_CHECK( passed.eError == SIR_LANG_VM_OK && passed.result.nBits == 21 && nInvocations == 1 );
	invocation.nOwnerLane = 9;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_CAPABILITY && nInvocations == 1 );
	invocation.nOwnerLane = 8;
	invocation.pInvokeNative = nullptr;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_CAPABILITY );
	capability.nEffects = UINT32_MAX;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CAPABILITY );
	capability.nEffects = SIR_LANG_EFFECT_STATE_READ;
	capability.nBindingRevision = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CAPABILITY );
	return true;
}

static bool testProgramFormatRejections( void ) noexcept
{
	sirLangCIRFunction_t functions[] = { { 5800, 0, 2, 1, 0, 0, SIR_LANG_VALUE_U16 }, { 5801, 0, 2, 1, 0, 0, SIR_LANG_VALUE_U16 } };
	sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 1, 1 },
	                                   { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	sirLangCIRProgram_t program = makeProgram( functions, code, 2 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	program.nFunctionCount = 2;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	functions[1].nFunctionId = 5800;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program.nFunctionCount = 1;
	functions[0].nRegisterCount = SIR_LANG_VM_MAX_REGISTERS + 1;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	functions[0].nRegisterCount = 1;
	code[1].eOpcode = SIR_LANG_OP_ADD;
	code[1].iDestination = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE ||
	                SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_REGISTER );
	return true;
}

static bool testProgramInputGuards( void ) noexcept
{
	const sirLangCIRFunction_t baselineFunction = { 6000, 0, 4, 3, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 4, 1 },
	                                         { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 1, 0, 0, 0, 2, 2 },
	                                         { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, 0, 2, 0, 1, 0, 0, 3 },
	                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 2, 0, 0, 0, 4 } };
	sirLangCIRProgram_t program = makeProgram( &baselineFunction, code, 4 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	program.pFunctions = nullptr;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.pInstructions = nullptr;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.pParameterTypes = nullptr;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.nCallArgumentCount = 1;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.nCapabilityCount = 1;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.nFunctionCount = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.nRevision = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.nLayoutDigest = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	program = makeProgram( &baselineFunction, code, 4 );
	program.nBindingDigest = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	const sirLangCIRFunction_t malformedFunctions[] = {
	    { 0, 0, 4, 3, 0, 0, SIR_LANG_VALUE_U16 },     { 6000, 0, 4, 0, 0, 0, SIR_LANG_VALUE_U16 },
	    { 6000, 0, 4, 65, 0, 0, SIR_LANG_VALUE_U16 }, { 6000, 0, 0, 3, 0, 0, SIR_LANG_VALUE_U16 },
	    { 6000, 0, 5, 3, 0, 0, SIR_LANG_VALUE_U16 },  { 6000, 1, 4, 3, 0, 0, SIR_LANG_VALUE_U16 },
	    { 6000, 0, 4, 3, 33, 0, SIR_LANG_VALUE_U16 }, { 6000, 0, 4, 3, 1, 1, SIR_LANG_VALUE_U16 } };
	for ( const sirLangCIRFunction_t& malformed : malformedFunctions ) {
		program = makeProgram( &malformed, code, 4 );
		SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	}
	const sirLangValueKind_t nInvalidType = SIR_LANG_VALUE_OTHER;
	const sirLangCIRFunction_t parameterFunction = { 6001, 0, 2, 1, 1, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t parameterCode[] = { { SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 0, 1 },
	                                                  { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	program = makeProgram( &parameterFunction, parameterCode, 2 );
	program.pParameterTypes = &nInvalidType;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );

	static constexpr sirLangCIRFunction_t szDuplicates[] = { { 6000, 0, 4, 3, 0, 0, SIR_LANG_VALUE_U16 },
	                                                         { 6000, 4, 4, 3, 0, 0, SIR_LANG_VALUE_U16 } };
	program = makeProgram( szDuplicates, code, 4 );
	program.nFunctionCount = 2;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_FORMAT );
	return true;
}

static bool testVerifierOperandGuards( void ) noexcept
{
	const sirLangCIRFunction_t function = { 6100, 0, 4, 3, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t baseline[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 4, 1 },
	                                             { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 1, 0, 0, 0, 2, 2 },
	                                             { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, 0, 2, 0, 1, 0, 0, 3 },
	                                             { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 2, 0, 0, 0, 4 } };
	sirLangCIRInstruction_t code[4] = {};
	memcpy( code, baseline, sizeof( code ) );
	sirLangCIRProgram_t program = makeProgram( &function, code, 4 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	code[0].nImmediate = 65536;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[0].iDestination = 3;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_REGISTER );
	memcpy( code, baseline, sizeof( code ) );
	code[1].iDestination = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_REGISTER );
	memcpy( code, baseline, sizeof( code ) );
	code[2].iFirstSource = 40;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].iSecondSource = 40;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eType = SIR_LANG_VALUE_BOOL;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_EQUAL;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_LESS;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_NOT;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_AND;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_OR;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_CALL;
	code[2].iFirstSource = UINT32_MAX;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CALL );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_CALL;
	code[2].iFirstSource = 0;
	code[2].iSecondSource = 1;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CALL );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_CALL;
	code[2].iFirstSource = 0;
	code[2].iSecondSource = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_JUMP;
	code[2].iDestination = UINT32_MAX;
	code[2].nAuxiliary = 99;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CONTROL_FLOW );
	memcpy( code, baseline, sizeof( code ) );
	code[2].eOpcode = SIR_LANG_OP_JUMP_FALSE;
	code[2].iDestination = UINT32_MAX;
	code[2].nAuxiliary = 3;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CONTROL_FLOW );
	memcpy( code, baseline, sizeof( code ) );
	code[3].iDestination = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_REGISTER );
	memcpy( code, baseline, sizeof( code ) );
	code[3].iFirstSource = 30;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	const sirLangCIRFunction_t voidFunction = { 6110, 0, 1, 1, 0, 0, SIR_LANG_VALUE_VOID };
	sirLangCIRInstruction_t voidReturn = { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_VOID, 0, UINT32_MAX, UINT32_MAX, 0, 0, 0, 1 };
	program = makeProgram( &voidFunction, &voidReturn, 1 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, makeInvocation( 6110 ) ).eError == SIR_LANG_VM_OK );
	voidReturn.iFirstSource = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_TYPE );
	return true;
}

static bool testRuntimeInputGuards( void ) noexcept
{
	const sirLangCIRFunction_t function = { 6200, 0, 2, 1, 1, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 0, 1 },
	                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	sirLangCIRProgram_t program = makeProgram( &function, code, 2 );
	sirLangVMValue_t argument = { 5, SIR_LANG_VALUE_U16 };
	sirLangVMInvocation_t invocation = makeInvocation( 6200 );
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).result.nBits == 5 );
	invocation.nFuel = 0;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_FORMAT );
	invocation = makeInvocation( 6200 );
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nExpectedLayoutDigest = 0;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_FORMAT );
	invocation = makeInvocation( 6200 );
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nExpectedBindingDigest = 0;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_FORMAT );
	invocation = makeInvocation( 6200 );
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nFunctionId = 0;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_ARGUMENT );
	invocation = makeInvocation( 6200 );
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_ARGUMENT );
	invocation.nArgumentCount = 1;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_ARGUMENT );
	invocation.pArguments = &argument;
	argument.nBits = 65536;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_TYPE );
	argument = { 5, SIR_LANG_VALUE_U8 };
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_TYPE );
	const sirLangCIRFunction_t floatFunction = { 6201, 0, 2, 1, 1, 0, SIR_LANG_VALUE_F64 };
	const sirLangCIRInstruction_t floatCode[] = { { SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_F64, 0, 0, 0, 0, 0, 0, 1 },
	                                              { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_F64, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	const sirLangValueKind_t floatType = SIR_LANG_VALUE_F64;
	program = makeProgram( &floatFunction, floatCode, 2 );
	program.pParameterTypes = &floatType;
	argument = { UINT64_MAX, SIR_LANG_VALUE_F64 };
	invocation = makeInvocation( 6201 );
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	const sirLangVMRunResult_t floatResult = SIRLangRunGraphVM( &program, invocation );
	SIR_TEST_CHECK( floatResult.eError == SIR_LANG_VM_OK && floatResult.result.nBits == UINT64_MAX );
	return true;
}

typedef struct sirLangNativeResultFixture_s
{
	sirLangVMError_t eResult;
	sirLangValueKind_t eReturnType;
	uint64_t nBits;
	uint32_t nCalls;
} sirLangNativeResultFixture_t;

static sirLangVMError_t customNative( const sirLangVMCall_t& call, sirLangVMValue_t *pOutput, void *pUser ) noexcept
{
	( void )call;
	sirLangNativeResultFixture_t *pState = ( sirLangNativeResultFixture_t * )pUser;
	++pState->nCalls;
	*pOutput = { pState->nBits, pState->eReturnType };
	return pState->eResult;
}

static bool testNativeOutputGuards( void ) noexcept
{
	const sirLangCIRFunction_t function = { 6300, 0, 2, 1, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CALL_NATIVE, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 0, 1 },
	                                         { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	sirLangCIRCapability_t capability = { 6301, 6302, 1, 0, SIR_LANG_EFFECT_PURE, 0, 0, SIR_LANG_VALUE_U16 };
	sirLangCIRProgram_t program = makeProgram( &function, code, 2 );
	program.pCapabilities = &capability;
	program.nCapabilityCount = 1;
	sirLangNativeResultFixture_t fixture = { SIR_LANG_VM_OK, SIR_LANG_VALUE_U16, 42, 0 };
	sirLangVMInvocation_t invocation = makeInvocation( 6300 );
	invocation.pInvokeNative = customNative;
	invocation.pUser = &fixture;
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).result.nBits == 42 && fixture.nCalls == 1 );
	fixture = { SIR_LANG_VM_INVALID_CALL, SIR_LANG_VALUE_U16, 1, 0 };
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_CALL && fixture.nCalls == 1 );
	fixture = { SIR_LANG_VM_OK, SIR_LANG_VALUE_U8, 1, 0 };
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_TYPE && fixture.nCalls == 1 );
	fixture = { SIR_LANG_VM_OK, SIR_LANG_VALUE_U16, 65536, 0 };
	SIR_TEST_CHECK( SIRLangRunGraphVM( &program, invocation ).eError == SIR_LANG_VM_INVALID_TYPE && fixture.nCalls == 1 );
	capability.nFunctionId = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CAPABILITY );
	capability.nFunctionId = 6301;
	capability.nSignatureId = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CAPABILITY );
	capability.nSignatureId = 6302;
	capability.nParameterCount = 33;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CAPABILITY );
	capability.nParameterCount = 1;
	capability.iFirstParameterType = 1;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_INVALID_CAPABILITY );
	return true;
}

typedef struct sirLangVerifierCase_s
{
	sirLangOpcode_t eOpcode;
	sirLangValueKind_t eFirstType;
	sirLangValueKind_t eSecondType;
	sirLangValueKind_t eResultType;
	uint32_t iFirst;
	uint32_t iSecond;
} sirLangVerifierCase_t;

static bool testRejectVerifierCase( const sirLangVerifierCase_t& test ) noexcept
{
	const sirLangCIRFunction_t function = { 6400, 0, 4, 3, 0, 0, test.eResultType };
	const sirLangCIRInstruction_t code[] = { { SIR_LANG_OP_CONST, test.eFirstType, 0, 0, 0, 0, 0, 1, 1 },
	                                         { SIR_LANG_OP_CONST, test.eSecondType, 0, 1, 0, 0, 0, 2, 2 },
	                                         { test.eOpcode, test.eResultType, 0, 2, test.iFirst, test.iSecond, 0, 0, 3 },
	                                         { SIR_LANG_OP_RETURN, test.eResultType, 0, UINT32_MAX, 2, 0, 0, 0, 4 } };
	const sirLangCIRProgram_t program = makeProgram( &function, code, 4 );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) != SIR_LANG_VM_OK );
	return true;
}

static bool testVerifierTypeBranches( void ) noexcept
{
	static constexpr sirLangVerifierCase_t s_szCases[] = {
	    { SIR_LANG_OP_MOVE, SIR_LANG_VALUE_U8, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_MOVE, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 2, 1 },
	    { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U8, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U8, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U8, 0, 1 },
	    { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 2, 1 },
	    { SIR_LANG_OP_ADD, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 0, 2 },
	    { SIR_LANG_OP_EQUAL, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_EQUAL, SIR_LANG_VALUE_F32, SIR_LANG_VALUE_F32, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_EQUAL, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U8, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_EQUAL, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, 2, 1 },
	    { SIR_LANG_OP_EQUAL, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, 0, 2 },
	    { SIR_LANG_OP_NOT, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_NOT, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_NOT, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 2, 1 },
	    { SIR_LANG_OP_AND, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_AND, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_AND, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_AND, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 2, 1 },
	    { SIR_LANG_OP_AND, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 0, 2 },
	    { SIR_LANG_OP_OR, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_OR, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_BOOL, 0, 1 },
	    { SIR_LANG_OP_OR, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_OR, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 2, 1 },
	    { SIR_LANG_OP_OR, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, SIR_LANG_VALUE_BOOL, 0, 2 },
	    { SIR_LANG_OP_PHI, SIR_LANG_VALUE_U8, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_PHI, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U8, SIR_LANG_VALUE_U16, 0, 1 },
	    { SIR_LANG_OP_PHI, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 2, 1 },
	    { SIR_LANG_OP_PHI, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, SIR_LANG_VALUE_U16, 0, 2 } };
	for ( const sirLangVerifierCase_t& test : s_szCases ) {
		SIR_TEST_CHECK( testRejectVerifierCase( test ) );
	}
	return true;
}

static bool testSignedPolarityBranches( void ) noexcept
{
	static constexpr sirLangNumericCase_t s_szCases[] = {
	    { SIR_LANG_OP_ADD, 3, UINT64_MAX, 2, SIR_LANG_VM_OK },
	    { SIR_LANG_OP_ADD, UINT64_MAX - 2, 1, UINT64_MAX - 1, SIR_LANG_VM_OK },
	    { SIR_LANG_OP_ADD, ( uint64_t )INT64_MIN, UINT64_MAX, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
	    { SIR_LANG_OP_SUBTRACT, 3, UINT64_MAX, 4, SIR_LANG_VM_OK },
	    { SIR_LANG_OP_SUBTRACT, ( uint64_t )INT64_MAX, UINT64_MAX, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
	    { SIR_LANG_OP_SUBTRACT, UINT64_MAX - 2, UINT64_MAX, UINT64_MAX - 1, SIR_LANG_VM_OK },
	    { SIR_LANG_OP_MULTIPLY, ( uint64_t )INT64_MAX, UINT64_MAX, ( uint64_t )( -INT64_MAX ), SIR_LANG_VM_OK },
	    { SIR_LANG_OP_MULTIPLY, ( uint64_t )INT64_MAX, UINT64_MAX - 1, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
	    { SIR_LANG_OP_MULTIPLY, ( uint64_t )INT64_MIN, 2, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
	    { SIR_LANG_OP_MULTIPLY, UINT64_MAX - 2, UINT64_MAX - 1, 6, SIR_LANG_VM_OK },
	    { SIR_LANG_OP_MULTIPLY, UINT64_MAX - 2, ( uint64_t )INT64_MIN, 0, SIR_LANG_VM_ARITHMETIC_OVERFLOW },
	    { SIR_LANG_OP_MULTIPLY, 0, ( uint64_t )INT64_MIN, 0, SIR_LANG_VM_OK },
	    { SIR_LANG_OP_DIVIDE, ( uint64_t )INT64_MIN, 2, ( uint64_t )( INT64_MIN / 2 ), SIR_LANG_VM_OK } };
	for ( const sirLangNumericCase_t& test : s_szCases ) {
		SIR_TEST_CHECK( testNumericCase( SIR_LANG_VALUE_I64, test ) );
	}
	static constexpr sirLangNumericCase_t s_UnsignedZero = { SIR_LANG_OP_MULTIPLY, 4, 0, 0, SIR_LANG_VM_OK };
	SIR_TEST_CHECK( testNumericCase( SIR_LANG_VALUE_U64, s_UnsignedZero ) );
	return true;
}

int main( void )
{
	if ( !testIntegerWidths() || !testFloatingPointCanonicalConstants() || !testLogicAndComparisons() || !testRecursiveCallLimit() ||
	     !testNativeDispatchContracts() || !testProgramFormatRejections() || !testProgramInputGuards() || !testVerifierOperandGuards() ||
	     !testRuntimeInputGuards() || !testNativeOutputGuards() || !testVerifierTypeBranches() || !testSignedPolarityBranches() ) {
		return 1;
	}
	puts( "GraphVM numeric and canonical constant tests passed" );
	return 0;
}
