// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "authoring/vt/build/terrain_3d_vt_page_builder.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static uint32_t t3d_vt_page_builder_wrap_coordinate( int64_t nCoordinate, uint32_t nExtent, t3dVTCookWrapMode_t eWrapMode )
{
	if ( nExtent == 0u ) {
		return 0u;
	}
	if ( eWrapMode == T3D_VT_COOK_WRAP_CLAMP ) {
		if ( nCoordinate < 0 ) {
			return 0u;
		}
		if ( nCoordinate >= ( int64_t )nExtent ) {
			return nExtent - 1u;
		}
		return ( uint32_t )nCoordinate;
	}
	int64_t nWrapped = nCoordinate % ( int64_t )nExtent;
	if ( nWrapped < 0 ) {
		nWrapped += nExtent;
	}
	return ( uint32_t )nWrapped;
}

static uint8_t t3d_vt_page_builder_neutral_component( uint32_t nChannel, uint32_t nComponent )
{
	switch ( nChannel ) {
	case T3D_VT_CHANNEL_ALBEDO: {
		static const uint8_t s_Albedo[4] = {
		    T3D_VT_NEUTRAL_ALBEDO_R8,
		    T3D_VT_NEUTRAL_ALBEDO_G8,
		    T3D_VT_NEUTRAL_ALBEDO_B8,
		    T3D_VT_NEUTRAL_ALBEDO_A8,
		};
		return s_Albedo[nComponent < 4u ? nComponent : 3u];
	}
	case T3D_VT_CHANNEL_NORMAL:
		return nComponent == 0u ? T3D_VT_NEUTRAL_NORMAL_X8 : T3D_VT_NEUTRAL_NORMAL_Y8;
	case T3D_VT_CHANNEL_SURFACE: {
		static const uint8_t s_Surface[4] = {
		    T3D_VT_NEUTRAL_SURFACE_AO8,
		    T3D_VT_NEUTRAL_SURFACE_ROUGHNESS8,
		    T3D_VT_NEUTRAL_SURFACE_METALLIC8,
		    T3D_VT_NEUTRAL_SURFACE_ALPHA8,
		};
		return s_Surface[nComponent < 4u ? nComponent : 3u];
	}
	case T3D_VT_CHANNEL_HEIGHT:
	default:
		return T3D_VT_NEUTRAL_HEIGHT8;
	}
}

uint32_t t3d_vt_page_builder_channel_components( const uint32_t nChannel )
{
	switch ( nChannel ) {
	case T3D_VT_CHANNEL_ALBEDO:
	case T3D_VT_CHANNEL_SURFACE:
		return 4u;
	case T3D_VT_CHANNEL_NORMAL:
		return 2u;
	case T3D_VT_CHANNEL_HEIGHT:
		return 1u;
	case T3D_VT_CHANNEL_HEIGHT16:
		return 2u;
	default:
		return 0u;
	}
}

uint32_t t3d_vt_page_builder_mip_count( uint32_t nWidth, uint32_t nHeight )
{
	if ( nWidth == 0u || nHeight == 0u ) {
		return 0u;
	}
	uint32_t nMipCount = 1u;
	while ( ( nWidth > 1u || nHeight > 1u ) && nMipCount < T3D_VT_MAX_MIPS ) {
		nWidth = nWidth > 1u ? ( nWidth >> 1u ) : 1u;
		nHeight = nHeight > 1u ? ( nHeight >> 1u ) : 1u;
		++nMipCount;
	}
	return nMipCount;
}

bool t3d_vt_page_builder_layout_mips( const uint32_t nWidth, const uint32_t nHeight, const uint32_t nComponents,
                                      t3dVTMipChainLayout_t& rLayout )
{
	memset( &rLayout, 0, sizeof( rLayout ) );
	if ( nWidth == 0u || nHeight == 0u || nComponents == 0u || nComponents > 4u ) {
		return false;
	}

	uint32_t nMipWidth = nWidth;
	uint32_t nMipHeight = nHeight;
	uint64_t nOffset = 0u;
	const uint32_t nMipCount = t3d_vt_page_builder_mip_count( nWidth, nHeight );
	for ( uint32_t iMip = 0u; iMip < nMipCount; ++iMip ) {
		const uint64_t nPixels = ( uint64_t )nMipWidth * ( uint64_t )nMipHeight;
		if ( nPixels > UINT64_MAX / nComponents ) {
			return false;
		}
		const uint64_t nBytes = nPixels * nComponents;
		if ( nOffset > UINT64_MAX - nBytes ) {
			return false;
		}
		rLayout.m_Levels[iMip].m_nWidth = nMipWidth;
		rLayout.m_Levels[iMip].m_nHeight = nMipHeight;
		rLayout.m_Levels[iMip].m_nOffset = nOffset;
		rLayout.m_Levels[iMip].m_nBytes = nBytes;
		nOffset += nBytes;
		nMipWidth = nMipWidth > 1u ? ( nMipWidth >> 1u ) : 1u;
		nMipHeight = nMipHeight > 1u ? ( nMipHeight >> 1u ) : 1u;
	}
	rLayout.m_nMipCount = nMipCount;
	rLayout.m_nComponents = nComponents;
	rLayout.m_nTotalBytes = nOffset;
	return true;
}

bool t3d_vt_page_builder_fill_neutral( const uint32_t nChannel, const uint32_t nWidth, const uint32_t nHeight, uint8_t *pDestination,
                                       const uint64_t nDestinationBytes )
{
	const uint32_t nComponents = t3d_vt_page_builder_channel_components( nChannel );
	const uint64_t nRequired = ( uint64_t )nWidth * ( uint64_t )nHeight * nComponents;
	if ( nComponents == 0u || nWidth == 0u || nHeight == 0u || pDestination == NULL || nDestinationBytes < nRequired ) {
		return false;
	}
	for ( uint64_t iPixel = 0u; iPixel < ( uint64_t )nWidth * nHeight; ++iPixel ) {
		for ( uint32_t iComponent = 0u; iComponent < nComponents; ++iComponent ) {
			pDestination[iPixel * nComponents + iComponent] = t3d_vt_page_builder_neutral_component( nChannel, iComponent );
		}
	}
	return true;
}

