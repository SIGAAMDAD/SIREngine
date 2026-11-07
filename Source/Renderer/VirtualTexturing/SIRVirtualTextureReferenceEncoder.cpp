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

#include <Renderer/VirtualTexturing/SIRVirtualTextureReferenceEncoder.h>

#include <string.h>

// Imported Terrain3D reference block codecs, preserving donor implementation.
// Source: src/authoring/vt/build/terrain_3d_vt_reference_encoder.cpp.
// The source is upstream-owned and retains original function naming and layout.
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define CLAMP( value, minimum, maximum ) MIN( MAX( ( value ), ( minimum ) ), ( maximum ) )

typedef struct t3dVTReferenceBitWriter_s
{
	uint8_t *m_pBytes;
	uint32_t m_nBit;
} t3dVTReferenceBitWriter_t;

// clang-format off
static uint16_t t3d_vt_reference_rgb565(
	const uint8_t r,
	const uint8_t g,
	const uint8_t b ) {
	return ( uint16_t ) ( ( ( uint16_t ) ( r >> 3u ) << 11u ) | ( ( uint16_t ) ( g >> 2u ) << 5u ) | ( b >> 3u ) );
}

static void t3d_vt_reference_rgb565_decode(
	const uint16_t nColor,
	uint8_t		  *pRgb ) {
	const uint32_t r = ( nColor >> 11u ) & 31u;
	const uint32_t g = ( nColor >> 5u ) & 63u;
	const uint32_t b = nColor & 31u;
	pRgb[0] = ( uint8_t ) ( ( r << 3u ) | ( r >> 2u ) );
	pRgb[1] = ( uint8_t ) ( ( g << 2u ) | ( g >> 4u ) );
	pRgb[2] = ( uint8_t ) ( ( b << 3u ) | ( b >> 2u ) );
}

static uint32_t t3d_vt_reference_rgb_distance(
	const uint8_t *pPixel,
	const uint8_t *pColor ) {
	const int32_t dr = ( int32_t ) pPixel[0] - pColor[0];
	const int32_t dg = ( int32_t ) pPixel[1] - pColor[1];
	const int32_t db = ( int32_t ) pPixel[2] - pColor[2];
	return ( uint32_t ) ( dr * dr + dg * dg + db * db );
}

static uint32_t t3d_vt_reference_bc1_indices(
	const uint8_t *pPixels,
	const uint32_t nStride,
	const uint16_t nColor0,
	const uint16_t nColor1,
	const bool	   bHasAlpha,
	uint32_t	  &rIndices ) {
	uint8_t Palette[4][3];
	t3d_vt_reference_rgb565_decode( nColor0, Palette[0] );
	t3d_vt_reference_rgb565_decode( nColor1, Palette[1] );
	for ( uint32_t c = 0u; c < 3u; ++c ) {
		if ( bHasAlpha ) {
			Palette[2][c] = ( uint8_t ) ( ( Palette[0][c] + Palette[1][c] ) / 2u );
			Palette[3][c] = 0u;
		} else {
			Palette[2][c] = ( uint8_t ) ( ( 2u * Palette[0][c] + Palette[1][c] ) / 3u );
			Palette[3][c] = ( uint8_t ) ( ( Palette[0][c] + 2u * Palette[1][c] ) / 3u );
		}
	}
	uint32_t error = 0u;
	rIndices = 0u;
	for ( uint32_t y = 0u; y < 4u; ++y ) {
		for ( uint32_t x = 0u; x < 4u; ++x ) {
			const uint8_t *pixel = pPixels + y * nStride + x * 4u;
			uint32_t	   best = 3u;
			if ( pixel[3] >= 128u ) {
				uint32_t best_error = UINT32_MAX;
				for ( uint32_t index = 0u; index < ( bHasAlpha ? 3u : 4u ); ++index ) {
					const uint32_t distance = t3d_vt_reference_rgb_distance( pixel, Palette[index] );
					if ( distance < best_error ) {
						best_error = distance;
						best = index;
					}
				}
				error += best_error;
			}
			rIndices |= best << ( ( y * 4u + x ) * 2u );
		}
	}
	return error;
}

