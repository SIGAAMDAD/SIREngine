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

int main( void )
{
	if ( !testIntegerWidths() || !testFloatingPointCanonicalConstants() ) {
		return 1;
	}
	puts( "GraphVM numeric and canonical constant tests passed" );
	return 0;
}
