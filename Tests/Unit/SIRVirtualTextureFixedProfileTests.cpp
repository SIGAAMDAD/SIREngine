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

#include <Renderer/VirtualTexturing/SIRVirtualTextureFixedProfile.h>

#include <stdio.h>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static bool testDimensions( void ) noexcept
{
	const uint32_t szAlbedo[] = { 256, 512, 1024, 2048, 4096, 4096 };
	const uint32_t szSecondary[] = { 128, 256, 512, 1024, 2048, 4096 };
	for ( uint32_t iProfile = 0; iProfile < SIR_VT_PACK_QUALITY_PROFILES; ++iProfile ) {
		const sirVirtualTextureFixedProfile_t *pProfile = sirVTFixedProfileGet( iProfile );
		SIR_TEST( pProfile != nullptr );
		SIR_TEST( sirVTFixedProfileExtent( iProfile, 0 ) == szAlbedo[iProfile] );
		SIR_TEST( sirVTFixedProfileExtent( iProfile, 1 ) == szSecondary[iProfile] );
		SIR_TEST( sirVTFixedProfileExtent( iProfile, 2 ) == szSecondary[iProfile] );
		SIR_TEST( sirVTFixedProfileExtent( iProfile, 5 ) == 0 );
		SIR_TEST( sirVTFixedProfileMip0Pages( iProfile, 0 ) == szAlbedo[iProfile] / SIR_VT_PAGE_INTERIOR );
		SIR_TEST( sirVTFixedProfileValidateExtent( iProfile, 0, szAlbedo[iProfile], szAlbedo[iProfile] ) );
		SIR_TEST( !sirVTFixedProfileValidateExtent( iProfile, 0, szAlbedo[iProfile] - 1, szAlbedo[iProfile] ) );
	}
	SIR_TEST( sirVTFixedProfileGet( 6 ) == nullptr && sirVTFixedProfileExtent( 6, 0 ) == 0 );
	SIR_TEST( sirVTFixedProfileMip0Pages( 0, 5 ) == 0 );
	return true;
}

static bool testMipSelection( void ) noexcept
{
	uint8_t nBaseMip = 0;
	sirVirtualTextureProfileMipRequest_t request = { 4, 2, 0, 16 };
	SIR_TEST( sirVTFixedProfileQualityBaseMip( request, &nBaseMip ) && nBaseMip == 2 );
	request.nChannel = 1;
	SIR_TEST( sirVTFixedProfileQualityBaseMip( request, &nBaseMip ) && nBaseMip == 2 );
	request.nMipCount = 2;
	SIR_TEST( !sirVTFixedProfileQualityBaseMip( request, &nBaseMip ) && nBaseMip == UINT8_MAX );
	request.nMipCount = 16;
	request.nHighestProfile = 0;
	request.nViewProfile = 3;
	SIR_TEST( !sirVTFixedProfileQualityBaseMip( request, &nBaseMip ) && nBaseMip == UINT8_MAX );
	request.nHighestProfile = 4;
	request.nViewProfile = 2;
	request.nChannel = 5;
	SIR_TEST( !sirVTFixedProfileQualityBaseMip( request, &nBaseMip ) && nBaseMip == UINT8_MAX );
	SIR_TEST( !sirVTFixedProfileQualityBaseMip( request, nullptr ) );
	return true;
}

static bool testConsumerProfiles( void ) noexcept
{
	SIR_TEST( sirVTFixedProfileClipmapMask( 0, false ) == 3 );
	SIR_TEST( sirVTFixedProfileClipmapMask( 0, true ) == 7 );
	SIR_TEST( sirVTFixedProfileInstanceMask( 0, true, true ) == 9 );
	SIR_TEST( sirVTFixedProfileInstanceMask( 1, true, true ) == 25 );
	SIR_TEST( sirVTFixedProfileClipmapMask( 6, false ) == 0 );
	const uint32_t szExtents[6] = { 8192, 512, 512, 512, 512, 0 };
	SIR_TEST( sirVTFixedProfileSourceSupports( 2, SIR_VT_PROFILE_CLIPMAP, 3, szExtents ) );
	SIR_TEST( !sirVTFixedProfileSourceSupports( 3, SIR_VT_PROFILE_CLIPMAP, 3, szExtents ) );
	SIR_TEST( sirVTFixedProfileHighestSupported( SIR_VT_PROFILE_CLIPMAP, 3, szExtents ) == 2 );
	SIR_TEST( !sirVTFixedProfileSourceSupports( 0, SIR_VT_PROFILE_COMPOSED, 3, szExtents ) );
	SIR_TEST( sirVTFixedProfileHighestSupported( SIR_VT_PROFILE_INSTANCE, 0, szExtents ) == UINT32_MAX );
	SIR_TEST( sirVTFixedProfileHighestSupported( SIR_VT_PROFILE_INSTANCE, 1, szExtents ) == 5 );
	SIR_TEST( sirVTFixedProfileHighestSupported( SIR_VT_PROFILE_INSTANCE, 1, nullptr ) == UINT32_MAX );
	SIR_TEST( !sirVTFixedProfileSourceSupports( 2, ( sirVirtualTextureProfileConsumer_t )99, 1, szExtents ) );
	return true;
}

int main( void )
{
	if ( !testDimensions() || !testMipSelection() || !testConsumerProfiles() ) {
		return 1;
	}
	return 0;
}