static void t3d_vt_reference_bc1_try_endpoints(
	const uint8_t *pPixels,
	const uint32_t nStride,
	uint16_t	   nColor0,
	uint16_t	   nColor1,
	const bool	   bHasAlpha,
	uint32_t	  &rBestError,
	uint16_t	  *pBestColors,
	uint32_t	  &rBestIndices ) {
	if ( ( bHasAlpha && nColor0 > nColor1 ) || ( !bHasAlpha && nColor0 < nColor1 ) ) {
		const uint16_t swap = nColor0;
		nColor0 = nColor1;
		nColor1 = swap;
	}
	if ( !bHasAlpha && nColor0 == nColor1 ) {
		if ( nColor0 < 0xffffu ) {
			++nColor0;
		} else {
			--nColor1;
		}
	}
	uint32_t	   indices = 0u;
	const uint32_t error = t3d_vt_reference_bc1_indices( pPixels, nStride, nColor0, nColor1, bHasAlpha, indices );
	if ( error < rBestError ) {
		rBestError = error;
		pBestColors[0] = nColor0;
		pBestColors[1] = nColor1;
		rBestIndices = indices;
	}
}

static uint16_t t3d_vt_reference_bc1_quantize(
	const float *pColor ) {
	uint32_t	   quantized[3];
	const uint32_t maxima[3] = { 31u, 63u, 31u };
	for ( uint32_t c = 0u; c < 3u; ++c ) {
		const float value = pColor[c] < 0.0f ? 0.0f : pColor[c] > 255.0f ? 255.0f : pColor[c];
		quantized[c] = ( uint32_t ) ( value * maxima[c] / 255.0f + 0.5f );
	}
	return ( uint16_t ) ( ( quantized[0] << 11u ) | ( quantized[1] << 5u ) | quantized[2] );
}