bool t3d_vt_page_builder_normalize_channel_u8( const uint32_t nChannel, const uint32_t nSurfaceSourceLayout,
                                               const t3dVTCookImageView_t& rSource, uint8_t *pDestination,
                                               const uint64_t nDestinationBytes )
{
	const uint32_t nTargetComponents = t3d_vt_page_builder_channel_components( nChannel );
	if ( rSource.m_pPixels == NULL || rSource.m_nWidth == 0u || rSource.m_nHeight == 0u || rSource.m_nComponents == 0u ||
	     rSource.m_nComponents > 4u || rSource.m_nRowBytes < rSource.m_nWidth * rSource.m_nComponents || pDestination == NULL ||
	     nTargetComponents == 0u ) {
		return false;
	}
	const uint64_t nRequired = ( uint64_t )rSource.m_nWidth * rSource.m_nHeight * nTargetComponents;
	if ( nDestinationBytes < nRequired ) {
		return false;
	}
	if ( nChannel == T3D_VT_CHANNEL_ALBEDO && rSource.m_nComponents < 3u ) {
		return false;
	}
	if ( nChannel == T3D_VT_CHANNEL_NORMAL && rSource.m_nComponents < 2u ) {
		return false;
	}
	if ( nChannel == T3D_VT_CHANNEL_SURFACE ) {
		if ( nSurfaceSourceLayout > T3D_VT_SURFACE_SOURCE_ROUGHNESS_A ) {
			return false;
		}
		if ( nSurfaceSourceLayout == T3D_VT_SURFACE_SOURCE_ORM && rSource.m_nComponents < 3u ) {
			return false;
		}
		if ( nSurfaceSourceLayout != T3D_VT_SURFACE_SOURCE_ORM ) {
			const uint32_t nRoughnessComponent = nSurfaceSourceLayout - T3D_VT_SURFACE_SOURCE_ROUGHNESS_R;
			if ( nRoughnessComponent >= rSource.m_nComponents ) {
				return false;
			}
		}
	}

	for ( uint32_t y = 0u; y < rSource.m_nHeight; ++y ) {
		const uint8_t *pSourceRow = rSource.m_pPixels + ( uint64_t )y * rSource.m_nRowBytes;
		uint8_t *pDestinationRow = pDestination + ( uint64_t )y * rSource.m_nWidth * nTargetComponents;
		for ( uint32_t x = 0u; x < rSource.m_nWidth; ++x ) {
			const uint8_t *pSource = pSourceRow + x * rSource.m_nComponents;
			uint8_t *pOut = pDestinationRow + x * nTargetComponents;
			switch ( nChannel ) {
			case T3D_VT_CHANNEL_ALBEDO:
				pOut[0] = pSource[0];
				pOut[1] = pSource[1];
				pOut[2] = pSource[2];
				pOut[3] = rSource.m_nComponents >= 4u ? pSource[3] : 255u;
				break;
			case T3D_VT_CHANNEL_NORMAL:
				pOut[0] = pSource[0];
				pOut[1] = pSource[1];
				break;
			case T3D_VT_CHANNEL_SURFACE:
				if ( nSurfaceSourceLayout == T3D_VT_SURFACE_SOURCE_ORM ) {
					pOut[0] = pSource[0];
					pOut[1] = pSource[1];
					pOut[2] = pSource[2];
					pOut[3] = rSource.m_nComponents >= 4u ? pSource[3] : T3D_VT_NEUTRAL_SURFACE_ALPHA8;
				} else {
					const uint32_t nRoughnessComponent = nSurfaceSourceLayout - T3D_VT_SURFACE_SOURCE_ROUGHNESS_R;
					pOut[0] = T3D_VT_NEUTRAL_SURFACE_AO8;
					pOut[1] = pSource[nRoughnessComponent];
					pOut[2] = T3D_VT_NEUTRAL_SURFACE_METALLIC8;
					pOut[3] = T3D_VT_NEUTRAL_SURFACE_ALPHA8;
				}
				break;
			case T3D_VT_CHANNEL_HEIGHT:
				pOut[0] = pSource[0];
				break;
			default:
				return false;
			}
		}
	}
	return true;
}

bool t3d_vt_page_builder_resample_u8( const t3dVTCookImageView_t& rSource, const uint32_t nTargetWidth, const uint32_t nTargetHeight,
                                      uint8_t *pDestination, const uint64_t nDestinationBytes )
{
	if ( rSource.m_pPixels == NULL || rSource.m_nWidth == 0u || rSource.m_nHeight == 0u || rSource.m_nComponents == 0u ||
	     rSource.m_nComponents > 4u || rSource.m_nRowBytes < rSource.m_nWidth * rSource.m_nComponents || nTargetWidth == 0u ||
	     nTargetHeight == 0u || pDestination == NULL ) {
		return false;
	}
	const uint64_t nRequired = ( uint64_t )nTargetWidth * ( uint64_t )nTargetHeight * rSource.m_nComponents;
	if ( nDestinationBytes < nRequired ) {
		return false;
	}

	for ( uint32_t y = 0u; y < nTargetHeight; ++y ) {
		const int64_t nSourceYFixed = ( int64_t )( ( ( ( uint64_t )y * 2u + 1u ) * rSource.m_nHeight * 32768u ) / nTargetHeight ) - 32768;
		int64_t nSourceY = nSourceYFixed >= 0 ? nSourceYFixed / 65536 : -1;
		uint32_t nWeightY = nSourceYFixed >= 0 ? ( uint32_t )( nSourceYFixed & 0xffff ) : ( uint32_t )( nSourceYFixed + 65536 );
		if ( nSourceY < 0 ) {
			nSourceY = 0;
			nWeightY = 0u;
		} else if ( nSourceY >= ( int64_t )rSource.m_nHeight - 1 ) {
			nSourceY = rSource.m_nHeight - 1u;
			nWeightY = 0u;
		}
		const uint32_t y0 = ( uint32_t )nSourceY;
		const uint32_t y1 = y0 + 1u < rSource.m_nHeight ? y0 + 1u : y0;
		for ( uint32_t x = 0u; x < nTargetWidth; ++x ) {
			const int64_t nSourceXFixed = ( int64_t )( ( ( ( uint64_t )x * 2u + 1u ) * rSource.m_nWidth * 32768u ) / nTargetWidth ) - 32768;
			int64_t nSourceX = nSourceXFixed >= 0 ? nSourceXFixed / 65536 : -1;
			uint32_t nWeightX = nSourceXFixed >= 0 ? ( uint32_t )( nSourceXFixed & 0xffff ) : ( uint32_t )( nSourceXFixed + 65536 );
			if ( nSourceX < 0 ) {
				nSourceX = 0;
				nWeightX = 0u;
			} else if ( nSourceX >= ( int64_t )rSource.m_nWidth - 1 ) {
				nSourceX = rSource.m_nWidth - 1u;
				nWeightX = 0u;
			}
			const uint32_t x0 = ( uint32_t )nSourceX;
			const uint32_t x1 = x0 + 1u < rSource.m_nWidth ? x0 + 1u : x0;
			const uint8_t *pRow0 = rSource.m_pPixels + ( uint64_t )y0 * rSource.m_nRowBytes;
			const uint8_t *pRow1 = rSource.m_pPixels + ( uint64_t )y1 * rSource.m_nRowBytes;
			uint8_t *pOut = pDestination + ( ( uint64_t )y * nTargetWidth + x ) * rSource.m_nComponents;
			for ( uint32_t iComponent = 0u; iComponent < rSource.m_nComponents; ++iComponent ) {
				const uint32_t nP00 = pRow0[x0 * rSource.m_nComponents + iComponent];
				const uint32_t nP10 = pRow0[x1 * rSource.m_nComponents + iComponent];
				const uint32_t nP01 = pRow1[x0 * rSource.m_nComponents + iComponent];
				const uint32_t nP11 = pRow1[x1 * rSource.m_nComponents + iComponent];
				const uint64_t nTop = ( uint64_t )nP00 * ( 65536u - nWeightX ) + ( uint64_t )nP10 * nWeightX;
				const uint64_t nBottom = ( uint64_t )nP01 * ( 65536u - nWeightX ) + ( uint64_t )nP11 * nWeightX;
				const uint64_t nValue = nTop * ( 65536u - nWeightY ) + nBottom * nWeightY;
				pOut[iComponent] = ( uint8_t )( ( nValue + ( 1ull << 31u ) ) >> 32u );
			}
		}
	}
	return true;
}

