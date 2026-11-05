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

#include <Tools/ShaderCooker/MaterialIR/Derivatives/SIRMaterialDerivatives.h>

#include <cmath>
#include <stdio.h>
#include <string.h>

static bool sirDerivativeFinite( const sirMaterialGradient_t& gradient ) noexcept
{
	if ( gradient.nComponents == 0 || gradient.nComponents > 4 ) {
		return false;
	}
	for ( uint32_t i = 0; i < gradient.nComponents; ++i ) {
		if ( !std::isfinite( gradient.szValue[i] ) || !std::isfinite( gradient.szDdx[i] ) || !std::isfinite( gradient.szDdy[i] ) ) {
			return false;
		}
	}

	return true;
}

static bool sirDerivativeVertexFinite( const sirMaterialScreenVertex_t& vertex ) noexcept
{
	return std::isfinite( vertex.fScreenX ) && std::isfinite( vertex.fScreenY ) && std::isfinite( vertex.fU ) &&
	       std::isfinite( vertex.fV ) && std::isfinite( vertex.fReciprocalW ) && vertex.fReciprocalW > 0.0;
}

sirShaderCookResult_t sirMaterialTriangleUVGradients( const sirMaterialTriangleSample_t& input, sirMaterialGradient_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || !std::isfinite( input.fScreenX ) || !std::isfinite( input.fScreenY ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( !sirDerivativeVertexFinite( input.vertices[i] ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
	}
	const sirMaterialScreenVertex_t& a = input.vertices[0];
	const sirMaterialScreenVertex_t& b = input.vertices[1];
	const sirMaterialScreenVertex_t& c = input.vertices[2];
	const double fABX = b.fScreenX - a.fScreenX;
	const double fABY = b.fScreenY - a.fScreenY;
	const double fACX = c.fScreenX - a.fScreenX;
	const double fACY = c.fScreenY - a.fScreenY;
	const double fArea = fABX * fACY - fACX * fABY;
	const double fExtent =
	    std::fmax( 1.0, std::fmax( std::fmax( std::fabs( fABX ), std::fabs( fABY ) ), std::fmax( std::fabs( fACX ), std::fabs( fACY ) ) ) );
	if ( !std::isfinite( fArea ) || std::fabs( fArea ) <= 1e-12 * fExtent * fExtent ) {
		*pOutput = {};
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	const double fL1 = ( ( input.fScreenX - a.fScreenX ) * fACY - ( input.fScreenY - a.fScreenY ) * fACX ) / fArea;
	const double fL2 = ( fABX * ( input.fScreenY - a.fScreenY ) - fABY * ( input.fScreenX - a.fScreenX ) ) / fArea;
	const double fL0 = 1.0 - fL1 - fL2;
	const double szL[3] = { fL0, fL1, fL2 };
	const double szDx[3] = { ( b.fScreenY - c.fScreenY ) / fArea, fACY / fArea, -fABY / fArea };
	const double szDy[3] = { ( c.fScreenX - b.fScreenX ) / fArea, -fACX / fArea, fABX / fArea };
	double fQ = 0.0;
	double fQx = 0.0;
	double fQy = 0.0;
	double szN[2] = {};
	double szNx[2] = {};
	double szNy[2] = {};
	for ( uint32_t i = 0; i < 3; ++i ) {
		const double fW = input.vertices[i].fReciprocalW;
		const double szUV[2] = { input.vertices[i].fU, input.vertices[i].fV };
		fQ += szL[i] * fW;
		fQx += szDx[i] * fW;
		fQy += szDy[i] * fW;
		for ( uint32_t j = 0; j < 2; ++j ) {
			szN[j] += szL[i] * fW * szUV[j];
			szNx[j] += szDx[i] * fW * szUV[j];
			szNy[j] += szDy[i] * fW * szUV[j];
		}
	}
	if ( !std::isfinite( fQ ) || fQ <= 1e-15 ) {
		*pOutput = {};
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	sirMaterialGradient_t result = {};
	result.nComponents = 2;
	result.bValid = true;
	for ( uint32_t j = 0; j < 2; ++j ) {
		result.szValue[j] = szN[j] / fQ;
		result.szDdx[j] = ( szNx[j] - result.szValue[j] * fQx ) / fQ;
		result.szDdy[j] = ( szNy[j] - result.szValue[j] * fQy ) / fQ;
	}
	if ( !sirDerivativeFinite( result ) ) {
		*pOutput = {};
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	*pOutput = result;
	return SIR_SHADER_COOK_OK;
}

static uint32_t sirDerivativeNodeWidth( sirMaterialIRType_t eType ) noexcept
{
	if ( eType >= SIR_MATERIAL_IR_FLOAT && eType <= SIR_MATERIAL_IR_FLOAT4 ) {
		return ( uint32_t )eType;
	}
	return eType == SIR_MATERIAL_IR_BOOL ? 1U : 0U;
}

static bool sirDerivativeValidInput( const sirMaterialGradientEval_t& input, uint16_t iIndex, uint32_t nComponents ) noexcept
{
	return iIndex < input.nPriorCount && input.pPrior[iIndex].nComponents == nComponents && sirDerivativeFinite( input.pPrior[iIndex] );
}

sirShaderCookResult_t sirMaterialPropagateGradients( const sirMaterialGradientEval_t& input, sirMaterialGradient_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || input.pNode == nullptr || ( input.nPriorCount != 0 && input.pPrior == nullptr ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const sirMaterialIRNode_t& node = *input.pNode;
	const uint32_t nWidth = sirDerivativeNodeWidth( node.eType );
	if ( nWidth == 0 ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	if ( node.eOp == SIR_MATERIAL_IR_CONSTANT || node.eOp == SIR_MATERIAL_IR_PARAMETER ) {
		// The caller seeds authored constants or dynamic parameters, along
		// with their known gradients, before requesting propagation.
		return pOutput->nComponents == nWidth && sirDerivativeFinite( *pOutput ) ? SIR_SHADER_COOK_OK : SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const uint32_t nA = node.iA;
	const uint32_t nB = node.iB;
	const uint32_t nC = node.iC;
	sirMaterialGradient_t result = {};
	result.nComponents = nWidth;
	switch ( node.eOp ) {
	case SIR_MATERIAL_IR_ADD:
	case SIR_MATERIAL_IR_MULTIPLY:
		if ( !sirDerivativeValidInput( input, node.iA, nWidth ) || !sirDerivativeValidInput( input, node.iB, nWidth ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		result.bValid = input.pPrior[nA].bValid && input.pPrior[nB].bValid;
		result.bDiscontinuous = input.pPrior[nA].bDiscontinuous || input.pPrior[nB].bDiscontinuous;
		for ( uint32_t i = 0; i < nWidth; ++i ) {
			const double fA = input.pPrior[nA].szValue[i];
			const double fB = input.pPrior[nB].szValue[i];
			if ( node.eOp == SIR_MATERIAL_IR_ADD ) {
				result.szValue[i] = fA + fB;
				result.szDdx[i] = input.pPrior[nA].szDdx[i] + input.pPrior[nB].szDdx[i];
				result.szDdy[i] = input.pPrior[nA].szDdy[i] + input.pPrior[nB].szDdy[i];
			} else {
				result.szValue[i] = fA * fB;
				result.szDdx[i] = fA * input.pPrior[nB].szDdx[i] + fB * input.pPrior[nA].szDdx[i];
				result.szDdy[i] = fA * input.pPrior[nB].szDdy[i] + fB * input.pPrior[nA].szDdy[i];
			}
		}
		break;
	case SIR_MATERIAL_IR_STATIC_SWITCH:
		if ( !sirDerivativeValidInput( input, node.iA, nWidth ) || !sirDerivativeValidInput( input, node.iB, nWidth ) ||
		     node.nSwitchBit == 0 || ( node.nSwitchBit & ( node.nSwitchBit - 1 ) ) != 0 ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		result = input.pPrior[( input.nEnabledSwitches & node.nSwitchBit ) != 0 ? nA : nB];
		break;
	case SIR_MATERIAL_IR_SELECT: {
		if ( !sirDerivativeValidInput( input, node.iA, 1 ) || !sirDerivativeValidInput( input, node.iB, nWidth ) ||
		     !sirDerivativeValidInput( input, node.iC, nWidth ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		const sirMaterialGradient_t& condition = input.pPrior[nA];
		result = input.pPrior[condition.szValue[0] != 0.0 ? nB : nC];
		const bool bUniform = condition.bValid && !condition.bDiscontinuous && condition.szDdx[0] == 0.0 && condition.szDdy[0] == 0.0;
		result.bValid = result.bValid && bUniform;
		result.bDiscontinuous = result.bDiscontinuous || !bUniform;
		break;
	}
	case SIR_MATERIAL_IR_OUTPUT:
		if ( !sirDerivativeValidInput( input, node.iA, nWidth ) ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		result = input.pPrior[nA];
		break;
	case SIR_MATERIAL_IR_SAMPLE_2D:
		if ( nWidth != 4 || node.iB >= input.nPriorCount || !sirDerivativeValidInput( input, node.iB, 2 ) ||
		     node.iA >= input.nPriorCount ) {
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		// SampleGrad is emitted separately for this node. A texture's
		// output derivative requires its partials/texture samples, so
		// downstream arithmetic must use explicit fallback if needed.
		result = *pOutput;
		result.nComponents = 4;
		result.bValid = false;
		result.bDiscontinuous = input.pPrior[nB].bDiscontinuous;
		break;
	default:
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	if ( !sirDerivativeFinite( result ) ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	*pOutput = result;
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirMaterialTriplanarGradients( const sirMaterialGradient_t& worldPosition,
                                                     sirMaterialTriplanarGradients_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || worldPosition.nComponents < 3 || !sirDerivativeFinite( worldPosition ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	sirMaterialTriplanarGradients_t result = {};
	sirMaterialGradient_t *szAxes[3] = { &result.xy, &result.xz, &result.yz };
	const uint32_t szCoord[3][2] = { { 0, 1 }, { 0, 2 }, { 1, 2 } };
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		sirMaterialGradient_t& uv = *szAxes[iAxis];
		uv.nComponents = 2;
		uv.bValid = worldPosition.bValid;
		uv.bDiscontinuous = worldPosition.bDiscontinuous;
		for ( uint32_t i = 0; i < 2; ++i ) {
			const uint32_t j = szCoord[iAxis][i];
			uv.szValue[i] = worldPosition.szValue[j];
			uv.szDdx[i] = worldPosition.szDdx[j];
			uv.szDdy[i] = worldPosition.szDdy[j];
		}
	}
	*pOutput = result;
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirMaterialPlanTextureSample( const sirMaterialGradient_t& uv, const sirMaterialTextureFootprint_t& texture,
                                                    sirMaterialSamplePlan_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || texture.nWidth == 0 || texture.nHeight == 0 || texture.nMaximumMip > 31 || texture.nReserved != 0 ||
	     uv.nComponents != 2 || !sirDerivativeFinite( uv ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	sirMaterialSamplePlan_t result = {};
	if ( !uv.bValid || uv.bDiscontinuous ) {
		result.eMode = SIR_MATERIAL_SAMPLE_EXPLICIT_LOD;
		result.fAnisotropy = 1.0;
		*pOutput = result;
		return SIR_SHADER_COOK_OK;
	}
	// The SVD of the texture-space 2x2 Jacobian gives the actual
	// anisotropic ellipse. Column lengths alone are wrong for shear.
	const double fUx = uv.szDdx[0] * texture.nWidth;
	const double fVx = uv.szDdx[1] * texture.nHeight;
	const double fUy = uv.szDdy[0] * texture.nWidth;
	const double fVy = uv.szDdy[1] * texture.nHeight;
	const double fXX = fUx * fUx + fVx * fVx;
	const double fYY = fUy * fUy + fVy * fVy;
	const double fXY = fUx * fUy + fVx * fVy;
	const double fTrace = fXX + fYY;
	const double fDiscriminant = std::hypot( fXX - fYY, 2.0 * fXY );
	const double fMajorSquared = 0.5 * ( fTrace + fDiscriminant );
	const double fDeterminant = fUx * fVy - fUy * fVx;
	const double fMinorSquared = fMajorSquared > 0.0 ? ( fDeterminant * fDeterminant ) / fMajorSquared : 0.0;
	if ( !std::isfinite( fMajorSquared ) || !std::isfinite( fMinorSquared ) ||
	     fMajorSquared < 0.0 || fMinorSquared < 0.0 ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	result.eMode = SIR_MATERIAL_SAMPLE_GRADIENT;
	result.bDerivativeValid = true;
	result.fMajorAxis = std::sqrt( fMajorSquared );
	result.fMinorAxis = std::sqrt( fMinorSquared );
	result.fAnisotropy = result.fMajorAxis / std::fmax( result.fMinorAxis, 1e-12 );
	result.fMipLevel = std::fmax( 0.0, std::fmin( ( double )texture.nMaximumMip, std::log2( std::fmax( result.fMajorAxis, 1.0 ) ) ) );
	if ( !std::isfinite( result.fAnisotropy ) || !std::isfinite( result.fMipLevel ) ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	*pOutput = result;
	return SIR_SHADER_COOK_OK;
}

static bool sirDerivativeIdentifier( const char *pszName ) noexcept
{
	if ( pszName == nullptr ) {
		return false;
	}
	uint32_t nLength = 0;
	for ( ; nLength < 64 && pszName[nLength] != 0; ++nLength ) {
		const unsigned char c = ( unsigned char )pszName[nLength];
		const bool bLetter = ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' );
		const bool bDigit = c >= '0' && c <= '9';
		if ( !bLetter && c != '_' && ( nLength == 0 || !bDigit ) ) {
			return false;
		}
	}
	return nLength != 0 && nLength < 64;
}

sirShaderCookResult_t sirMaterialEmitSampleHLSL( const sirMaterialSampleHLSL_t& input, char *pszOutput, uint32_t nCapacity,
                                                 uint32_t *pRequiredBytes ) noexcept
{
	if ( pRequiredBytes == nullptr || !sirDerivativeIdentifier( input.pszTexture ) || !sirDerivativeIdentifier( input.pszSampler ) ||
	     !sirDerivativeIdentifier( input.pszUV ) ||
	     ( input.eMode != SIR_MATERIAL_SAMPLE_GRADIENT && input.eMode != SIR_MATERIAL_SAMPLE_EXPLICIT_LOD ) ||
	     ( input.eMode == SIR_MATERIAL_SAMPLE_GRADIENT &&
	       ( !sirDerivativeIdentifier( input.pszDdx ) || !sirDerivativeIdentifier( input.pszDdy ) ) ) ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const bool bGradient = input.eMode == SIR_MATERIAL_SAMPLE_GRADIENT;
	const int nLength = bGradient
	                        ? snprintf( nullptr, 0, "%s.SampleGrad( %s, %s, %s, %s )", input.pszTexture, input.pszSampler, input.pszUV,
	                                    input.pszDdx, input.pszDdy )
	                        : snprintf( nullptr, 0, "%s.SampleLevel( %s, %s, 0.0 )", input.pszTexture, input.pszSampler, input.pszUV );
	if ( nLength < 0 || ( uint32_t )nLength == UINT32_MAX ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	const uint32_t nRequired = ( uint32_t )nLength + 1;
	*pRequiredBytes = nRequired;
	if ( pszOutput == nullptr || nCapacity < nRequired ) {
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	const int nWritten =
	    bGradient ? snprintf( pszOutput, nCapacity, "%s.SampleGrad( %s, %s, %s, %s )", input.pszTexture, input.pszSampler, input.pszUV,
		                      input.pszDdx, input.pszDdy )
		          : snprintf( pszOutput, nCapacity, "%s.SampleLevel( %s, %s, 0.0 )", input.pszTexture, input.pszSampler, input.pszUV );
	return nWritten == nLength ? SIR_SHADER_COOK_OK : SIR_SHADER_COOK_UNSUPPORTED;
}