static void t3d_vt_reference_encode_bc1_block(
	const uint8_t *pPixels,
	const uint32_t nStride,
	uint8_t		  *pBlock ) {
	uint16_t candidates[16];
	uint32_t candidate_count = 0u;
	bool	 has_alpha = false;
	uint32_t min_luma = UINT32_MAX, max_luma = 0u;
	uint16_t min_color = 0u, max_color = 0u;
	for ( uint32_t y = 0u; y < 4u; ++y ) {
		for ( uint32_t x = 0u; x < 4u; ++x ) {
			const uint8_t *pixel = pPixels + y * nStride + x * 4u;
			if ( pixel[3] < 128u ) {
				has_alpha = true;
				continue;
			}
			const uint32_t luma = pixel[0] * 77u + pixel[1] * 150u + pixel[2] * 29u;
			const uint16_t color = t3d_vt_reference_rgb565( pixel[0], pixel[1], pixel[2] );
			if ( luma < min_luma ) {
				min_luma = luma;
				min_color = color;
			}
			if ( luma >= max_luma ) {
				max_luma = luma;
				max_color = color;
			}
			const float	   rgb[3] = { ( float ) pixel[0], ( float ) pixel[1], ( float ) pixel[2] };
			const uint16_t rounded = t3d_vt_reference_bc1_quantize( rgb );
			uint32_t	   i = 0u;
			while ( i < candidate_count && candidates[i] != rounded ) {
				++i;
			}
			if ( i == candidate_count ) {
				candidates[candidate_count++] = rounded;
			}
		}
	}
	if ( candidate_count == 0u ) {
		// Transparent colors never influence endpoints or visible blade colors.
		memset( pBlock, 0, 4u );
		memset( pBlock + 4u, 0xff, 4u );
		return;
	}
	uint16_t best_colors[2] = { 0u, 0u };
	uint32_t best_error = UINT32_MAX, best_indices = 0u;
	// Retain the old extrema pair as a candidate, then search opaque RGB565 pairs.
	t3d_vt_reference_bc1_try_endpoints( pPixels,
										nStride,
										min_color,
										max_color,
										has_alpha,
										best_error,
										best_colors,
										best_indices );
	for ( uint32_t a = 0u; a < candidate_count; ++a ) {
		for ( uint32_t b = a; b < candidate_count; ++b ) {
			t3d_vt_reference_bc1_try_endpoints( pPixels,
												nStride,
												candidates[a],
												candidates[b],
												has_alpha,
												best_error,
												best_colors,
												best_indices );
		}
	}
	// Four fixed least-squares/index iterations fit interpolated colors too.
	for ( uint32_t iteration = 0u; iteration < 4u && best_error != 0u; ++iteration ) {
		float aa = 0.0f, ab = 0.0f, bb = 0.0f;
		float ar[3] = {}, br[3] = {};
		for ( uint32_t y = 0u; y < 4u; ++y ) {
			for ( uint32_t x = 0u; x < 4u; ++x ) {
				const uint8_t *pixel = pPixels + y * nStride + x * 4u;
				if ( pixel[3] < 128u ) {
					continue;
				}
				const uint32_t index = ( best_indices >> ( ( y * 4u + x ) * 2u ) ) & 3u;
				const float	   a = index == 0u	 ? 1.0f
								   : index == 1u ? 0.0f
								   : has_alpha	 ? 0.5f
								   : index == 2u ? 2.0f / 3.0f
												 : 1.0f / 3.0f;
				const float	   b = 1.0f - a;
				aa += a * a;
				ab += a * b;
				bb += b * b;
				for ( uint32_t c = 0u; c < 3u; ++c ) {
					ar[c] += a * pixel[c];
					br[c] += b * pixel[c];
				}
			}
		}
		const float determinant = aa * bb - ab * ab;
		if ( determinant <= 0.000001f ) {
			break;
		}
		float color0[3], color1[3];
		for ( uint32_t c = 0u; c < 3u; ++c ) {
			color0[c] = ( ar[c] * bb - br[c] * ab ) / determinant;
			color1[c] = ( br[c] * aa - ar[c] * ab ) / determinant;
		}
		const uint32_t previous_error = best_error;
		t3d_vt_reference_bc1_try_endpoints( pPixels,
											nStride,
											t3d_vt_reference_bc1_quantize( color0 ),
											t3d_vt_reference_bc1_quantize( color1 ),
											has_alpha,
											best_error,
											best_colors,
											best_indices );
		if ( best_error == previous_error ) {
			break;
		}
	}
	// Bounded RGB565 neighbor refinement catches quantization and single-color cases.
	const uint32_t shifts[3] = { 11u, 5u, 0u }, maxima[3] = { 31u, 63u, 31u };
	for ( uint32_t pass = 0u; pass < 2u && best_error != 0u; ++pass ) {
		for ( uint32_t endpoint = 0u; endpoint < 2u; ++endpoint ) {
			for ( uint32_t c = 0u; c < 3u; ++c ) {
				for ( int32_t delta = -1; delta <= 1; delta += 2 ) {
					uint16_t	  colors[2] = { best_colors[0], best_colors[1] };
					const int32_t component = ( colors[endpoint] >> shifts[c] ) & maxima[c];
					if ( component + delta < 0 || component + delta > ( int32_t ) maxima[c] ) {
						continue;
					}
					colors[endpoint] = ( uint16_t ) ( ( colors[endpoint] & ~( maxima[c] << shifts[c] ) ) |
													  ( ( uint32_t ) ( component + delta ) << shifts[c] ) );
					t3d_vt_reference_bc1_try_endpoints( pPixels,
														nStride,
														colors[0],
														colors[1],
														has_alpha,
														best_error,
														best_colors,
														best_indices );
				}
			}
		}
	}
	pBlock[0] = ( uint8_t ) best_colors[0];
	pBlock[1] = ( uint8_t ) ( best_colors[0] >> 8u );
	pBlock[2] = ( uint8_t ) best_colors[1];
	pBlock[3] = ( uint8_t ) ( best_colors[1] >> 8u );
	for ( uint32_t byte = 0u; byte < 4u; ++byte ) {
		pBlock[4u + byte] = ( uint8_t ) ( best_indices >> ( byte * 8u ) );
	}
}