bool t3d_vt_page_builder_build_mips_u8( uint8_t *pImageBytes, const uint64_t nImageBytes, const t3dVTMipChainLayout_t& rLayout )
{
	if ( pImageBytes == NULL || rLayout.m_nMipCount == 0u || rLayout.m_nComponents == 0u || nImageBytes < rLayout.m_nTotalBytes ) {
		return false;
	}
	for ( uint32_t iMip = 1u; iMip < rLayout.m_nMipCount; ++iMip ) {
		const t3dVTMipLevelLayout_t& rSource = rLayout.m_Levels[iMip - 1u];
		const t3dVTMipLevelLayout_t& rDestination = rLayout.m_Levels[iMip];
		const uint8_t *pSource = pImageBytes + rSource.m_nOffset;
		uint8_t *pDestination = pImageBytes + rDestination.m_nOffset;
		for ( uint32_t y = 0u; y < rDestination.m_nHeight; ++y ) {
			const uint32_t y0 = y * 2u;
			const uint32_t y1 = y0 + 1u < rSource.m_nHeight ? y0 + 1u : y0;
			for ( uint32_t x = 0u; x < rDestination.m_nWidth; ++x ) {
				const uint32_t x0 = x * 2u;
				const uint32_t x1 = x0 + 1u < rSource.m_nWidth ? x0 + 1u : x0;
				for ( uint32_t iComponent = 0u; iComponent < rLayout.m_nComponents; ++iComponent ) {
					const uint64_t nI00 = ( ( uint64_t )y0 * rSource.m_nWidth + x0 ) * rLayout.m_nComponents + iComponent;
					const uint64_t nI10 = ( ( uint64_t )y0 * rSource.m_nWidth + x1 ) * rLayout.m_nComponents + iComponent;
					const uint64_t nI01 = ( ( uint64_t )y1 * rSource.m_nWidth + x0 ) * rLayout.m_nComponents + iComponent;
					const uint64_t nI11 = ( ( uint64_t )y1 * rSource.m_nWidth + x1 ) * rLayout.m_nComponents + iComponent;
					const uint32_t nSum = ( uint32_t )pSource[nI00] + pSource[nI10] + pSource[nI01] + pSource[nI11];
					pDestination[( ( uint64_t )y * rDestination.m_nWidth + x ) * rLayout.m_nComponents + iComponent] =
					    ( uint8_t )( ( nSum + 2u ) >> 2u );
				}
			}
		}
	}
	return true;
}

static uint8_t t3d_vt_page_builder_encode_normal_component( const float fValue )
{
	const float fClamped = fValue < -1.0f ? -1.0f : ( fValue > 1.0f ? 1.0f : fValue );
	const float fEncoded = ( fClamped * 0.5f + 0.5f ) * 255.0f;
	return ( uint8_t )( fEncoded + 0.5f );
}

bool t3d_vt_page_builder_build_channel_mips_u8( const uint32_t nChannel, const uint32_t nQuality, uint8_t *pImageBytes,
                                                const uint64_t nImageBytes, const t3dVTMipChainLayout_t& rLayout )
{
	if ( nChannel != T3D_VT_CHANNEL_NORMAL || nQuality == 0u ) {
		return t3d_vt_page_builder_build_mips_u8( pImageBytes, nImageBytes, rLayout );
	}
	if ( pImageBytes == NULL || rLayout.m_nMipCount == 0u || rLayout.m_nComponents != 2u || nImageBytes < rLayout.m_nTotalBytes ) {
		return false;
	}
	for ( uint32_t iMip = 1u; iMip < rLayout.m_nMipCount; ++iMip ) {
		const t3dVTMipLevelLayout_t& rSource = rLayout.m_Levels[iMip - 1u];
		const t3dVTMipLevelLayout_t& rDestination = rLayout.m_Levels[iMip];
		const uint8_t *pSource = pImageBytes + rSource.m_nOffset;
		uint8_t *pDestination = pImageBytes + rDestination.m_nOffset;
		for ( uint32_t y = 0u; y < rDestination.m_nHeight; ++y ) {
			const uint32_t y0 = y * 2u;
			const uint32_t y1 = y0 + 1u < rSource.m_nHeight ? y0 + 1u : y0;
			for ( uint32_t x = 0u; x < rDestination.m_nWidth; ++x ) {
				const uint32_t x0 = x * 2u;
				const uint32_t x1 = x0 + 1u < rSource.m_nWidth ? x0 + 1u : x0;
				const uint32_t nCoordinates[8] = { x0, y0, x1, y0, x0, y1, x1, y1 };
				float fNormalX = 0.0f;
				float fNormalY = 0.0f;
				float fNormalZ = 0.0f;
				for ( uint32_t iSample = 0u; iSample < 4u; ++iSample ) {
					const uint32_t nSampleX = nCoordinates[iSample * 2u];
					const uint32_t nSampleY = nCoordinates[iSample * 2u + 1u];
					const uint64_t nIndex = ( ( uint64_t )nSampleY * rSource.m_nWidth + nSampleX ) * 2u;
					const float fX = ( ( float )pSource[nIndex] / 255.0f ) * 2.0f - 1.0f;
					const float fY = ( ( float )pSource[nIndex + 1u] / 255.0f ) * 2.0f - 1.0f;
					const float fZSquared = 1.0f - fX * fX - fY * fY;
					fNormalX += fX;
					fNormalY += fY;
					fNormalZ += fZSquared > 0.0f ? sqrtf( fZSquared ) : 0.0f;
				}
				const float fLengthSquared = fNormalX * fNormalX + fNormalY * fNormalY + fNormalZ * fNormalZ;
				if ( fLengthSquared > 0.0000001f ) {
					const float fInverseLength = 1.0f / sqrtf( fLengthSquared );
					fNormalX *= fInverseLength;
					fNormalY *= fInverseLength;
				} else {
					fNormalX = 0.0f;
					fNormalY = 0.0f;
				}
				const uint64_t nDestinationIndex = ( ( uint64_t )y * rDestination.m_nWidth + x ) * 2u;
				pDestination[nDestinationIndex] = t3d_vt_page_builder_encode_normal_component( fNormalX );
				pDestination[nDestinationIndex + 1u] = t3d_vt_page_builder_encode_normal_component( fNormalY );
			}
		}
	}
	return true;
}

