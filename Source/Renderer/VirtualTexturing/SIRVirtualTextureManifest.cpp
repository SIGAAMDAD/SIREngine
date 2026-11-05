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

static constexpr uint64_t SIR_VT_HASH_OFFSET = UINT64_C( 14695981039346656037 );
static constexpr uint64_t SIR_VT_HASH_PRIME = UINT64_C( 1099511628211 );

static uint64_t sirVTHashBytes( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	uint64_t nHash = SIR_VT_HASH_OFFSET;
	for ( uint64_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash ^= pBytes[iByte];
		nHash *= SIR_VT_HASH_PRIME;
	}
	return nHash;
}

static uint64_t sirVTHashPackage( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	uint64_t nHash = SIR_VT_HASH_OFFSET;
	for ( uint64_t iByte = 0; iByte < nBytes; ++iByte ) {
		// Header hash field is treated as zero when computing the checksum.
		const uint8_t nValue = iByte >= 72U && iByte < 80U ? 0 : pBytes[iByte];
		nHash ^= nValue;
		nHash *= SIR_VT_HASH_PRIME;
	}
	return nHash;
}

static void sirVTWriteLE( uint8_t *pBytes, uint64_t nValue, uint32_t nWidth ) noexcept
{
	for ( uint32_t iByte = 0; iByte < nWidth; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8U ) );
	}
}

static uint64_t sirVTReadLE( const uint8_t *pBytes, uint32_t nWidth ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < nWidth; ++iByte ) {
		nValue |= ( uint64_t )pBytes[iByte] << ( iByte * 8U );
	}
	return nValue;
}

static bool sirVTAddChecked( uint64_t nLeft, uint64_t nRight, uint64_t *pSum ) noexcept
{
	if ( UINT64_MAX - nLeft < nRight ) {
		return false;
	}
	*pSum = nLeft + nRight;
	return true;
}

static bool sirVTMulChecked( uint64_t nLeft, uint64_t nRight, uint64_t *pProduct ) noexcept
{
	if ( nRight != 0 && nLeft > UINT64_MAX / nRight ) {
		return false;
	}
	*pProduct = nLeft * nRight;
	return true;
}

static bool sirVTPageDimensions( const sirVirtualTextureLayout_t& layout, uint32_t *pPageBytes ) noexcept
{
	uint32_t nBlockWidth = 1;
	uint32_t nBlockBytes = 4;
	if ( layout.eCodec == SIR_VT_CODEC_BC1 ) {
		nBlockWidth = 4;
		nBlockBytes = 8;
	} else if ( layout.eCodec == SIR_VT_CODEC_BC3 || layout.eCodec == SIR_VT_CODEC_BC7 ) {
		nBlockWidth = 4;
		nBlockBytes = 16;
	} else if ( layout.eCodec != SIR_VT_CODEC_RGBA8 ) {
		return false;
	}

	const uint64_t nPhysical = ( uint64_t )layout.nInteriorSize + ( uint64_t )layout.nBorder * 2U;
	if ( nPhysical == 0 || nPhysical > UINT32_MAX || nPhysical % nBlockWidth != 0 ) {
		return false;
	}
	const uint64_t nBlocks = nPhysical / nBlockWidth;
	uint64_t nBytes = 0;
	if ( !sirVTMulChecked( nBlocks * nBlocks, nBlockBytes, &nBytes ) || nBytes > UINT32_MAX ) {
		return false;
	}
	*pPageBytes = ( uint32_t )nBytes;
	return true;
}

static uint32_t sirVTMipDimension( uint32_t nDimension, uint32_t nMip ) noexcept
{
	return nMip >= 32 ? 1U : ( nDimension >> nMip ) != 0 ? nDimension >> nMip : 1U;
}

static uint32_t sirVTTileDimension( uint32_t nDimension, uint32_t nInterior ) noexcept
{
	return 1U + ( nDimension - 1U ) / nInterior;
}

static bool sirVTKeysBelong( const sirVirtualTextureLayout_t& layout, const sirVirtualTexturePageKey_t& key ) noexcept
{
	return layout.source.m_nAssetId == key.source.m_nAssetId && layout.source.m_nTypeId == key.source.m_nTypeId &&
	       layout.source.m_nVariantId == key.source.m_nVariantId && layout.nRevision == key.nRevision;
}

