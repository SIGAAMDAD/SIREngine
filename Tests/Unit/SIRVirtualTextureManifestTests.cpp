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

#include <Renderer/VirtualTexturing/SIRVirtualTextureManifest.h>
#include <stdio.h>

static constexpr uint32_t SIR_TEST_PAGE_COUNT = 20;
static constexpr uint32_t SIR_TEST_PAGE_BYTES = 256;
static constexpr uint32_t SIR_TEST_PACKAGE_BYTES = 80 + 32 * SIR_TEST_PAGE_COUNT + SIR_TEST_PAGE_BYTES * SIR_TEST_PAGE_COUNT;

static bool sirVTCheck( bool bPassed, const char *pszMessage ) noexcept
{
	if ( !bPassed ) {
		fprintf( stderr, "FAIL: %s\n", pszMessage );
	}
	return bPassed;
}

static sirVirtualTextureLayout_t sirVTMakeLayout( void ) noexcept
{
	sirVirtualTextureLayout_t layout = {};
	layout.source.m_nAssetId = UINT64_C( 0x1020304050607080 );
	layout.source.m_nTypeId = 7;
	layout.source.m_nVariantId = 11;
	layout.nRevision = 9;
	layout.nWidth = 10;
	layout.nHeight = 7;
	layout.nInteriorSize = 4;
	layout.nBorder = 2;
	layout.nMipCount = 4;
	layout.nLayerCount = 2;
	layout.eCodec = SIR_VT_CODEC_RGBA8;
	return layout;
}

static void sirVTPrepareTiles( sirVirtualTextureCookPage_t *pPages, uint8_t *pBytes, uint32_t nPageBytes ) noexcept
{
	const sirVirtualTextureLayout_t layout = sirVTMakeLayout();
	uint32_t iPage = 0;
	for ( uint32_t iLayer = 0; iLayer < layout.nLayerCount; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
			const uint32_t nWidth = layout.nWidth >> iMip ? layout.nWidth >> iMip : 1;
			const uint32_t nHeight = layout.nHeight >> iMip ? layout.nHeight >> iMip : 1;
			const uint32_t nColumns = 1 + ( nWidth - 1 ) / layout.nInteriorSize;
			const uint32_t nRows = 1 + ( nHeight - 1 ) / layout.nInteriorSize;
			for ( uint32_t iY = 0; iY < nRows; ++iY ) {
				for ( uint32_t iX = 0; iX < nColumns; ++iX ) {
					pPages[iPage] = { pBytes + ( uint64_t )iPage * nPageBytes, nPageBytes, iX, iY, ( uint16_t )iMip, ( uint16_t )iLayer };
					for ( uint32_t iByte = 0; iByte < nPageBytes; ++iByte ) {
						pBytes[( uint64_t )iPage * nPageBytes + iByte] = ( uint8_t )( iPage * 31U + iByte * 3U );
					}
					++iPage;
				}
			}
		}
	}
}

static sirVirtualTexturePageKey_t sirVTMakeKey( const sirVirtualTextureLayout_t& layout, uint16_t nMip, uint16_t nLayer, uint32_t nX,
                                                uint32_t nY ) noexcept
{
	sirVirtualTexturePageKey_t key = {};
	key.source = layout.source;
	key.nRevision = layout.nRevision;
	key.nMip = nMip;
	key.nLayer = nLayer;
	key.nX = nX;
	key.nY = nY;
	return key;
}

static void sirVTRewriteIntegrity( uint8_t *pBytes, uint32_t nSize ) noexcept
{
	const uint64_t nPrime = UINT64_C( 1099511628211 );
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( uint32_t iByte = 0; iByte < nSize; ++iByte ) {
		const uint8_t nValue = iByte >= 72 && iByte < 80 ? 0 : pBytes[iByte];
		nHash ^= nValue;
		nHash *= nPrime;
	}
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		pBytes[72 + iByte] = ( uint8_t )( nHash >> ( iByte * 8U ) );
	}
}