static bool t3d_vt_page_builder_validate_logical_source( const t3dVTLogicalChannel_t Channel, const uint32_t nOrmSourceLayout,
                                                         const t3dVTCookImageView_t& rSource )
{
	if ( ( uint32_t )Channel >= T3D_VT_LOGICAL_CHANNEL_COUNT || rSource.m_pPixels == NULL || rSource.m_nWidth == 0u ||
	     rSource.m_nHeight == 0u || rSource.m_nComponents == 0u || rSource.m_nComponents > 4u ||
	     ( uint64_t )rSource.m_nWidth * rSource.m_nComponents > rSource.m_nRowBytes ) {
		return false;
	}
	switch ( Channel ) {
	case T3D_VT_LOGICAL_CHANNEL_ALBEDO:
		return rSource.m_nComponents >= 3u;
	case T3D_VT_LOGICAL_CHANNEL_PBR:
		return rSource.m_nComponents >= 4u;
	case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
		return true;
	case T3D_VT_LOGICAL_CHANNEL_NORMAL:
		return rSource.m_nComponents >= 2u;
	case T3D_VT_LOGICAL_CHANNEL_ORM:
		if ( nOrmSourceLayout > T3D_VT_SURFACE_SOURCE_ROUGHNESS_A ) {
			return false;
		}
		if ( nOrmSourceLayout == T3D_VT_SURFACE_SOURCE_ORM ) {
			return rSource.m_nComponents >= 3u;
		}
		return nOrmSourceLayout - T3D_VT_SURFACE_SOURCE_ROUGHNESS_R < rSource.m_nComponents;
	default:
		return false;
	}
}

uint32_t t3d_vt_page_builder_logical_channel_components( const t3dVTLogicalChannel_t Channel )
{
	switch ( Channel ) {
	case T3D_VT_LOGICAL_CHANNEL_ALBEDO:
	case T3D_VT_LOGICAL_CHANNEL_PBR:
	case T3D_VT_LOGICAL_CHANNEL_ORM:
		return 4u;
	case T3D_VT_LOGICAL_CHANNEL_NORMAL:
		return 2u;
	case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
		return 1u;
	default:
		return 0u;
	}
}

bool t3d_vt_page_builder_fill_logical_neutral( const t3dVTLogicalChannel_t Channel, const uint32_t nWidth, const uint32_t nHeight,
                                               uint8_t *pDestination, const uint64_t nDestinationBytes )
{
	const uint32_t nComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	const uint64_t nPixelCount = ( uint64_t )nWidth * nHeight;
	const uint64_t nRequired = nPixelCount * nComponents;
	if ( nComponents == 0u || nWidth == 0u || nHeight == 0u || pDestination == NULL || nDestinationBytes < nRequired ) {
		return false;
	}
	uint8_t Sample[4] = { 255u, 255u, 255u, 255u };
	switch ( Channel ) {
	case T3D_VT_LOGICAL_CHANNEL_ALBEDO:
		break;
	case T3D_VT_LOGICAL_CHANNEL_PBR:
		Sample[0] = 128u;
		Sample[1] = 128u;
		Sample[2] = 255u;
		Sample[3] = 255u;
		break;
	case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
		Sample[0] = T3D_VT_NEUTRAL_HEIGHT8;
		break;
	case T3D_VT_LOGICAL_CHANNEL_NORMAL:
		Sample[0] = T3D_VT_NEUTRAL_NORMAL_X8;
		Sample[1] = T3D_VT_NEUTRAL_NORMAL_Y8;
		break;
	case T3D_VT_LOGICAL_CHANNEL_ORM:
		Sample[0] = 255u;
		Sample[1] = 255u;
		Sample[2] = 0u;
		Sample[3] = 255u;
		break;
	default:
		return false;
	}
	for ( uint64_t iPixel = 0u; iPixel < nPixelCount; ++iPixel ) {
		memcpy( pDestination + iPixel * nComponents, Sample, nComponents );
	}
	return true;
}

static void t3d_vt_page_builder_write_orm_sample( const uint8_t *pSource, const uint32_t nComponents, const uint32_t nOrmSourceLayout,
                                                  uint8_t *pDestination )
{
	if ( nOrmSourceLayout == T3D_VT_SURFACE_SOURCE_ORM ) {
		pDestination[0] = pSource[0];
		pDestination[1] = pSource[1];
		pDestination[2] = pSource[2];
	} else {
		const uint32_t nRoughnessComponent = nOrmSourceLayout - T3D_VT_SURFACE_SOURCE_ROUGHNESS_R;
		pDestination[0] = T3D_VT_NEUTRAL_SURFACE_AO8;
		pDestination[1] = pSource[nRoughnessComponent < nComponents ? nRoughnessComponent : 0u];
		pDestination[2] = T3D_VT_NEUTRAL_SURFACE_METALLIC8;
	}
	pDestination[3] = 255u;
}

// Extend only the RGB of fully transparent pixels from nearby visible texels.
// Alpha stays authored; this prevents bilinear filtering from adding dark fringes.
static void t3d_vt_page_builder_bleed_albedo_rgb( uint8_t *pPixels, const uint32_t nWidth, const uint32_t nHeight )
{
	for ( uint32_t y = 0u; y < nHeight; ++y ) {
		for ( uint32_t x = 0u; x < nWidth; ++x ) {
			uint8_t *out = pPixels + ( ( uint64_t )y * nWidth + x ) * 4u;
			if ( out[3] != 0u ) {
				continue;
			}
			const uint8_t *nearest = NULL;
			int32_t best_distance = 9;
			for ( int32_t dy = -2; dy <= 2; ++dy ) {
				for ( int32_t dx = -2; dx <= 2; ++dx ) {
					const int32_t sx = ( int32_t )x + dx, sy = ( int32_t )y + dy;
					const int32_t distance = dx * dx + dy * dy;
					if ( sx < 0 || sy < 0 || sx >= ( int32_t )nWidth || sy >= ( int32_t )nHeight || distance >= best_distance ) {
						continue;
					}
					const uint8_t *pixel = pPixels + ( ( uint64_t )sy * nWidth + sx ) * 4u;
					if ( pixel[3] != 0u ) {
						nearest = pixel;
						best_distance = distance;
					}
				}
			}
			if ( nearest != NULL ) {
				memcpy( out, nearest, 3u );
			}
		}
	}
}