sirVirtualTextureManifestResult_t sirVTMeasureManifest( const sirVirtualTextureLayout_t& layout, uint32_t *pCount,
                                                        uint64_t *pBytes ) noexcept
{
	if ( pCount == nullptr || pBytes == nullptr || layout.source.m_nAssetId == 0 || layout.nRevision == 0 || layout.nWidth == 0 ||
	     layout.nHeight == 0 || layout.nLayerCount == 0 || layout.nInteriorSize == 0 || layout.nMipCount == 0 ||
	     layout.nMipCount > SIR_VT_MAX_MIPS ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}

	uint32_t nPageBytes = 0;
	if ( !sirVTPageDimensions( layout, &nPageBytes ) ) {
		return SIR_VT_MANIFEST_UNSUPPORTED;
	}

	uint32_t nExpectedMips = 1;
	uint32_t nLargest = layout.nWidth > layout.nHeight ? layout.nWidth : layout.nHeight;
	while ( nLargest > 1 ) {
		nLargest >>= 1U;
		++nExpectedMips;
	}
	if ( nExpectedMips != layout.nMipCount ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}

	uint64_t nPerLayer = 0;
	for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
		const uint64_t nColumns = sirVTTileDimension( sirVTMipDimension( layout.nWidth, iMip ), layout.nInteriorSize );
		const uint64_t nRows = sirVTTileDimension( sirVTMipDimension( layout.nHeight, iMip ), layout.nInteriorSize );
		uint64_t nTiles = 0;
		if ( !sirVTMulChecked( nColumns, nRows, &nTiles ) || !sirVTAddChecked( nPerLayer, nTiles, &nPerLayer ) ) {
			return SIR_VT_MANIFEST_OVERFLOW;
		}
	}

	uint64_t nPages = 0;
	uint64_t nTable = 0;
	uint64_t nPayload = 0;
	uint64_t nTotal = 0;
	if ( !sirVTMulChecked( nPerLayer, layout.nLayerCount, &nPages ) || nPages > UINT32_MAX ||
	     !sirVTMulChecked( nPages, SIR_VT_MANIFEST_RECORD_BYTES, &nTable ) || !sirVTMulChecked( nPages, nPageBytes, &nPayload ) ||
	     !sirVTAddChecked( SIR_VT_MANIFEST_HEADER_BYTES, nTable, &nTotal ) || !sirVTAddChecked( nTotal, nPayload, &nTotal ) ) {
		return SIR_VT_MANIFEST_OVERFLOW;
	}
	*pCount = ( uint32_t )nPages;
	*pBytes = nTotal;
	return SIR_VT_MANIFEST_SUCCESS;
}