static bool sirVTTestRoundTrip( void ) noexcept
{
	const sirVirtualTextureLayout_t layout = sirVTMakeLayout();
	uint32_t nCount = 0;
	uint64_t nSize = 0;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nCount, &nSize ) == SIR_VT_MANIFEST_SUCCESS && nCount == SIR_TEST_PAGE_COUNT &&
	                      nSize == SIR_TEST_PACKAGE_BYTES,
	                  "measured deterministic size" ) ) {
		return false;
	}

	uint8_t szTiles[SIR_TEST_PAGE_COUNT * SIR_TEST_PAGE_BYTES] = {};
	sirVirtualTextureCookPage_t pages[SIR_TEST_PAGE_COUNT] = {};
	sirVTPrepareTiles( pages, szTiles, SIR_TEST_PAGE_BYTES );
	uint8_t szEncoded[SIR_TEST_PACKAGE_BYTES] = {};
	uint8_t szSecond[SIR_TEST_PACKAGE_BYTES] = {};
	if ( !sirVTCheck( sirVTCookManifest( layout, pages, nCount, { szEncoded, sizeof( szEncoded ) } ) == SIR_VT_MANIFEST_SUCCESS &&
	                      sirVTCookManifest( layout, pages, nCount, { szSecond, sizeof( szSecond ) } ) == SIR_VT_MANIFEST_SUCCESS &&
	                      memcmp( szEncoded, szSecond, sizeof( szEncoded ) ) == 0,
	                  "deterministic canonical bytes" ) ) {
		return false;
	}
	if ( !sirVTCheck( szEncoded[0] == 'S' && szEncoded[1] == 'V' && szEncoded[2] == 'T' && szEncoded[3] == '1',
	                  "little-endian file signature" ) ) {
		return false;
	}

	sirVirtualTextureManifestView_t view = {};
	if ( !sirVTCheck( sirVTParseManifest( szEncoded, sizeof( szEncoded ), &view ) == SIR_VT_MANIFEST_SUCCESS && view.nPagesPerLayer == 10 &&
	                      view.szMipPageStarts[1] == 6 && view.szMipPageStarts[2] == 8 && view.szMipPageStarts[3] == 9,
	                  "manifest roundtrip and mip table" ) ) {
		return false;
	}

	for ( uint32_t iPage = 0; iPage < nCount; ++iPage ) {
		const sirVirtualTextureCookPage_t& page = pages[iPage];
		const sirVirtualTexturePageKey_t key = sirVTMakeKey( layout, page.nMip, page.nLayer, page.nX, page.nY );
		sirVirtualTexturePageView_t result = {};
		if ( !sirVTCheck( sirVTGetManifestPage( view, key, &result ) == SIR_VT_MANIFEST_SUCCESS && result.nByteCount == page.nByteCount &&
		                      memcmp( result.pBytes, page.pBytes, page.nByteCount ) == 0,
		                  "page roundtrip and byte equivalence" ) ) {
			return false;
		}
	}

	const sirVirtualTexturePageKey_t invalid = sirVTMakeKey( layout, 0, 1, 3, 0 );
	sirVirtualTexturePageView_t scratch = {};
	if ( !sirVTCheck( sirVTGetManifestPage( view, invalid, &scratch ) == SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "out-of-bounds page rejected" ) ) {
		return false;
	}

	// A physical cache contains a parent but not a requested child.
	sirVirtualTextureSlot_t slots[2] = {};
	uint32_t buckets[4] = {};
	SIRVirtualTextureResidency residency;
	if ( !sirVTCheck( residency.initialize( { slots, buckets, 2, 4, 3 } ) == SIR_GRAPHICS_SUCCESS, "initialize residency" ) ) {
		return false;
	}
	const sirVirtualTexturePageKey_t parent = sirVTMakeKey( layout, 1, 1, 1, 0 );
	sirVirtualTextureHandle_t parentHandle = {};
	bool bCreated = false;
	if ( !sirVTCheck( residency.reserve( parent, &parentHandle, &bCreated ) == SIR_GRAPHICS_SUCCESS && bCreated, "reserve parent page" ) ) {
		return false;
	}
	const sirVirtualTexturePageKey_t fine = sirVTMakeKey( layout, 0, 1, 2, 1 );
	sirVirtualTextureSample_t sample = {};
	if ( !sirVTCheck( sirVTResolveResidentFallback( view, fine, residency, &sample ) == SIR_GRAPHICS_BUSY,
	                  "unuploaded pages cannot sample" ) ) {
		return false;
	}

	sirGraphicsCompletionSet_t completed = {};
	completed.nDeviceGeneration = 3;
	completed.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_COPY;
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 21;
	if ( !sirVTCheck( residency.submitUpload( parentHandle, completed ) == SIR_GRAPHICS_SUCCESS, "parent upload" ) ) {
		return false;
	}
	uint32_t nTransitions = 0;
	if ( !sirVTCheck( residency.collect( completed, &nTransitions ) == SIR_GRAPHICS_SUCCESS && nTransitions == 1,
	                  "parent upload completion" ) ) {
		return false;
	}
	if ( !sirVTCheck( sirVTResolveResidentFallback( view, fine, residency, &sample ) == SIR_GRAPHICS_SUCCESS &&
	                      sample.nFallbackLevels == 1 && sample.key.nMip == 1 && sample.key.nX == 1 && sample.key.nY == 0 &&
	                      sample.handle.nSlot == parentHandle.nSlot,
	                  "nearest resident mip ancestor" ) ) {
		return false;
	}
	const sirVirtualTexturePageKey_t unrelated = sirVTMakeKey( layout, 0, 1, 0, 0 );
	if ( !sirVTCheck( sirVTResolveResidentFallback( view, unrelated, residency, &sample ) == SIR_GRAPHICS_BUSY,
	                  "unrelated texture pages do not alias" ) ) {
		return false;
	}
	if ( !sirVTCheck( residency.retire( parentHandle, completed ) == SIR_GRAPHICS_SUCCESS &&
	                      residency.collect( completed, &nTransitions ) == SIR_GRAPHICS_SUCCESS,
	                  "retire parent before replacing with coarse mip" ) ) {
		return false;
	}
	const sirVirtualTexturePageKey_t root = sirVTMakeKey( layout, 3, 1, 0, 0 );
	sirVirtualTextureHandle_t rootHandle = {};
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 22;
	if ( !sirVTCheck( residency.reserve( root, &rootHandle, &bCreated ) == SIR_GRAPHICS_SUCCESS && bCreated &&
	                      residency.submitUpload( rootHandle, completed ) == SIR_GRAPHICS_SUCCESS &&
	                      residency.collect( completed, &nTransitions ) == SIR_GRAPHICS_SUCCESS &&
	                      sirVTResolveResidentFallback( view, fine, residency, &sample ) == SIR_GRAPHICS_SUCCESS && sample.key.nMip == 3 &&
	                      sample.nFallbackLevels == 3,
	                  "last mip is a deterministic full-coverage fallback" ) ) {
		return false;
	}
	const sirVirtualTexturePageKey_t differentLayer = sirVTMakeKey( layout, 0, 0, 2, 1 );
	if ( !sirVTCheck( sirVTResolveResidentFallback( view, differentLayer, residency, &sample ) == SIR_GRAPHICS_BUSY,
	                  "fallback remains inside material layer" ) ) {
		return false;
	}
	return true;
}

