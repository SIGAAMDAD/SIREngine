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

#include <Tools/ShaderCooker/MaterialIR/SIRMaterialIR.h>

static uint64_t sirMaterialIRHash( uint64_t nHash, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nHash ^= ( uint8_t )( nValue >> ( iByte * 8 ) );
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

static bool sirMaterialIRNumeric( sirMaterialIRType_t eType ) noexcept
{
	return eType >= SIR_MATERIAL_IR_FLOAT && eType <= SIR_MATERIAL_IR_FLOAT4;
}

static bool sirMaterialIRInput( const sirMaterialIRGraph_t& graph, uint32_t iNode, uint16_t iInput, sirMaterialIRType_t eType ) noexcept
{
	return iInput != SIR_MATERIAL_IR_NO_INPUT && iInput < iNode && graph.pNodes[iInput].eType == eType;
}

static bool sirMaterialIRValidateNode( const sirMaterialIRGraph_t& graph, uint32_t iNode, uint32_t nStaticBits,
                                       uint32_t *pSwitchMask ) noexcept
{
	const sirMaterialIRNode_t& node = graph.pNodes[iNode];
	if ( node.eType < SIR_MATERIAL_IR_FLOAT || node.eType > SIR_MATERIAL_IR_SAMPLER ) {
		return false;
	}
	uint32_t nUsed = 0;
	switch ( node.eOp ) {
	case SIR_MATERIAL_IR_CONSTANT:
	case SIR_MATERIAL_IR_PARAMETER:
		if ( node.iA != SIR_MATERIAL_IR_NO_INPUT || node.iB != SIR_MATERIAL_IR_NO_INPUT || node.iC != SIR_MATERIAL_IR_NO_INPUT ) {
			return false;
		}
		break;
	case SIR_MATERIAL_IR_ADD:
	case SIR_MATERIAL_IR_MULTIPLY:
		if ( !sirMaterialIRNumeric( node.eType ) || !sirMaterialIRInput( graph, iNode, node.iA, node.eType ) ||
		     !sirMaterialIRInput( graph, iNode, node.iB, node.eType ) || node.iC != SIR_MATERIAL_IR_NO_INPUT ) {
			return false;
		}
		nUsed = pSwitchMask[node.iA] | pSwitchMask[node.iB];
		break;
	case SIR_MATERIAL_IR_SAMPLE_2D:
		if ( node.eType != SIR_MATERIAL_IR_FLOAT4 || !sirMaterialIRInput( graph, iNode, node.iA, SIR_MATERIAL_IR_TEXTURE2D ) ||
		     !sirMaterialIRInput( graph, iNode, node.iB, SIR_MATERIAL_IR_FLOAT2 ) || node.iC != SIR_MATERIAL_IR_NO_INPUT ) {
			return false;
		}
		nUsed = pSwitchMask[node.iA] | pSwitchMask[node.iB];
		break;
	case SIR_MATERIAL_IR_SELECT:
		if ( !sirMaterialIRInput( graph, iNode, node.iA, SIR_MATERIAL_IR_BOOL ) ||
		     !sirMaterialIRInput( graph, iNode, node.iB, node.eType ) || !sirMaterialIRInput( graph, iNode, node.iC, node.eType ) ) {
			return false;
		}
		nUsed = pSwitchMask[node.iA] | pSwitchMask[node.iB] | pSwitchMask[node.iC];
		break;
	case SIR_MATERIAL_IR_STATIC_SWITCH:
		if ( node.nSwitchBit == 0 || ( node.nSwitchBit & nStaticBits ) != node.nSwitchBit ||
		     ( node.nSwitchBit & ( node.nSwitchBit - 1 ) ) != 0 || !sirMaterialIRInput( graph, iNode, node.iA, node.eType ) ||
		     !sirMaterialIRInput( graph, iNode, node.iB, node.eType ) || node.iC != SIR_MATERIAL_IR_NO_INPUT ) {
			return false;
		}
		nUsed = pSwitchMask[node.iA] | pSwitchMask[node.iB] | node.nSwitchBit;
		break;
	case SIR_MATERIAL_IR_OUTPUT:
		if ( !sirMaterialIRInput( graph, iNode, node.iA, node.eType ) || node.iB != SIR_MATERIAL_IR_NO_INPUT ||
		     node.iC != SIR_MATERIAL_IR_NO_INPUT ) {
			return false;
		}
		nUsed = pSwitchMask[node.iA];
		break;
	default:
		return false;
	}
	pSwitchMask[iNode] = nUsed;
	return true;
}

static uint64_t sirMaterialIRGraphHash( const sirMaterialIRGraph_t& graph ) noexcept
{
	uint64_t nHash = sirMaterialIRHash( 14695981039346656037ULL, graph.nSourceHash );
	for ( uint32_t iNode = 0; iNode < graph.nNodeCount; ++iNode ) {
		const sirMaterialIRNode_t& node = graph.pNodes[iNode];
		nHash = sirMaterialIRHash( nHash, node.eOp );
		nHash = sirMaterialIRHash( nHash, node.eType );
		nHash = sirMaterialIRHash( nHash, node.iA );
		nHash = sirMaterialIRHash( nHash, node.iB );
		nHash = sirMaterialIRHash( nHash, node.iC );
		nHash = sirMaterialIRHash( nHash, node.nParameter );
		nHash = sirMaterialIRHash( nHash, node.nSwitchBit );
		nHash = sirMaterialIRHash( nHash, node.nValueHash );
	}
	return nHash;
}

static bool sirMaterialIRPermutationValid( const sirMaterialIRGraph_t& graph, uint32_t nMask, uint64_t nFeatures ) noexcept
{
	for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
		const sirMaterialIRSwitch_t& item = graph.pSwitches[iSwitch];
		if ( item.bDynamic || ( nMask & item.nBit ) == 0 ) {
			continue;
		}
		if ( ( nMask & item.nRequires ) != item.nRequires || ( nMask & item.nExcludes ) != 0 ||
		     ( nFeatures & item.nRequiredFeatures ) != item.nRequiredFeatures ) {
			return false;
		}
	}
	return true;
}

