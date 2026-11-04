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

#include <Scripting/Language/SIRLangBytecode.h>
#include <Scripting/Core/SIRScriptSHA256.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write32( uint8_t *pDestination, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pDestination[i] = ( uint8_t )( nValue >> ( 8 * i ) );
	}
}

static void resign( uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	uint8_t szPair[64] = {};
	SIRScriptSHA256::digest( pBytes, 64, szPair );
	SIRScriptSHA256::digest( pBytes + SIR_LANG_BYTECODE_HEADER_BYTES, nBytes - SIR_LANG_BYTECODE_HEADER_BYTES, szPair + 32 );
	SIRScriptSHA256::digest( szPair, 64, pBytes + 64 );
}

static void testRoundTrip( void ) noexcept
{
	const sirLangCIRFunction_t function = { 123, 0, 2, 1, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t szCode[2] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 42, 1 },
	                                            { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	const sirLangValueKind_t szEmpty[1] = {};
	sirLangCIRProgram_t original = {};
	original.pFunctions = &function;
	original.nFunctionCount = 1;
	original.pInstructions = szCode;
	original.nInstructionCount = 2;
	original.pParameterTypes = szEmpty;
	original.nParameterTypeCount = 0;
	original.nSchemaId = 5;
	original.nLayoutDigest = 6;
	original.nBindingDigest = 7;
	original.nRevision = SIR_LANG_VM_VERSION;
	assert( SIRLangVerifyCIR( &original ) == SIR_LANG_VM_OK );
	uint8_t szBytes[2048] = {};
	uint32_t nUsed = 0;
	assert( SIRLangEncodeGraphVM( &original, szBytes, sizeof( szBytes ), &nUsed ) == SIR_LANG_VM_OK );
	assert( nUsed == 96 + 32 + 2 * 32 );
	sirLangCIRFunction_t szDecodedFunctions[2] = {};
	sirLangCIRInstruction_t szDecodedInstructions[8] = {};
	sirLangValueKind_t szDecodedTypes[4] = {};
	uint32_t szDecodedCalls[4] = {};
	sirLangCIRStorage_t decoded = {};
	decoded.pFunctions = szDecodedFunctions;
	decoded.nFunctionCapacity = 2;
	decoded.pInstructions = szDecodedInstructions;
	decoded.nInstructionCapacity = 8;
	decoded.pParameterTypes = szDecodedTypes;
	decoded.nParameterTypeCapacity = 4;
	decoded.pCallArguments = szDecodedCalls;
	decoded.nCallArgumentCapacity = 4;
	assert( SIRLangDecodeGraphVM( szBytes, nUsed, &decoded ) == SIR_LANG_VM_OK );
	assert( decoded.program.nFunctionCount == 1 );
	assert( decoded.program.nInstructionCount == 2 );
	assert( decoded.program.pInstructions[0].nImmediate == 42 );
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = 123;
	invocation.nFuel = 16;
	invocation.nExpectedLayoutDigest = 6;
	invocation.nExpectedBindingDigest = 7;
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &decoded.program, invocation );
	assert( result.eError == SIR_LANG_VM_OK && result.result.nBits == 42 );
	uint8_t szAgain[2048] = {};
	uint32_t nAgain = 0;
	assert( SIRLangEncodeGraphVM( &decoded.program, szAgain, sizeof( szAgain ), &nAgain ) == SIR_LANG_VM_OK );
	assert( nAgain == nUsed && memcmp( szBytes, szAgain, nUsed ) == 0 );

	szBytes[nUsed - 3] ^= 0x80;
	assert( SIRLangDecodeGraphVM( szBytes, nUsed, &decoded ) == SIR_LANG_VM_INVALID_FORMAT );
	assert( decoded.program.nFunctionCount == 0 );
	szBytes[nUsed - 3] ^= 0x80;
	const uint32_t nCodeOffset = 96 + 32;
	szBytes[nCodeOffset] = 255;
	resign( szBytes, nUsed );
	assert( SIRLangDecodeGraphVM( szBytes, nUsed, &decoded ) == SIR_LANG_VM_INVALID_OPCODE );
	szBytes[nCodeOffset] = SIR_LANG_OP_CONST;
	resign( szBytes, nUsed );
	szBytes[0] = 'X';
	assert( SIRLangDecodeGraphVM( szBytes, nUsed, &decoded ) == SIR_LANG_VM_INVALID_FORMAT );
	szBytes[0] = 'S';
	write32( szBytes + 40, UINT32_MAX );
	assert( SIRLangDecodeGraphVM( szBytes, nUsed, &decoded ) == SIR_LANG_VM_CAPACITY );
	assert( SIRLangEncodeGraphVM( &original, szAgain, 20, &nAgain ) == SIR_LANG_VM_CAPACITY );
}

int main( void )
{
	testRoundTrip();
	puts( "SIR-Lang bytecode roundtrip and mutation tests passed" );
	return 0;
}