static bool sirVTTestCorruption( void ) noexcept
{
	const sirVirtualTextureLayout_t layout = sirVTMakeLayout();
	uint8_t szTiles[SIR_TEST_PAGE_COUNT * SIR_TEST_PAGE_BYTES] = {};
	sirVirtualTextureCookPage_t pages[SIR_TEST_PAGE_COUNT] = {};
	sirVTPrepareTiles( pages, szTiles, SIR_TEST_PAGE_BYTES );
	uint8_t szEncoded[SIR_TEST_PACKAGE_BYTES] = {};
	uint8_t szCorrupt[SIR_TEST_PACKAGE_BYTES] = {};
	if ( !sirVTCheck( sirVTCookManifest( layout, pages, SIR_TEST_PAGE_COUNT, { szEncoded, sizeof( szEncoded ) } ) ==
	                      SIR_VT_MANIFEST_SUCCESS,
	                  "cook corruption fixture" ) ) {
		return false;
	}
	sirVirtualTextureManifestView_t view = {};
	memcpy( szCorrupt, szEncoded, sizeof( szEncoded ) );
	szCorrupt[SIR_TEST_PACKAGE_BYTES - 1] ^= 0x80;
	if ( !sirVTCheck( sirVTParseManifest( szCorrupt, sizeof( szCorrupt ), &view ) == SIR_VT_MANIFEST_CORRUPT,
	                  "payload checksum rejects corruption" ) ) {
		return false;
	}

	memcpy( szCorrupt, szEncoded, sizeof( szEncoded ) );
	szCorrupt[80 + 4] = 8; // forged page X, with otherwise valid total checksum
	sirVTRewriteIntegrity( szCorrupt, sizeof( szCorrupt ) );
	if ( !sirVTCheck( sirVTParseManifest( szCorrupt, sizeof( szCorrupt ), &view ) == SIR_VT_MANIFEST_CORRUPT,
	                  "recomputed package hash cannot hide invalid coordinates" ) ) {
		return false;
	}

	memcpy( szCorrupt, szEncoded, sizeof( szEncoded ) );
	szCorrupt[80 + 16] = 0; // wrong offset, recomputed package checksum
	sirVTRewriteIntegrity( szCorrupt, sizeof( szCorrupt ) );
	if ( !sirVTCheck( sirVTParseManifest( szCorrupt, sizeof( szCorrupt ), &view ) == SIR_VT_MANIFEST_CORRUPT,
	                  "record overlap/gap is rejected" ) ) {
		return false;
	}

	memcpy( szCorrupt, szEncoded, sizeof( szEncoded ) );
	szCorrupt[80 + 24] ^= 1; // wrong per-page hash, recomputed package checksum
	sirVTRewriteIntegrity( szCorrupt, sizeof( szCorrupt ) );
	if ( !sirVTCheck( sirVTParseManifest( szCorrupt, sizeof( szCorrupt ), &view ) == SIR_VT_MANIFEST_CORRUPT,
	                  "page payload hash is verified independently" ) ) {
		return false;
	}

	memcpy( szCorrupt, szEncoded, sizeof( szEncoded ) );
	szCorrupt[4] = 2;
	if ( !sirVTCheck( sirVTParseManifest( szCorrupt, sizeof( szCorrupt ), &view ) == SIR_VT_MANIFEST_UNSUPPORTED,
	                  "future format version rejected" ) ) {
		return false;
	}

	const sirVirtualTexturePageView_t sentinel = {};
	if ( !sirVTCheck( sirVTParseManifest( szEncoded, 79, &view ) == SIR_VT_MANIFEST_TOO_SMALL &&
	                      sirVTParseManifest( nullptr, sizeof( szEncoded ), &view ) == SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "truncation and null input rejected" ) ) {
		return false;
	}
	( void )sentinel;
	return true;
}

