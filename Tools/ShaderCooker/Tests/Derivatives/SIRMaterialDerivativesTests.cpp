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
#include <stdlib.h>
#include <string.h>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static bool testNear( double fActual, double fExpected, double fTolerance = 1e-9 ) noexcept
{
	return std::fabs( fActual - fExpected ) <= fTolerance;
}

static sirMaterialTriangleSample_t testTriangle( void ) noexcept
{
	sirMaterialTriangleSample_t input = {};
	input.vertices[0] = { 0.0, 0.0, 0.0, 0.0, 1.0 };
	input.vertices[1] = { 4.0, 0.0, 1.0, 0.0, 1.0 };
	input.vertices[2] = { 0.0, 4.0, 0.0, 1.0, 1.0 };
	input.fScreenX = 1.0;
	input.fScreenY = 2.0;
	return input;
}

static void testTriangleGradientAndFootprint( void ) noexcept
{
	sirMaterialTriangleSample_t input = testTriangle();
	sirMaterialGradient_t uv = {};
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_OK );
	REQUIRE( uv.bValid && !uv.bDiscontinuous && uv.nComponents == 2 );
	REQUIRE( testNear( uv.szValue[0], 0.25 ) && testNear( uv.szValue[1], 0.5 ) );
	REQUIRE( testNear( uv.szDdx[0], 0.25 ) && testNear( uv.szDdx[1], 0.0 ) );
	REQUIRE( testNear( uv.szDdy[0], 0.0 ) && testNear( uv.szDdy[1], 0.25 ) );
	const sirMaterialTextureFootprint_t texture = { 1024, 1024, 10, 0 };
	sirMaterialSamplePlan_t plan = {};
	REQUIRE( sirMaterialPlanTextureSample( uv, texture, &plan ) == SIR_SHADER_COOK_OK );
	REQUIRE( plan.eMode == SIR_MATERIAL_SAMPLE_GRADIENT && plan.bDerivativeValid );
	REQUIRE( testNear( plan.fMajorAxis, 256.0 ) && testNear( plan.fMinorAxis, 256.0 ) );
	REQUIRE( testNear( plan.fMipLevel, 8.0 ) && testNear( plan.fAnisotropy, 1.0 ) );
	input.vertices[1].fU = -1.0;
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_OK );
	REQUIRE( testNear( uv.szDdx[0], -0.25 ) );
	input.vertices[1].fU = 1.0;
	input.vertices[1].fU = 8.0;
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_OK );
	REQUIRE( sirMaterialPlanTextureSample( uv, texture, &plan ) == SIR_SHADER_COOK_OK );
	REQUIRE( testNear( plan.fMajorAxis, 2048.0 ) && testNear( plan.fMinorAxis, 256.0 ) );
	REQUIRE( testNear( plan.fAnisotropy, 8.0 ) && testNear( plan.fMipLevel, 10.0 ) );
	uv.bValid = false;
	REQUIRE( sirMaterialPlanTextureSample( uv, texture, &plan ) == SIR_SHADER_COOK_OK );
	REQUIRE( plan.eMode == SIR_MATERIAL_SAMPLE_EXPLICIT_LOD && !plan.bDerivativeValid );
	REQUIRE( testNear( plan.fMipLevel, 0.0 ) );
	REQUIRE( sirMaterialPlanTextureSample( uv, { 0, 1024, 10, 0 }, &plan ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

static double testPerspectiveU( const sirMaterialTriangleSample_t& input, double fX, double fY ) noexcept
{
	const double fL1 = fX * 0.25;
	const double fL2 = fY * 0.25;
	const double szL[3] = { 1.0 - fL1 - fL2, fL1, fL2 };
	double fQ = 0.0;
	double fNumerator = 0.0;
	for ( uint32_t i = 0; i < 3; ++i ) {
		fQ += szL[i] * input.vertices[i].fReciprocalW;
		fNumerator += szL[i] * input.vertices[i].fReciprocalW * input.vertices[i].fU;
	}
	return fNumerator / fQ;
}

static void testPerspectiveFiniteDifference( void ) noexcept
{
	sirMaterialTriangleSample_t input = testTriangle();
	input.fScreenX = 1.1;
	input.fScreenY = 0.8;
	input.vertices[1].fReciprocalW = 0.5;
	input.vertices[2].fReciprocalW = 2.0;
	input.vertices[0].fU = 0.875;
	input.vertices[2].fU = -0.25;
	sirMaterialGradient_t uv = {};
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_OK );
	const double fEpsilon = 1e-4;
	const double fDX = ( testPerspectiveU( input, input.fScreenX + fEpsilon, input.fScreenY ) -
	                     testPerspectiveU( input, input.fScreenX - fEpsilon, input.fScreenY ) ) /
	                   ( 2.0 * fEpsilon );
	const double fDY = ( testPerspectiveU( input, input.fScreenX, input.fScreenY + fEpsilon ) -
	                     testPerspectiveU( input, input.fScreenX, input.fScreenY - fEpsilon ) ) /
	                   ( 2.0 * fEpsilon );
	REQUIRE( testNear( uv.szDdx[0], fDX, 1e-8 ) && testNear( uv.szDdy[0], fDY, 1e-8 ) );
	// Mirroring and unwrapped island coordinates are triangle-local; no
	// raster-quad derivatives from adjacent triangles can contaminate them.
	input.vertices[0].fU = 9.0;
	input.vertices[1].fU = 10.0;
	input.vertices[2].fU = 8.0;
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_OK );
	REQUIRE( testNear( uv.szDdx[0],
	                   ( testPerspectiveU( input, input.fScreenX + fEpsilon, input.fScreenY ) -
	                     testPerspectiveU( input, input.fScreenX - fEpsilon, input.fScreenY ) ) /
	                       ( 2.0 * fEpsilon ),
	                   1e-8 ) );
}