bool t3d_vt_page_builder_normalize_logical_channel_u8( const t3dVTLogicalChannel_t Channel, const uint32_t nOrmSourceLayout,
                                                       const t3dVTCookImageView_t& rSource, uint8_t *pDestination,
                                                       const uint64_t nDestinationBytes )
{
	const uint32_t nTargetComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	const uint64_t nRequired = ( uint64_t )rSource.m_nWidth * rSource.m_nHeight * nTargetComponents;
	if ( nTargetComponents == 0u || pDestination == NULL || nDestinationBytes < nRequired ||
	     !t3d_vt_page_builder_validate_logical_source( Channel, nOrmSourceLayout, rSource ) ) {
		return false;
	}
	for ( uint32_t y = 0u; y < rSource.m_nHeight; ++y ) {
		const uint8_t *pSourceRow = rSource.m_pPixels + ( uint64_t )y * rSource.m_nRowBytes;
		uint8_t *pDestinationRow = pDestination + ( uint64_t )y * rSource.m_nWidth * nTargetComponents;
		for ( uint32_t x = 0u; x < rSource.m_nWidth; ++x ) {
			const uint8_t *pSource = pSourceRow + x * rSource.m_nComponents;
			uint8_t *pOut = pDestinationRow + x * nTargetComponents;
			switch ( Channel ) {
			case T3D_VT_LOGICAL_CHANNEL_ALBEDO:
				pOut[0] = pSource[0];
				pOut[1] = pSource[1];
				pOut[2] = pSource[2];
				pOut[3] = rSource.m_nComponents >= 4u ? pSource[3] : 255u;
				break;
			case T3D_VT_LOGICAL_CHANNEL_PBR:
				memcpy( pOut, pSource, 4u );
				break;
			case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
				pOut[0] = pSource[0];
				break;
			case T3D_VT_LOGICAL_CHANNEL_NORMAL:
				pOut[0] = pSource[0];
				pOut[1] = pSource[1];
				break;
			case T3D_VT_LOGICAL_CHANNEL_ORM:
				t3d_vt_page_builder_write_orm_sample( pSource, rSource.m_nComponents, nOrmSourceLayout, pOut );
				break;
			default:
				return false;
			}
		}
	}
	if ( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO ) {
		t3d_vt_page_builder_bleed_albedo_rgb( pDestination, rSource.m_nWidth, rSource.m_nHeight );
	}
	return true;
}

static float t3d_vt_page_builder_srgb_to_linear( const uint8_t nValue )
{
	const float fSrgb = ( float )nValue / 255.0f;
	return fSrgb <= 0.04045f ? fSrgb / 12.92f : powf( ( fSrgb + 0.055f ) / 1.055f, 2.4f );
}

static uint8_t t3d_vt_page_builder_linear_to_srgb( float fValue )
{
	fValue = fValue < 0.0f ? 0.0f : ( fValue > 1.0f ? 1.0f : fValue );
	const float fSrgb = fValue <= 0.0031308f ? fValue * 12.92f : 1.055f * powf( fValue, 1.0f / 2.4f ) - 0.055f;
	const float fEncoded = fSrgb * 255.0f;
	return ( uint8_t )( fEncoded + 0.5f );
}

static void t3d_vt_page_builder_resample_axis( const uint32_t nDestinationCoordinate, const uint32_t nDestinationExtent,
                                               const uint32_t nSourceExtent, uint32_t& rSource0, uint32_t& rSource1, uint32_t& rWeight )
{
	const int64_t nSourceFixed =
	    ( int64_t )( ( ( ( uint64_t )nDestinationCoordinate * 2u + 1u ) * nSourceExtent * 32768u ) / nDestinationExtent ) - 32768;
	int64_t nSource = nSourceFixed >= 0 ? nSourceFixed / 65536 : -1;
	uint32_t nWeight = nSourceFixed >= 0 ? ( uint32_t )( nSourceFixed & 0xffff ) : ( uint32_t )( nSourceFixed + 65536 );
	if ( nSource < 0 ) {
		nSource = 0;
		nWeight = 0u;
	} else if ( nSource >= ( int64_t )nSourceExtent - 1 ) {
		nSource = nSourceExtent - 1u;
		nWeight = 0u;
	}
	rSource0 = ( uint32_t )nSource;
	rSource1 = rSource0 + 1u < nSourceExtent ? rSource0 + 1u : rSource0;
	rWeight = nWeight;
}

static uint8_t t3d_vt_page_builder_bilerp_u8( const uint8_t nP00, const uint8_t nP10, const uint8_t nP01, const uint8_t nP11,
                                              const uint32_t nWeightX, const uint32_t nWeightY )
{
	const uint64_t nTop = ( uint64_t )nP00 * ( 65536u - nWeightX ) + ( uint64_t )nP10 * nWeightX;
	const uint64_t nBottom = ( uint64_t )nP01 * ( 65536u - nWeightX ) + ( uint64_t )nP11 * nWeightX;
	const uint64_t nValue = nTop * ( 65536u - nWeightY ) + nBottom * nWeightY;
	return ( uint8_t )( ( nValue + ( 1ull << 31u ) ) >> 32u );
}

static float t3d_vt_page_builder_bilerp_float( const float fP00, const float fP10, const float fP01, const float fP11,
                                               const uint32_t nWeightX, const uint32_t nWeightY )
{
	const float fWeightX = ( float )nWeightX / 65536.0f;
	const float fWeightY = ( float )nWeightY / 65536.0f;
	const float fTop = fP00 + ( fP10 - fP00 ) * fWeightX;
	const float fBottom = fP01 + ( fP11 - fP01 ) * fWeightX;
	return fTop + ( fBottom - fTop ) * fWeightY;
}

static void t3d_vt_page_builder_decode_normal_xy( const uint8_t *pSource, float& rX, float& rY, float& rZ )
{
	rX = ( ( float )pSource[0] / 255.0f ) * 2.0f - 1.0f;
	rY = ( ( float )pSource[1] / 255.0f ) * 2.0f - 1.0f;
	const float fZSquared = 1.0f - rX * rX - rY * rY;
	rZ = fZSquared > 0.0f ? sqrtf( fZSquared ) : 0.0f;
}

static void t3d_vt_page_builder_bilerp_normal( const uint8_t *p00, const uint8_t *p10, const uint8_t *p01, const uint8_t *p11,
                                               const uint32_t nWeightX, const uint32_t nWeightY, uint8_t *pDestination )
{
	float x00, y00, z00, x10, y10, z10, x01, y01, z01, x11, y11, z11;
	t3d_vt_page_builder_decode_normal_xy( p00, x00, y00, z00 );
	t3d_vt_page_builder_decode_normal_xy( p10, x10, y10, z10 );
	t3d_vt_page_builder_decode_normal_xy( p01, x01, y01, z01 );
	t3d_vt_page_builder_decode_normal_xy( p11, x11, y11, z11 );
	float fX = t3d_vt_page_builder_bilerp_float( x00, x10, x01, x11, nWeightX, nWeightY );
	float fY = t3d_vt_page_builder_bilerp_float( y00, y10, y01, y11, nWeightX, nWeightY );
	float fZ = t3d_vt_page_builder_bilerp_float( z00, z10, z01, z11, nWeightX, nWeightY );
	const float fLengthSquared = fX * fX + fY * fY + fZ * fZ;
	if ( fLengthSquared > 0.0000001f ) {
		const float fInverseLength = 1.0f / sqrtf( fLengthSquared );
		fX *= fInverseLength;
		fY *= fInverseLength;
	} else {
		fX = 0.0f;
		fY = 0.0f;
	}
	pDestination[0] = t3d_vt_page_builder_encode_normal_component( fX );
	pDestination[1] = t3d_vt_page_builder_encode_normal_component( fY );
}

static uint64_t t3d_vt_page_builder_alpha_coverage_source( const t3dVTCookImageView_t& rSource, const uint8_t nThreshold )
{
	if ( rSource.m_nComponents < 4u ) {
		return ( uint64_t )rSource.m_nWidth * rSource.m_nHeight;
	}
	uint64_t nCovered = 0u;
	for ( uint32_t y = 0u; y < rSource.m_nHeight; ++y ) {
		const uint8_t *pRow = rSource.m_pPixels + ( uint64_t )y * rSource.m_nRowBytes;
		for ( uint32_t x = 0u; x < rSource.m_nWidth; ++x ) {
			nCovered += pRow[x * rSource.m_nComponents + 3u] >= nThreshold ? 1u : 0u;
		}
	}
	return nCovered;
}