static bool sirVTTestInvalidCook( void ) noexcept
{
	sirVirtualTextureLayout_t layout = sirVTMakeLayout();
	uint32_t nPages = 0;
	uint64_t nBytes = 0;
	layout.nBorder = 1;
	layout.eCodec = SIR_VT_CODEC_BC1;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nBytes ) == SIR_VT_MANIFEST_UNSUPPORTED,
	                  "block compression gutter alignment rejected" ) ) {
		return false;
	}
	layout.nBorder = 2;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nBytes ) == SIR_VT_MANIFEST_SUCCESS && nPages == 20 &&
	                      nBytes == 80 + 20 * ( 32 + 32 ),
	                  "BC1 layout uses block byte counts" ) ) {
		return false;
	}
	layout.eCodec = SIR_VT_CODEC_BC7;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nBytes ) == SIR_VT_MANIFEST_SUCCESS && nBytes == 80 + 20 * ( 32 + 64 ),
	                  "BC7 page byte count" ) ) {
		return false;
	}
	layout.eCodec = ( sirVirtualTextureCodec_t )77;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nBytes ) == SIR_VT_MANIFEST_UNSUPPORTED,
	                  "unknown compression format rejected" ) ) {
		return false;
	}
	layout = sirVTMakeLayout();
	layout.nMipCount = 3;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nBytes ) == SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "missing root mip rejected" ) ) {
		return false;
	}
	layout.nMipCount = 4;
	layout.nWidth = UINT32_MAX;
	layout.nHeight = UINT32_MAX;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nBytes ) == SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "wrong mip count rejected before products" ) ) {
		return false;
	}
	layout.nMipCount = 32;
	layout.nInteriorSize = 1;
	layout.nBorder = 0;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nBytes ) == SIR_VT_MANIFEST_OVERFLOW,
	                  "large tile address count cannot wrap" ) ) {
		return false;
	}

	layout = sirVTMakeLayout();
	uint8_t szTiles[SIR_TEST_PAGE_COUNT * SIR_TEST_PAGE_BYTES] = {};
	sirVirtualTextureCookPage_t pages[SIR_TEST_PAGE_COUNT] = {};
	sirVTPrepareTiles( pages, szTiles, SIR_TEST_PAGE_BYTES );
	uint8_t szOutput[SIR_TEST_PACKAGE_BYTES] = {};
	const sirVirtualTextureCookPage_t original = pages[1];
	pages[1] = pages[0];
	if ( !sirVTCheck( sirVTCookManifest( layout, pages, SIR_TEST_PAGE_COUNT, { szOutput, sizeof( szOutput ) } ) ==
	                      SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "noncanonical tile order rejected" ) ) {
		return false;
	}
	pages[1] = original;
	if ( !sirVTCheck( sirVTCookManifest( layout, pages, SIR_TEST_PAGE_COUNT, { szOutput, sizeof( szOutput ) - 1 } ) ==
	                      SIR_VT_MANIFEST_TOO_SMALL,
	                  "undersized output rejected" ) ) {
		return false;
	}
	pages[0].pBytes = szOutput + 300;
	if ( !sirVTCheck( sirVTCookManifest( layout, pages, SIR_TEST_PAGE_COUNT, { szOutput, sizeof( szOutput ) } ) ==
	                      SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "input/output alias rejected" ) ) {
		return false;
	}
	return true;
}

