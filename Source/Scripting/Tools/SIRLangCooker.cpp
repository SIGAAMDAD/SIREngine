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

#include <Scripting/Language/SIRLangSourceCompiler.h>
#include <Scripting/Language/SIRLangBytecode.h>
#include <Scripting/Language/SIRLangCookRecipe.h>
#include <Scripting/Language/SIRLangDiagnostics.h>
#include <Scripting/Core/SIRScriptSHA256.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr uint32_t SIR_LANG_COOK_MAX_SOURCE = 131072;
constexpr uint32_t SIR_LANG_COOK_MAX_TOKENS = 8192;
constexpr uint32_t SIR_LANG_COOK_MAX_NODES = 8192;
constexpr uint32_t SIR_LANG_COOK_MAX_DIAGNOSTICS = 128;
constexpr uint32_t SIR_LANG_COOK_MAX_FUNCTIONS = 256;
constexpr uint32_t SIR_LANG_COOK_MAX_PARAMETERS = 1024;
constexpr uint32_t SIR_LANG_COOK_MAX_CALLS = 2048;
constexpr uint32_t SIR_LANG_COOK_MAX_INSTRUCTIONS = 16384;
constexpr uint32_t SIR_LANG_COOK_MAX_BYTES = 1048576;

typedef struct sirLangCookBuffers_s
{
	uint8_t szSource[SIR_LANG_COOK_MAX_SOURCE];
	uint8_t szBytecode[SIR_LANG_COOK_MAX_BYTES];
	sirLangToken_t szTokens[SIR_LANG_COOK_MAX_TOKENS];
	sirLangSyntaxNode_t szNodes[SIR_LANG_COOK_MAX_NODES];
	sirLangParseDiagnostic_t szParseDiagnostics[SIR_LANG_COOK_MAX_DIAGNOSTICS];
	sirLangSemanticSymbol_t szSymbols[SIR_LANG_COOK_MAX_NODES];
	sirLangResolvedReference_t szReferences[SIR_LANG_COOK_MAX_NODES];
	sirLangSemanticDiagnostic_t szSemanticDiagnostics[SIR_LANG_COOK_MAX_DIAGNOSTICS];
	sirLangFunctionSignature_t szFunctions[SIR_LANG_COOK_MAX_FUNCTIONS];
	sirLangSignatureParameter_t szParameters[SIR_LANG_COOK_MAX_PARAMETERS];
	sirLangResolvedCall_t szCalls[SIR_LANG_COOK_MAX_CALLS];
	sirLangTypeIdentity_t szNodeTypes[SIR_LANG_COOK_MAX_NODES];
	sirLangSignatureDiagnostic_t szSignatureDiagnostics[SIR_LANG_COOK_MAX_DIAGNOSTICS];
	sirLangHIRNode_t szHIR[SIR_LANG_COOK_MAX_NODES];
	sirLangCIRFunction_t szCIRFunctions[SIR_LANG_COOK_MAX_FUNCTIONS];
	sirLangCIRInstruction_t szInstructions[SIR_LANG_COOK_MAX_INSTRUCTIONS];
	sirLangValueKind_t szParameterTypes[SIR_LANG_COOK_MAX_PARAMETERS];
	uint32_t szCallArguments[SIR_LANG_COOK_MAX_CALLS];
	sirLangEffectFunction_t szEffects[SIR_LANG_COOK_MAX_FUNCTIONS];
	sirLangCompileWorkspace_t workspace;
} sirLangCookBuffers_t;

static sirLangCookBuffers_t s_Buffers = {};

static bool sirLangParseId( const char *pszText, uint64_t *pValue ) noexcept
{
	if ( pszText == nullptr || pValue == nullptr || *pszText == '\0' || *pszText == '-' ) {
		return false;
	}
	errno = 0;
	char *pszEnd = nullptr;
	const unsigned long long nParsed = strtoull( pszText, &pszEnd, 0 );
	if ( errno != 0 || pszEnd == pszText || *pszEnd != '\0' || nParsed == 0 ) {
		return false;
	}
	*pValue = ( uint64_t )nParsed;
	return true;
}

static bool sirLangParseOptionalFlags( const char *pszText, uint64_t *pValue ) noexcept
{
	if ( pszText == nullptr || pValue == nullptr || *pszText == '\0' || *pszText == '-' ) {
		return false;
	}
	errno = 0;
	char *pszEnd = nullptr;
	const unsigned long long nParsed = strtoull( pszText, &pszEnd, 0 );
	if ( errno != 0 || pszEnd == pszText || *pszEnd != '\0' ) {
		return false;
	}
	*pValue = ( uint64_t )nParsed;
	return true;
}

