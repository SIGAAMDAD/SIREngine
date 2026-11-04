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

#include <Scripting/Core/SIRScriptBindingRegistry.h>
#include <Scripting/Language/SIRLangBytecode.h>
#include <Scripting/Language/SIRLangNativeBinding.h>

#include <benchmark/benchmark.h>

static sirLangCIRProgram_t makeScalarProgram( sirLangCIRFunction_t *pFunction, sirLangCIRInstruction_t szInstructions[2],
                                              sirLangValueKind_t *pType ) noexcept
{

	*pFunction = { 100, 0, 2, 1, 1, 0, SIR_LANG_VALUE_U16 };
	szInstructions[0] = { SIR_LANG_OP_PARAMETER, SIR_LANG_VALUE_U16, 0, 0, 0, 0, 0, 0, 0 };
	szInstructions[1] = { SIR_LANG_OP_RETURN, SIR_LANG_VALUE_U16, 0, UINT32_MAX, 0, 0, 0, 0, 0 };
	*pType = SIR_LANG_VALUE_U16;
	sirLangCIRProgram_t program = {};
	program.pFunctions = pFunction;
	program.nFunctionCount = 1;
	program.pInstructions = szInstructions;
	program.nInstructionCount = 2;
	program.pParameterTypes = pType;
	program.nParameterTypeCount = 1;
	program.nSchemaId = 1;
	program.nLayoutDigest = 2;
	program.nBindingDigest = 3;
	program.nRevision = SIR_LANG_VM_VERSION;
	return program;
}

static void SIRBenchmarkGraphVMScalarInvoke( benchmark::State& state )
{
	sirLangCIRFunction_t function = {};
	sirLangCIRInstruction_t szInstructions[2] = {};
	sirLangValueKind_t eParameter = SIR_LANG_VALUE_U16;
	const sirLangCIRProgram_t program = makeScalarProgram( &function, szInstructions, &eParameter );
	if ( SIRLangVerifyCIR( &program ) != SIR_LANG_VM_OK ) {
		state.SkipWithError( "invalid GraphVM fixture" );
		return;
	}

	const sirLangVMValue_t argument = { 42, SIR_LANG_VALUE_U16 };
	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = function.nFunctionId;
	invocation.pArguments = &argument;
	invocation.nArgumentCount = 1;
	invocation.nFuel = 16;
	invocation.nExpectedLayoutDigest = program.nLayoutDigest;
	invocation.nExpectedBindingDigest = program.nBindingDigest;
	for ( auto _ : state ) {
		( void )_;
		const sirLangVMRunResult_t result = SIRLangRunGraphVM( &program, invocation );
		if ( result.eError != SIR_LANG_VM_OK || result.result.nBits != 42 ) {
			state.SkipWithError( "GraphVM invocation failed" );
			break;
		}
		benchmark::DoNotOptimize( result.result.nBits );
	}
}
BENCHMARK( SIRBenchmarkGraphVMScalarInvoke );

static void SIRBenchmarkGraphVMBytecodeDecode( benchmark::State& state )
{
	sirLangCIRFunction_t function = {};
	sirLangCIRInstruction_t szInstructions[2] = {};
	sirLangValueKind_t eParameter = SIR_LANG_VALUE_U16;
	const sirLangCIRProgram_t program = makeScalarProgram( &function, szInstructions, &eParameter );
	uint8_t szBytecode[1024] = {};
	uint32_t nBytes = 0;
	if ( SIRLangEncodeGraphVM( &program, szBytecode, sizeof( szBytecode ), &nBytes ) != SIR_LANG_VM_OK ) {
		state.SkipWithError( "GraphVM serialization failed" );
		return;
	}

	sirLangCIRFunction_t szDecodedFunctions[2] = {};
	sirLangCIRInstruction_t szDecodedInstructions[4] = {};
	sirLangValueKind_t szDecodedTypes[4] = {};
	uint32_t szDecodedArgs[4] = {};
	sirLangCIRStorage_t storage = {};
	storage.pFunctions = szDecodedFunctions;
	storage.nFunctionCapacity = 2;
	storage.pInstructions = szDecodedInstructions;
	storage.nInstructionCapacity = 4;
	storage.pParameterTypes = szDecodedTypes;
	storage.nParameterTypeCapacity = 4;
	storage.pCallArguments = szDecodedArgs;
	storage.nCallArgumentCapacity = 4;

	for ( auto _ : state ) {
		( void )_;
		const sirLangVMError_t eResult = SIRLangDecodeGraphVM( szBytecode, nBytes, &storage );
		if ( eResult != SIR_LANG_VM_OK ) {
			state.SkipWithError( "GraphVM bytecode decode failed" );
			break;
		}
		benchmark::DoNotOptimize( storage.program.nFunctionCount );
	}
	state.SetBytesProcessed( state.iterations() * nBytes );
}
BENCHMARK( SIRBenchmarkGraphVMBytecodeDecode );

static sirScriptResult_t sirBenchmarkNativeThunk( void *, sirScriptReadFrame_t arguments, sirScriptWriteFrame_t result ) noexcept
{
	if ( arguments.nBytes != sizeof( sirLangVMValue_t ) || result.nBytes != sizeof( sirLangVMValue_t ) ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	const sirLangVMValue_t& input = *( const sirLangVMValue_t * )arguments.pData;
	*( sirLangVMValue_t * )result.pData = { input.nBits + 1, input.eType };
	return SIR_SCRIPT_OK;
}

static void SIRBenchmarkGraphVMNativeBinding( benchmark::State& state )
{
	SIRScriptBindingRegistry registry;
	constexpr uint32_t TYPE_FLAGS = SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS;
	if ( registry.registerType( { 10, sizeof( sirLangVMValue_t ), alignof( sirLangVMValue_t ), TYPE_FLAGS } ) != SIR_SCRIPT_OK ||
	     registry.registerType( { 11, sizeof( sirLangVMValue_t ), alignof( sirLangVMValue_t ), TYPE_FLAGS } ) != SIR_SCRIPT_OK ||
	     registry.registerFunction( { 999, 123, 10, 11, 0, 1, SIR_LANG_EFFECT_STATE_READ }, sirBenchmarkNativeThunk, nullptr ) !=
	         SIR_SCRIPT_OK ||
	     registry.publish() != SIR_SCRIPT_OK ) {
		state.SkipWithError( "native registry setup failed" );
		return;
	}

	const sirLangNativeBindingContext_t context = { &registry, registry.schemaFingerprint(), 0 };
	const sirLangVMValue_t argument = { 42, SIR_LANG_VALUE_U16 };
	const sirLangVMCall_t call = { 999, 123, 1, 0, SIR_LANG_EFFECT_STATE_READ, &argument, 1 };
	for ( auto _ : state ) {
		( void )_;
		sirLangVMValue_t result = {};
		const sirLangVMError_t eResult = SIRLangInvokeNativeBinding( call, &result, ( void * )&context );
		if ( eResult != SIR_LANG_VM_OK || result.nBits != 43 ) {
			state.SkipWithError( "native dispatch failed" );
			break;
		}
		benchmark::DoNotOptimize( result.nBits );
	}
}
BENCHMARK( SIRBenchmarkGraphVMNativeBinding );