static uint64_t t3d_vt_page_builder_alpha_coverage_scaled( const uint8_t *pPixels, const uint64_t nPixelCount, const uint8_t nThreshold,
                                                           const float fScale )
{
	uint64_t nCovered = 0u;
	for ( uint64_t iPixel = 0u; iPixel < nPixelCount; ++iPixel ) {
		float fAlpha = ( float )pPixels[iPixel * 4u + 3u] * fScale;
		fAlpha = fAlpha > 255.0f ? 255.0f : fAlpha;
		nCovered += ( uint8_t )( fAlpha + 0.5f ) >= nThreshold ? 1u : 0u;
	}
	return nCovered;
}

static void t3d_vt_page_builder_preserve_alpha_coverage( const t3dVTCookImageView_t& rSource, const uint8_t nThreshold,
                                                         uint8_t *pDestination, const uint32_t nTargetWidth, const uint32_t nTargetHeight )
{
	if ( nThreshold == 0u || pDestination == NULL || rSource.m_nWidth == 0u || rSource.m_nHeight == 0u || nTargetWidth == 0u ||
	     nTargetHeight == 0u ) {
		return;
	}
	const uint64_t nSourcePixels = ( uint64_t )rSource.m_nWidth * rSource.m_nHeight;
	const uint64_t nTargetPixels = ( uint64_t )nTargetWidth * nTargetHeight;
	const uint64_t nSourceCovered = t3d_vt_page_builder_alpha_coverage_source( rSource, nThreshold );
	const uint64_t nDesiredCovered = ( nSourceCovered * nTargetPixels + nSourcePixels / 2u ) / nSourcePixels;
	if ( nDesiredCovered == 0u ) {
		return;
	}
	float fLow = 0.0f;
	float fHigh = 1.0f;
	while ( fHigh < 16.0f &&
	        t3d_vt_page_builder_alpha_coverage_scaled( pDestination, nTargetPixels, nThreshold, fHigh ) < nDesiredCovered ) {
		fHigh *= 2.0f;
	}
	for ( uint32_t iIteration = 0u; iIteration < 16u; ++iIteration ) {
		const float fMid = ( fLow + fHigh ) * 0.5f;
		if ( t3d_vt_page_builder_alpha_coverage_scaled( pDestination, nTargetPixels, nThreshold, fMid ) >= nDesiredCovered ) {
			fHigh = fMid;
		} else {
			fLow = fMid;
		}
	}
	for ( uint64_t iPixel = 0u; iPixel < nTargetPixels; ++iPixel ) {
		float fAlpha = ( float )pDestination[iPixel * 4u + 3u] * fHigh;
		fAlpha = fAlpha > 255.0f ? 255.0f : fAlpha;
		pDestination[iPixel * 4u + 3u] = ( uint8_t )( fAlpha + 0.5f );
	}
}

static void t3d_vt_page_builder_read_orm_rgb( const uint8_t *pSource, const uint32_t nComponents, const uint32_t nOrmSourceLayout,
                                              uint8_t& rAo, uint8_t& rRoughness, uint8_t& rMetallic )
{
	if ( nOrmSourceLayout == T3D_VT_SURFACE_SOURCE_ORM ) {
		rAo = pSource[0];
		rRoughness = pSource[1];
		rMetallic = pSource[2];
		return;
	}
	const uint32_t nRoughnessComponent = nOrmSourceLayout - T3D_VT_SURFACE_SOURCE_ROUGHNESS_R;
	rAo = T3D_VT_NEUTRAL_SURFACE_AO8;
	rRoughness = pSource[nRoughnessComponent < nComponents ? nRoughnessComponent : 0u];
	rMetallic = T3D_VT_NEUTRAL_SURFACE_METALLIC8;
}

