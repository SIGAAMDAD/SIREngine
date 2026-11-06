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

#include <stdio.h>
#include <string.h>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testAdversarialCIR( void ) noexcept
{
	const sirLangCIRFunction_t function = { 900, 0, 2, 1, 0, 0, SIR_LANG_VALUE_U16 };
	const sirLangCIRInstruction_t baseline[] = { { SIR_LANG_OP_CONST, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 42, 1 },
	                                             { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 2 } };
	sirLangCIRInstruction_t instructions[2] = {};
	sirLangCIRProgram_t program = {};
	static const sirLangValueKind_t szTypes[] = { SIR_LANG_VALUE_U16 };
	program.pParameterTypes = szTypes;
	program.nParameterTypeCount = 1;
	program.pFunctions = &function;
	program.nFunctionCount = 1;
	program.pInstructions = instructions;
	program.nInstructionCount = 2;
	program.nSchemaId = 1;
	program.nLayoutDigest = 2;
	program.nBindingDigest = 3;
	program.nRevision = SIR_LANG_VM_VERSION;
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = 900;
	invocation.nFuel = 32;
	invocation.nExpectedLayoutDigest = 2;
	invocation.nExpectedBindingDigest = 3;
	memcpy( instructions, baseline, sizeof( instructions ) );
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) == SIR_LANG_VM_OK );
	const sirLangVMRunResult_t initial = SIRLangRunGraphVM( &program, invocation );
	SIR_TEST_CHECK( initial.eError == SIR_LANG_VM_OK && initial.result.nBits == 42 );

	// Every individual field is mutated independently. Invalid bytecode must
	// be rejected deterministically; accepted bytecode must execute without
	// invalid memory access or nondeterministic result changes.
	static constexpr uint64_t s_szValues[] = { 0, 1, 2, 3, 4, 15, 16, 31, 32, 63, 64, 65, 255, 256, UINT32_MAX, UINT64_MAX };
	for ( uint32_t iInstruction = 0; iInstruction < 2; ++iInstruction ) {
		for ( uint32_t iField = 0; iField < 8; ++iField ) {
			for ( uint64_t nValue : s_szValues ) {
				memcpy( instructions, baseline, sizeof( instructions ) );
				sirLangCIRInstruction_t& item = instructions[iInstruction];
				switch ( iField ) {
				case 0:
					item.eOpcode = ( sirLangOpcode_t )nValue;
					break;
				case 1:
					item.eType = ( sirLangValueKind_t )nValue;
					break;
				case 2:
					item.nFlags = ( uint16_t )nValue;
					break;
				case 3:
					item.iDestination = ( uint32_t )nValue;
					break;
				case 4:
					item.iFirstSource = ( uint32_t )nValue;
					break;
				case 5:
					item.iSecondSource = ( uint32_t )nValue;
					break;
				case 6:
					item.nAuxiliary = ( uint32_t )nValue;
					break;
				case 7:
					item.nImmediate = nValue;
					break;
				}
				const sirLangVMError_t eFirst = SIRLangVerifyCIR( &program );
				const sirLangVMError_t eSecond = SIRLangVerifyCIR( &program );
				SIR_TEST_CHECK( eFirst == eSecond );
				if ( eFirst == SIR_LANG_VM_OK ) {
					const sirLangVMRunResult_t first = SIRLangRunGraphVM( &program, invocation );
					const sirLangVMRunResult_t second = SIRLangRunGraphVM( &program, invocation );
					SIR_TEST_CHECK( first.eError == second.eError && first.result.eType == second.result.eType &&
					                first.result.nBits == second.result.nBits && first.nFuelConsumed == second.nFuelConsumed );
				}
			}
		}
	}
	memcpy( instructions, baseline, sizeof( instructions ) );
	// Failures on malformed program metadata must not dereference missing
	// function, instruction, call-argument or capability buffers.
	SIR_TEST_CHECK( SIRLangVerifyCIR( nullptr ) == SIR_LANG_VM_INVALID_FORMAT );
	program.pFunctions = nullptr;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) != SIR_LANG_VM_OK );
	program.pFunctions = &function;
	program.pInstructions = nullptr;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) != SIR_LANG_VM_OK );
	program.pInstructions = instructions;
	program.nRevision = 0;
	SIR_TEST_CHECK( SIRLangVerifyCIR( &program ) != SIR_LANG_VM_OK );
	return true;
}

int main( void )
{
	if ( !testAdversarialCIR() ) {
		return 1;
	}
	puts( "GraphVM adversarial CIR: PASS" );
	return 0;
}
