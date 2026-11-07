/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureTileCooker.h>

#include <stdio.h>
#include <vector>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static bool testLayout( void ) noexcept
{
	sirVirtualTextureCookMipLayout_t layout = {};
	SIR_TEST( !sirVTMeasureCookedMips( 0, 4, 1, &layout ) );
	SIR_TEST( !sirVTMeasureCookedMips( 4, 0, 1, &layout ) );
	SIR_TEST( !sirVTMeasureCookedMips( 4, 4, 0, &layout ) );
	SIR_TEST( !sirVTMeasureCookedMips( 4, 4, 5, &layout ) );
	SIR_TEST( !sirVTMeasureCookedMips( 4, 4, 1, nullptr ) );
	SIR_TEST( sirVTMeasureCookedMips( 7, 5, 3, &layout ) );
	SIR_TEST( layout.nMipCount == 3 && layout.nTotalBytes == 126 );
	SIR_TEST( layout.szLevels[0].nWidth == 7 && layout.szLevels[0].nHeight == 5 && layout.szLevels[0].nBytes == 105 );
	SIR_TEST( layout.szLevels[1].nWidth == 3 && layout.szLevels[1].nHeight == 2 && layout.szLevels[1].nOffset == 105 );
	SIR_TEST( layout.szLevels[2].nWidth == 1 && layout.szLevels[2].nHeight == 1 && layout.szLevels[2].nOffset == 123 );
	SIR_TEST( sirVTCountCookedPages( 0 ) == 0 && sirVTCountCookedPages( 1 ) == 1 );
	SIR_TEST( sirVTCountCookedPages( 128 ) == 1 && sirVTCountCookedPages( 129 ) == 2 );
	SIR_TEST( sirVTCountCookedPages( UINT32_MAX ) == 33554432 );
	SIR_TEST( sirVTMeasureCookedMips( 32768, 32768, 1, &layout ) );
	SIR_TEST( !sirVTMeasureCookedMips( UINT32_MAX, UINT32_MAX, 4, &layout ) );
	return true;
}

static bool testMipFilter( void ) noexcept
{
	sirVirtualTextureCookMipLayout_t layout = {};
	SIR_TEST( sirVTMeasureCookedMips( 4, 4, 1, &layout ) );
	SIR_TEST( layout.nMipCount == 3 && layout.nTotalBytes == 21 );
	std::vector<uint8_t> pixels( ( size_t )layout.nTotalBytes, 0 );
	for ( uint32_t i = 0; i < 16; ++i ) {
		pixels[i] = ( uint8_t )i;
	}
	SIR_TEST( sirVTBuildCookedMips( layout, pixels.data(), pixels.size() ) );
	SIR_TEST( pixels[16] == 3 && pixels[17] == 5 && pixels[18] == 11 && pixels[19] == 13 && pixels[20] == 8 );
	SIR_TEST( !sirVTBuildCookedMips( layout, pixels.data(), pixels.size() - 1 ) );
	const sirVirtualTextureCookMipLayout_t valid = layout;
	layout.szLevels[1].nOffset++;
	SIR_TEST( !sirVTBuildCookedMips( layout, pixels.data(), pixels.size() ) );
	layout = valid;
	layout.nMipCount = 0;
	SIR_TEST( !sirVTBuildCookedMips( layout, pixels.data(), pixels.size() ) );
	return true;
}

static bool testGutters( void ) noexcept
{
	std::vector<uint8_t> source( 4 * 4 );
	for ( uint32_t y = 0; y < 4; ++y ) {
		for ( uint32_t x = 0; x < 4; ++x ) {
			source[y * 4 + x] = ( uint8_t )( y * 16 + x );
		}
	}
	const size_t nTileSize = SIR_VT_PHYSICAL_TILE * SIR_VT_PHYSICAL_TILE;
	std::vector<uint8_t> tile( nTileSize, 0 );
	sirVirtualTextureCookTileRequest_t request = { source.data(), source.size(), 4, 4, 1, 0, 0, SIR_VT_COOK_WRAP_CLAMP };
	SIR_TEST( sirVTBuildCookedTile( request, tile.data(), tile.size() ) );
	SIR_TEST( tile[0] == 0 && tile[3 * SIR_VT_PHYSICAL_TILE + 3] == 0 );
	SIR_TEST( tile[4 * SIR_VT_PHYSICAL_TILE + 4] == 0 && tile[7 * SIR_VT_PHYSICAL_TILE + 7] == 51 );
	SIR_TEST( tile[nTileSize - 1] == 51 );
	request.eWrapMode = SIR_VT_COOK_WRAP_REPEAT;
	SIR_TEST( sirVTBuildCookedTile( request, tile.data(), tile.size() ) );
	SIR_TEST( tile[3 * SIR_VT_PHYSICAL_TILE + 3] == 51 && tile[4 * SIR_VT_PHYSICAL_TILE + 4] == 0 );
	SIR_TEST( !sirVTBuildCookedTile( request, tile.data(), tile.size() - 1 ) );
	request.nMipBytes--;
	SIR_TEST( !sirVTBuildCookedTile( request, tile.data(), tile.size() ) );
	request.nMipBytes++;
	request.eWrapMode = ( sirVirtualTextureCookWrap_t )3;
	SIR_TEST( !sirVTBuildCookedTile( request, tile.data(), tile.size() ) );
	request.eWrapMode = SIR_VT_COOK_WRAP_CLAMP;
	request.pMipPixels = tile.data();
	request.nMipBytes = tile.size();
	SIR_TEST( !sirVTBuildCookedTile( request, tile.data(), tile.size() ) );
	return true;
}

int main( void )
{
	if ( !testLayout() || !testMipFilter() || !testGutters() ) {
		return 1;
	}
	return 0;
}