sirVirtualTextureManifestResult_t sirVTBuildRGBA8Tile( const sirVirtualTextureLayout_t& layout, const sirVirtualTexturePageKey_t& key,
                                                       const sirVirtualTextureImage_t& image,
                                                       sirVirtualTextureMutableBuffer_t output ) noexcept
{
	if ( layout.eCodec != SIR_VT_CODEC_RGBA8 || !sirVTKeysBelong( layout, key ) || key.nMip >= layout.nMipCount ||
	     key.nLayer >= layout.nLayerCount || image.pBytes == nullptr || output.pBytes == nullptr ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}
	uint32_t nPageCount = 0;
	uint64_t nManifestBytes = 0;
	const sirVirtualTextureManifestResult_t eLayout = sirVTMeasureManifest( layout, &nPageCount, &nManifestBytes );
	if ( eLayout != SIR_VT_MANIFEST_SUCCESS ) {
		return eLayout;
	}
	const uint32_t nMipWidth = sirVTMipDimension( layout.nWidth, key.nMip );
	const uint32_t nMipHeight = sirVTMipDimension( layout.nHeight, key.nMip );
	if ( image.nWidth != nMipWidth || image.nHeight != nMipHeight || key.nX >= sirVTTileDimension( nMipWidth, layout.nInteriorSize ) ||
	     key.nY >= sirVTTileDimension( nMipHeight, layout.nInteriorSize ) ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}

	const uint64_t nMinimumPitch = ( uint64_t )image.nWidth * 4U;
	uint64_t nRequiredImage = 0;
	uint64_t nLastRow = 0;
	uint32_t nOutputBytes = 0;
	if ( image.nRowPitch < nMinimumPitch || !sirVTMulChecked( image.nHeight - 1U, image.nRowPitch, &nLastRow ) ||
	     !sirVTAddChecked( nLastRow, nMinimumPitch, &nRequiredImage ) || image.nByteCount < nRequiredImage ||
	     !sirVTPageDimensions( layout, &nOutputBytes ) ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}
	if ( output.nCapacity < nOutputBytes ) {
		return SIR_VT_MANIFEST_TOO_SMALL;
	}
	const uintptr_t nInputStart = ( uintptr_t )image.pBytes;
	const uintptr_t nOutputStart = ( uintptr_t )output.pBytes;
	if ( nRequiredImage > UINTPTR_MAX - nInputStart || nOutputBytes > UINTPTR_MAX - nOutputStart ) {
		return SIR_VT_MANIFEST_OVERFLOW;
	}
	if ( nInputStart < nOutputStart + nOutputBytes && nOutputStart < nInputStart + ( uintptr_t )nRequiredImage ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}

	const uint32_t nPhysical = ( uint32_t )layout.nInteriorSize + ( uint32_t )layout.nBorder * 2U;
	const int64_t nBaseX = ( int64_t )key.nX * layout.nInteriorSize - layout.nBorder;
	const int64_t nBaseY = ( int64_t )key.nY * layout.nInteriorSize - layout.nBorder;
	uint8_t *pDest = ( uint8_t * )output.pBytes;
	for ( uint32_t iY = 0; iY < nPhysical; ++iY ) {
		const int64_t nUnclampedY = nBaseY + iY;
		const uint32_t nSourceY = nUnclampedY < 0 ? 0 : nUnclampedY >= image.nHeight ? image.nHeight - 1U : ( uint32_t )nUnclampedY;
		for ( uint32_t iX = 0; iX < nPhysical; ++iX ) {
			const int64_t nUnclampedX = nBaseX + iX;
			const uint32_t nSourceX = nUnclampedX < 0 ? 0 : nUnclampedX >= image.nWidth ? image.nWidth - 1U : ( uint32_t )nUnclampedX;
			memcpy( pDest + ( ( uint64_t )iY * nPhysical + iX ) * 4U, image.pBytes + ( uint64_t )nSourceY * image.nRowPitch + nSourceX * 4U,
			        4 );
		}
	}
	return SIR_VT_MANIFEST_SUCCESS;
}