static void t3d_vt_reference_bc4_palette(
	const uint8_t nEndpoint0,
	const uint8_t nEndpoint1,
	uint8_t		 *pPalette ) {
	pPalette[0] = nEndpoint0;
	pPalette[1] = nEndpoint1;
	if ( nEndpoint0 > nEndpoint1 ) {
		for ( uint32_t i = 1u; i <= 6u; ++i ) {
			pPalette[i + 1u] = ( uint8_t ) ( ( ( 7u - i ) * nEndpoint0 + i * nEndpoint1 + 3u ) / 7u );
		}
	} else {
		for ( uint32_t i = 1u; i <= 4u; ++i ) {
			pPalette[i + 1u] = ( uint8_t ) ( ( ( 5u - i ) * nEndpoint0 + i * nEndpoint1 + 2u ) / 5u );
		}
		pPalette[6] = 0u;
		pPalette[7] = 255u;
	}
}

static void t3d_vt_reference_encode_bc4_block(
	const uint8_t *pPixels,
	const uint32_t nStride,
	const uint32_t nPixelStride,
	const uint32_t nComponent,
	uint8_t		  *pBlock ) {
	uint8_t nMin = 255u;
	uint8_t nMax = 0u;
	for ( uint32_t y = 0u; y < 4u; ++y ) {
		for ( uint32_t x = 0u; x < 4u; ++x ) {
			const uint8_t nValue = pPixels[y * nStride + x * nPixelStride + nComponent];
			nMin = nValue < nMin ? nValue : nMin;
			nMax = nValue > nMax ? nValue : nMax;
		}
	}
	pBlock[0] = nMax;
	pBlock[1] = nMin;
	uint8_t Palette[8];
	t3d_vt_reference_bc4_palette( nMax, nMin, Palette );
	uint64_t nIndices = 0u;
	for ( uint32_t y = 0u; y < 4u; ++y ) {
		for ( uint32_t x = 0u; x < 4u; ++x ) {
			const uint8_t nValue = pPixels[y * nStride + x * nPixelStride + nComponent];
			uint32_t	  nBest = 0u;
			uint32_t	  nBestDistance = UINT32_MAX;
			for ( uint32_t iPalette = 0u; iPalette < 8u; ++iPalette ) {
				const int32_t  nDelta = ( int32_t ) nValue - Palette[iPalette];
				const uint32_t nDistance = ( uint32_t ) ( nDelta < 0 ? -nDelta : nDelta );
				if ( nDistance < nBestDistance ) {
					nBestDistance = nDistance;
					nBest = iPalette;
				}
			}
			nIndices |= ( uint64_t ) nBest << ( ( y * 4u + x ) * 3u );
		}
	}
	for ( uint32_t iByte = 0u; iByte < 6u; ++iByte ) {
		pBlock[2u + iByte] = ( uint8_t ) ( nIndices >> ( iByte * 8u ) );
	}
}

// t3dVTReferenceBitWriter_s centralized in Terrain3DTypes.h.

static void t3d_vt_reference_write_bits(
	t3dVTReferenceBitWriter_t &rWriter,
	uint32_t				   nValue,
	const uint32_t			   nBitCount ) {
	for ( uint32_t iBit = 0u; iBit < nBitCount; ++iBit ) {
		if ( ( nValue & ( 1u << iBit ) ) != 0u ) {
			rWriter.m_pBytes[rWriter.m_nBit >> 3u] |= ( uint8_t ) ( 1u << ( rWriter.m_nBit & 7u ) );
		}
		++rWriter.m_nBit;
	}
}