bool t3d_vt_page_builder_resample_logical_channel_u8( const t3dVTLogicalChannel_t Channel, const uint32_t nOrmSourceLayout,
                                                      const bool bAlphaScissor, const uint8_t nAlphaThreshold,
                                                      const t3dVTCookImageView_t& rSource, const uint32_t nTargetWidth,
                                                      const uint32_t nTargetHeight, uint8_t *pDestination,
                                                      const uint64_t nDestinationBytes )
{
	const uint32_t nTargetComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	const uint64_t nRequired = ( uint64_t )nTargetWidth * nTargetHeight * nTargetComponents;
	if ( nTargetComponents == 0u || nTargetWidth == 0u || nTargetHeight == 0u || pDestination == NULL || nDestinationBytes < nRequired ||
	     !t3d_vt_page_builder_validate_logical_source( Channel, nOrmSourceLayout, rSource ) ) {
		return false;
	}
	if ( Channel == T3D_VT_LOGICAL_CHANNEL_HEIGHT ) {
		for ( uint32_t y = 0u; y < nTargetHeight; ++y ) {
			const uint32_t sy = ( uint64_t( y ) * rSource.m_nHeight ) / nTargetHeight;
			for ( uint32_t x = 0u; x < nTargetWidth; ++x ) {
				const uint32_t sx = ( uint64_t( x ) * rSource.m_nWidth ) / nTargetWidth;
				const uint8_t *src = rSource.m_pPixels + uint64_t( sy ) * rSource.m_nRowBytes + sx * rSource.m_nComponents;
				uint8_t *dst = pDestination + ( uint64_t( y ) * nTargetWidth + x ) * nTargetComponents;
				memcpy( dst, src, nTargetComponents );
			}
		}
		return true;
	}
	if ( rSource.m_nWidth == nTargetWidth && rSource.m_nHeight == nTargetHeight ) {
		return t3d_vt_page_builder_normalize_logical_channel_u8( Channel, nOrmSourceLayout, rSource, pDestination, nDestinationBytes );
	}

	for ( uint32_t y = 0u; y < nTargetHeight; ++y ) {
		uint32_t y0, y1, nWeightY;
		t3d_vt_page_builder_resample_axis( y, nTargetHeight, rSource.m_nHeight, y0, y1, nWeightY );
		const uint8_t *pRow0 = rSource.m_pPixels + ( uint64_t )y0 * rSource.m_nRowBytes;
		const uint8_t *pRow1 = rSource.m_pPixels + ( uint64_t )y1 * rSource.m_nRowBytes;
		for ( uint32_t x = 0u; x < nTargetWidth; ++x ) {
			uint32_t x0, x1, nWeightX;
			t3d_vt_page_builder_resample_axis( x, nTargetWidth, rSource.m_nWidth, x0, x1, nWeightX );
			const uint8_t *p00 = pRow0 + x0 * rSource.m_nComponents;
			const uint8_t *p10 = pRow0 + x1 * rSource.m_nComponents;
			const uint8_t *p01 = pRow1 + x0 * rSource.m_nComponents;
			const uint8_t *p11 = pRow1 + x1 * rSource.m_nComponents;
			uint8_t *pOut = pDestination + ( ( uint64_t )y * nTargetWidth + x ) * nTargetComponents;
			switch ( Channel ) {
			case T3D_VT_LOGICAL_CHANNEL_ALBEDO: {
				const float a00 = rSource.m_nComponents >= 4u ? p00[3] / 255.0f : 1.0f;
				const float a10 = rSource.m_nComponents >= 4u ? p10[3] / 255.0f : 1.0f;
				const float a01 = rSource.m_nComponents >= 4u ? p01[3] / 255.0f : 1.0f;
				const float a11 = rSource.m_nComponents >= 4u ? p11[3] / 255.0f : 1.0f;
				const float alpha = t3d_vt_page_builder_bilerp_float( a00, a10, a01, a11, nWeightX, nWeightY );
				for ( uint32_t iComponent = 0u; iComponent < 3u; ++iComponent ) {
					const float fValue =
					    t3d_vt_page_builder_bilerp_float( t3d_vt_page_builder_srgb_to_linear( p00[iComponent] ) * a00,
						                                  t3d_vt_page_builder_srgb_to_linear( p10[iComponent] ) * a10,
						                                  t3d_vt_page_builder_srgb_to_linear( p01[iComponent] ) * a01,
						                                  t3d_vt_page_builder_srgb_to_linear( p11[iComponent] ) * a11, nWeightX, nWeightY );
					pOut[iComponent] = t3d_vt_page_builder_linear_to_srgb( alpha > 0.00001f ? fValue / alpha : 0.0f );
				}
				pOut[3] = t3d_vt_page_builder_bilerp_u8(
				    rSource.m_nComponents >= 4u ? p00[3] : 255u, rSource.m_nComponents >= 4u ? p10[3] : 255u,
				    rSource.m_nComponents >= 4u ? p01[3] : 255u, rSource.m_nComponents >= 4u ? p11[3] : 255u, nWeightX, nWeightY );
				break;
			}
			case T3D_VT_LOGICAL_CHANNEL_PBR:
				t3d_vt_page_builder_bilerp_normal( p00, p10, p01, p11, nWeightX, nWeightY, pOut );
				pOut[2] = t3d_vt_page_builder_bilerp_u8( p00[2], p10[2], p01[2], p11[2], nWeightX, nWeightY );
				pOut[3] = t3d_vt_page_builder_bilerp_u8( p00[3], p10[3], p01[3], p11[3], nWeightX, nWeightY );
				break;
			case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
				pOut[0] = t3d_vt_page_builder_bilerp_u8( p00[0], p10[0], p01[0], p11[0], nWeightX, nWeightY );
				break;
			case T3D_VT_LOGICAL_CHANNEL_NORMAL:
				t3d_vt_page_builder_bilerp_normal( p00, p10, p01, p11, nWeightX, nWeightY, pOut );
				break;
			case T3D_VT_LOGICAL_CHANNEL_ORM: {
				uint8_t ao00, rough00, metal00, ao10, rough10, metal10, ao01, rough01, metal01, ao11, rough11, metal11;
				t3d_vt_page_builder_read_orm_rgb( p00, rSource.m_nComponents, nOrmSourceLayout, ao00, rough00, metal00 );
				t3d_vt_page_builder_read_orm_rgb( p10, rSource.m_nComponents, nOrmSourceLayout, ao10, rough10, metal10 );
				t3d_vt_page_builder_read_orm_rgb( p01, rSource.m_nComponents, nOrmSourceLayout, ao01, rough01, metal01 );
				t3d_vt_page_builder_read_orm_rgb( p11, rSource.m_nComponents, nOrmSourceLayout, ao11, rough11, metal11 );
				pOut[0] = t3d_vt_page_builder_bilerp_u8( ao00, ao10, ao01, ao11, nWeightX, nWeightY );
				pOut[1] = t3d_vt_page_builder_bilerp_u8( rough00, rough10, rough01, rough11, nWeightX, nWeightY );
				pOut[2] = t3d_vt_page_builder_bilerp_u8( metal00, metal10, metal01, metal11, nWeightX, nWeightY );
				pOut[3] = 255u;
				break;
			}
			default:
				return false;
			}
		}
	}
	if ( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && bAlphaScissor ) {
		t3d_vt_page_builder_preserve_alpha_coverage( rSource, nAlphaThreshold, pDestination, nTargetWidth, nTargetHeight );
	}
	if ( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO ) {
		t3d_vt_page_builder_bleed_albedo_rgb( pDestination, nTargetWidth, nTargetHeight );
	}
	return true;
}

// Higher authoring qualities use a wider, symmetric area kernel on albedo.
// Work in premultiplied linear light, matching the normal bilinear path.
static void t3d_vt_page_builder_filter_albedo_high_quality( const t3dVTCookImageView_t& rSource, uint8_t *pDestination,
                                                            const uint32_t nTargetWidth, const uint32_t nTargetHeight,
                                                            const uint32_t nMipQuality )
{
	const float weights[4] = { 1.0f, nMipQuality == 2u ? 1.0f : 3.0f, nMipQuality == 2u ? 1.0f : 3.0f, 1.0f };
	for ( uint32_t y = 0u; y < nTargetHeight; ++y ) {
		const int32_t cy = ( int32_t )( ( ( uint64_t )y * 2u + 1u ) * rSource.m_nHeight / ( nTargetHeight * 2u ) ) - 1;
		for ( uint32_t x = 0u; x < nTargetWidth; ++x ) {
			const int32_t cx = ( int32_t )( ( ( uint64_t )x * 2u + 1u ) * rSource.m_nWidth / ( nTargetWidth * 2u ) ) - 1;
			float sum[3] = { 0.0f, 0.0f, 0.0f };
			float alpha = 0.0f, total = 0.0f;
			for ( uint32_t iy = 0u; iy < 4u; ++iy ) {
				const uint32_t sy = ( uint32_t )CLAMP( cy + ( int32_t )iy - 1, 0, ( int32_t )rSource.m_nHeight - 1 );
				for ( uint32_t ix = 0u; ix < 4u; ++ix ) {
					const uint32_t sx = ( uint32_t )CLAMP( cx + ( int32_t )ix - 1, 0, ( int32_t )rSource.m_nWidth - 1 );
					const uint8_t *p = rSource.m_pPixels + ( uint64_t )sy * rSource.m_nRowBytes + sx * 4u;
					const float weight = weights[iy] * weights[ix];
					const float a = ( float )p[3] / 255.0f;
					for ( uint32_t c = 0u; c < 3u; ++c ) {
						sum[c] += weight * a * t3d_vt_page_builder_srgb_to_linear( p[c] );
					}
					alpha += weight * a;
					total += weight;
				}
			}
			uint8_t *out = pDestination + ( ( uint64_t )y * nTargetWidth + x ) * 4u;
			for ( uint32_t c = 0u; c < 3u; ++c ) {
				out[c] = t3d_vt_page_builder_linear_to_srgb( alpha > 0.00001f ? sum[c] / alpha : 0.0f );
			}
			out[3] = ( uint8_t )CLAMP( ( int32_t )( 255.0f * alpha / total + 0.5f ), 0, 255 );
		}
	}
}