sirVirtualTextureManifestResult_t sirVTCookManifest( const sirVirtualTextureLayout_t& layout, const sirVirtualTextureCookPage_t *pPages,
                                                     uint32_t nCount, sirVirtualTextureMutableBuffer_t output ) noexcept
{
	uint32_t nExpectedPages = 0;
	uint64_t nRequiredBytes = 0;
	const sirVirtualTextureManifestResult_t eLayout = sirVTMeasureManifest( layout, &nExpectedPages, &nRequiredBytes );
	if ( eLayout != SIR_VT_MANIFEST_SUCCESS ) {
		return eLayout;
	}
	if ( pPages == nullptr || nCount != nExpectedPages || output.pBytes == nullptr ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}
	if ( output.nCapacity < nRequiredBytes ) {
		return SIR_VT_MANIFEST_TOO_SMALL;
	}

	uint32_t nPageBytes = 0;
	if ( !sirVTPageDimensions( layout, &nPageBytes ) ) {
		return SIR_VT_MANIFEST_UNSUPPORTED;
	}

	const uintptr_t nOutputStart = ( uintptr_t )output.pBytes;
	if ( nRequiredBytes > UINTPTR_MAX - nOutputStart ) {
		return SIR_VT_MANIFEST_OVERFLOW;
	}
	const uintptr_t nOutputEnd = nOutputStart + ( uintptr_t )nRequiredBytes;
	uint32_t iPage = 0;
	for ( uint32_t iLayer = 0; iLayer < layout.nLayerCount; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
			const uint32_t nColumns = sirVTTileDimension( sirVTMipDimension( layout.nWidth, iMip ), layout.nInteriorSize );
			const uint32_t nRows = sirVTTileDimension( sirVTMipDimension( layout.nHeight, iMip ), layout.nInteriorSize );
			for ( uint32_t iY = 0; iY < nRows; ++iY ) {
				for ( uint32_t iX = 0; iX < nColumns; ++iX, ++iPage ) {
					const sirVirtualTextureCookPage_t& page = pPages[iPage];
					if ( page.nX != iX || page.nY != iY || page.nMip != iMip || page.nLayer != iLayer || page.pBytes == nullptr ||
					     page.nByteCount != nPageBytes ) {
						return SIR_VT_MANIFEST_INVALID_ARGUMENT;
					}
					const uintptr_t nInputStart = ( uintptr_t )page.pBytes;
					if ( nPageBytes > UINTPTR_MAX - nInputStart ) {
						return SIR_VT_MANIFEST_OVERFLOW;
					}
					const uintptr_t nInputEnd = nInputStart + nPageBytes;
					if ( nInputStart < nOutputEnd && nOutputStart < nInputEnd ) {
						return SIR_VT_MANIFEST_INVALID_ARGUMENT;
					}
				}
			}
		}
	}

	uint8_t *pOutput = ( uint8_t * )output.pBytes;
	memset( pOutput, 0, ( size_t )SIR_VT_MANIFEST_HEADER_BYTES );
	const uint64_t nPayloadOffset = SIR_VT_MANIFEST_HEADER_BYTES + ( uint64_t )nExpectedPages * SIR_VT_MANIFEST_RECORD_BYTES;
	sirVTWriteLE( pOutput + 0, SIR_VT_MANIFEST_MAGIC, 4 );
	sirVTWriteLE( pOutput + 4, SIR_VT_MANIFEST_VERSION, 2 );
	sirVTWriteLE( pOutput + 6, SIR_VT_MANIFEST_HEADER_BYTES, 2 );
	sirVTWriteLE( pOutput + 8, layout.source.m_nAssetId, 8 );
	sirVTWriteLE( pOutput + 16, layout.source.m_nTypeId, 4 );
	sirVTWriteLE( pOutput + 20, layout.source.m_nVariantId, 4 );
	sirVTWriteLE( pOutput + 24, layout.nRevision, 8 );
	sirVTWriteLE( pOutput + 32, layout.nWidth, 4 );
	sirVTWriteLE( pOutput + 36, layout.nHeight, 4 );
	sirVTWriteLE( pOutput + 40, layout.nInteriorSize, 2 );
	sirVTWriteLE( pOutput + 42, layout.nBorder, 2 );
	sirVTWriteLE( pOutput + 44, layout.nLayerCount, 2 );
	sirVTWriteLE( pOutput + 46, layout.nMipCount, 2 );
	sirVTWriteLE( pOutput + 48, layout.eCodec, 4 );
	sirVTWriteLE( pOutput + 52, nExpectedPages, 4 );
	sirVTWriteLE( pOutput + 56, nPayloadOffset, 8 );
	sirVTWriteLE( pOutput + 64, nRequiredBytes, 8 );

	uint64_t nPayloadCursor = nPayloadOffset;
	for ( uint32_t iRecord = 0; iRecord < nExpectedPages; ++iRecord ) {
		const sirVirtualTextureCookPage_t& page = pPages[iRecord];
		uint8_t *pRecord = pOutput + SIR_VT_MANIFEST_HEADER_BYTES + ( uint64_t )iRecord * SIR_VT_MANIFEST_RECORD_BYTES;
		sirVTWriteLE( pRecord + 0, page.nMip, 2 );
		sirVTWriteLE( pRecord + 2, page.nLayer, 2 );
		sirVTWriteLE( pRecord + 4, page.nX, 4 );
		sirVTWriteLE( pRecord + 8, page.nY, 4 );
		sirVTWriteLE( pRecord + 12, page.nByteCount, 4 );
		sirVTWriteLE( pRecord + 16, nPayloadCursor, 8 );
		sirVTWriteLE( pRecord + 24, sirVTHashBytes( page.pBytes, page.nByteCount ), 8 );
		memcpy( pOutput + nPayloadCursor, page.pBytes, page.nByteCount );
		nPayloadCursor += page.nByteCount;
	}
	sirVTWriteLE( pOutput + 72, sirVTHashPackage( pOutput, nRequiredBytes ), 8 );
	return SIR_VT_MANIFEST_SUCCESS;
}