static void testDegenerateReject( void ) noexcept
{
	sirMaterialTriangleSample_t input = testTriangle();
	sirMaterialGradient_t uv = {};
	input.vertices[2].fScreenX = 2.0;
	input.vertices[2].fScreenY = 0.0;
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_UNSUPPORTED );
	REQUIRE( !uv.bValid );
	input = testTriangle();
	input.vertices[1].fReciprocalW = 0.0;
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	input.vertices[1].fReciprocalW = 1.0;
	input.vertices[0].fU = NAN;
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	input = testTriangle();
	input.fScreenX = NAN;
	REQUIRE( sirMaterialTriangleUVGradients( input, &uv ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	REQUIRE( sirMaterialTriangleUVGradients( input, nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

static sirMaterialGradient_t testScalar( double fValue, double fDx, double fDy ) noexcept
{
	sirMaterialGradient_t gradient = {};
	gradient.nComponents = 1;
	gradient.szValue[0] = fValue;
	gradient.szDdx[0] = fDx;
	gradient.szDdy[0] = fDy;
	gradient.bValid = true;
	return gradient;
}

static void testChainRuleAndBranches( void ) noexcept
{
	sirMaterialGradient_t szPrior[3] = { testScalar( 2.0, 1.0, 2.0 ), testScalar( 3.0, 4.0, 5.0 ), testScalar( 1.0, 0.0, 0.0 ) };
	sirMaterialIRNode_t node = {};
	node.eOp = SIR_MATERIAL_IR_MULTIPLY;
	node.eType = SIR_MATERIAL_IR_FLOAT;
	node.iA = 0;
	node.iB = 1;
	node.iC = SIR_MATERIAL_IR_NO_INPUT;
	sirMaterialGradientEval_t eval = { &node, szPrior, 3, 0 };
	sirMaterialGradient_t output = {};
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.bValid && testNear( output.szValue[0], 6.0 ) );
	REQUIRE( testNear( output.szDdx[0], 11.0 ) && testNear( output.szDdy[0], 16.0 ) );
	node.eOp = SIR_MATERIAL_IR_ADD;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( testNear( output.szValue[0], 5.0 ) && testNear( output.szDdx[0], 5.0 ) && testNear( output.szDdy[0], 7.0 ) );
	node.eOp = SIR_MATERIAL_IR_STATIC_SWITCH;
	node.nSwitchBit = 4;
	eval.nEnabledSwitches = 4;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( testNear( output.szValue[0], 2.0 ) );
	eval.nEnabledSwitches = 0;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( testNear( output.szValue[0], 3.0 ) );
	node.eOp = SIR_MATERIAL_IR_SELECT;
	node.iA = 2;
	node.iB = 0;
	node.iC = 1;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.bValid && testNear( output.szValue[0], 2.0 ) );
	szPrior[2].szDdx[0] = 0.5;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( !output.bValid && output.bDiscontinuous );
	node.eOp = SIR_MATERIAL_IR_OUTPUT;
	node.iA = 0;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_OK );
	REQUIRE( output.bValid );
	node.eOp = ( sirMaterialIROp_t )99;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_UNSUPPORTED );
	node.eOp = SIR_MATERIAL_IR_ADD;
	node.iB = 9;
	REQUIRE( sirMaterialPropagateGradients( eval, &output ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

static void testTriplanarAndSamplingEmission( void ) noexcept
{
	sirMaterialGradient_t world = {};
	world.nComponents = 3;
	world.bValid = true;
	world.szValue[0] = 2.0;
	world.szValue[1] = 3.0;
	world.szValue[2] = 4.0;
	world.szDdx[0] = 0.125;
	world.szDdx[1] = 0.5;
	world.szDdx[2] = 1.0;
	world.szDdy[0] = -0.25;
	world.szDdy[1] = 0.25;
	world.szDdy[2] = 0.75;
	sirMaterialTriplanarGradients_t result = {};
	REQUIRE( sirMaterialTriplanarGradients( world, &result ) == SIR_SHADER_COOK_OK );
	REQUIRE( testNear( result.xy.szDdx[1], 0.5 ) );
	REQUIRE( testNear( result.xz.szDdx[1], 1.0 ) );
	REQUIRE( testNear( result.yz.szDdy[0], 0.25 ) && testNear( result.yz.szDdy[1], 0.75 ) );
	world.bDiscontinuous = true;
	REQUIRE( sirMaterialTriplanarGradients( world, &result ) == SIR_SHADER_COOK_OK );
	REQUIRE( result.xy.bDiscontinuous && result.xz.bDiscontinuous && result.yz.bDiscontinuous );
	char szHLSL[180] = {};
	uint32_t nRequired = 0;
	sirMaterialSampleHLSL_t sample = { "AlbedoTex", "AnisoSampler", "uv", "uvDx", "uvDy", SIR_MATERIAL_SAMPLE_GRADIENT };
	REQUIRE( sirMaterialEmitSampleHLSL( sample, nullptr, 0, &nRequired ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( nRequired > 25 );
	REQUIRE( sirMaterialEmitSampleHLSL( sample, szHLSL, nRequired - 1, &nRequired ) == SIR_SHADER_COOK_CAPACITY_EXCEEDED );
	REQUIRE( sirMaterialEmitSampleHLSL( sample, szHLSL, sizeof( szHLSL ), &nRequired ) == SIR_SHADER_COOK_OK );
	REQUIRE( strcmp( szHLSL, "AlbedoTex.SampleGrad( AnisoSampler, uv, uvDx, uvDy )" ) == 0 );
	sample.eMode = SIR_MATERIAL_SAMPLE_EXPLICIT_LOD;
	REQUIRE( sirMaterialEmitSampleHLSL( sample, szHLSL, sizeof( szHLSL ), &nRequired ) == SIR_SHADER_COOK_OK );
	REQUIRE( strcmp( szHLSL, "AlbedoTex.SampleLevel( AnisoSampler, uv, 0.0 )" ) == 0 );
	sample.pszTexture = "Bad.Name";
	REQUIRE( sirMaterialEmitSampleHLSL( sample, szHLSL, sizeof( szHLSL ), &nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
	sample.pszTexture = "AlbedoTex";
	sample.eMode = ( sirMaterialSampleMode_t )3;
	REQUIRE( sirMaterialEmitSampleHLSL( sample, szHLSL, sizeof( szHLSL ), &nRequired ) == SIR_SHADER_COOK_INVALID_ARGUMENT );
}

int main( void )
{
	testTriangleGradientAndFootprint();
	testPerspectiveFiniteDifference();
	testDegenerateReject();
	testChainRuleAndBranches();
	testTriplanarAndSamplingEmission();
	return 0;
}