bool t3d_vt_page_builder_build_logical_channel_mips_u8( const t3dVTLogicalChannel_t Channel, const bool bAlphaScissor,
                                                        const uint8_t nAlphaThreshold, uint8_t *pImageBytes, const uint64_t nImageBytes,
                                                        const t3dVTMipChainLayout_t& rLayout, const uint32_t nMipQuality )
{
	const uint32_t nComponents = t3d_vt_page_builder_logical_channel_components( Channel );
	if ( pImageBytes == NULL || rLayout.m_nMipCount == 0u || rLayout.m_nMipCount > T3D_VT_MAX_MIPS ||
	     rLayout.m_nComponents != nComponents || nComponents == 0u || nImageBytes < rLayout.m_nTotalBytes ) {
		return false;
	}
	for ( uint32_t iMip = 1u; iMip < rLayout.m_nMipCount; ++iMip ) {
		const t3dVTMipLevelLayout_t& rSourceLayout = rLayout.m_Levels[iMip - 1u];
		const t3dVTMipLevelLayout_t& rDestinationLayout = rLayout.m_Levels[iMip];
		if ( rSourceLayout.m_nWidth == 0u || rSourceLayout.m_nHeight == 0u || rDestinationLayout.m_nWidth == 0u ||
		     rDestinationLayout.m_nHeight == 0u || rSourceLayout.m_nOffset > nImageBytes ||
		     rSourceLayout.m_nBytes > nImageBytes - rSourceLayout.m_nOffset || rDestinationLayout.m_nOffset > nImageBytes ||
		     rDestinationLayout.m_nBytes > nImageBytes - rDestinationLayout.m_nOffset ) {
			return false;
		}
		t3dVTCookImageView_t Source;
		Source.m_pPixels = pImageBytes + rSourceLayout.m_nOffset;
		Source.m_nWidth = rSourceLayout.m_nWidth;
		Source.m_nHeight = rSourceLayout.m_nHeight;
		Source.m_nComponents = nComponents;
		Source.m_nRowBytes = rSourceLayout.m_nWidth * nComponents;
		if ( nMipQuality == 0u ) {
			// Fast point sampled mips. All channels are already normalized at mip zero.
			const uint8_t *pSrc = Source.m_pPixels;
			uint8_t *pDst = pImageBytes + rDestinationLayout.m_nOffset;
			for ( uint32_t y = 0u; y < rDestinationLayout.m_nHeight; ++y ) {
				const uint32_t sy = MIN( Source.m_nHeight - 1u, ( uint32_t )( ( ( uint64_t )y * 2u + 1u ) * Source.m_nHeight /
				                                                              ( rDestinationLayout.m_nHeight * 2u ) ) );
				for ( uint32_t x = 0u; x < rDestinationLayout.m_nWidth; ++x ) {
					const uint32_t sx = MIN( Source.m_nWidth - 1u, ( uint32_t )( ( ( uint64_t )x * 2u + 1u ) * Source.m_nWidth /
					                                                             ( rDestinationLayout.m_nWidth * 2u ) ) );
					memcpy( pDst + ( ( uint64_t )y * rDestinationLayout.m_nWidth + x ) * nComponents,
					        pSrc + ( ( uint64_t )sy * Source.m_nWidth + sx ) * nComponents, nComponents );
				}
			}
			continue;
		}
		if ( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && nMipQuality >= 2u ) {
			uint8_t *out = pImageBytes + rDestinationLayout.m_nOffset;
			t3d_vt_page_builder_filter_albedo_high_quality( Source, out, rDestinationLayout.m_nWidth, rDestinationLayout.m_nHeight,
			                                                nMipQuality );
			if ( bAlphaScissor ) {
				t3d_vt_page_builder_preserve_alpha_coverage( Source, nAlphaThreshold, out, rDestinationLayout.m_nWidth,
				                                             rDestinationLayout.m_nHeight );
			}
			t3d_vt_page_builder_bleed_albedo_rgb( out, rDestinationLayout.m_nWidth, rDestinationLayout.m_nHeight );
			continue;
		}
		if ( !t3d_vt_page_builder_resample_logical_channel_u8( Channel, T3D_VT_SURFACE_SOURCE_ORM, bAlphaScissor, nAlphaThreshold, Source,
		                                                       rDestinationLayout.m_nWidth, rDestinationLayout.m_nHeight,
		                                                       pImageBytes + rDestinationLayout.m_nOffset, rDestinationLayout.m_nBytes ) ) {
			return false;
		}
	}
	return true;
}

bool t3d_vt_page_builder_build_tile_u8( const uint8_t *pMipPixels, const uint32_t nMipWidth, const uint32_t nMipHeight,
                                        const uint32_t nComponents, const uint32_t nPageX, const uint32_t nPageY,
                                        const t3dVTCookWrapMode_t eWrapMode, uint8_t *pDestination, const uint64_t nDestinationBytes )
{
	const uint64_t nRequired = ( uint64_t )T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * nComponents;
	if ( pMipPixels == NULL || nMipWidth == 0u || nMipHeight == 0u || nComponents == 0u || nComponents > 4u || pDestination == NULL ||
	     nDestinationBytes < nRequired ) {
		return false;
	}
	for ( uint32_t y = 0u; y < T3D_VT_PHYSICAL_TILE; ++y ) {
		const int64_t nGlobalY = ( int64_t )nPageY * T3D_VT_PAGE_INTERIOR + ( int64_t )y - T3D_VT_PAGE_GUTTER;
		const uint32_t nSourceY = t3d_vt_page_builder_wrap_coordinate( nGlobalY, nMipHeight, eWrapMode );
		for ( uint32_t x = 0u; x < T3D_VT_PHYSICAL_TILE; ++x ) {
			const int64_t nGlobalX = ( int64_t )nPageX * T3D_VT_PAGE_INTERIOR + ( int64_t )x - T3D_VT_PAGE_GUTTER;
			const uint32_t nSourceX = t3d_vt_page_builder_wrap_coordinate( nGlobalX, nMipWidth, eWrapMode );
			const uint8_t *pSource = pMipPixels + ( ( uint64_t )nSourceY * nMipWidth + nSourceX ) * nComponents;
			uint8_t *pOut = pDestination + ( ( uint64_t )y * T3D_VT_PHYSICAL_TILE + x ) * nComponents;
			memcpy( pOut, pSource, nComponents );
		}
	}
	return true;
}

uint32_t t3d_vt_page_builder_pages_wide( const uint32_t nMipWidth )
{
	return nMipWidth == 0u ? 0u : ( nMipWidth + T3D_VT_PAGE_INTERIOR - 1u ) / T3D_VT_PAGE_INTERIOR;
}

uint32_t t3d_vt_page_builder_pages_high( const uint32_t nMipHeight )
{
	return nMipHeight == 0u ? 0u : ( nMipHeight + T3D_VT_PAGE_INTERIOR - 1u ) / T3D_VT_PAGE_INTERIOR;
}