sirVirtualTextureManifestResult_t sirVTParseManifest( const void *pBytes, uint64_t nBytes, sirVirtualTextureManifestView_t *pView ) noexcept
{
	if ( pBytes == nullptr || pView == nullptr ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}
	if ( nBytes < SIR_VT_MANIFEST_HEADER_BYTES || nBytes > SIZE_MAX ) {
		return SIR_VT_MANIFEST_TOO_SMALL;
	}
	const uint8_t *pData = ( const uint8_t * )pBytes;
	if ( sirVTReadLE( pData, 4 ) != SIR_VT_MANIFEST_MAGIC || sirVTReadLE( pData + 6, 2 ) != SIR_VT_MANIFEST_HEADER_BYTES ) {
		return SIR_VT_MANIFEST_CORRUPT;
	}
	if ( sirVTReadLE( pData + 4, 2 ) != SIR_VT_MANIFEST_VERSION ) {
		return SIR_VT_MANIFEST_UNSUPPORTED;
	}
	const uint64_t nEncodedTotal = sirVTReadLE( pData + 64, 8 );
	if ( nEncodedTotal != nBytes || sirVTReadLE( pData + 72, 8 ) != sirVTHashPackage( pData, nBytes ) ) {
		return SIR_VT_MANIFEST_CORRUPT;
	}

	sirVirtualTextureManifestView_t candidate = {};
	candidate.layout.source.m_nAssetId = sirVTReadLE( pData + 8, 8 );
	candidate.layout.source.m_nTypeId = ( uint32_t )sirVTReadLE( pData + 16, 4 );
	candidate.layout.source.m_nVariantId = ( uint32_t )sirVTReadLE( pData + 20, 4 );
	candidate.layout.nRevision = sirVTReadLE( pData + 24, 8 );
	candidate.layout.nWidth = ( uint32_t )sirVTReadLE( pData + 32, 4 );
	candidate.layout.nHeight = ( uint32_t )sirVTReadLE( pData + 36, 4 );
	candidate.layout.nInteriorSize = ( uint16_t )sirVTReadLE( pData + 40, 2 );
	candidate.layout.nBorder = ( uint16_t )sirVTReadLE( pData + 42, 2 );
	candidate.layout.nLayerCount = ( uint16_t )sirVTReadLE( pData + 44, 2 );
	candidate.layout.nMipCount = ( uint16_t )sirVTReadLE( pData + 46, 2 );
	candidate.layout.eCodec = ( sirVirtualTextureCodec_t )sirVTReadLE( pData + 48, 4 );
	candidate.nPageCount = ( uint32_t )sirVTReadLE( pData + 52, 4 );
	candidate.nPayloadOffset = sirVTReadLE( pData + 56, 8 );

	uint32_t nComputedCount = 0;
	uint64_t nComputedBytes = 0;
	if ( candidate.layout.nMipCount > SIR_VT_MAX_MIPS ||
	     sirVTMeasureManifest( candidate.layout, &nComputedCount, &nComputedBytes ) != SIR_VT_MANIFEST_SUCCESS ||
	     nComputedCount != candidate.nPageCount || nComputedBytes != nBytes ||
	     candidate.nPayloadOffset != SIR_VT_MANIFEST_HEADER_BYTES + ( uint64_t )nComputedCount * SIR_VT_MANIFEST_RECORD_BYTES ) {
		return SIR_VT_MANIFEST_CORRUPT;
	}

	uint32_t nExpectedPageBytes = 0;
	if ( !sirVTPageDimensions( candidate.layout, &nExpectedPageBytes ) ) {
		return SIR_VT_MANIFEST_CORRUPT;
	}
	uint32_t nPerLayer = 0;
	for ( uint32_t iMip = 0; iMip < candidate.layout.nMipCount; ++iMip ) {
		candidate.szMipPageStarts[iMip] = nPerLayer;
		candidate.szMipColumns[iMip] =
		    sirVTTileDimension( sirVTMipDimension( candidate.layout.nWidth, iMip ), candidate.layout.nInteriorSize );
		candidate.szMipRows[iMip] =
		    sirVTTileDimension( sirVTMipDimension( candidate.layout.nHeight, iMip ), candidate.layout.nInteriorSize );
		nPerLayer += candidate.szMipColumns[iMip] * candidate.szMipRows[iMip];
	}
	candidate.nPagesPerLayer = nPerLayer;

	uint64_t nPayloadCursor = candidate.nPayloadOffset;
	uint32_t iRecord = 0;
	for ( uint32_t iLayer = 0; iLayer < candidate.layout.nLayerCount; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip < candidate.layout.nMipCount; ++iMip ) {
			for ( uint32_t iY = 0; iY < candidate.szMipRows[iMip]; ++iY ) {
				for ( uint32_t iX = 0; iX < candidate.szMipColumns[iMip]; ++iX, ++iRecord ) {
					const uint8_t *pRecord = pData + SIR_VT_MANIFEST_HEADER_BYTES + ( uint64_t )iRecord * SIR_VT_MANIFEST_RECORD_BYTES;
					if ( sirVTReadLE( pRecord + 0, 2 ) != iMip || sirVTReadLE( pRecord + 2, 2 ) != iLayer ||
					     sirVTReadLE( pRecord + 4, 4 ) != iX || sirVTReadLE( pRecord + 8, 4 ) != iY ||
					     sirVTReadLE( pRecord + 12, 4 ) != nExpectedPageBytes || sirVTReadLE( pRecord + 16, 8 ) != nPayloadCursor ||
					     sirVTReadLE( pRecord + 24, 8 ) != sirVTHashBytes( pData + nPayloadCursor, nExpectedPageBytes ) ) {
						return SIR_VT_MANIFEST_CORRUPT;
					}
					nPayloadCursor += nExpectedPageBytes;
				}
			}
		}
	}
	if ( nPayloadCursor != nBytes ) {
		return SIR_VT_MANIFEST_CORRUPT;
	}
	candidate.pBytes = pData;
	candidate.nByteCount = nBytes;
	*pView = candidate;
	return SIR_VT_MANIFEST_SUCCESS;
}

