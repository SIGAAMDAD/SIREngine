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

#include <Scripting/Core/SIRScriptServer.h>
#include <Scripting/Core/SIRScriptCachedProgramSource.h>
#include <Scripting/Language/SIRLangGraphVMBackend.h>
#include <Scripting/Language/SIRLangSourceCompiler.h>

#include <assert.h>
#include <chrono>
#include <filesystem>
#include <stdlib.h>
#include <string.h>
#include <string>

typedef struct sirLangRuntimeFixture_s
{
	sirLangToken_t szTokens[256];
	sirLangSyntaxNode_t szNodes[256];
	sirLangParseDiagnostic_t szParseDiagnostics[32];
	sirLangSemanticSymbol_t szSymbols[256];
	sirLangResolvedReference_t szReferences[256];
	sirLangSemanticDiagnostic_t szSemanticDiagnostics[32];
	sirLangFunctionSignature_t szFunctions[32];
	sirLangSignatureParameter_t szParameters[64];
	sirLangResolvedCall_t szCalls[64];
	sirLangTypeIdentity_t szNodeTypes[256];
	sirLangSignatureDiagnostic_t szSignatureDiagnostics[32];
	sirLangHIRNode_t szHIR[256];
	sirLangCIRFunction_t szCIRFunctions[32];
	sirLangCIRInstruction_t szInstructions[512];
	sirLangValueKind_t szParameterTypes[128];
	uint32_t szCallArguments[128];
	sirLangEffectFunction_t szEffects[32];
	sirLangCIRFunction_t szDecodedFunctions[32];
	sirLangCIRInstruction_t szDecodedInstructions[512];
	sirLangValueKind_t szDecodedTypes[128];
	uint32_t szDecodedArgs[128];
	sirLangGraphVMProgramSlot_t programSlots[1];
	sirLangGraphVMInstanceSlot_t instanceSlots[2];
	sirLangGraphVMBackend_t backend;
	sirLangCompileWorkspace_t compiler;
	uint8_t szBytecode[32768];
} sirLangRuntimeFixture_t;

static void initCompiler( sirLangRuntimeFixture_t *pFixture, uint64_t nBindingDigest ) noexcept
{
	sirLangCompileWorkspace_t& workspace = pFixture->compiler;
	workspace.syntax.pTokens = pFixture->szTokens;
	workspace.syntax.nTokenCapacity = 256;
	workspace.syntax.pNodes = pFixture->szNodes;
	workspace.syntax.nNodeCapacity = 256;
	workspace.syntax.pDiagnostics = pFixture->szParseDiagnostics;
	workspace.syntax.nDiagnosticCapacity = 32;
	workspace.semantic.pSymbols = pFixture->szSymbols;
	workspace.semantic.nSymbolCapacity = 256;
	workspace.semantic.pReferences = pFixture->szReferences;
	workspace.semantic.nReferenceCapacity = 256;
	workspace.semantic.pDiagnostics = pFixture->szSemanticDiagnostics;
	workspace.semantic.nDiagnosticCapacity = 32;
	workspace.signatures.pFunctions = pFixture->szFunctions;
	workspace.signatures.nFunctionCapacity = 32;
	workspace.signatures.pParameters = pFixture->szParameters;
	workspace.signatures.nParameterCapacity = 64;
	workspace.signatures.pCalls = pFixture->szCalls;
	workspace.signatures.nCallCapacity = 64;
	workspace.signatures.pNodeTypes = pFixture->szNodeTypes;
	workspace.signatures.nNodeTypeCapacity = 256;
	workspace.signatures.pDiagnostics = pFixture->szSignatureDiagnostics;
	workspace.signatures.nDiagnosticCapacity = 32;
	workspace.hir.pNodes = pFixture->szHIR;
	workspace.hir.nCapacity = 256;
	workspace.cir.pFunctions = pFixture->szCIRFunctions;
	workspace.cir.nFunctionCapacity = 32;
	workspace.cir.pInstructions = pFixture->szInstructions;
	workspace.cir.nInstructionCapacity = 512;
	workspace.cir.pParameterTypes = pFixture->szParameterTypes;
	workspace.cir.nParameterTypeCapacity = 128;
	workspace.cir.pCallArguments = pFixture->szCallArguments;
	workspace.cir.nCallArgumentCapacity = 128;
	workspace.cir.nModuleId = 101;
	workspace.cir.nSchemaId = 201;
	workspace.cir.nLayoutDigest = 301;
	workspace.cir.nBindingDigest = nBindingDigest;
	workspace.effects.pFunctions = pFixture->szEffects;
	workspace.effects.nCapacity = 32;
}

static void initBackend( sirLangRuntimeFixture_t *pFixture, uint64_t nBindingDigest ) noexcept
{
	sirLangGraphVMProgramSlot_t& slot = pFixture->programSlots[0];
	slot.storage.pFunctions = pFixture->szDecodedFunctions;
	slot.storage.nFunctionCapacity = 32;
	slot.storage.pInstructions = pFixture->szDecodedInstructions;
	slot.storage.nInstructionCapacity = 512;
	slot.storage.pParameterTypes = pFixture->szDecodedTypes;
	slot.storage.nParameterTypeCapacity = 128;
	slot.storage.pCallArguments = pFixture->szDecodedArgs;
	slot.storage.nCallArgumentCapacity = 128;
	sirLangGraphVMBackend_t& backend = pFixture->backend;
	backend.pPrograms = pFixture->programSlots;
	backend.nProgramCapacity = 1;
	backend.pInstances = pFixture->instanceSlots;
	backend.nInstanceCapacity = 2;
	backend.nFormatId = 77;
	backend.nSchemaId = 201;
	backend.nLayoutDigest = 301;
	backend.nBindingDigest = nBindingDigest;
	backend.nFuelPerInvoke = 128;
}

