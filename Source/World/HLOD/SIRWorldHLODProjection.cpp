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

#include <World/HLOD/SIRWorldHLODProjection.h>

#include <float.h>
#include <math.h>

sirWorldHLODResult_t sirWorldHLODProjectErrors( const sirWorldHLODProjectionInput_t& input,
                                                const sirWorldHLODProjectionOutput_t& output ) noexcept
{
	if ( input.pBounds == nullptr || input.pGeometricErrors == nullptr || output.pProjectedErrors == nullptr || input.nNodeCount == 0 ||
	     output.nCapacity < input.nNodeCount || input.view.nViewportHeightPixels == 0 || !isfinite( input.view.fNearPlane ) ||
	     input.view.fNearPlane <= 0.0 || !isfinite( input.view.fVerticalParameter ) || input.view.fVerticalParameter <= 0.0 ||
	     input.view.eMode > SIR_WORLD_HLOD_PROJECTION_ORTHOGRAPHIC ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	if ( input.view.eMode == SIR_WORLD_HLOD_PROJECTION_PERSPECTIVE && input.view.fVerticalParameter >= 3.14159265358979323846 ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		if ( !isfinite( input.view.fCameraPosition[iAxis] ) || !isfinite( input.view.fCameraForward[iAxis] ) ) {
			return SIR_WORLD_HLOD_INVALID_ARGUMENT;
		}
	}

	const double fForwardLength =
	    hypot( hypot( input.view.fCameraForward[0], input.view.fCameraForward[1] ), input.view.fCameraForward[2] );
	if ( !isfinite( fForwardLength ) || fForwardLength <= 0.0 ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	// A float-derived AABB is always representable in double. Reject all invalid
	// source records before touching output, including errors on offscreen nodes.
	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		if ( !isfinite( input.pGeometricErrors[iNode] ) || input.pGeometricErrors[iNode] < 0.0f ) {
			return SIR_WORLD_HLOD_INVALID_HIERARCHY;
		}
		for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
			const float fMin = input.pBounds[iNode].fMinimum[iAxis];
			const float fMax = input.pBounds[iNode].fMaximum[iAxis];
			if ( !isfinite( fMin ) || !isfinite( fMax ) || fMin > fMax ) {
				return SIR_WORLD_HLOD_INVALID_HIERARCHY;
			}
		}
	}

	const double fScale = input.view.eMode == SIR_WORLD_HLOD_PROJECTION_PERSPECTIVE
	                          ? ( double )input.view.nViewportHeightPixels / ( 2.0 * tan( input.view.fVerticalParameter * 0.5 ) )
	                          : ( double )input.view.nViewportHeightPixels / input.view.fVerticalParameter;
	if ( !isfinite( fScale ) || fScale <= 0.0 ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	const double szForward[3] = { input.view.fCameraForward[0] / fForwardLength, input.view.fCameraForward[1] / fForwardLength,
	                              input.view.fCameraForward[2] / fForwardLength };
	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		double fNearestDepth = 0.0;
		for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
			const double fMin = ( double )input.pBounds[iNode].fMinimum[iAxis];
			const double fMax = ( double )input.pBounds[iNode].fMaximum[iAxis];
			const double fCenter = fMin + ( fMax - fMin ) * 0.5;
			const double fHalfExtent = ( fMax - fMin ) * 0.5;
			fNearestDepth += ( fCenter - input.view.fCameraPosition[iAxis] ) * szForward[iAxis] - fHalfExtent * fabs( szForward[iAxis] );
		}
		const double fDepth = fNearestDepth > input.view.fNearPlane ? fNearestDepth : input.view.fNearPlane;
		const double fPixels = input.view.eMode == SIR_WORLD_HLOD_PROJECTION_PERSPECTIVE
		                           ? ( ( double )input.pGeometricErrors[iNode] * fScale ) / fDepth
		                           : ( double )input.pGeometricErrors[iNode] * fScale;
		output.pProjectedErrors[iNode] = fPixels >= FLT_MAX ? FLT_MAX : ( float )fPixels;
	}
	return SIR_WORLD_HLOD_OK;
}
