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

#include <Tools/ShaderCooker/MaterialIR/SIRMaterialIREmitter.h>

#include <stdio.h>
#include <string.h>

static const char *sirMaterialEmitType( sirMaterialIRType_t eType ) noexcept
{
	switch ( eType ) {
	case SIR_MATERIAL_IR_FLOAT:
		return "float";
	case SIR_MATERIAL_IR_FLOAT2:
		return "float2";
	case SIR_MATERIAL_IR_FLOAT3:
		return "float3";
	case SIR_MATERIAL_IR_FLOAT4:
		return "float4";
	case SIR_MATERIAL_IR_BOOL:
		return "bool";
	default:
		return nullptr;
	}
}

static bool sirMaterialEmitIdentifierValid( const char *pszName ) noexcept
{
	if ( pszName == nullptr || strncmp( pszName, "sirValue", 8 ) == 0 ) {
		return false;
	}
	for ( uint32_t iChar = 0; iChar < 64; ++iChar ) {
		const char c = pszName[iChar];
		if ( c == '\0' ) {
			return iChar != 0;
		}
		const bool bLetter = ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_';
		if ( !bLetter && !( iChar > 0 && c >= '0' && c <= '9' ) ) {
			return false;
		}
	}
	return false;
}

static bool sirMaterialEmitAppend( char *pszBuffer, uint32_t nCapacity, uint32_t *pLength, const char *pszText ) noexcept
{
	const size_t nBytes = strlen( pszText );
	if ( *pLength >= nCapacity || nBytes >= nCapacity - *pLength ) {
		return false;
	}
	memcpy( pszBuffer + *pLength, pszText, nBytes + 1 );
	*pLength += ( uint32_t )nBytes;
	return true;
}

static bool sirMaterialEmitPermutationValid( const sirMaterialIRGraph_t& graph, uint32_t nMask, uint64_t nFeatures ) noexcept
{
	uint32_t nAllowedMask = 0;
	for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
		const sirMaterialIRSwitch_t& item = graph.pSwitches[iSwitch];
		if ( item.bDynamic ) {
			continue;
		}
		nAllowedMask |= item.nBit;
		if ( ( nMask & item.nBit ) != 0 && ( ( nMask & item.nRequires ) != item.nRequires || ( nMask & item.nExcludes ) != 0 ||
		                                     ( nFeatures & item.nRequiredFeatures ) != item.nRequiredFeatures ) ) {
			return false;
		}
	}
	return ( nMask & ~nAllowedMask ) == 0;
}

static void sirMaterialEmitReachable( const sirMaterialIRGraph_t& graph, uint16_t iOutput, uint32_t nMask, bool *pReachable ) noexcept
{
	pReachable[iOutput] = true;
	for ( uint32_t i = iOutput + 1; i != 0; --i ) {
		const uint32_t iNode = i - 1;
		if ( !pReachable[iNode] ) {
			continue;
		}
		const sirMaterialIRNode_t& node = graph.pNodes[iNode];
		switch ( node.eOp ) {
		case SIR_MATERIAL_IR_OUTPUT:
			pReachable[node.iA] = true;
			break;
		case SIR_MATERIAL_IR_ADD:
		case SIR_MATERIAL_IR_MULTIPLY:
		case SIR_MATERIAL_IR_SAMPLE_2D:
			pReachable[node.iA] = true;
			pReachable[node.iB] = true;
			break;
		case SIR_MATERIAL_IR_SELECT:
			pReachable[node.iA] = true;
			pReachable[node.iB] = true;
			pReachable[node.iC] = true;
			break;
		case SIR_MATERIAL_IR_STATIC_SWITCH:
			pReachable[( nMask & node.nSwitchBit ) != 0 ? node.iA : node.iB] = true;
			break;
		default:
			break;
		}
	}
}