static sirScriptResult_t runtimeTestAlloc( void *, uint32_t nBytes, uint32_t nAlignment,
                                          sirScriptPayloadAllocation_t *pResult ) noexcept
{
	if ( pResult == nullptr || nBytes == 0 || nAlignment > alignof( max_align_t ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	void *pMemory = malloc( nBytes );
	if ( pMemory == nullptr ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	*pResult = { pMemory, ( uint64_t )( uintptr_t )pMemory };
	return SIR_SCRIPT_OK;
}

static void runtimeTestFree( void *, sirScriptPayloadAllocation_t allocation ) noexcept
{
	free( allocation.pData );
}

static void testRealSourceThroughRuntime( void )
{
	static sirLangRuntimeFixture_t s_Fixture = {};
	SIRScriptArtifactCache cache;
	SIRScriptCachedProgramSource source;
	SIRScriptServer server;
	SIRScriptBindingRegistry& bindings = server.bindings();
	assert( bindings.publish() == SIR_SCRIPT_OK );
	const uint64_t nFingerprint = bindings.schemaFingerprint();
	assert( nFingerprint != 0 );

	initCompiler( &s_Fixture, nFingerprint );
	static constexpr char szSource[] = "fn identity(value: u16) -> u16:\n"
	                                    "    return value\n";
	assert( SIRLangCompileSource( { ( const uint8_t * )szSource, sizeof( szSource ) - 1 }, &s_Fixture.compiler ) );
	const uint64_t nMethodId = s_Fixture.compiler.cir.program.pFunctions[0].nFunctionId;
	uint32_t nEncoded = 0;
	assert( SIRLangEncodeGraphVM( &s_Fixture.compiler.cir.program, s_Fixture.szBytecode,
	                              sizeof( s_Fixture.szBytecode ), &nEncoded ) == SIR_LANG_VM_OK );

	const std::filesystem::path root = std::filesystem::temp_directory_path() /
	    ( "sir-lang-runtime-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	assert( std::filesystem::create_directory( root ) );
	assert( cache.configure( root.string().c_str(), 32768 ) == SIR_SCRIPT_OK );
	const sirScriptCodeArtifact_t artifact = { 123, 222, 1, 77, nFingerprint, 88, 901 };
	sirScriptCacheKey_t key = {};
	assert( SIRScriptArtifactCache::makeKey( szSource, sizeof( szSource ) - 1, &key ) == SIR_SCRIPT_OK );
	sirScriptCachedArtifact_t descriptor = {};
	assert( cache.store( artifact, key, s_Fixture.szBytecode, nEncoded, 8, &descriptor ) == SIR_SCRIPT_OK );

	const sirScriptPayloadAllocatorAPI_t allocator = {
	    SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION, nullptr, 32768, 8, runtimeTestAlloc, runtimeTestFree
	};
	assert( source.configure( &cache, allocator, 32768 ) == SIR_SCRIPT_OK );
	sirScriptCachedEntryHandle_t entry = {};
	assert( source.registerArtifact( descriptor, &entry ) == SIR_SCRIPT_OK );
	assert( server.codeManager().configure( 88, source.makeProgramSourceAPI() ) == SIR_SCRIPT_OK );
	assert( server.useCodeManagerSource() == SIR_SCRIPT_OK );
	sirScriptCodeHandle_t code = {};
	assert( server.codeManager().stage( artifact, &code ) == SIR_SCRIPT_OK );
	assert( server.codeManager().activate( code ) == SIR_SCRIPT_OK );

	initBackend( &s_Fixture, nFingerprint );
	SIRScriptRuntime& runtime = server.runtime();
	assert( runtime.registerBackend( 123, SIRLangGraphVMBackendAPI( &s_Fixture.backend ) ) == SIR_SCRIPT_OK );
	assert( runtime.startBackend( 123 ) == SIR_SCRIPT_OK );
	sirScriptProgramHandle_t program = {};
	assert( runtime.loadProgram( 123, 222, 1, &program ) == SIR_SCRIPT_OK );
	assert( source.stats().nActiveLeases == 1 );
	sirScriptInstanceHandle_t instance = {};
	assert( runtime.createInstance( program, 456, &instance ) == SIR_SCRIPT_OK );

	const sirLangVMValue_t argument = { 42, SIR_LANG_VALUE_U16 };
	sirLangVMValue_t result = {};
	assert( runtime.invoke( instance, nMethodId, { &argument, sizeof( argument ), alignof( sirLangVMValue_t ) },
	                       { &result, sizeof( result ), alignof( sirLangVMValue_t ) } ) == SIR_SCRIPT_OK );
	assert( result.nBits == 42 && result.eType == SIR_LANG_VALUE_U16 );
	assert( runtime.unloadProgram( program ) == SIR_SCRIPT_BUSY );
	assert( source.unregisterArtifact( entry ) == SIR_SCRIPT_BUSY );

	assert( runtime.destroyInstance( instance ) == SIR_SCRIPT_OK );
	assert( runtime.unloadProgram( program ) == SIR_SCRIPT_OK );
	assert( source.stats().nActiveLeases == 0 );
	assert( runtime.stopBackend( 123 ) == SIR_SCRIPT_OK );
	assert( runtime.unregisterBackend( 123 ) == SIR_SCRIPT_OK );
	assert( server.codeManager().shutdown() == SIR_SCRIPT_OK );
	assert( source.unregisterArtifact( entry ) == SIR_SCRIPT_OK );
	assert( source.shutdown() == SIR_SCRIPT_OK );
	assert( cache.erase( key ) == SIR_SCRIPT_OK );
	assert( std::filesystem::remove( root ) );
}

int main( void )
{
	testRealSourceThroughRuntime();
	return 0;
}