static bool sirMaterialIRStageTopologyValid( uint32_t nStages ) noexcept
{
	const bool bMesh = ( nStages & SIR_GRAPHICS_STAGE_MESH ) != 0;
	const bool bTask = ( nStages & SIR_GRAPHICS_STAGE_TASK ) != 0;
	const bool bTessControl = ( nStages & SIR_GRAPHICS_STAGE_TESS_CONTROL ) != 0;
	const bool bTessEvaluation = ( nStages & SIR_GRAPHICS_STAGE_TESS_EVALUATION ) != 0;
	if ( ( bTask && !bMesh ) || bTessControl != bTessEvaluation ) {
		return false;
	}
	if ( ( bMesh || bTask ) && ( nStages & ( SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_GEOMETRY | SIR_GRAPHICS_STAGE_TESS_CONTROL |
	                                         SIR_GRAPHICS_STAGE_TESS_EVALUATION ) ) != 0 ) {
		return false;
	}
	if ( ( bTessControl || ( nStages & SIR_GRAPHICS_STAGE_GEOMETRY ) != 0 ) && ( nStages & SIR_GRAPHICS_STAGE_VERTEX ) == 0 ) {
		return false;
	}
	return true;
}

static bool sirMaterialIRStageFeaturesValid( const sirMaterialIRPass_t& pass ) noexcept
{
	uint64_t nStageFeatures = pass.ePass == SIR_SHADER_PASS_COMPUTE ? SIR_GRAPHICS_FEATURE_COMPUTE : SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( ( pass.nStageMask & SIR_GRAPHICS_STAGE_GEOMETRY ) != 0 ) {
		nStageFeatures |= SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
	}
	if ( ( pass.nStageMask & ( SIR_GRAPHICS_STAGE_TESS_CONTROL | SIR_GRAPHICS_STAGE_TESS_EVALUATION ) ) != 0 ) {
		nStageFeatures |= SIR_GRAPHICS_FEATURE_TESSELLATION;
	}
	if ( ( pass.nStageMask & ( SIR_GRAPHICS_STAGE_MESH | SIR_GRAPHICS_STAGE_TASK ) ) != 0 ) {
		nStageFeatures |= SIR_GRAPHICS_FEATURE_MESH_SHADER;
	}
	return ( pass.nRequiredFeatures & nStageFeatures ) == nStageFeatures;
}