// Bounded mode-6 RGBA fit. Keep alpha continuous and constrain cutoff classes
// after quantization; constant-color averaging destroys foliage edge detail.
static void t3d_vt_reference_encode_bc7_mode6_block(
	const uint8_t *pPixels,
	const uint32_t nStride,
	const uint8_t  nThreshold,
	uint8_t		  *pBlock ) {
	static const uint32_t weights[16] = { 0u, 4u, 9u, 13u, 17u, 21u, 26u, 30u, 34u, 38u, 43u, 47u, 51u, 55u, 60u, 64u };
	uint8_t				  pixels[16][4];
	uint32_t			  visible = 0u, min_alpha = 255u, max_alpha = 0u;
	for ( uint32_t i = 0u; i < 16u; ++i ) {
		memcpy( pixels[i], pPixels + ( i / 4u ) * nStride + ( i % 4u ) * 4u, 4u );
		if ( pixels[i][3] > pixels[visible][3] ) {
			visible = i;
		}
		min_alpha = MIN( min_alpha, ( uint32_t ) pixels[i][3] );
		max_alpha = MAX( max_alpha, ( uint32_t ) pixels[i][3] );
	}
	// Transparent RGB carries no authored color; avoid a black endpoint fringe.
	for ( uint32_t i = 0u; i < 16u; ++i ) {
		if ( pixels[i][3] == 0u ) {
			memcpy( pixels[i], pixels[visible], 3u );
		}
	}
	uint32_t first = 0u, last = 0u, largest = 0u;
	for ( uint32_t i = 0u; i < 16u; ++i ) {
		for ( uint32_t j = i + 1u; j < 16u; ++j ) {
			uint32_t distance = 0u;
			for ( uint32_t c = 0u; c < 4u; ++c ) {
				const int32_t d = ( int32_t ) pixels[i][c] - pixels[j][c];
				distance += d * d * ( c == 3u ? 4u : 1u );
			}
			if ( distance > largest ) {
				largest = distance;
				first = i;
				last = j;
			}
		}
	}
	float endpoints[2][4];
	for ( uint32_t c = 0u; c < 4u; ++c ) {
		endpoints[0][c] = pixels[first][c];
		endpoints[1][c] = pixels[last][c];
	}
	uint32_t   quantized[2][4]{}, pbits[2]{}, indices[16]{};
	const bool cutoff_crosses = nThreshold != 0u && min_alpha < nThreshold && max_alpha >= nThreshold;
	for ( uint32_t iteration = 0u; iteration < 5u; ++iteration ) {
		if ( cutoff_crosses ) {
			const bool ascending = endpoints[0][3] <= endpoints[1][3];
			endpoints[0][3] = ascending ? 0.0f : 255.0f;
			endpoints[1][3] = ascending ? 255.0f : 0.0f;
		}
		for ( uint32_t end = 0u; end < 2u; ++end ) {
			float best_error = 1e30f;
			for ( uint32_t parity = 0u; parity < 2u; ++parity ) {
				uint32_t q[4];
				float	 error = 0.0f;
				for ( uint32_t c = 0u; c < 4u; ++c ) {
					const int32_t v = CLAMP( ( int32_t ) ( ( endpoints[end][c] - parity ) * 0.5f + 0.5f ), 0, 127 );
					q[c] = ( uint32_t ) v * 2u + parity;
					if ( c == 3u && nThreshold != 0u ) {
						if ( min_alpha >= nThreshold && q[c] < nThreshold ) {
							q[c] = nThreshold + ( ( nThreshold & 1u ) != parity ? 1u : 0u );
						}
						if ( max_alpha < nThreshold && q[c] >= nThreshold ) {
							q[c] = nThreshold - 1u - ( ( ( nThreshold - 1u ) & 1u ) != parity ? 1u : 0u );
						}
					}
					if ( q[c] > 255u ) {
						error = 1e30f;
						break;
					}
					const float d = endpoints[end][c] - q[c];
					error += d * d * ( c == 3u ? 4.0f : 1.0f );
				}
				if ( error < best_error ) {
					best_error = error;
					pbits[end] = parity;
					memcpy( quantized[end], q, sizeof( q ) );
				}
			}
		}
		for ( uint32_t i = 0u; i < 16u; ++i ) {
			uint64_t best_error = UINT64_MAX;
			for ( uint32_t index = 0u; index < 16u; ++index ) {
				uint64_t error = 0u;
				for ( uint32_t c = 0u; c < 4u; ++c ) {
					const uint32_t value =
						( ( 64u - weights[index] ) * quantized[0][c] + weights[index] * quantized[1][c] + 32u ) >> 6u;
					const int32_t d = ( int32_t ) value - pixels[i][c];
					error += ( uint64_t ) ( d * d ) * ( c == 3u ? 4u : pixels[i][3] == 0u ? 0u : 1u );
					if ( c == 3u && nThreshold != 0u && ( value >= nThreshold ) != ( pixels[i][3] >= nThreshold ) ) {
						error += 1ull << 40u;
					}
				}
				if ( error < best_error ) {
					best_error = error;
					indices[i] = index;
				}
			}
		}
		if ( iteration == 4u ) {
			break;
		}
		// Least-squares endpoints for the selected palette, fixed work per block.
		for ( uint32_t c = 0u; c < 4u; ++c ) {
			float aa = 0.0f, ab = 0.0f, bb = 0.0f, ax = 0.0f, bx = 0.0f;
			for ( uint32_t i = 0u; i < 16u; ++i ) {
				if ( c < 3u && pixels[i][3] == 0u ) {
					continue;
				}
				const float beta = weights[indices[i]] / 64.0f, alpha = 1.0f - beta;
				aa += alpha * alpha;
				ab += alpha * beta;
				bb += beta * beta;
				ax += alpha * pixels[i][c];
				bx += beta * pixels[i][c];
			}
			const float determinant = aa * bb - ab * ab;
			if ( determinant > 0.0001f ) {
				endpoints[0][c] = CLAMP( ( ax * bb - bx * ab ) / determinant, 0.0f, 255.0f );
				endpoints[1][c] = CLAMP( ( bx * aa - ax * ab ) / determinant, 0.0f, 255.0f );
			}
		}
	}
	// The anchor has only three index bits. Endpoint exchange preserves colors.
	if ( indices[0] >= 8u ) {
		for ( uint32_t c = 0u; c < 4u; ++c ) {
			const uint32_t q = quantized[0][c];
			quantized[0][c] = quantized[1][c];
			quantized[1][c] = q;
		}
		const uint32_t p = pbits[0];
		pbits[0] = pbits[1];
		pbits[1] = p;
		for ( uint32_t i = 0u; i < 16u; ++i ) {
			indices[i] = 15u - indices[i];
		}
	}
	memset( pBlock, 0, 16u );
	t3dVTReferenceBitWriter_t writer{ pBlock, 0u };
	t3d_vt_reference_write_bits( writer, 0x40u, 7u );
	for ( uint32_t c = 0u; c < 4u; ++c ) {
		t3d_vt_reference_write_bits( writer, quantized[0][c] >> 1u, 7u );
		t3d_vt_reference_write_bits( writer, quantized[1][c] >> 1u, 7u );
	}
	t3d_vt_reference_write_bits( writer, pbits[0], 1u );
	t3d_vt_reference_write_bits( writer, pbits[1], 1u );
	for ( uint32_t i = 0u; i < 16u; ++i ) {
		t3d_vt_reference_write_bits( writer, indices[i], i == 0u ? 3u : 4u );
	}
}