static bool sirVTTestCompressedRoundTrip( void ) noexcept
{
	sirVirtualTextureLayout_t layout = sirVTMakeLayout();
	layout.eCodec = SIR_VT_CODEC_BC1;
	uint8_t szTiles[SIR_TEST_PAGE_COUNT * 32] = {};
	sirVirtualTextureCookPage_t pages[SIR_TEST_PAGE_COUNT] = {};
	sirVTPrepareTiles( pages, szTiles, 32 );
	uint8_t szEncoded[80 + SIR_TEST_PAGE_COUNT * ( 32 + 32 )] = {};
	uint32_t nPages = 0;
	uint64_t nSize = 0;
	if ( !sirVTCheck( sirVTMeasureManifest( layout, &nPages, &nSize ) == SIR_VT_MANIFEST_SUCCESS && nSize == sizeof( szEncoded ) &&
	                      sirVTCookManifest( layout, pages, nPages, { szEncoded, sizeof( szEncoded ) } ) == SIR_VT_MANIFEST_SUCCESS,
	                  "BC1 cooked blocks have exact size" ) ) {
		return false;
	}
	sirVirtualTextureManifestView_t view = {};
	const sirVirtualTexturePageKey_t key = sirVTMakeKey( layout, 0, 1, 2, 1 );
	sirVirtualTexturePageView_t page = {};
	if ( !sirVTCheck( sirVTParseManifest( szEncoded, sizeof( szEncoded ), &view ) == SIR_VT_MANIFEST_SUCCESS &&
	                      sirVTGetManifestPage( view, key, &page ) == SIR_VT_MANIFEST_SUCCESS && page.nByteCount == 32 &&
	                      memcmp( page.pBytes, pages[15].pBytes, 32 ) == 0,
	                  "block-compressed package roundtrip" ) ) {
		return false;
	}
	return true;
}

