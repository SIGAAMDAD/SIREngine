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

#include <Terrain/Data/SIRTerrainHeightfieldView.h>

#include <math.h>

bool SIRTerrainHeightfieldView::isValidView( const sirTerrainR16View_t& view ) noexcept
{
	if ( !view.pSamples || !SIRTerrainHeightfieldCodec::isValidDescriptor( view.descriptor ) ) {
		return false;
	}
	const size_t nResolution = static_cast<size_t>(view.descriptor.nResolution);
	return view.nSampleBytes == nResolution * nResolution * sizeof( uint16_t );
}

float SIRTerrainHeightfieldView::decodeSample( const sirTerrainR16View_t& view, uint32_t nX, uint32_t nZ ) noexcept
{
	const size_t nOffset = ( static_cast<size_t>(nZ) * view.descriptor.nResolution + nX ) * sizeof( uint16_t );
	const uint16_t nValue = ( uint16_t )( static_cast<uint16_t>(view.pSamples[nOffset]) | ( static_cast<uint16_t>(view.pSamples[nOffset + 1]) << 8u ) );
	const double fRange = static_cast<double>(view.descriptor.fMaxHeight) - view.descriptor.fMinHeight;
	return ( float )( static_cast<double>(view.descriptor.fMinHeight) + fRange * ( static_cast<double>(nValue) / 65535.0 ) );
}

SIRTerrainCodecResult SIRTerrainHeightfieldView::open( sirTerrainConstByteSpan_t input, sirTerrainR16View_t& view,
                                                         uint64_t nMinimumRevision ) noexcept
{
	sirTerrainRegionDescriptor_t descriptor {};
	const SIRTerrainCodecResult eResult = SIRTerrainHeightfieldCodec::inspect( input, descriptor, nMinimumRevision );
	if ( eResult != SIRTerrainCodecResult::SIR_TERRAIN_CODEC_SUCCESS ) {
		return eResult;
	}
	view.descriptor = descriptor;
	view.pSamples = input.pData + SIRTerrainHeightfieldCodec::HEADER_BYTES;
	view.nSampleBytes = input.nCapacity - SIRTerrainHeightfieldCodec::HEADER_BYTES;
	return SIRTerrainCodecResult::SIR_TERRAIN_CODEC_SUCCESS;
}

SIRTerrainSampleResult SIRTerrainHeightfieldView::read( const sirTerrainR16View_t& view, uint32_t nX, uint32_t nZ,
                                                          float& fHeight ) noexcept
{
	if ( !isValidView( view ) ) {
		return SIRTerrainSampleResult::SIR_TERRAIN_SAMPLE_INVALID_VIEW;
	}
	if ( nX >= view.descriptor.nResolution || nZ >= view.descriptor.nResolution ) {
		return SIRTerrainSampleResult::SIR_TERRAIN_SAMPLE_OUT_OF_BOUNDS;
	}
	fHeight = decodeSample( view, nX, nZ );
	return SIRTerrainSampleResult::SIR_TERRAIN_SAMPLE_SUCCESS;
}

SIRTerrainSampleResult SIRTerrainHeightfieldView::sampleBilinear( const sirTerrainR16View_t& view, double fCellX, double fCellZ,
                                                                    float& fHeight ) noexcept
{
	if ( !isValidView( view ) ) {
		return SIRTerrainSampleResult::SIR_TERRAIN_SAMPLE_INVALID_VIEW;
	}
	if ( !isfinite( fCellX ) || !isfinite( fCellZ ) ) {
		return SIRTerrainSampleResult::SIR_TERRAIN_SAMPLE_INVALID_ARGUMENT;
	}
	const double fMaximum = static_cast<double>( view.descriptor.nResolution - 1u );
	if ( fCellX < 0.0 || fCellZ < 0.0 || fCellX > fMaximum || fCellZ > fMaximum ) {
		return SIRTerrainSampleResult::SIR_TERRAIN_SAMPLE_OUT_OF_BOUNDS;
	}
	const uint32_t nX0 = static_cast<uint32_t>(fCellX);
	const uint32_t nZ0 = static_cast<uint32_t>(fCellZ);
	const uint32_t nX1 = nX0 + ( nX0 < view.descriptor.nResolution - 1u ? 1u : 0u );
	const uint32_t nZ1 = nZ0 + ( nZ0 < view.descriptor.nResolution - 1u ? 1u : 0u );
	const double fX = fCellX - nX0;
	const double fZ = fCellZ - nZ0;
	const double fH00 = decodeSample( view, nX0, nZ0 );
	const double fH10 = decodeSample( view, nX1, nZ0 );
	const double fH01 = decodeSample( view, nX0, nZ1 );
	const double fH11 = decodeSample( view, nX1, nZ1 );
	const double fRow0 = fH00 + ( fH10 - fH00 ) * fX;
	const double fRow1 = fH01 + ( fH11 - fH01 ) * fX;
	fHeight = static_cast<float>(  fRow0 + ( fRow1 - fRow0 ) * fZ  );
	return SIRTerrainSampleResult::SIR_TERRAIN_SAMPLE_SUCCESS;
}