// clang-format on

#undef MIN
#undef MAX
#undef CLAMP

// Keep this a cold cooker operation: no allocations, file I/O or GPU work.
// The input is already the guttered 136x136 tile built by the page cooker.
bool sirVTReferenceEncodeTile( const sirVirtualTextureReferenceEncodeRequest_t& request, uint32_t *pWrittenBytes ) noexcept
{
	if ( pWrittenBytes == nullptr ) {
		return false;
	}
	*pWrittenBytes = 0;
	if ( request.pSourcePixels == nullptr || request.pDestination == nullptr ) {
		return false;
	}
	uint32_t nComponents = 0;
	switch ( request.eFamily ) {
	case SIR_VT_PHYSICAL_BC1:
	case SIR_VT_PHYSICAL_BC7:
		nComponents = 4;
		break;
	case SIR_VT_PHYSICAL_BC5:
	case SIR_VT_PHYSICAL_R16:
		nComponents = 2;
		break;
	case SIR_VT_PHYSICAL_R8:
		nComponents = 1;
		break;
	default:
		return false;
	}
	const uint64_t nRawBytes = ( uint64_t )SIR_VT_PHYSICAL_TILE * SIR_VT_PHYSICAL_TILE * nComponents;
	const uint32_t nEncodedBytes = sirVTPackPhysicalTileBytes( request.eFamily );
	if ( request.nComponents != nComponents || request.nSourceBytes < nRawBytes || request.nDestinationCapacity < nEncodedBytes ||
	     nEncodedBytes == 0 || nRawBytes > SIZE_MAX ) {
		return false;
	}
	const uintptr_t nSource = ( uintptr_t )request.pSourcePixels;
	const uintptr_t nTarget = ( uintptr_t )request.pDestination;
	if ( nRawBytes > UINTPTR_MAX - nSource || nEncodedBytes > UINTPTR_MAX - nTarget ||
	     ( nSource < nTarget + nEncodedBytes && nTarget < nSource + nRawBytes ) ) {
		return false;
	}
	if ( request.eFamily == SIR_VT_PHYSICAL_R8 || request.eFamily == SIR_VT_PHYSICAL_R16 ) {
		memcpy( request.pDestination, request.pSourcePixels, ( size_t )nRawBytes );
		*pWrittenBytes = nEncodedBytes;
		return true;
	}
	const uint32_t nStride = SIR_VT_PHYSICAL_TILE * nComponents;
	const uint32_t nBlocks = SIR_VT_PHYSICAL_TILE / 4U;
	uint8_t *pOutput = request.pDestination;
	for ( uint32_t yBlock = 0; yBlock < nBlocks; ++yBlock ) {
		for ( uint32_t xBlock = 0; xBlock < nBlocks; ++xBlock ) {
			const uint8_t *pBlock = request.pSourcePixels + ( ( uint64_t )yBlock * 4U * SIR_VT_PHYSICAL_TILE + xBlock * 4U ) * nComponents;
			switch ( request.eFamily ) {
			case SIR_VT_PHYSICAL_BC1:
				t3d_vt_reference_encode_bc1_block( pBlock, nStride, pOutput );
				pOutput += 8;
				break;
			case SIR_VT_PHYSICAL_BC5:
				t3d_vt_reference_encode_bc4_block( pBlock, nStride, 2U, 0U, pOutput );
				t3d_vt_reference_encode_bc4_block( pBlock, nStride, 2U, 1U, pOutput + 8U );
				pOutput += 16;
				break;
			case SIR_VT_PHYSICAL_BC7:
				t3d_vt_reference_encode_bc7_mode6_block( pBlock, nStride, request.nAlphaThreshold, pOutput );
				pOutput += 16;
				break;
			default:
				return false;
			}
		}
	}
	*pWrittenBytes = nEncodedBytes;
	return true;
}