static uint16_t sirMaterialIRRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )( ( uint16_t )pBytes[0] | ( ( uint16_t )pBytes[1] << 8 ) );
}

static uint32_t sirMaterialIRRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

// Verify the canonical 24-byte little-endian reflection records before
// deriving the same signature consumed by native shader-package validation.
static bool sirMaterialIRBindingsValid( const sirMaterialIRGraph_t& graph, uint32_t nStages ) noexcept
{
	uint16_t nPreviousSet = 0;
	uint16_t nPreviousSpace = 0;
	uint16_t nPreviousBinding = 0;
	for ( uint32_t iBinding = 0; iBinding < graph.nBindingCount; ++iBinding ) {
		const uint8_t *pRecord = graph.pEncodedBindings + iBinding * SIR_SHADER_PACKAGE_BINDING_BYTES;
		const uint16_t nSet = sirMaterialIRRead16( pRecord );
		const uint16_t nBinding = sirMaterialIRRead16( pRecord + 2 );
		const uint32_t nType = sirMaterialIRRead32( pRecord + 4 );
		const uint32_t nFrequency = pRecord[8];
		const uint32_t nVisibility = pRecord[9];
		const uint16_t nSpace = sirMaterialIRRead16( pRecord + 10 );
		const uint16_t nCount = sirMaterialIRRead16( pRecord + 12 );
		const uint16_t nFlags = sirMaterialIRRead16( pRecord + 14 );
		uint64_t nNameHash = 0;
		for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
			nNameHash |= ( uint64_t )pRecord[16 + iByte] << ( 8 * iByte );
		}
		if ( nType > SIR_GRAPHICS_BINDING_SAMPLER || nFrequency > SIR_SHADER_BINDING_DRAW || nVisibility == 0 ||
		     ( nVisibility & ~nStages ) != 0 || nCount == 0 || nFlags != 0 || nNameHash == 0 ) {
			return false;
		}
		if ( iBinding != 0 && ( nSet < nPreviousSet || ( nSet == nPreviousSet && nSpace < nPreviousSpace ) ||
		                        ( nSet == nPreviousSet && nSpace == nPreviousSpace && nBinding <= nPreviousBinding ) ) ) {
			return false;
		}
		nPreviousSet = nSet;
		nPreviousSpace = nSpace;
		nPreviousBinding = nBinding;
	}
	return true;
}