static void sirLangInitializeBuffers( sirLangCookBuffers_t *pBuffers ) noexcept
{
	sirLangCompileWorkspace_t& workspace = pBuffers->workspace;
	workspace.syntax.pTokens = pBuffers->szTokens;
	workspace.syntax.nTokenCapacity = SIR_LANG_COOK_MAX_TOKENS;
	workspace.syntax.pNodes = pBuffers->szNodes;
	workspace.syntax.nNodeCapacity = SIR_LANG_COOK_MAX_NODES;
	workspace.syntax.pDiagnostics = pBuffers->szParseDiagnostics;
	workspace.syntax.nDiagnosticCapacity = SIR_LANG_COOK_MAX_DIAGNOSTICS;

	workspace.semantic.pSymbols = pBuffers->szSymbols;
	workspace.semantic.nSymbolCapacity = SIR_LANG_COOK_MAX_NODES;
	workspace.semantic.pReferences = pBuffers->szReferences;
	workspace.semantic.nReferenceCapacity = SIR_LANG_COOK_MAX_NODES;
	workspace.semantic.pDiagnostics = pBuffers->szSemanticDiagnostics;
	workspace.semantic.nDiagnosticCapacity = SIR_LANG_COOK_MAX_DIAGNOSTICS;

	workspace.signatures.pFunctions = pBuffers->szFunctions;
	workspace.signatures.nFunctionCapacity = SIR_LANG_COOK_MAX_FUNCTIONS;
	workspace.signatures.pParameters = pBuffers->szParameters;
	workspace.signatures.nParameterCapacity = SIR_LANG_COOK_MAX_PARAMETERS;
	workspace.signatures.pCalls = pBuffers->szCalls;
	workspace.signatures.nCallCapacity = SIR_LANG_COOK_MAX_CALLS;
	workspace.signatures.pNodeTypes = pBuffers->szNodeTypes;
	workspace.signatures.nNodeTypeCapacity = SIR_LANG_COOK_MAX_NODES;
	workspace.signatures.pDiagnostics = pBuffers->szSignatureDiagnostics;
	workspace.signatures.nDiagnosticCapacity = SIR_LANG_COOK_MAX_DIAGNOSTICS;

	workspace.hir.pNodes = pBuffers->szHIR;
	workspace.hir.nCapacity = SIR_LANG_COOK_MAX_NODES;

	workspace.cir.pFunctions = pBuffers->szCIRFunctions;
	workspace.cir.nFunctionCapacity = SIR_LANG_COOK_MAX_FUNCTIONS;
	workspace.cir.pInstructions = pBuffers->szInstructions;
	workspace.cir.nInstructionCapacity = SIR_LANG_COOK_MAX_INSTRUCTIONS;
	workspace.cir.pParameterTypes = pBuffers->szParameterTypes;
	workspace.cir.nParameterTypeCapacity = SIR_LANG_COOK_MAX_PARAMETERS;
	workspace.cir.pCallArguments = pBuffers->szCallArguments;
	workspace.cir.nCallArgumentCapacity = SIR_LANG_COOK_MAX_CALLS;

	workspace.effects.pFunctions = pBuffers->szEffects;
	workspace.effects.nCapacity = SIR_LANG_COOK_MAX_FUNCTIONS;
}

static bool sirLangReadSource( const char *pszPath, sirLangSourceView_t *pSource ) noexcept
{
	FILE *pFile = fopen( pszPath, "rb" );
	if ( pFile == nullptr ) {
		return false;
	}

	const size_t nRead = fread( s_Buffers.szSource, 1, sizeof( s_Buffers.szSource ), pFile );
	const int nExtra = fgetc( pFile );
	const bool bSuccess = !ferror( pFile ) && nExtra == EOF && nRead != 0;
	( void )fclose( pFile );
	if ( !bSuccess ) {
		return false;
	}

	pSource->pBytes = s_Buffers.szSource;
	pSource->nBytes = ( uint32_t )nRead;
	return true;
}

static bool sirLangWriteArtifact( const char *pszPath, uint32_t nBytes ) noexcept
{
	if ( pszPath == nullptr || nBytes == 0 || nBytes > sizeof( s_Buffers.szBytecode ) ) {
		return false;
	}

	// Publish only a complete compiled object. A pre-existing target is never
	// deleted or partially overwritten by a failed cook.
	FILE *pExisting = fopen( pszPath, "rb" );
	if ( pExisting != nullptr ) {
		( void )fclose( pExisting );
		return false;
	}

	char szTemporary[4096] = {};
	const size_t nLength = strlen( pszPath );
	if ( nLength > sizeof( szTemporary ) - sizeof( ".tmp" ) ) {
		return false;
	}
	memcpy( szTemporary, pszPath, nLength );
	memcpy( szTemporary + nLength, ".tmp", sizeof( ".tmp" ) );
	FILE *pFile = fopen( szTemporary, "wb" );
	if ( pFile == nullptr ) {
		return false;
	}

	const bool bWritten = fwrite( s_Buffers.szBytecode, 1, nBytes, pFile ) == nBytes;
	const bool bClosed = fclose( pFile ) == 0;
	if ( !bWritten || !bClosed || rename( szTemporary, pszPath ) != 0 ) {
		( void )remove( szTemporary );
		return false;
	}
	return true;
}

