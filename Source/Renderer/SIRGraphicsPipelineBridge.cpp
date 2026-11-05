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

#include <Renderer/SIRGraphicsPipelineBridge.h>
#include <Renderer/SIRGraphicsValidation.h>

static sirGraphicsABIShaderCode_t sirGraphicsEncodeShaderCode( const sirGraphicsShaderCode_t& source ) noexcept
{
	sirGraphicsABIShaderCode_t code = {};
	code.nSize = sizeof( code );
	code.nFormat = ( uint32_t )source.eFormat;
	code.pBytes = source.pBytes;
	code.nByteCount = source.nByteCount;
	code.nContentHash = source.nContentHash;
	code.pszEntryPoint = source.pszEntryPoint;
	return code;
}

static sirGraphicsABIBlendState_t sirGraphicsEncodeBlendState( const sirGraphicsBlendState_t& source ) noexcept
{
	sirGraphicsABIBlendState_t blend = {};
	blend.nSourceColor = ( uint32_t )source.eSourceColor;
	blend.nDestinationColor = ( uint32_t )source.eDestinationColor;
	blend.nColorOperation = ( uint32_t )source.eColorOperation;
	blend.nSourceAlpha = ( uint32_t )source.eSourceAlpha;
	blend.nDestinationAlpha = ( uint32_t )source.eDestinationAlpha;
	blend.nAlphaOperation = ( uint32_t )source.eAlphaOperation;
	blend.nColorWriteMask = source.nColorWriteMask;
	blend.nEnabled = source.bEnabled ? 1U : 0U;
	return blend;
}

sirGraphicsResult_t sirGraphicsEncodeABIPipeline( const sirGraphicsPipelineDesc_t& source, sirGraphicsABIPipeline_t *pEncoded ) noexcept
{
	if ( pEncoded == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	const sirGraphicsResult_t eStatus = sirGraphicsValidatePipeline( source );
	if ( eStatus != SIR_GRAPHICS_SUCCESS ) {
		return eStatus;
	}

	sirGraphicsABIPipeline_t encoded = {};
	encoded.nSize = sizeof( encoded );
	encoded.nType = ( uint32_t )source.eType;
	encoded.nModel = ( uint32_t )source.eModel;
	encoded.vertexShader = sirGraphicsEncodeShaderCode( source.vertexShader );
	encoded.pixelShader = sirGraphicsEncodeShaderCode( source.pixelShader );
	encoded.computeShader = sirGraphicsEncodeShaderCode( source.computeShader );
	encoded.pBindings = source.pBindings;
	encoded.pAttributes = source.pAttributes;
	encoded.nBindingCount = source.nBindingCount;
	encoded.nAttributeCount = source.nAttributeCount;
	encoded.nColorFormatCount = source.nColorFormatCount;
	encoded.nDepthFormat = ( uint32_t )source.eDepthFormat;
	encoded.nSampleCount = source.nSampleCount;
	encoded.nTopology = ( uint32_t )source.eTopology;
	encoded.nCullMode = ( uint32_t )source.eCullMode;
	encoded.nDepthCompare = ( uint32_t )source.eDepthCompare;
	encoded.nDepthTest = source.bDepthTest ? 1U : 0U;
	encoded.nDepthWrite = source.bDepthWrite ? 1U : 0U;
	encoded.pModern = source.pModern;
	encoded.pProgrammable = source.pProgrammable;
	encoded.pFixed = source.pFixed;

	for ( uint32_t iColor = 0; iColor < 8; ++iColor ) {
		encoded.szColorFormats[iColor] = ( uint32_t )source.szColorFormats[iColor];
		encoded.szBlendStates[iColor] = sirGraphicsEncodeBlendState( source.szBlendStates[iColor] );
	}

	*pEncoded = encoded;
	return SIR_GRAPHICS_SUCCESS;
}