static uint64_t sirMaterialIRReflectionHash( const sirMaterialIRGraph_t& graph ) noexcept
{
	uint64_t nHash = sirMaterialIRHash( 14695981039346656037ULL, graph.nBindingCount );
	for ( uint32_t iByte = 0; iByte < graph.nBindingCount * SIR_SHADER_PACKAGE_BINDING_BYTES; ++iByte ) {
		nHash ^= graph.pEncodedBindings[iByte];
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

sirShaderCookResult_t sirMaterialIRCompile( const sirMaterialIRGraph_t& graph, const sirMaterialIRCompileOptions_t& options,
                                            sirMaterialIRCompileOutput_t *pOutput ) noexcept
{
	if ( pOutput == nullptr ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	pOutput->nRequiredCount = 0;
	pOutput->nProducedCount = 0;
	if ( graph.nAssetId == 0 || graph.nSourceHash == 0 || graph.nNodeCount == 0 ||
	     graph.nNodeCount > SIR_MATERIAL_IR_MAX_NODES || graph.nPassCount == 0 || graph.nPassCount > SIR_MATERIAL_IR_MAX_PASSES ||
	     graph.nSwitchCount > SIR_MATERIAL_IR_MAX_SWITCHES || graph.nBindingCount > SIR_MATERIAL_IR_MAX_BINDINGS ||
	     graph.pNodes == nullptr || graph.pPasses == nullptr || ( graph.nSwitchCount != 0 && graph.pSwitches == nullptr ) ||
	     ( graph.nBindingCount != 0 && graph.pEncodedBindings == nullptr ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	uint32_t nDeclaredBits = 0;
	uint32_t nStaticBits = 0;
	uint32_t nStaticCount = 0;
	for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
		const sirMaterialIRSwitch_t& item = graph.pSwitches[iSwitch];
		if ( item.nBit == 0 || ( item.nBit & ( item.nBit - 1 ) ) != 0 || ( ( nDeclaredBits & item.nBit ) != 0 ) ||
		     ( item.nRequires & item.nExcludes ) != 0 || item.nBit > ( 1U << 15 ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		nDeclaredBits |= item.nBit;
		if ( !item.bDynamic ) {
			nStaticBits |= item.nBit;
			++nStaticCount;
		}
	}
	for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
		const sirMaterialIRSwitch_t& item = graph.pSwitches[iSwitch];
		if ( ( ( item.nRequires | item.nExcludes ) & ~nStaticBits ) != 0 ||
		     ( item.bDynamic && ( item.nRequires | item.nExcludes ) != 0 ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
	}
	uint32_t szNodeMasks[SIR_MATERIAL_IR_MAX_NODES] = {};
	for ( uint32_t iNode = 0; iNode < graph.nNodeCount; ++iNode ) {
		if ( !sirMaterialIRValidateNode( graph, iNode, nStaticBits, szNodeMasks ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
	}
	uint32_t nPreviousPass = 0;
	uint32_t nStages = 0;
	for ( uint32_t iPass = 0; iPass < graph.nPassCount; ++iPass ) {
		const sirMaterialIRPass_t& pass = graph.pPasses[iPass];
		if ( pass.ePass <= nPreviousPass || pass.ePass > SIR_SHADER_PASS_COMPUTE || pass.iOutput >= graph.nNodeCount ||
		     graph.pNodes[pass.iOutput].eOp != SIR_MATERIAL_IR_OUTPUT || pass.nStageMask == 0 ||
		     ( pass.nStageMask & ~( ( SIR_GRAPHICS_STAGE_TASK << 1 ) - 1U ) ) != 0 ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		if ( ( pass.ePass == SIR_SHADER_PASS_COMPUTE ) != ( pass.nStageMask == SIR_GRAPHICS_STAGE_COMPUTE ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		if ( ( pass.nStageMask & SIR_GRAPHICS_STAGE_COMPUTE ) != 0 && pass.nStageMask != SIR_GRAPHICS_STAGE_COMPUTE ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		if ( !sirMaterialIRStageTopologyValid( pass.nStageMask ) || !sirMaterialIRStageFeaturesValid( pass ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		nPreviousPass = pass.ePass;
		nStages |= pass.nStageMask;
	}
	if ( !sirMaterialIRBindingsValid( graph, nStages ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	// Enumerate semantic static bits in bit order, independent of switch
	// descriptor order. Up to eight static choices, with bounded CPU work.
	uint32_t szStaticBits[SIR_MATERIAL_IR_MAX_SWITCHES] = {};
	uint32_t iStatic = 0;
	for ( uint32_t nBit = 0; nBit < 16; ++nBit ) {
		for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
			if ( !graph.pSwitches[iSwitch].bDynamic && graph.pSwitches[iSwitch].nBit == ( 1U << nBit ) ) {
				szStaticBits[iStatic++] = 1U << nBit;
			}
		}
	}
	const uint32_t nCombinations = 1U << nStaticCount;
	uint32_t nRequired = 0;
	for ( uint32_t iPass = 0; iPass < graph.nPassCount; ++iPass ) {
		const sirMaterialIRPass_t& pass = graph.pPasses[iPass];
		if ( ( pass.nRequiredFeatures & options.nAvailableFeatures ) != pass.nRequiredFeatures ) {
			continue;
		}
		for ( uint32_t iChoice = 0; iChoice < nCombinations; ++iChoice ) {
			uint32_t nMask = 0;
			for ( uint32_t iBit = 0; iBit < nStaticCount; ++iBit ) {
				if ( ( iChoice & ( 1U << iBit ) ) != 0 ) {
					nMask |= szStaticBits[iBit];
				}
			}
			if ( sirMaterialIRPermutationValid( graph, nMask, options.nAvailableFeatures ) ) {
				++nRequired;
			}
		}
		if ( options.bGenerateUberFallback ) {
			++nRequired;
		}
	}
	pOutput->nRequiredCount = nRequired;
	if ( nRequired == 0 ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	if ( nRequired > pOutput->nCapacity || pOutput->pEntries == nullptr ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	const uint64_t nGraphHash = sirMaterialIRGraphHash( graph );
	const uint64_t nReflection = sirMaterialIRReflectionHash( graph );
	uint32_t iOutput = 0;
	for ( uint32_t iPass = 0; iPass < graph.nPassCount; ++iPass ) {
		const sirMaterialIRPass_t& pass = graph.pPasses[iPass];
		if ( ( pass.nRequiredFeatures & options.nAvailableFeatures ) != pass.nRequiredFeatures ) {
			continue;
		}
		const uint32_t nRelevantBits = szNodeMasks[pass.iOutput];
		const uint64_t nBaseHash = sirMaterialIRHash( sirMaterialIRHash( nGraphHash, pass.iOutput ), pass.nStageMask );
		for ( uint32_t iChoice = 0; iChoice < nCombinations; ++iChoice ) {
			uint32_t nMask = 0;
			uint64_t nRequiredFeatures = pass.nRequiredFeatures;
			for ( uint32_t iBit = 0; iBit < nStaticCount; ++iBit ) {
				if ( ( iChoice & ( 1U << iBit ) ) != 0 ) {
					nMask |= szStaticBits[iBit];
				}
			}
			if ( !sirMaterialIRPermutationValid( graph, nMask, options.nAvailableFeatures ) ) {
				continue;
			}
			for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
				const sirMaterialIRSwitch_t& item = graph.pSwitches[iSwitch];
				if ( !item.bDynamic && ( nMask & item.nBit ) != 0 ) {
					nRequiredFeatures |= item.nRequiredFeatures;
				}
			}
			pOutput->pEntries[iOutput++] = {
			    pass.ePass, pass.nStageMask, nMask, nRequiredFeatures, sirMaterialIRHash( nBaseHash, nMask & nRelevantBits ), nReflection,
			    false };
		}
		if ( options.bGenerateUberFallback ) {
			pOutput->pEntries[iOutput++] = {
			    pass.ePass, pass.nStageMask, UINT32_MAX, pass.nRequiredFeatures, sirMaterialIRHash( nBaseHash, UINT32_MAX ), nReflection,
			    true };
		}
	}
	pOutput->nProducedCount = iOutput;
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirMaterialIRBuildPipelineRecipe( const sirMaterialIRGraph_t& graph, const sirMaterialShaderMapEntry_t& entry,
                                                        const sirMaterialIRPipelineSettings_t& settings,
                                                        sirShaderPipelineRecipe_t *pRecipe ) noexcept
{
	if ( pRecipe == nullptr || settings.nPackageManifestHash == 0 || settings.nSampleCount == 0 ||
	     ( settings.nSampleCount & ( settings.nSampleCount - 1 ) ) != 0 || entry.nCodeHash == 0 || entry.nReflectionSignature == 0 ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const sirMaterialIRCompileOptions_t options = { entry.nRequiredFeatures, false };
	sirMaterialIRCompileOutput_t check = {};
	const sirShaderCookResult_t eValidation = sirMaterialIRCompile( graph, options, &check );
	if ( eValidation != SIR_SHADER_COOK_CAPACITY_EXCEEDED ) {
		return eValidation == SIR_SHADER_COOK_UNSUPPORTED ? SIR_SHADER_COOK_INVALID_ARGUMENT : eValidation;
	}
	const sirMaterialIRPass_t *pPass = nullptr;
	for ( uint32_t iPass = 0; iPass < graph.nPassCount; ++iPass ) {
		if ( graph.pPasses[iPass].ePass == entry.ePass ) {
			pPass = &graph.pPasses[iPass];
			break;
		}
	}
	if ( pPass == nullptr || pPass->nStageMask != entry.nStageMask ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	uint32_t nStaticBits = 0;
	uint64_t nRequiredFeatures = pPass->nRequiredFeatures;
	for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
		if ( !graph.pSwitches[iSwitch].bDynamic ) {
			nStaticBits |= graph.pSwitches[iSwitch].nBit;
		}
	}
	const uint32_t nMask = entry.nPermutationMask;
	if ( entry.bUberFallback ) {
		if ( nMask != UINT32_MAX ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
	} else {
		if ( ( nMask & ~nStaticBits ) != 0 || !sirMaterialIRPermutationValid( graph, nMask, entry.nRequiredFeatures ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		for ( uint32_t iSwitch = 0; iSwitch < graph.nSwitchCount; ++iSwitch ) {
			const sirMaterialIRSwitch_t& item = graph.pSwitches[iSwitch];
			if ( !item.bDynamic && ( nMask & item.nBit ) != 0 ) {
				nRequiredFeatures |= item.nRequiredFeatures;
			}
		}
	}
	if ( nRequiredFeatures != entry.nRequiredFeatures || ( entry.ePass == SIR_SHADER_PASS_COMPUTE && settings.nSampleCount != 1 ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	uint32_t szNodeMasks[SIR_MATERIAL_IR_MAX_NODES] = {};
	for ( uint32_t iNode = 0; iNode < graph.nNodeCount; ++iNode ) {
		if ( !sirMaterialIRValidateNode( graph, iNode, nStaticBits, szNodeMasks ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
	}
	const uint64_t nGraphHash = sirMaterialIRGraphHash( graph );
	const uint64_t nBaseHash = sirMaterialIRHash( sirMaterialIRHash( nGraphHash, pPass->iOutput ), pPass->nStageMask );
	const uint32_t nCodeMask = entry.bUberFallback ? UINT32_MAX : nMask & szNodeMasks[pPass->iOutput];
	if ( entry.nCodeHash != sirMaterialIRHash( nBaseHash, nCodeMask ) ||
	     entry.nReflectionSignature != sirMaterialIRReflectionHash( graph ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	sirShaderPipelineRecipe_t recipe = {};
	recipe.pass.ePass = entry.ePass;
	recipe.pass.eDepthFormat = settings.eDepthFormat;
	recipe.pass.nColorFormatsHash = settings.nColorFormatsHash;
	recipe.pass.nBindingSignature = entry.nReflectionSignature;
	recipe.pass.nStageMask = entry.nStageMask;
	recipe.pass.nSampleCount = settings.nSampleCount;
	recipe.pass.bAlphaMasked = settings.bAlphaMasked;
	recipe.pass.bDoubleSided = settings.bDoubleSided;
	recipe.pass.bWritesVelocity = settings.bWritesVelocity;
	recipe.nAssetId = graph.nAssetId;
	recipe.nSourceRevision = graph.nSourceHash;
	recipe.nPackageManifestHash = settings.nPackageManifestHash;
	recipe.nPackageSchema = SIR_SHADER_PACKAGE_SCHEMA;
	recipe.nPermutationKey = entry.nPermutationMask;
	recipe.nRenderStateHash = settings.nRenderStateHash;
	recipe.nRequiredFeatures = entry.nRequiredFeatures;
	*pRecipe = recipe;
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirMaterialIRBuildCookedPass( const sirMaterialIRGraph_t& graph, const sirMaterialShaderMapEntry_t& entry,
                                                    const sirMaterialIRCookedPassSettings_t& settings,
                                                    sirShaderMaterialPassRecord_t *pPass ) noexcept
{
	if ( pPass == nullptr || settings.nDependencyCount == 0 || settings.nDependencyCount > SIR_SHADER_MATERIAL_MAX_DEPENDENCIES ||
	     settings.iShaderDependency >= settings.nDependencyCount ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	constexpr uint64_t VALID_FEATURES = ( SIR_GRAPHICS_FEATURE_PRESENT << 1 ) - 1ULL;
	if ( ( entry.nRequiredFeatures & ~VALID_FEATURES ) != 0 || ( entry.nRequiredFeatures & SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) != 0 ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	sirShaderPipelineRecipe_t recipe = {};
	const sirShaderCookResult_t eResult = sirMaterialIRBuildPipelineRecipe( graph, entry, settings.pipeline, &recipe );
	if ( eResult != SIR_SHADER_COOK_OK ) {
		return eResult;
	}
	sirShaderMaterialPassRecord_t pass = {};
	pass.nPass = ( uint32_t )recipe.pass.ePass;
	pass.iShaderDependency = settings.iShaderDependency;
	pass.nRequiredFeatures = recipe.nRequiredFeatures;
	pass.nPermutationKey = recipe.nPermutationKey;
	pass.nRenderStateHash = recipe.nRenderStateHash;
	*pPass = pass;
	return SIR_SHADER_COOK_OK;
}