int main( int nArgc, char **ppszArgv )
{
	if ( ( nArgc != 9 && nArgc != 10 ) || strcmp( ppszArgv[1], ppszArgv[2] ) == 0 ) {
		fprintf( stderr, "Usage: SIRLangCooker <source.sir> <target.sirbc> <module_id> <schema_id> <layout_digest> "
		                 "<binding_digest> <target_id> <toolchain_id> [optimization_flags]\n" );
		return 2;
	}

	sirLangInitializeBuffers( &s_Buffers );
	sirLangCompileWorkspace_t& workspace = s_Buffers.workspace;
	if ( !sirLangParseId( ppszArgv[3], &workspace.cir.nModuleId ) ||
	     !sirLangParseId( ppszArgv[4], &workspace.cir.nSchemaId ) ||
	     !sirLangParseId( ppszArgv[5], &workspace.cir.nLayoutDigest ) ||
	     !sirLangParseId( ppszArgv[6], &workspace.cir.nBindingDigest ) ) {
		fprintf( stderr, "SIRLangCooker: invalid module/schema/layout/binding identity\n" );
		return 2;
	}

	uint64_t nTargetId = 0;
	uint64_t nToolchainId = 0;
	uint64_t nOptimizationFlags = 0;
	if ( !sirLangParseId( ppszArgv[7], &nTargetId ) || !sirLangParseId( ppszArgv[8], &nToolchainId ) ) {
		fprintf( stderr, "SIRLangCooker: target/toolchain identities must be nonzero\n" );
		return 2;
	}
	if ( nArgc == 10 && !sirLangParseOptionalFlags( ppszArgv[9], &nOptimizationFlags ) ) {
		fprintf( stderr, "SIRLangCooker: invalid optimization flags\n" );
		return 2;
	}

	sirLangSourceView_t source = {};
	if ( !sirLangReadSource( ppszArgv[1], &source ) ) {
		fprintf( stderr, "SIRLangCooker: unable to read source within %u-byte limit\n", SIR_LANG_COOK_MAX_SOURCE );
		return 3;
	}

	if ( !SIRLangCompileSource( source, &workspace ) ) {
		fprintf( stderr, "SIRLangCooker: compilation failed at phase %u, error %u, node %u\n",
		         ( unsigned )workspace.report.ePhase, ( unsigned )workspace.report.nStageResult,
		         ( unsigned )workspace.report.iErrorNode );
		sirLangEditorDiagnostic_t szErrors[16] = {};
		const uint32_t nErrors = SIRLangCollectCompileDiagnostics( &workspace, szErrors, 16 );
		for ( uint32_t iError = 0; iError < nErrors; ++iError ) {
			const sirLangEditorDiagnostic_t& error = szErrors[iError];
			fprintf( stderr, "SIRLangCooker: %u:%u phase %u code %u\n", error.nLine, error.nColumn,
			         ( unsigned )error.ePhase, error.nCode );
		}
		return 4;
	}

	// A cooked package cannot claim a complete dependency closure until every
	// imported module is recorded in the canonical recipe. Reject instead of
	// silently creating an incomplete content-addressed artifact.
	for ( uint32_t iNode = 0; iNode < workspace.syntax.nNodeCount; ++iNode ) {
		if ( workspace.syntax.pNodes[iNode].eKind == SIR_LANG_SYNTAX_IMPORT ) {
			fprintf( stderr, "SIRLangCooker: import closure not resolved\n" );
			return 4;
		}
	}
	sirLangCookIdentity_t identity = {};
	identity.nModuleId = workspace.cir.nModuleId;
	identity.nSchemaId = workspace.cir.nSchemaId;
	identity.nLayoutDigest = workspace.cir.nLayoutDigest;
	identity.nBindingDigest = workspace.cir.nBindingDigest;
	identity.nTargetId = nTargetId;
	identity.nToolchainId = nToolchainId;
	identity.nOptimizationFlags = nOptimizationFlags;
	SIRScriptSHA256::digest( source.pBytes, source.nBytes, identity.szSourceDigest );
	sirScriptCacheKey_t key = {};
	if ( SIRLangMakeCookKey( identity, nullptr, 0, &key ) != SIR_SCRIPT_OK ) {
		fprintf( stderr, "SIRLangCooker: invalid canonical recipe\n" );
		return 4;
	}

	uint32_t nBytes = 0;
	const sirLangVMError_t eEncoded =
	    SIRLangEncodeGraphVM( &workspace.cir.program, s_Buffers.szBytecode, sizeof( s_Buffers.szBytecode ), &nBytes );
	if ( eEncoded != SIR_LANG_VM_OK ) {
		fprintf( stderr, "SIRLangCooker: encode failure %u\n", ( unsigned )eEncoded );
		return 5;
	}
	if ( !sirLangWriteArtifact( ppszArgv[2], nBytes ) ) {
		fprintf( stderr, "SIRLangCooker: failed to publish cooked artifact\n" );
		return 6;
	}

	printf( "SIRLangCooker: produced %u bytes\n", nBytes );
	printf( "cache_key=" );
	for ( uint32_t iByte = 0; iByte < sizeof( key.szDigest ); ++iByte ) {
		printf( "%02x", ( unsigned )key.szDigest[iByte] );
	}
	printf( "\n" );
	return 0;
}