static bool sirVTTestRGBA8Gutters( void ) noexcept
{
	const sirVirtualTextureLayout_t layout = sirVTMakeLayout();
	uint8_t szSource[7 * 44] = {};
	for ( uint32_t iY = 0; iY < 7; ++iY ) {
		for ( uint32_t iX = 0; iX < 10; ++iX ) {
			uint8_t *pPixel = szSource + iY * 44 + iX * 4;
			pPixel[0] = ( uint8_t )iX;
			pPixel[1] = ( uint8_t )iY;
			pPixel[2] = ( uint8_t )( iX + iY * 13U );
			pPixel[3] = 255;
		}
	}
	const sirVirtualTextureImage_t image = { szSource, sizeof( szSource ), 10, 7, 44 };
	uint8_t szFirst[256] = {};
	uint8_t szAdjacent[256] = {};
	uint8_t szCorner[256] = {};
	const sirVirtualTexturePageKey_t first = sirVTMakeKey( layout, 0, 0, 0, 0 );
	const sirVirtualTexturePageKey_t adjacent = sirVTMakeKey( layout, 0, 0, 1, 0 );
	const sirVirtualTexturePageKey_t corner = sirVTMakeKey( layout, 0, 0, 2, 1 );
	if ( !sirVTCheck( sirVTBuildRGBA8Tile( layout, first, image, { szFirst, sizeof( szFirst ) } ) == SIR_VT_MANIFEST_SUCCESS &&
	                      sirVTBuildRGBA8Tile( layout, adjacent, image, { szAdjacent, sizeof( szAdjacent ) } ) == SIR_VT_MANIFEST_SUCCESS &&
	                      sirVTBuildRGBA8Tile( layout, corner, image, { szCorner, sizeof( szCorner ) } ) == SIR_VT_MANIFEST_SUCCESS,
	                  "generate mip 0 tiles and clamped gutters" ) ) {
		return false;
	}
	for ( uint32_t iY = 0; iY < 8; ++iY ) {
		// Neighboring tiles have 4 shared columns (two gutters on each side).
		if ( !sirVTCheck( memcmp( szFirst + ( iY * 8 + 4 ) * 4, szAdjacent + iY * 8 * 4, 4 * 4 ) == 0,
		                  "adjacent tile overlap is byte-identical" ) ) {
			return false;
		}
	}
	if ( !sirVTCheck( szFirst[0] == 0 && szFirst[1] == 0 && szFirst[3] == 255, "negative borders clamp to first texel" ) ) {
		return false;
	}
	const uint8_t *pLast = szCorner + ( 7 * 8 + 7 ) * 4;
	if ( !sirVTCheck( pLast[0] == 9 && pLast[1] == 6 && pLast[2] == 9 + 6 * 13 && pLast[3] == 255,
	                  "partial corner tile clamps to last source texel" ) ) {
		return false;
	}
	sirVirtualTextureImage_t invalid = image;
	invalid.nRowPitch = 39;
	if ( !sirVTCheck( sirVTBuildRGBA8Tile( layout, first, invalid, { szFirst, sizeof( szFirst ) } ) == SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "short row pitch rejected" ) ) {
		return false;
	}
	invalid = image;
	invalid.nByteCount--;
	if ( !sirVTCheck( sirVTBuildRGBA8Tile( layout, first, invalid, { szFirst, sizeof( szFirst ) } ) == SIR_VT_MANIFEST_SUCCESS,
	                  "trailing pitch bytes are not required" ) ) {
		return false;
	}
	invalid.nByteCount = 44 * 6 + 39;
	if ( !sirVTCheck( sirVTBuildRGBA8Tile( layout, first, invalid, { szFirst, sizeof( szFirst ) } ) == SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "truncated last row rejected" ) ) {
		return false;
	}
	if ( !sirVTCheck( sirVTBuildRGBA8Tile( layout, first, image, { szFirst, sizeof( szFirst ) - 1 } ) == SIR_VT_MANIFEST_TOO_SMALL,
	                  "undersized tile output rejected" ) ) {
		return false;
	}
	if ( !sirVTCheck( sirVTBuildRGBA8Tile( layout, first, image, { szSource, sizeof( szSource ) } ) == SIR_VT_MANIFEST_INVALID_ARGUMENT,
	                  "source/output overlap forbidden" ) ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirVTTestRoundTrip() || !sirVTTestCorruption() || !sirVTTestInvalidCook() || !sirVTTestCompressedRoundTrip() ||
	     !sirVTTestRGBA8Gutters() ) {
		return 1;
	}
	printf( "SIRVirtualTextureManifestTests: PASS\n" );
	return 0;
}
