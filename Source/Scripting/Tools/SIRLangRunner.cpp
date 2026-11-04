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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr uint32_t SIR_LANG_RUN_MAX_BYTES = 1048576;
constexpr uint32_t SIR_LANG_RUN_MAX_FUNCTIONS = 256;
constexpr uint32_t SIR_LANG_RUN_MAX_INSTRUCTIONS = 16384;
constexpr uint32_t SIR_LANG_RUN_MAX_PARAMETERS = 1024;
constexpr uint32_t SIR_LANG_RUN_MAX_CALL_ARGUMENTS = 2048;

typedef struct sirLangRunnerBuffers_s
{
	uint8_t szBytecode[SIR_LANG_RUN_MAX_BYTES];
	sirLangCIRFunction_t szFunctions[SIR_LANG_RUN_MAX_FUNCTIONS];
	sirLangCIRInstruction_t szInstructions[SIR_LANG_RUN_MAX_INSTRUCTIONS];
	sirLangValueKind_t szParameterTypes[SIR_LANG_RUN_MAX_PARAMETERS];
	uint32_t szCallArguments[SIR_LANG_RUN_MAX_CALL_ARGUMENTS];
	sirLangVMValue_t szArguments[SIR_LANG_VM_MAX_PARAMETERS];
	sirLangCIRStorage_t storage;
} sirLangRunnerBuffers_t;

static sirLangRunnerBuffers_t s_RunBuffers = {};

static bool sirLangRunReadFile( const char *pszPath, uint32_t *pUsed ) noexcept
{
	if ( pszPath == nullptr || pUsed == nullptr ) {
		return false;
	}
	FILE *pFile = fopen( pszPath, "rb" );
	if ( pFile == nullptr ) {
		return false;
	}
	const size_t nBytes = fread( s_RunBuffers.szBytecode, 1, sizeof( s_RunBuffers.szBytecode ), pFile );
	const int nExtra = fgetc( pFile );
	const bool bOk = nBytes >= SIR_LANG_BYTECODE_HEADER_BYTES && nExtra == EOF && !ferror( pFile );
	( void )fclose( pFile );
	if ( !bOk ) {
		return false;
	}
	*pUsed = ( uint32_t )nBytes;
	return true;
}

static void sirLangRunSetup( sirLangRunnerBuffers_t *pBuffers ) noexcept
{
	sirLangCIRStorage_t& storage = pBuffers->storage;
	storage.pFunctions = pBuffers->szFunctions;
	storage.nFunctionCapacity = SIR_LANG_RUN_MAX_FUNCTIONS;
	storage.pInstructions = pBuffers->szInstructions;
	storage.nInstructionCapacity = SIR_LANG_RUN_MAX_INSTRUCTIONS;
	storage.pParameterTypes = pBuffers->szParameterTypes;
	storage.nParameterTypeCapacity = SIR_LANG_RUN_MAX_PARAMETERS;
	storage.pCallArguments = pBuffers->szCallArguments;
	storage.nCallArgumentCapacity = SIR_LANG_RUN_MAX_CALL_ARGUMENTS;
	// Standalone execution has no host capabilities. Any artifact requiring
	// native bindings is rejected by the bytecode decoder.
	storage.pCapabilities = nullptr;
	storage.nCapabilityCount = 0;
}

static bool sirLangRunParseUnsigned( const char *pszValue, uint64_t *pResult ) noexcept
{
	if ( pszValue == nullptr || pResult == nullptr || *pszValue == '\0' || *pszValue == '-' ) {
		return false;
	}
	errno = 0;
	char *pszEnd = nullptr;
	const unsigned long long nValue = strtoull( pszValue, &pszEnd, 0 );
	if ( errno != 0 || pszEnd == pszValue || *pszEnd != '\0' ) {
		return false;
	}
	*pResult = ( uint64_t )nValue;
	return true;
}

