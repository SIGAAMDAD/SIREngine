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
#include <string.h>

static void sirLangWrite32( uint8_t *pDestination, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pDestination[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static void sirLangWrite64( uint8_t *pDestination, uint64_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		pDestination[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static uint32_t sirLangRead32( const uint8_t *pSource ) noexcept
{
	uint32_t nValue = 0;
	for ( uint32_t i = 0; i < 4; ++i ) {
		nValue |= ( uint32_t )pSource[i] << ( i * 8 );
	}
	return nValue;
}

static uint64_t sirLangRead64( const uint8_t *pSource ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t i = 0; i < 8; ++i ) {
		nValue |= ( uint64_t )pSource[i] << ( i * 8 );
	}
	return nValue;
}

static uint64_t sirLangBytecodeSize( const sirLangCIRProgram_t *pProgram ) noexcept
{
	return SIR_LANG_BYTECODE_HEADER_BYTES + ( uint64_t )pProgram->nFunctionCount * SIR_LANG_BYTECODE_FUNCTION_BYTES +
	       ( uint64_t )pProgram->nInstructionCount * SIR_LANG_BYTECODE_INSTRUCTION_BYTES + ( uint64_t )pProgram->nParameterTypeCount +
	       ( uint64_t )pProgram->nCallArgumentCount * 4 + ( uint64_t )pProgram->nCapabilityCount * SIR_LANG_BYTECODE_CAPABILITY_BYTES;
}

static void sirLangComputeDigest( const uint8_t *pBytes, uint32_t nBytes, uint8_t szDigest[32] ) noexcept
{
	uint8_t szPair[64] = {};
	SIRScriptSHA256::digest( pBytes, 64, szPair );
	SIRScriptSHA256::digest( pBytes + SIR_LANG_BYTECODE_HEADER_BYTES, nBytes - SIR_LANG_BYTECODE_HEADER_BYTES, szPair + 32 );
	SIRScriptSHA256::digest( szPair, sizeof( szPair ), szDigest );
}

sirLangVMError_t SIRLangEncodeGraphVM( const sirLangCIRProgram_t *pProgram, uint8_t *pBytes, uint32_t nCapacity, uint32_t *pUsed ) noexcept
{
	if ( pUsed == nullptr || pBytes == nullptr ) {
		return SIR_LANG_VM_INVALID_ARGUMENT;
	}
	*pUsed = 0;
	const sirLangVMError_t eVerified = SIRLangVerifyCIR( pProgram );
	if ( eVerified != SIR_LANG_VM_OK ) {
		return eVerified;
	}
	const uint64_t nRequired = sirLangBytecodeSize( pProgram );
	if ( nRequired > UINT32_MAX || nRequired > nCapacity ) {
		return SIR_LANG_VM_CAPACITY;
	}
	const uint32_t nBytes = ( uint32_t )nRequired;
	memset( pBytes, 0, nBytes );
	const uint8_t szMagic[8] = { 'S', 'I', 'R', 'L', 'B', 'C', '0', '1' };
	memcpy( pBytes, szMagic, sizeof( szMagic ) );
	sirLangWrite32( pBytes + 8, SIR_LANG_VM_VERSION );
	sirLangWrite32( pBytes + 12, SIR_LANG_BYTECODE_HEADER_BYTES );
	sirLangWrite64( pBytes + 16, pProgram->nSchemaId );
	sirLangWrite64( pBytes + 24, pProgram->nLayoutDigest );
	sirLangWrite64( pBytes + 32, pProgram->nBindingDigest );
	sirLangWrite32( pBytes + 40, pProgram->nFunctionCount );
	sirLangWrite32( pBytes + 44, pProgram->nInstructionCount );
	sirLangWrite32( pBytes + 48, pProgram->nParameterTypeCount );
	sirLangWrite32( pBytes + 52, pProgram->nCallArgumentCount );
	sirLangWrite32( pBytes + 56, pProgram->nCapabilityCount );
	sirLangWrite32( pBytes + 60, nBytes );
	uint8_t *pWrite = pBytes + SIR_LANG_BYTECODE_HEADER_BYTES;
	for ( uint32_t i = 0; i < pProgram->nFunctionCount; ++i ) {
		const sirLangCIRFunction_t& function = pProgram->pFunctions[i];
		sirLangWrite64( pWrite, function.nFunctionId );
		sirLangWrite32( pWrite + 8, function.iFirstInstruction );
		sirLangWrite32( pWrite + 12, function.nInstructionCount );
		sirLangWrite32( pWrite + 16, function.nRegisterCount );
		sirLangWrite32( pWrite + 20, function.nParameterCount );
		sirLangWrite32( pWrite + 24, function.iFirstParameterType );
		pWrite[28] = ( uint8_t )function.eReturnType;
		pWrite += SIR_LANG_BYTECODE_FUNCTION_BYTES;
	}
	for ( uint32_t i = 0; i < pProgram->nInstructionCount; ++i ) {
		const sirLangCIRInstruction_t& ins = pProgram->pInstructions[i];
		pWrite[0] = ( uint8_t )ins.eOpcode;
		pWrite[1] = ( uint8_t )ins.eType;
		pWrite[2] = ( uint8_t )ins.nFlags;
		pWrite[3] = ( uint8_t )( ins.nFlags >> 8 );
		sirLangWrite32( pWrite + 4, ins.iDestination );
		sirLangWrite32( pWrite + 8, ins.iFirstSource );
		sirLangWrite32( pWrite + 12, ins.iSecondSource );
		sirLangWrite32( pWrite + 16, ins.nAuxiliary );
		sirLangWrite64( pWrite + 20, ins.nImmediate );
		sirLangWrite32( pWrite + 28, ins.iSourceNode );
		pWrite += SIR_LANG_BYTECODE_INSTRUCTION_BYTES;
	}
	for ( uint32_t i = 0; i < pProgram->nParameterTypeCount; ++i ) {
		*pWrite++ = ( uint8_t )pProgram->pParameterTypes[i];
	}
	for ( uint32_t i = 0; i < pProgram->nCallArgumentCount; ++i ) {
		sirLangWrite32( pWrite, pProgram->pCallArguments[i] );
		pWrite += 4;
	}
	for ( uint32_t i = 0; i < pProgram->nCapabilityCount; ++i ) {
		const sirLangCIRCapability_t& capability = pProgram->pCapabilities[i];
		sirLangWrite64( pWrite, capability.nFunctionId );
		sirLangWrite64( pWrite + 8, capability.nSignatureId );
		sirLangWrite32( pWrite + 16, capability.nBindingRevision );
		sirLangWrite32( pWrite + 20, capability.nOwnerLane );
		sirLangWrite32( pWrite + 24, capability.nEffects );
		sirLangWrite32( pWrite + 28, capability.iFirstParameterType );
		sirLangWrite32( pWrite + 32, capability.nParameterCount );
		pWrite[36] = ( uint8_t )capability.eReturnType;
		pWrite += SIR_LANG_BYTECODE_CAPABILITY_BYTES;
	}
	if ( ( uint32_t )( pWrite - pBytes ) != nBytes ) {
		return SIR_LANG_VM_INVALID_FORMAT;
	}
	sirLangComputeDigest( pBytes, nBytes, pBytes + 64 );
	*pUsed = nBytes;
	return SIR_LANG_VM_OK;
}

sirLangVMError_t SIRLangDecodeGraphVM( const uint8_t *pBytes, uint32_t nBytes, sirLangCIRStorage_t *pStorage ) noexcept
{
	if ( pStorage == nullptr || pBytes == nullptr ) {
		return SIR_LANG_VM_INVALID_ARGUMENT;
	}
	pStorage->program = {};
	if ( nBytes < SIR_LANG_BYTECODE_HEADER_BYTES || memcmp( pBytes, "SIRLBC01", 8 ) != 0 ||
	     sirLangRead32( pBytes + 8 ) != SIR_LANG_VM_VERSION || sirLangRead32( pBytes + 12 ) != SIR_LANG_BYTECODE_HEADER_BYTES ||
	     sirLangRead32( pBytes + 60 ) != nBytes ) {
		return SIR_LANG_VM_INVALID_FORMAT;
	}
	const uint32_t nFunctions = sirLangRead32( pBytes + 40 );
	const uint32_t nInstructions = sirLangRead32( pBytes + 44 );
	const uint32_t nTypes = sirLangRead32( pBytes + 48 );
	const uint32_t nCallArguments = sirLangRead32( pBytes + 52 );
	const uint32_t nCapabilities = sirLangRead32( pBytes + 56 );
	if ( nFunctions == 0 || nFunctions > pStorage->nFunctionCapacity || nInstructions > pStorage->nInstructionCapacity ||
	     nTypes > pStorage->nParameterTypeCapacity || nCallArguments > pStorage->nCallArgumentCapacity ||
	     nCapabilities > pStorage->nCapabilityCount || pStorage->pFunctions == nullptr || pStorage->pInstructions == nullptr ||
	     pStorage->pParameterTypes == nullptr || ( nCallArguments && pStorage->pCallArguments == nullptr ) ||
	     ( nCapabilities && pStorage->pCapabilities == nullptr ) ) {
		return SIR_LANG_VM_CAPACITY;
	}
	const uint64_t nExpectedSize = SIR_LANG_BYTECODE_HEADER_BYTES + ( uint64_t )nFunctions * SIR_LANG_BYTECODE_FUNCTION_BYTES +
	                               ( uint64_t )nInstructions * SIR_LANG_BYTECODE_INSTRUCTION_BYTES + nTypes +
	                               ( uint64_t )nCallArguments * 4 + ( uint64_t )nCapabilities * SIR_LANG_BYTECODE_CAPABILITY_BYTES;
	if ( nExpectedSize != nBytes ) {
		return SIR_LANG_VM_INVALID_FORMAT;
	}
	uint8_t szExpectedDigest[32] = {};
	uint8_t szDigest[32] = {};
	memcpy( szExpectedDigest, pBytes + 64, sizeof( szExpectedDigest ) );
	// The digest binds the fixed header prefix and payload without copying
	// untrusted package bytes to an unbounded scratch allocation.
	sirLangComputeDigest( pBytes, nBytes, szDigest );
	if ( memcmp( szExpectedDigest, szDigest, 32 ) != 0 ) {
		return SIR_LANG_VM_INVALID_FORMAT;
	}
	const uint8_t *pRead = pBytes + SIR_LANG_BYTECODE_HEADER_BYTES;
	for ( uint32_t i = 0; i < nFunctions; ++i ) {
		sirLangCIRFunction_t& function = pStorage->pFunctions[i];
		function = {};
		function.nFunctionId = sirLangRead64( pRead );
		function.iFirstInstruction = sirLangRead32( pRead + 8 );
		function.nInstructionCount = sirLangRead32( pRead + 12 );
		function.nRegisterCount = sirLangRead32( pRead + 16 );
		function.nParameterCount = sirLangRead32( pRead + 20 );
		function.iFirstParameterType = sirLangRead32( pRead + 24 );
		function.eReturnType = ( sirLangValueKind_t )pRead[28];
		if ( pRead[29] || pRead[30] || pRead[31] ) {
			return SIR_LANG_VM_INVALID_FORMAT;
		}
		pRead += SIR_LANG_BYTECODE_FUNCTION_BYTES;
	}
	for ( uint32_t i = 0; i < nInstructions; ++i ) {
		sirLangCIRInstruction_t& ins = pStorage->pInstructions[i];
		ins = {};
		ins.eOpcode = ( sirLangOpcode_t )pRead[0];
		ins.eType = ( sirLangValueKind_t )pRead[1];
		ins.nFlags = ( uint16_t )( pRead[2] | ( uint16_t )pRead[3] << 8 );
		ins.iDestination = sirLangRead32( pRead + 4 );
		ins.iFirstSource = sirLangRead32( pRead + 8 );
		ins.iSecondSource = sirLangRead32( pRead + 12 );
		ins.nAuxiliary = sirLangRead32( pRead + 16 );
		ins.nImmediate = sirLangRead64( pRead + 20 );
		ins.iSourceNode = sirLangRead32( pRead + 28 );
		pRead += SIR_LANG_BYTECODE_INSTRUCTION_BYTES;
	}
	for ( uint32_t i = 0; i < nTypes; ++i ) {
		pStorage->pParameterTypes[i] = ( sirLangValueKind_t )*pRead++;
	}
	for ( uint32_t i = 0; i < nCallArguments; ++i ) {
		pStorage->pCallArguments[i] = sirLangRead32( pRead );
		pRead += 4;
	}
	for ( uint32_t i = 0; i < nCapabilities; ++i ) {
		// Immutable manifest must be supplied separately by the host. Validate
		// every serialized capability record against that exact manifest.
		const sirLangCIRCapability_t& capability = pStorage->pCapabilities[i];
		if ( capability.nFunctionId != sirLangRead64( pRead ) || capability.nSignatureId != sirLangRead64( pRead + 8 ) ||
		     capability.nBindingRevision != sirLangRead32( pRead + 16 ) || capability.nOwnerLane != sirLangRead32( pRead + 20 ) ||
		     capability.nEffects != sirLangRead32( pRead + 24 ) || capability.iFirstParameterType != sirLangRead32( pRead + 28 ) ||
		     capability.nParameterCount != sirLangRead32( pRead + 32 ) || capability.eReturnType != ( sirLangValueKind_t )pRead[36] ||
		     pRead[37] || pRead[38] || pRead[39] ) {
			return SIR_LANG_VM_INVALID_CAPABILITY;
		}
		pRead += SIR_LANG_BYTECODE_CAPABILITY_BYTES;
	}
	sirLangCIRProgram_t candidate = {};
	candidate.pFunctions = pStorage->pFunctions;
	candidate.nFunctionCount = nFunctions;
	candidate.pInstructions = pStorage->pInstructions;
	candidate.nInstructionCount = nInstructions;
	candidate.pParameterTypes = pStorage->pParameterTypes;
	candidate.nParameterTypeCount = nTypes;
	candidate.pCallArguments = pStorage->pCallArguments;
	candidate.nCallArgumentCount = nCallArguments;
	candidate.pCapabilities = pStorage->pCapabilities;
	candidate.nCapabilityCount = nCapabilities;
	candidate.nSchemaId = sirLangRead64( pBytes + 16 );
	candidate.nLayoutDigest = sirLangRead64( pBytes + 24 );
	candidate.nBindingDigest = sirLangRead64( pBytes + 32 );
	candidate.nRevision = sirLangRead32( pBytes + 8 );
	const sirLangVMError_t eVerified = SIRLangVerifyCIR( &candidate );
	if ( eVerified != SIR_LANG_VM_OK ) {
		return eVerified;
	}
	pStorage->program = candidate;
	return SIR_LANG_VM_OK;
}