sirVirtualTextureManifestResult_t sirVTGetManifestPage( const sirVirtualTextureManifestView_t& view, const sirVirtualTexturePageKey_t& key,
                                                        sirVirtualTexturePageView_t *pPage ) noexcept
{
	if ( view.pBytes == nullptr || pPage == nullptr || !sirVTKeysBelong( view.layout, key ) || key.nMip >= view.layout.nMipCount ||
	     key.nLayer >= view.layout.nLayerCount || key.nX >= view.szMipColumns[key.nMip] || key.nY >= view.szMipRows[key.nMip] ) {
		return SIR_VT_MANIFEST_INVALID_ARGUMENT;
	}
	const uint64_t iRecord = ( uint64_t )key.nLayer * view.nPagesPerLayer + view.szMipPageStarts[key.nMip] +
	                         ( uint64_t )key.nY * view.szMipColumns[key.nMip] + key.nX;
	if ( iRecord >= view.nPageCount ) {
		return SIR_VT_MANIFEST_CORRUPT;
	}
	const uint8_t *pRecord = view.pBytes + SIR_VT_MANIFEST_HEADER_BYTES + iRecord * SIR_VT_MANIFEST_RECORD_BYTES;
	sirVirtualTexturePageView_t result = {};
	result.key = key;
	result.nByteCount = ( uint32_t )sirVTReadLE( pRecord + 12, 4 );
	result.nOffset = sirVTReadLE( pRecord + 16, 8 );
	result.nContentHash = sirVTReadLE( pRecord + 24, 8 );
	result.pBytes = view.pBytes + result.nOffset;
	*pPage = result;
	return SIR_VT_MANIFEST_SUCCESS;
}

sirGraphicsResult_t sirVTResolveResidentFallback( const sirVirtualTextureManifestView_t& view, const sirVirtualTexturePageKey_t& key,
                                                  const SIRVirtualTextureResidency& residency, sirVirtualTextureSample_t *pSample ) noexcept
{
	if ( pSample == nullptr || view.pBytes == nullptr || !sirVTKeysBelong( view.layout, key ) || key.nMip >= view.layout.nMipCount ||
	     key.nLayer >= view.layout.nLayerCount || key.nX >= view.szMipColumns[key.nMip] || key.nY >= view.szMipRows[key.nMip] ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVirtualTexturePageKey_t candidate = key;
	for ( uint32_t iMip = key.nMip; iMip < view.layout.nMipCount; ++iMip ) {
		sirVirtualTextureHandle_t handle = {};
		if ( residency.find( candidate, &handle ) == SIR_GRAPHICS_SUCCESS ) {
			*pSample = { handle, candidate, ( uint16_t )( iMip - key.nMip ) };
			return SIR_GRAPHICS_SUCCESS;
		}
		candidate.nX >>= 1U;
		candidate.nY >>= 1U;
		candidate.nMip++;
	}
	return SIR_GRAPHICS_BUSY;
}