sirShaderCookResult_t sirMaterialIREmitHLSL( const sirMaterialIREmitRequest_t& request, sirMaterialIREmitOutput_t *pOutput ) noexcept
{
	if ( pOutput == nullptr ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	pOutput->nRequiredBytes = 0;
	if ( request.pGraph == nullptr || request.pSymbols == nullptr ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const sirMaterialIRGraph_t& graph = *request.pGraph;
	const sirMaterialIRCompileOptions_t options = { request.nAvailableFeatures, false };
	sirMaterialIRCompileOutput_t compiled = {};
	const sirShaderCookResult_t ePlan = sirMaterialIRCompile( graph, options, &compiled );
	if ( ePlan != SIR_SHADER_COOK_CAPACITY_EXCEEDED ) {
		return ePlan;
	}
	if ( request.nSymbolCount < graph.nNodeCount ||
	     !sirMaterialEmitPermutationValid( graph, request.nPermutationMask, request.nAvailableFeatures ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	uint16_t iOutput = SIR_MATERIAL_IR_NO_INPUT;
	for ( uint32_t iPass = 0; iPass < graph.nPassCount; ++iPass ) {
		const sirMaterialIRPass_t& pass = graph.pPasses[iPass];
		if ( pass.ePass != request.ePass ) {
			continue;
		}
		if ( ( pass.nRequiredFeatures & request.nAvailableFeatures ) != pass.nRequiredFeatures ) {
			return SIR_SHADER_COOK_UNSUPPORTED;
		}
		iOutput = pass.iOutput;
		break;
	}
	if ( iOutput == SIR_MATERIAL_IR_NO_INPUT ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	const char *pszResultType = sirMaterialEmitType( graph.pNodes[iOutput].eType );
	if ( pszResultType == nullptr ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	bool szReachable[SIR_MATERIAL_IR_MAX_NODES] = {};
	sirMaterialEmitReachable( graph, iOutput, request.nPermutationMask, szReachable );
	constexpr uint32_t MAX_EMITTED_BYTES = 16384;
	char szSource[MAX_EMITTED_BYTES] = {};
	char szLine[256] = {};
	uint32_t nWritten = 0;
	const int nHeader =
	    snprintf( szLine, sizeof( szLine ), "%s sirEvaluateMaterialPass%u( void )\n{\n", pszResultType, ( uint32_t )request.ePass );
	if ( nHeader < 0 || ( uint32_t )nHeader >= sizeof( szLine ) ||
	     !sirMaterialEmitAppend( szSource, MAX_EMITTED_BYTES, &nWritten, szLine ) ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	for ( uint32_t iNode = 0; iNode < iOutput; ++iNode ) {
		if ( !szReachable[iNode] ) {
			continue;
		}
		const sirMaterialIRNode_t& node = graph.pNodes[iNode];
		const char *pszType = sirMaterialEmitType( node.eType );
		const bool bLeaf = node.eOp == SIR_MATERIAL_IR_CONSTANT || node.eOp == SIR_MATERIAL_IR_PARAMETER;
		if ( bLeaf ) {
			if ( !sirMaterialEmitIdentifierValid( request.pSymbols[iNode] ) ) {
				return SIR_SHADER_COOK_INVALID_ARGUMENT;
			}
			if ( pszType == nullptr ) {
				continue;
			}
		} else if ( pszType == nullptr ) {
			return SIR_SHADER_COOK_UNSUPPORTED;
		}
		int nLength = -1;
		switch ( node.eOp ) {
		case SIR_MATERIAL_IR_CONSTANT:
		case SIR_MATERIAL_IR_PARAMETER:
			nLength = snprintf( szLine, sizeof( szLine ), "\t%s sirValue%u = %s;\n", pszType, iNode, request.pSymbols[iNode] );
			break;
		case SIR_MATERIAL_IR_ADD:
		case SIR_MATERIAL_IR_MULTIPLY:
			nLength = snprintf( szLine, sizeof( szLine ), "\t%s sirValue%u = sirValue%u %c sirValue%u;\n", pszType, iNode, node.iA,
			                    node.eOp == SIR_MATERIAL_IR_ADD ? '+' : '*', node.iB );
			break;
		case SIR_MATERIAL_IR_SELECT:
			nLength = snprintf( szLine, sizeof( szLine ), "\t%s sirValue%u = sirValue%u ? sirValue%u : sirValue%u;\n", pszType, iNode,
			                    node.iA, node.iB, node.iC );
			break;
		case SIR_MATERIAL_IR_STATIC_SWITCH:
			nLength = snprintf( szLine, sizeof( szLine ), "\t%s sirValue%u = sirValue%u;\n", pszType, iNode,
			                    ( request.nPermutationMask & node.nSwitchBit ) != 0 ? node.iA : node.iB );
			break;
		case SIR_MATERIAL_IR_SAMPLE_2D:
			if ( !sirMaterialEmitIdentifierValid( request.pszSampler ) || graph.pNodes[node.iA].eOp != SIR_MATERIAL_IR_PARAMETER ) {
				return SIR_SHADER_COOK_UNSUPPORTED;
			}
			nLength = snprintf( szLine, sizeof( szLine ), "\tfloat4 sirValue%u = %s.SampleLevel( %s, sirValue%u, 0.0 );\n", iNode,
			                    request.pSymbols[node.iA], request.pszSampler, node.iB );
			break;
		default:
			return SIR_SHADER_COOK_UNSUPPORTED;
		}
		if ( nLength < 0 || ( uint32_t )nLength >= sizeof( szLine ) ||
		     !sirMaterialEmitAppend( szSource, MAX_EMITTED_BYTES, &nWritten, szLine ) ) {
			return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
		}
	}
	const int nTail = snprintf( szLine, sizeof( szLine ), "\treturn sirValue%u;\n}\n", graph.pNodes[iOutput].iA );
	if ( nTail < 0 || ( uint32_t )nTail >= sizeof( szLine ) || !sirMaterialEmitAppend( szSource, MAX_EMITTED_BYTES, &nWritten, szLine ) ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	pOutput->nRequiredBytes = nWritten + 1;
	if ( pOutput->pszBuffer == nullptr || pOutput->nCapacity < pOutput->nRequiredBytes ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	memcpy( pOutput->pszBuffer, szSource, pOutput->nRequiredBytes );
	return SIR_SHADER_COOK_OK;
}