static bool sirLangRunParseArgument( const char *pszValue, sirLangValueKind_t eType, sirLangVMValue_t *pValue ) noexcept
{
	if ( pszValue == nullptr || pValue == nullptr ) {
		return false;
	}
	uint64_t nBits = 0;
	if ( eType == SIR_LANG_VALUE_BOOL ) {
		if ( strcmp( pszValue, "true" ) == 0 || strcmp( pszValue, "1" ) == 0 ) {
			nBits = 1;
		} else if ( strcmp( pszValue, "false" ) == 0 || strcmp( pszValue, "0" ) == 0 ) {
			nBits = 0;
		} else {
			return false;
		}
	} else if ( !sirLangRunParseUnsigned( pszValue, &nBits ) ) {
		return false;
	}
	pValue->nBits = nBits;
	pValue->eType = eType;
	return true;
}

int main( int nArgc, char **ppszArgv )
{
	if ( nArgc < 3 ) {
		fprintf( stderr, "Usage: SIRLangRunner <program.sirbc> <function_index> [unsigned_arg ...]\n" );
		return 2;
	}
	uint32_t nBytes = 0;
	if ( !sirLangRunReadFile( ppszArgv[1], &nBytes ) ) {
		fprintf( stderr, "SIRLangRunner: unable to read bounded bytecode file\n" );
		return 3;
	}
	sirLangRunSetup( &s_RunBuffers );
	const sirLangVMError_t eDecode = SIRLangDecodeGraphVM( s_RunBuffers.szBytecode, nBytes, &s_RunBuffers.storage );
	if ( eDecode != SIR_LANG_VM_OK ) {
		fprintf( stderr, "SIRLangRunner: invalid or incompatible bytecode (%u)\n", ( unsigned )eDecode );
		return 4;
	}

	uint64_t iFunction = 0;
	if ( !sirLangRunParseUnsigned( ppszArgv[2], &iFunction ) || iFunction >= s_RunBuffers.storage.program.nFunctionCount ) {
		fprintf( stderr, "SIRLangRunner: invalid function index\n" );
		return 2;
	}
	const sirLangCIRFunction_t& function = s_RunBuffers.storage.program.pFunctions[iFunction];
	if ( nArgc - 3 != ( int )function.nParameterCount || function.nParameterCount > SIR_LANG_VM_MAX_PARAMETERS ) {
		fprintf( stderr, "SIRLangRunner: expected %u arguments\n", function.nParameterCount );
		return 2;
	}
	for ( uint32_t i = 0; i < function.nParameterCount; ++i ) {
		const sirLangValueKind_t eType =
		    s_RunBuffers.storage.program.pParameterTypes[function.iFirstParameterType + i];
		if ( !sirLangRunParseArgument( ppszArgv[3 + i], eType, &s_RunBuffers.szArguments[i] ) ) {
			fprintf( stderr, "SIRLangRunner: invalid argument %u\n", i );
			return 2;
		}
	}

	sirLangVMInvocation_t invocation = {};
	invocation.nFunctionId = function.nFunctionId;
	invocation.pArguments = function.nParameterCount != 0 ? s_RunBuffers.szArguments : nullptr;
	invocation.nArgumentCount = function.nParameterCount;
	invocation.nFuel = 1000000;
	invocation.nExpectedLayoutDigest = s_RunBuffers.storage.program.nLayoutDigest;
	invocation.nExpectedBindingDigest = s_RunBuffers.storage.program.nBindingDigest;
	const sirLangVMRunResult_t result = SIRLangRunGraphVM( &s_RunBuffers.storage.program, invocation );
	if ( result.eError != SIR_LANG_VM_OK ) {
		fprintf( stderr, "SIRLangRunner: execution failed (%u) at node %u\n",
		         ( unsigned )result.eError, result.nSourceNode );
		return 5;
	}
	printf( "value_type=%u value_bits=%llu fuel=%llu\n", ( unsigned )result.result.eType,
	        ( unsigned long long )result.result.nBits, ( unsigned long long )result.nFuelConsumed );
	return 0;
}
