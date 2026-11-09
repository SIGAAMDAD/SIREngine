// Copyright ? 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.
// Consolidated from authoring/vt/build/terrain_3d_megatexture_cooker.h
#include "Terrain3DTypes.h"

#include "authoring/vt/build/terrain_3d_vt_page_builder.h"

#include <stdint.h>

// t3dMegatexturePageAddress_s centralized in Terrain3DTypes.h.

uint32_t t3d_megatexture_cooker_page_count( uint32_t nWidth, uint32_t nHeight );
bool t3d_megatexture_cooker_build_page_u8( const uint8_t *pMipPixels, uint32_t nMipWidth, uint32_t nMipHeight, uint32_t nComponents,
                                           const t3dMegatexturePageAddress_t& rAddress, t3dVTCookWrapMode_t eWrapMode,
                                           uint8_t *pDestination, uint64_t nDestinationBytes );
#include "authoring/vt/build/terrain_3d_incremental_megavt_cooker.h"
#include "authoring/vt/io/terrain_3d_vt_godot_cooker.h"
#include "core/utility/terrain_3d_hash_bytes.h"
#include "core/utility/terrain_3d_generation.h"
#include "core/utility/terrain_3d_numeric.h"
#include "core/utility/terrain_3d_checked_math.h"
#include "data/persistence/terrain_3d_file_publication.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <math.h>

static bool t3dMegaVTCaptureColors( const t3dMegaVTAuthoringInputs_t& rInputs, const Vector2i& rLocation,
                                    t3dMegaVTColorCapture_t& rCapture )
{
	Terrain3DRegionSource *const pData = &rInputs.m_source;
	if ( pData == NULL || !pData->ensureCpuRegionResident( rLocation ) ) {
		return false;
	}
	// Capture each map immediately: subsequent loads may evict the live region.
	for ( int z = -1; z <= 1; ++z ) {
		for ( int x = -1; x <= 1; ++x ) {
			t3dMegaVTColorSource_t& source = rCapture.m_neighbors[( z + 1 ) * 3 + x + 1];
			source = t3dMegaVTColorSource_t{};
			const Vector2i location = rLocation + Vector2i( x, z );
			if ( !pData->hasRegion( location ) ) {
				continue;
			}
			if ( !pData->ensureCpuRegionResident( location ) ) {
				return false;
			}
			Ref<Terrain3DRegion> region = pData->getRegion( location );
			if ( region.is_null() ) {
				return false;
			}
			const Ref<Image> color = region->getColorMap();
			if ( color.is_null() || color->is_empty() ) {
				continue;
			}
			if ( color->get_format() != Image::FORMAT_RGBA8 || color->get_width() <= 0 || color->get_height() <= 0 ) {
				return false;
			}
			source.m_nWidth = ( uint32_t )color->get_width();
			source.m_nHeight = ( uint32_t )color->get_height();
			source.m_bytes = color->get_data();
			if ( ( uint64_t )source.m_bytes.size() < ( uint64_t )source.m_nWidth * source.m_nHeight * 4u ) {
				return false;
			}
		}
	}
	return true;
}

static uint64_t t3dMegaVTSourceFingerprint( const t3dMegaVTColorCapture_t& rCapture )
{
	uint64_t hash = 1469598103934665603ULL;
	// Cooker recipe revisions are cache dependencies, not source pixels.
	hash = t3dHashFnv1a64LittleU32( hash, T3D_VT_ABI_VERSION );
	hash = t3dHashFnv1a64LittleU32( hash, T3D_VT_FILTER_ABI_VERSION );
	hash = t3dHashFnv1a64LittleU32( hash, T3D_VT_COMPRESSOR_ABI_VERSION );
	hash = t3dHashFnv1a64LittleU32( hash, rCapture.m_nExtent );
	for ( uint32_t i = 0u; i < 9u; ++i ) {
		const t3dMegaVTColorSource_t& source = rCapture.m_neighbors[i];
		hash = t3dHashFnv1a64LittleU32( hash, source.m_nWidth );
		hash = t3dHashFnv1a64LittleU32( hash, source.m_nHeight );
		hash = t3dHashFnv1a64( source.m_bytes.ptr(), ( uint64_t )source.m_bytes.size(), hash );
	}
	return hash == 0u ? 1u : hash;
}

static PackedByteArray t3dMegaVTSourceSignatureBytes( const uint64_t nFingerprint )
{
	PackedByteArray result;
	if ( result.resize( 16 ) != OK ) {
		return PackedByteArray();
	}
	uint8_t *const pBytes = result.ptrw();
	const uint64_t values[2] = { 0x3152435354563344ULL, nFingerprint }; // D3VT SCR1 versioned header.
	for ( uint32_t i = 0u; i < 16u; ++i ) {
		pBytes[i] = ( uint8_t )( values[i / 8u] >> ( ( i % 8u ) * 8u ) );
	}
	return result;
}

static bool t3dMegaVTSourceSignatureMatches( const PackedByteArray& rBytes, const uint64_t nFingerprint )
{
	const PackedByteArray expected = t3dMegaVTSourceSignatureBytes( nFingerprint );
	return rBytes.size() == 16 && expected.size() == 16 && memcmp( rBytes.ptr(), expected.ptr(), 16u ) == 0;
}

static float t3dMegaVTColorTap( const t3dMegaVTColorCapture_t& rCapture, int x, int z, const uint32_t nComponent )
{
	const t3dMegaVTColorSource_t& center = rCapture.m_neighbors[4];
	if ( center.m_nWidth == 0u || center.m_nHeight == 0u ) {
		return nComponent == 3u ? 0.5f : 1.0f;
	}
	const int dx = x < 0 ? -1 : ( x >= ( int )center.m_nWidth ? 1 : 0 );
	const int dz = z < 0 ? -1 : ( z >= ( int )center.m_nHeight ? 1 : 0 );
	const t3dMegaVTColorSource_t& neighbor = rCapture.m_neighbors[( dz + 1 ) * 3 + dx + 1];
	const bool bNeighbor = neighbor.m_nWidth == center.m_nWidth && neighbor.m_nHeight == center.m_nHeight;
	const t3dMegaVTColorSource_t& source = bNeighbor ? neighbor : center;
	if ( bNeighbor ) {
		x -= dx * ( int )center.m_nWidth;
		z -= dz * ( int )center.m_nHeight;
	}
	x = CLAMP( x, 0, ( int )source.m_nWidth - 1 );
	z = CLAMP( z, 0, ( int )source.m_nHeight - 1 );
	return ( float )source.m_bytes.ptr()[( ( uint64_t )z * source.m_nWidth + x ) * 4u + nComponent] / 255.0f;
}

static float t3dMegaVTColorSample( const t3dMegaVTColorCapture_t& rCapture, const float u, const float v, const uint32_t nComponent )
{
	const t3dMegaVTColorSource_t& center = rCapture.m_neighbors[4];
	if ( center.m_nWidth == 0u || center.m_nHeight == 0u ) {
		return nComponent == 3u ? 0.5f : 1.0f;
	}
	const float x = u * center.m_nWidth - 0.5f;
	const float z = v * center.m_nHeight - 0.5f;
	const int ix = ( int )floorf( x ), iz = ( int )floorf( z );
	const float fx = x - ix, fz = z - iz;
	const float a = t3dMegaVTColorTap( rCapture, ix, iz, nComponent );
	const float b = t3dMegaVTColorTap( rCapture, ix + 1, iz, nComponent );
	const float c = t3dMegaVTColorTap( rCapture, ix, iz + 1, nComponent );
	const float d = t3dMegaVTColorTap( rCapture, ix + 1, iz + 1, nComponent );
	return ( a + ( b - a ) * fx ) * ( 1.0f - fz ) + ( c + ( d - c ) * fx ) * fz;
}

static uint8_t t3dMegaVTLinearToSrgbByte( const float fValue )
{
	const float fSrgb = fValue <= 0.0031308f ? fValue * 12.92f : 1.055f * powf( fValue, 1.0f / 2.4f ) - 0.055f;
	return t3dPackUnorm8Nearest( fSrgb );
}

static bool t3dMegaVTBuildPixels( const t3dMegaVTColorCapture_t& rCapture, t3dMegaVTRegionPixels_t& rPixels,
                                  const t3dAtomicBool_t& rCancel )
{
	const uint32_t nExtent = rCapture.m_nExtent;
	if ( nExtent != 256u && nExtent != 512u ) {
		return false;
	}
	const bool bReuse = rPixels.m_nExtent == nExtent && rPixels.m_nRegionIndex == rCapture.m_nRegionIndex &&
	                    rPixels.m_albedo.size() == ( int64_t )nExtent * nExtent * 4u &&
	                    rPixels.m_pbr.size() == ( int64_t )( nExtent / 2u ) * ( nExtent / 2u ) * 4u;
	const uint32_t nPages = nExtent / T3D_VT_PAGE_INTERIOR;
	const uint32_t nDirtyMask = bReuse ? rCapture.m_nDirtyMip0Mask : ( 1u << ( nPages * nPages ) ) - 1u;
	rPixels.m_nExtent = nExtent;
	rPixels.m_nRegionIndex = rCapture.m_nRegionIndex;
	if ( rPixels.m_albedo.resize( ( int64_t )nExtent * nExtent * 4u ) != OK ||
	     rPixels.m_pbr.resize( ( int64_t )( nExtent / 2u ) * ( nExtent / 2u ) * 4u ) != OK ) {
		return false;
	}
	// Albedo stores linear painted color via the V1 sRGB family. PBR's blue
	// channel stores painted wetness alpha; normal/AO remain semantic neutral.
	for ( uint32_t channel = 0u; channel < 2u; ++channel ) {
		const uint32_t nSide = channel == 0u ? nExtent : nExtent / 2u;
		uint8_t *const pPixels = channel == 0u ? rPixels.m_albedo.ptrw() : rPixels.m_pbr.ptrw();
		for ( uint32_t z = 0u; z < nSide; ++z ) {
			if ( rCancel.load( T3D_MEMORY_ORDER_ACQUIRE ) ) {
				return false;
			}
			for ( uint32_t x = 0u; x < nSide; ++x ) {
				const uint32_t nPage = ( z * nExtent / nSide / T3D_VT_PAGE_INTERIOR ) * nPages + x * nExtent / nSide / T3D_VT_PAGE_INTERIOR;
				if ( ( nDirtyMask & ( 1u << nPage ) ) == 0u ) {
					continue;
				}
				uint8_t *const pPixel = pPixels + ( ( uint64_t )z * nSide + x ) * 4u;
				const float u = ( x + 0.5f ) / nSide, v = ( z + 0.5f ) / nSide;
				if ( channel == 0u ) {
					for ( uint32_t c = 0u; c < 3u; ++c ) {
						pPixel[c] = t3dMegaVTLinearToSrgbByte( t3dMegaVTColorSample( rCapture, u, v, c ) );
					}
					pPixel[3] = 255u;
				} else {
					pPixel[0] = 128u;
					pPixel[1] = 128u;
					pPixel[2] = t3dPackUnorm8Nearest( t3dMegaVTColorSample( rCapture, u, v, 3u ) );
					pPixel[3] = 255u;
				}
			}
		}
	}
	return true;
}

static bool t3dMegaVTCookPixels( const t3dMegaVTColorCapture_t& rCapture, const t3dMegaVTRegionPixels_t& rPixels,
                                 PackedByteArray& rFragment, const uint64_t nByteLimit )
{
	t3dVTCookSparseSource_t source{};
	source.m_nStableKey = t3d_vt_terrain_region_stable_key( rCapture.m_nRegionIndex );
	source.m_nGenerationSeed = rCapture.m_nGeneration;
	source.m_nConsumer = T3D_VT_CONSUMER_CLIPMAP;
	source.m_nWrapMode = T3D_VT_COOK_WRAP_CLAMP;
	source.m_nAllocationFlags = T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP | T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE;
	source.m_nAuthoredChannelMask =
	    T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR );
	const uint32_t nExtent = rPixels.m_nExtent;
	source.m_Channels[T3D_VT_LOGICAL_CHANNEL_ALBEDO] = { rPixels.m_albedo.ptr(), nExtent, nExtent, 4u, nExtent * 4u };
	source.m_Channels[T3D_VT_LOGICAL_CHANNEL_PBR] = { rPixels.m_pbr.ptr(), nExtent / 2u, nExtent / 2u, 4u, nExtent * 2u };
	source.m_nSourceFingerprint[T3D_VT_LOGICAL_CHANNEL_ALBEDO] = t3dHashPublicationBytes( rPixels.m_albedo.ptr(), rPixels.m_albedo.size() );
	source.m_nSourceFingerprint[T3D_VT_LOGICAL_CHANNEL_PBR] = t3dHashPublicationBytes( rPixels.m_pbr.ptr(), rPixels.m_pbr.size() );
	return t3d_vt_godot_cook_source_fragment( source, rFragment, nByteLimit );
}

Terrain3DIncrementalMegaVTCooker::~Terrain3DIncrementalMegaVTCooker( void )
{
	m_cancel.store( true, T3D_MEMORY_ORDER_RELEASE );
	if ( m_worker.joinable() ) {
		m_worker.join();
	}
}

bool Terrain3DIncrementalMegaVTCooker::captureRegionCold( const t3dMegaVTAuthoringInputs_t& rInputs,
                                                          const t3dTerrainMegaVTIncrementalCookRequest_t& rRequest )
{
	// Capture and worker storage must be admitted before taking Image snapshots.
	// Native owner-thread metadata is charged by its allocator separately.
	const uint64_t nCaptureLimit = 9u * ( ( uint64_t )T3D_REGION_SIZE * T3D_REGION_SIZE * 4u + 64u );
	const uint64_t nPayloadBytes = getRetainedPayloadBytesCold();
	const uint64_t nRetained = getRetainedCpuBytesCold();
	if ( nRetained > m_nCpuByteLimit || nCaptureLimit + m_nWorkerByteLimit > m_nCpuByteLimit - nRetained ||
	     !m_cpuMemory.resize( nPayloadBytes + nCaptureLimit + m_nWorkerByteLimit ) ) {
		return false;
	}
	if ( !t3dContainerReserveCold( m_pixelCache, T3D_MEGAVT_PIXEL_CACHE_REGION_LIMIT ) ) {
		return false;
	}
	m_capture = t3dMegaVTColorCapture_t{};
	m_capture.m_nExtent = T3D_REGION_WORLD_SIZE_METERS * rRequest.m_base.m_nTexelsPerMeter;
	m_capture.m_nRegionIndex = m_ticket.m_nRegionMapIndex;
	m_capture.m_nGeneration = m_ticket.m_nGeneration;
	m_capture.m_nDirtyMip0Mask = rRequest.m_nDirtyMip0Mask;
	m_pixels = t3dMegaVTRegionPixels_t{};
	for ( size_t i = 0u; i < m_pixelCache.size(); ++i ) {
		if ( m_pixelCache[i].m_nRegionIndex == m_capture.m_nRegionIndex ) {
			m_pixels = m_pixelCache[i];
			break;
		}
	}
	if ( !t3dMegaVTCaptureColors( rInputs, Vector2i( rRequest.m_base.m_nRegionX, rRequest.m_base.m_nRegionZ ), m_capture ) ) {
		return false;
	}
	m_capture.m_nSourceFingerprint = t3dMegaVTSourceFingerprint( m_capture );
	return true;
}

void Terrain3DIncrementalMegaVTCooker::workerEntry( Terrain3DIncrementalMegaVTCooker *const pCooker )
{
	// Native/EASTL allocations use a thread-local account; explicitly propagate it.
	Terrain3DMemoryAccountScope authoring_scope( pCooker->m_authoringMemoryAccount.get() );
	pCooker->cookCapturedRegion();
}

void Terrain3DIncrementalMegaVTCooker::cookCapturedRegion( void )
{
	m_bWorkerSucceeded = t3dMegaVTBuildPixels( m_capture, m_pixels, m_cancel ) && !m_cancel.load( T3D_MEMORY_ORDER_ACQUIRE ) &&
	                     t3dMegaVTCookPixels( m_capture, m_pixels, m_workerFragment, m_nWorkerByteLimit - 2u * 512u * 512u * 4u );
	m_done.store( true, T3D_MEMORY_ORDER_RELEASE );
}

void Terrain3DIncrementalMegaVTCooker::retireClaimCold( Terrain3DTerrainMegaVT& rMegaVT, Terrain3DMegaVTCookHost& rHost,
                                                        const String& rError, const bool bTransient )
{
	if ( !rError.is_empty() && m_ticket.m_nClaimId != 0u && m_ticket.m_nRegionMapIndex < T3D_REGION_SLOTS ) {
		const uint32_t index = m_ticket.m_nRegionMapIndex;
		m_nRetrySourceGeneration[index] = m_ticket.m_nGeneration;
		if ( bTransient && m_nRetryAttempts[index] < 6u ) {
			const uint32_t attempt = ++m_nRetryAttempts[index];
			m_nRetryAfterUsec[index] = t3dSteadyClockUsec() + ( 250000ULL << ( attempt - 1u ) );
			m_nNextRetryUsec = m_nNextRetryUsec == 0u ? m_nRetryAfterUsec[index] : MIN( m_nNextRetryUsec, m_nRetryAfterUsec[index] );
		} else {
			m_nFailedSourceGenerations[index] = m_ticket.m_nGeneration;
			m_nRetryAfterUsec[index] = 0u;
		}
	}
	( void )rMegaVT.cancelIncrementalCookHot( m_ticket );
	m_ticket = t3dTerrainMegaVTCookTicket_t{};
	m_workerFragment.clear();
	m_capture = t3dMegaVTColorCapture_t{};
	m_pixels = t3dMegaVTRegionPixels_t{};
	trimCpuReservationCold();
	m_nPendingFingerprint = 0u;
	if ( m_bForceActive ) {
		m_bForceFailed = true;
	}
	if ( !rError.is_empty() ) {
		m_lastError = rError;
		++m_nFailures;
	} else {
		++m_nRejected;
	}
}

void Terrain3DIncrementalMegaVTCooker::finishWorkerCold( const t3dMegaVTAuthoringInputs_t& rInputs, Terrain3DTerrainMegaVT& rMegaVT,
                                                         Terrain3DMegaVTCookHost& rHost )
{
	if ( !m_worker.joinable() || !m_done.load( T3D_MEMORY_ORDER_ACQUIRE ) ) {
		return;
	}
	m_worker.join();
	if ( !rMegaVT.validateIncrementalCookCompletionHot( m_ticket ) ) {
		retireClaimCold( rMegaVT, rHost, String() );
		return;
	}
	if ( !m_bWorkerSucceeded || m_workerFragment.is_empty() ) {
		retireClaimCold( rMegaVT, rHost, "MegaVT region cook failed; dirty pages retained" );
		return;
	}
	const uint64_t nRetained = getRetainedCpuBytesCold();
	if ( nRetained > m_nCpuByteLimit ) {
		retireClaimCold( rMegaVT, rHost, "MegaVT CPU budget rejected retained fragment; retry scheduled", true );
		return;
	}
	if ( m_bForceActive ) {
		const uint32_t index = m_ticket.m_nRegionMapIndex;
		m_forceFragments[index] = m_workerFragment;
		m_forceTickets[index] = m_ticket;
		m_forceSourceFingerprints[index] = m_capture.m_nSourceFingerprint;
		m_ticket = t3dTerrainMegaVTCookTicket_t{};
		m_workerFragment.clear();
		m_capture = t3dMegaVTColorCapture_t{};
		m_pixels = t3dMegaVTRegionPixels_t{};
		++m_nForceCookedCount;
		trimCpuReservationCold();
		return;
	}
	// A completed cook is staged, not immediately merged into the live VT package.
	// A fresh edit may invalidate any previously staged ticket; publication checks all
	// tickets again before accepting the batch.
	const uint32_t index = m_ticket.m_nRegionMapIndex;
	if ( !m_fragments[index].is_empty() && m_fragments[index] == m_workerFragment ) {
		// A source generation may advance while the encoded VT pixels remain
		// identical. Refresh the cache signature without restarting GPU residency.
		if ( !publishFragmentFileCold( rInputs, m_ticket, m_workerFragment, m_capture.m_nSourceFingerprint ) ) {
			retireClaimCold( rMegaVT, rHost, "MegaVT unchanged-fragment cache write failed; retry scheduled", true );
			return;
		}
		if ( !rMegaVT.acceptIncrementalCookCompletionHot( m_ticket ) ) {
			retireClaimCold( rMegaVT, rHost, String() );
			return;
		}
		m_nRetryAfterUsec[index] = 0u;
		m_nRetryAttempts[index] = 0u;
		m_nRetrySourceGeneration[index] = 0u;
		if ( m_bStartupRecoveryRegion[index] ) {
			m_bStartupRecoveryRegion[index] = false;
			if ( m_nRecoveryOutstanding > 0u ) {
				--m_nRecoveryOutstanding;
			}
		}
		++m_nUnchangedFragments;
		++m_nAccepted;
		m_ticket = t3dTerrainMegaVTCookTicket_t{};
		m_workerFragment.clear();
		m_capture = t3dMegaVTColorCapture_t{};
		m_pixels = t3dMegaVTRegionPixels_t{};
		trimCpuReservationCold();
		rHost.persistMegaVTEditorStateCold();
		return;
	}
	if ( m_nStagedCount >= T3D_MEGAVT_PUBLICATION_BATCH_REGION_LIMIT ) {
		retireClaimCold( rMegaVT, rHost, "MegaVT staged publication batch capacity exceeded", true );
		return;
	}
	t3dMegaVTStagedRegion_t& staged = m_stagedRegions[m_nStagedCount++];
	staged.m_ticket = m_ticket;
	staged.m_fragment = m_workerFragment;
	staged.m_nSourceFingerprint = m_capture.m_nSourceFingerprint;
	// Keep at most four reusable cooked pixel surfaces. A failed/stale batch
	// invalidates these entries before a new claim can reuse them.
	for ( size_t i = 0u; i < m_pixelCache.size(); ++i ) {
		if ( m_pixelCache[i].m_nRegionIndex == m_pixels.m_nRegionIndex ) {
			m_pixelCache.erase( m_pixelCache.begin() + i );
			break;
		}
	}
	if ( m_pixelCache.size() == T3D_MEGAVT_PIXEL_CACHE_REGION_LIMIT ) {
		m_pixelCache.erase( m_pixelCache.begin() );
	}
	m_pixelCache.push_back( m_pixels );
	if ( m_nStagedCount == 1u ) {
		m_nBatchFirstUsec = t3dSteadyClockUsec();
	}
	m_ticket = t3dTerrainMegaVTCookTicket_t{};
	m_workerFragment.clear();
	m_capture = t3dMegaVTColorCapture_t{};
	m_pixels = t3dMegaVTRegionPixels_t{};
	trimCpuReservationCold();
	// Merge before staged fragments exhaust the independent authoring budget.
	// Admission still uses the account owner; this threshold limits long batches.
	if ( m_nStagedCount == T3D_MEGAVT_PUBLICATION_BATCH_REGION_LIMIT ||
	     getRetainedPayloadBytesCold() >= MIN( m_nCpuByteLimit / 4u, 128ull * 1024ull * 1024ull ) ) {
		requestStagedPublicationCold( rHost );
	}
}

static String t3dMegaVTCacheDirectory( const t3dMegaVTAuthoringInputs_t& rInputs )
{
	String directory = rInputs.m_dataDirectory.path_join( ".vt_cache/terrain_regions" );
	if ( rInputs.m_dataDirectory.is_empty() ) {
		ProjectSettings *const pSettings = ProjectSettings::get_singleton();
		if ( pSettings == NULL ) {
			return String();
		}
		directory = pSettings->globalize_path( "res://.godot/terrain3d/vt_cache/terrain_regions" );
	}
	return directory;
}

// Explicit-bake accounting; counters describe the most recent synchronous
// V1 region export (not physical VT residency or GPU memory).
typedef struct t3dMegaVTExportCacheCounters_s
{
	uint32_t m_nReused = 0u;
	uint32_t m_nCooked = 0u;
	uint32_t m_nCacheWrites = 0u;
	uint32_t m_nCacheWriteFailures = 0u;
	uint64_t m_nReusedBytes = 0u;
	uint64_t m_nCookedBytes = 0u;
	uint64_t m_nMergedBytes = 0u;
	uint32_t m_nNoOpPublications = 0u;
	uint32_t m_nLastGeneration = 0u;
} t3dMegaVTExportCacheCounters_t;

static t3dMegaVTExportCacheCounters_t s_exportCacheCounters;

static bool t3dMegaVTFileEqualsBytesCold( const String& rPath, const PackedByteArray& rBytes )
{
	if ( rPath.is_empty() || rBytes.is_empty() )
		return false;
	const Ref<FileAccess> file = FileAccess::open( rPath, FileAccess::READ );
	if ( file.is_null() || file->get_length() != ( uint64_t )rBytes.size() )
		return false;
	uint8_t scratch[65536u];
	const uint8_t *const expected = rBytes.ptr();
	uint64_t offset = 0u;
	while ( offset < ( uint64_t )rBytes.size() ) {
		const uint64_t count = MIN( ( uint64_t )sizeof( scratch ), ( uint64_t )rBytes.size() - offset );
		if ( file->get_buffer( scratch, count ) != count || memcmp( scratch, expected + offset, ( size_t )count ) != 0 )
			return false;
		offset += count;
	}
	return true;
}

bool Terrain3DIncrementalMegaVTCooker::publishedArtifactsMatchCold( const String& rPackagePath, const PackedByteArray& rPackage,
                                                                    const String& rDemandPath, const PackedByteArray& rDemand,
                                                                    const String& rV1Path, const PackedByteArray& rV1 )
{
	if ( !t3dMegaVTFileEqualsBytesCold( rPackagePath, rPackage ) )
		return false;
	if ( rDemandPath.is_empty() != rDemand.is_empty() || rV1Path.is_empty() != rV1.is_empty() )
		return false;
	if ( !rV1Path.is_empty() && rDemandPath.is_empty() )
		return false;
	const bool identical = ( rDemandPath.is_empty() || t3dMegaVTFileEqualsBytesCold( rDemandPath, rDemand ) ) &&
	                       ( rV1Path.is_empty() || t3dMegaVTFileEqualsBytesCold( rV1Path, rV1 ) );
	if ( identical )
		++s_exportCacheCounters.m_nNoOpPublications;
	return identical;
}

Dictionary Terrain3DIncrementalMegaVTCooker::getExportCacheDiagnosticsCold( void )
{
	Dictionary result;
	result["export_fragment_reused_count"] = ( int64_t )s_exportCacheCounters.m_nReused;
	result["export_fragment_cooked_count"] = ( int64_t )s_exportCacheCounters.m_nCooked;
	result["export_fragment_cache_writes"] = ( int64_t )s_exportCacheCounters.m_nCacheWrites;
	result["export_fragment_cache_write_failures"] = ( int64_t )s_exportCacheCounters.m_nCacheWriteFailures;
	result["export_fragment_reused_bytes"] = ( int64_t )s_exportCacheCounters.m_nReusedBytes;
	result["export_fragment_cooked_bytes"] = ( int64_t )s_exportCacheCounters.m_nCookedBytes;
	result["export_megavt_merged_bytes"] = ( int64_t )s_exportCacheCounters.m_nMergedBytes;
	result["export_package_noop_publications"] = ( int64_t )s_exportCacheCounters.m_nNoOpPublications;
	result["export_megavt_generation"] = ( int64_t )s_exportCacheCounters.m_nLastGeneration;
	return result;
}

// Validate both the source signature and the fragment's V1 allocation ABI.
// A partial cache publication, stale neighbors or different texel density
// always falls back to the established source cook.
static bool t3dMegaVTLoadExportFragmentCold( const String& rPath, const uint64_t nFingerprint, const uint32_t nRegionIndex,
                                             const uint32_t nExtent, const uint64_t nByteLimit, PackedByteArray& rFragment )
{
	const Ref<FileAccess> signature = FileAccess::open( rPath + String( ".source" ), FileAccess::READ );
	if ( signature.is_null() || signature->get_length() != 16u ||
	     !t3dMegaVTSourceSignatureMatches( signature->get_buffer( 16u ), nFingerprint ) )
		return false;
	const Ref<FileAccess> file = FileAccess::open( rPath, FileAccess::READ );
	if ( file.is_null() )
		return false;
	const uint64_t bytes = file->get_length();
	if ( bytes < T3D_VT_PACK_HEADER_BYTES || bytes > nByteLimit )
		return false;
	PackedByteArray candidate = file->get_buffer( bytes );
	t3dVTPackBaselineView_t view{};
	if ( ( uint64_t )candidate.size() != bytes || !t3d_vt_pack_open_baseline_view( candidate.ptr(), bytes, view ) )
		return false;
	if ( view.m_pHeader->m_nAllocationCount != 1u ||
	     view.m_pAllocationKeys[0].m_nStableKey != t3d_vt_terrain_region_stable_key( nRegionIndex ) )
		return false;
	if ( view.m_pAllocations[0].m_nGeneration != 1u || view.m_pAllocations[0].m_nConsumer != T3D_VT_CONSUMER_CLIPMAP )
		return false;
	const uint32_t flags = T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP;
	if ( ( view.m_pAllocations[0].m_nFlags & flags ) != flags )
		return false;
	const uint32_t pages = nExtent / T3D_VT_PAGE_INTERIOR;
	const t3dVTChannelAllocation_t& albedo = view.m_pChannelAllocations[T3D_VT_LOGICAL_CHANNEL_ALBEDO];
	const t3dVTChannelAllocation_t& pbr = view.m_pChannelAllocations[T3D_VT_LOGICAL_CHANNEL_PBR];
	if ( albedo.m_nMip0WidthPages != pages || albedo.m_nMip0HeightPages != pages || pbr.m_nMip0WidthPages != pages / 2u ||
	     pbr.m_nMip0HeightPages != pages / 2u )
		return false;
	rFragment = candidate;
	return true;
}

// Keep the source marker and fragment in the existing rollback-capable pair.
static bool t3dMegaVTPublishCachePairCold( const String& rPath, const PackedByteArray& rFragment, const uint64_t nSourceFingerprint,
                                           String& rError )
{
	const PackedByteArray signature = t3dMegaVTSourceSignatureBytes( nSourceFingerprint );
	if ( signature.size() != 16u ) {
		rError = "MegaVT source fingerprint staging failed";
		return false;
	}
	const String sourcePath = rPath + String( ".source" );
	Terrain3DFilePublication source( sourcePath, sourcePath + String( ".staged" ), sourcePath + String( ".previous" ) );
	Terrain3DFilePublication fragment( rPath, rPath + String( ".staged" ), rPath + String( ".previous" ) );
	if ( source.stagePacked( signature ) != OK || fragment.stagePacked( rFragment ) != OK || source.publish() != OK ||
	     fragment.publish() != OK ) {
		rError = !fragment.getError().is_empty() ? fragment.getError() : source.getError();
		( void )fragment.rollback();
		( void )source.rollback();
		return false;
	}
	( void )fragment.commit();
	( void )source.commit();
	return true;
}

bool Terrain3DIncrementalMegaVTCooker::publishFragmentFileCold( const t3dMegaVTAuthoringInputs_t& rInputs,
                                                                const t3dTerrainMegaVTCookTicket_t& rTicket,
                                                                const PackedByteArray& rFragment, const uint64_t nSourceFingerprint )
{
	const String directory = t3dMegaVTCacheDirectory( rInputs );
	if ( directory.is_empty() ) {
		m_lastError = "MegaVT cache directory is unavailable";
		return false;
	}
	const Error mkdirError = DirAccess::make_dir_recursive_absolute( directory );
	if ( mkdirError != OK ) {
		m_lastError = "MegaVT cache directory creation failed";
		return false;
	}
	const String path = directory.path_join( String::num_int64( rTicket.m_nRegionMapIndex ) + ".t3dvt" );
	return t3dMegaVTPublishCachePairCold( path, rFragment, nSourceFingerprint, m_lastError );
}

bool Terrain3DIncrementalMegaVTCooker::restoreCachedRegionCold( const t3dMegaVTAuthoringInputs_t& rInputs, const uint32_t nRegionIndex,
                                                                const uint32_t nSourceGeneration, const uint32_t nDirtyMip0Mask,
                                                                uint64_t& rRetainedBytes )
{
	if ( nRegionIndex >= T3D_REGION_SLOTS || m_bCacheRestoreAttempted[nRegionIndex] ) {
		return false;
	}
	// A miss is checked once per region lifecycle, never polled each frame.
	m_bCacheRestoreAttempted[nRegionIndex] = true;
	if ( nDirtyMip0Mask != 0u || nSourceGeneration == 0u ) {
		return false;
	}
	const String directory = t3dMegaVTCacheDirectory( rInputs );
	if ( directory.is_empty() ) {
		return false;
	}
	const String path = directory.path_join( String::num_int64( nRegionIndex ) + ".t3dvt" );
	if ( !FileAccess::file_exists( path ) || !FileAccess::file_exists( path + String( ".source" ) ) ) {
		return false;
	}
	int32_t x = 0, z = 0;
	Terrain3DTerrainMegaVT::regionIndexToLocation( nRegionIndex, x, z );
	t3dMegaVTColorCapture_t capture{};
	capture.m_nExtent = T3D_REGION_WORLD_SIZE_METERS * m_nTexelsPerMeter;
	if ( !t3dMegaVTCaptureColors( rInputs, Vector2i( x, z ), capture ) ) {
		++m_nCacheRejected;
		return false;
	}
	const uint64_t sourceHash = t3dMegaVTSourceFingerprint( capture );
	const Ref<FileAccess> signatureFile = FileAccess::open( path + String( ".source" ), FileAccess::READ );
	if ( signatureFile.is_null() || signatureFile->get_length() != 16u ||
	     !t3dMegaVTSourceSignatureMatches( signatureFile->get_buffer( 16u ), sourceHash ) ) {
		++m_nCacheRejected;
		return false;
	}
	const Ref<FileAccess> file = FileAccess::open( path, FileAccess::READ );
	const uint64_t nBytes = file.is_valid() ? file->get_length() : 0u;
	if ( nBytes < T3D_VT_PACK_HEADER_BYTES || nBytes > m_nWorkerByteLimit || rRetainedBytes > m_nCpuByteLimit ||
	     nBytes + 64u > m_nCpuByteLimit - rRetainedBytes ) {
		++m_nCacheRejected;
		return false;
	}
	if ( !m_cpuMemory.resize( getRetainedPayloadBytesCold() + nBytes + 64u ) ) {
		++m_nCacheRejected;
		m_lastError = "Total terrain memory budget rejected MegaVT cache fragment";
		return false;
	}
	const PackedByteArray bytes = file->get_buffer( nBytes );
	t3dVTPackBaselineView_t view{};
	if ( ( uint64_t )bytes.size() != nBytes || !t3d_vt_pack_open_baseline_view( bytes.ptr(), nBytes, view ) ||
	     view.m_pHeader->m_nAllocationCount != 1u ||
	     view.m_pAllocationKeys[0].m_nStableKey != t3d_vt_terrain_region_stable_key( nRegionIndex ) ||
	     view.m_pAllocations[0].m_nGeneration != nSourceGeneration || view.m_pAllocations[0].m_nConsumer != T3D_VT_CONSUMER_CLIPMAP ||
	     ( view.m_pAllocations[0].m_nFlags & ( T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP ) ) !=
	         ( T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP ) ) {
		++m_nCacheRejected;
		trimCpuReservationCold();
		return false;
	}
	const t3dVTChannelAllocation_t& albedo = view.m_pChannelAllocations[T3D_VT_LOGICAL_CHANNEL_ALBEDO];
	const t3dVTChannelAllocation_t& pbr = view.m_pChannelAllocations[T3D_VT_LOGICAL_CHANNEL_PBR];
	const uint32_t nPages = T3D_REGION_WORLD_SIZE_METERS * m_nTexelsPerMeter / T3D_VT_PAGE_INTERIOR;
	if ( albedo.m_nMip0WidthPages != nPages || albedo.m_nMip0HeightPages != nPages || pbr.m_nMip0WidthPages != nPages / 2u ||
	     pbr.m_nMip0HeightPages != nPages / 2u ) {
		++m_nCacheRejected;
		trimCpuReservationCold();
		return false;
	}
	m_fragments[nRegionIndex] = bytes;
	rRetainedBytes += nBytes + 64u;
	++m_nCacheRestored;
	return true;
}

void Terrain3DIncrementalMegaVTCooker::restoreCachedRegionsCold( const t3dMegaVTAuthoringInputs_t& rInputs, const uint32_t nTexelsPerMeter )
{
	Terrain3DMemoryAccountScope authoring_scope( m_authoringMemoryAccount.get() );
	if ( nTexelsPerMeter != T3D_TERRAIN_MEGAVT_MIN_TEXELS_PER_METER && nTexelsPerMeter != T3D_TERRAIN_MEGAVT_MAX_TEXELS_PER_METER ) {
		return;
	}
	m_nCpuByteLimit = rInputs.m_nCpuByteLimit;
	m_nTexelsPerMeter = nTexelsPerMeter;
	const uint64_t nRestoredBefore = m_nCacheRestored;
	const uint64_t nRejectedBefore = m_nCacheRejected;
	uint64_t nRetainedBytes = getRetainedCpuBytesCold();
	const TypedArray<Vector2i> locations = rInputs.m_source.getRegionLocations();
	for ( int64_t i = 0; i < locations.size(); ++i ) {
		const Vector2i location = locations[i];
		uint32_t index = 0u;
		if ( !Terrain3DTerrainMegaVT::locationToRegionIndex( location.x, location.y, index ) || m_bCacheRestoreAttempted[index] ||
		     !rInputs.m_source.ensureCpuRegionResident( location ) ) {
			continue;
		}
		const Ref<Terrain3DRegion> region = rInputs.m_source.getRegion( location );
		if ( region.is_null() || region->isDeleted() ) {
			continue;
		}
		const uint32_t nSourceGeneration = ( uint32_t )region->getMegaVTSourceGeneration();
		const uint32_t nDirty = ( uint32_t )region->getMegaVTDirtyMip0Mask();
		if ( !restoreCachedRegionCold( rInputs, index, nSourceGeneration, nDirty, nRetainedBytes ) && nDirty == 0u &&
		     nSourceGeneration != 0u && !m_bStartupRecoveryRegion[index] ) {
			m_bStartupRecoveryRegion[index] = true;
			m_nStartupRecoverySourceGeneration[index] = nSourceGeneration;
			++m_nRecoveryOutstanding;
			++m_nRecoveryTotal;
		}
	}
	UtilityFunctions::print( "[Terrain3D][Startup][MegaVT] CACHE RESTORE restored=", m_nCacheRestored - nRestoredBefore,
	                         " rejected=", m_nCacheRejected - nRejectedBefore );
}

void Terrain3DIncrementalMegaVTCooker::requestStagedPublicationCold( Terrain3DMegaVTCookHost& rHost )
{
	if ( m_nStagedCount == 0u || m_bBatchRequested ) {
		return;
	}
	m_bBatchRequested = true;
	m_nPendingFingerprint = 0u;
	m_nPublicationDeadlineUsec = t3dSteadyClockUsec() + 30000000u;
	rHost.requestMegaVTPackageRecomposeCold();
}

void Terrain3DIncrementalMegaVTCooker::discardStagedBatchCold( Terrain3DTerrainMegaVT& rMegaVT, Terrain3DMegaVTCookHost& rHost,
                                                               const String& rError, const bool bTransient )
{
	const uint64_t nNow = t3dSteadyClockUsec();
	for ( uint32_t i = 0u; i < m_nStagedCount; ++i ) {
		const t3dTerrainMegaVTCookTicket_t& ticket = m_stagedRegions[i].m_ticket;
		const uint32_t index = ticket.m_nRegionMapIndex;
		if ( index < T3D_REGION_SLOTS && ticket.m_nClaimId != 0u ) {
			m_nRetrySourceGeneration[index] = ticket.m_nGeneration;
			if ( bTransient && m_nRetryAttempts[index] < 6u ) {
				const uint32_t nAttempt = ++m_nRetryAttempts[index];
				m_nRetryAfterUsec[index] = nNow + ( 250000ULL << ( nAttempt - 1u ) );
				m_nNextRetryUsec = m_nNextRetryUsec == 0u ? m_nRetryAfterUsec[index] : MIN( m_nNextRetryUsec, m_nRetryAfterUsec[index] );
			} else if ( !rError.is_empty() ) {
				m_nFailedSourceGenerations[index] = ticket.m_nGeneration;
			}
		}
		( void )rMegaVT.cancelIncrementalCookHot( ticket );
		for ( size_t j = 0u; j < m_pixelCache.size(); ++j ) {
			if ( m_pixelCache[j].m_nRegionIndex == index ) {
				m_pixelCache.erase( m_pixelCache.begin() + j );
				break;
			}
		}
		m_stagedRegions[i] = t3dMegaVTStagedRegion_t{};
	}
	m_nStagedCount = 0u;
	m_bBatchRequested = false;
	m_nPendingFingerprint = 0u;
	m_nBatchFirstUsec = 0u;
	m_nIdlePublicationGeneration = UINT64_MAX;
	trimCpuReservationCold();
	if ( !rError.is_empty() ) {
		++m_nFailures;
		m_lastError = rError;
	} else {
		++m_nRejected;
	}
	// Roll back any unpublished candidate package, including stale staged regions.
	rHost.requestMegaVTPackageRecomposeCold();
}

void Terrain3DIncrementalMegaVTCooker::finishPublicationCold( const t3dMegaVTAuthoringInputs_t& rInputs, Terrain3DTerrainMegaVT& rMegaVT,
                                                              Terrain3DMegaVTCookHost& rHost )
{
	if ( !m_bBatchRequested || m_nStagedCount == 0u ) {
		return;
	}
	for ( uint32_t i = 0u; i < m_nStagedCount; ++i ) {
		if ( !rMegaVT.validateIncrementalCookCompletionHot( m_stagedRegions[i].m_ticket ) ) {
			discardStagedBatchCold( rMegaVT, rHost, "MegaVT batch contained an obsolete source generation", true );
			return;
		}
	}
	const uint64_t nNow = t3dSteadyClockUsec();
	if ( m_nPendingFingerprint == 0u || rHost.publishedMegaVTGpuFingerprintCold() != m_nPendingFingerprint ) {
		if ( nNow >= m_nPublicationDeadlineUsec ) {
			discardStagedBatchCold( rMegaVT, rHost, "MegaVT batched GPU publication failed or timed out", true );
		}
		return;
	}
	const String directory = t3dMegaVTCacheDirectory( rInputs );
	if ( directory.is_empty() || DirAccess::make_dir_recursive_absolute( directory ) != OK ) {
		discardStagedBatchCold( rMegaVT, rHost, "MegaVT batch cache directory unavailable", true );
		return;
	}
	// Stage every artifact first. If any publish fails, reverse the complete
	// transaction before accepting a dirty-generation ticket.
	eastl::vector<Terrain3DFilePublication, Terrain3DEastlAllocator> publications;
	if ( !t3dContainerReserveCold( publications, m_nStagedCount * 2u ) ) {
		discardStagedBatchCold( rMegaVT, rHost, "MegaVT batch publication metadata allocation failed", true );
		return;
	}
	String error;
	bool success = true;
	for ( uint32_t i = 0u; i < m_nStagedCount && success; ++i ) {
		const t3dMegaVTStagedRegion_t& region = m_stagedRegions[i];
		const String path = directory.path_join( String::num_int64( region.m_ticket.m_nRegionMapIndex ) + ".t3dvt" );
		const PackedByteArray signature = t3dMegaVTSourceSignatureBytes( region.m_nSourceFingerprint );
		if ( signature.size() != 16u ) {
			error = "MegaVT region source signature allocation failed";
			success = false;
			break;
		}
		publications.emplace_back( path + String( ".source" ), path + String( ".source.staged" ), path + String( ".source.previous" ) );
		if ( publications.back().stagePacked( signature ) != OK ) {
			error = publications.back().getError();
			success = false;
			break;
		}
		publications.emplace_back( path, path + String( ".staged" ), path + String( ".previous" ) );
		if ( publications.back().stagePacked( region.m_fragment ) != OK ) {
			error = publications.back().getError();
			success = false;
			break;
		}
	}
	for ( size_t i = 0u; i < publications.size() && success; ++i ) {
		if ( publications[i].publish() != OK ) {
			error = publications[i].getError();
			success = false;
		}
	}
	if ( !success ) {
		for ( size_t i = publications.size(); i > 0u; --i ) {
			if ( publications[i - 1u].rollback() != OK ) {
				error += String( "; rollback: " ) + publications[i - 1u].getError();
			}
		}
		discardStagedBatchCold( rMegaVT, rHost, error, true );
		return;
	}
	// No intervening editor update can mutate these owner-thread claims here.
	const bool bCompletingStartupRecovery = m_nRecoveryOutstanding != 0u;
	for ( uint32_t i = 0u; i < m_nStagedCount; ++i ) {
		const t3dMegaVTStagedRegion_t& region = m_stagedRegions[i];
		const uint32_t index = region.m_ticket.m_nRegionMapIndex;
		if ( !rMegaVT.acceptIncrementalCookCompletionHot( region.m_ticket ) ) {
			// Last-chance generation validation. Files can still be restored;
			// subsequent dirty claims must regenerate the current package.
			for ( size_t j = publications.size(); j > 0u; --j ) {
				( void )publications[j - 1u].rollback();
			}
			discardStagedBatchCold( rMegaVT, rHost, "MegaVT batch acceptance rejected a changed source", true );
			return;
		}
		m_fragments[index] = region.m_fragment;
		m_nRetryAttempts[index] = 0u;
		m_nRetryAfterUsec[index] = 0u;
		m_nRetrySourceGeneration[index] = 0u;
		if ( m_bStartupRecoveryRegion[index] ) {
			m_bStartupRecoveryRegion[index] = false;
			if ( m_nRecoveryOutstanding != 0u ) {
				--m_nRecoveryOutstanding;
			}
		}
		++m_nAccepted;
	}
	for ( size_t i = 0u; i < publications.size(); ++i ) {
		( void )publications[i].commit();
	}
	for ( uint32_t i = 0u; i < m_nStagedCount; ++i ) {
		m_stagedRegions[i] = t3dMegaVTStagedRegion_t{};
	}
	m_nStagedCount = 0u;
	m_nBatchFirstUsec = 0u;
	m_bBatchRequested = false;
	m_nPendingFingerprint = 0u;
	m_lastError = String();
	++m_nBatchedPublications;
	trimCpuReservationCold();
	rHost.persistMegaVTEditorStateCold();
	if ( bCompletingStartupRecovery && m_nRecoveryOutstanding == 0u ) {
		UtilityFunctions::print( "[Terrain3D][Startup][MegaVT] RECOVERY COMPLETE regions=", m_nRecoveryTotal,
		                         " batched_publications=", m_nBatchedPublications );
	}
}

void Terrain3DIncrementalMegaVTCooker::queueRegionCold( const t3dMegaVTAuthoringInputs_t& rInputs, Terrain3DTerrainMegaVT& rMegaVT,
                                                        Terrain3DMegaVTCookHost& rHost )
{
	if ( m_worker.joinable() || m_bBatchRequested || m_nStagedCount >= T3D_MEGAVT_PUBLICATION_BATCH_REGION_LIMIT || m_bForceFailed ||
	     ( m_bForceActive && m_nForceCookedCount == m_nForceRegionCount ) ) {
		return;
	}
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		const uint32_t index = m_nNextRegion++ % T3D_REGION_SLOTS;
		if ( m_bForceActive && m_forceTickets[index].m_nClaimId != 0u ) {
			continue;
		}
		bool bAlreadyStaged = false;
		for ( uint32_t j = 0u; j < m_nStagedCount; ++j ) {
			if ( m_stagedRegions[j].m_ticket.m_nRegionMapIndex == index ) {
				bAlreadyStaged = true;
				break;
			}
		}
		if ( bAlreadyStaged ) {
			continue;
		}
		const t3dTerrainMegaVTRegion_t *const region = rMegaVT.getRegion( index );
		if ( region != NULL && m_nRetrySourceGeneration[index] != 0u && m_nRetrySourceGeneration[index] != region->m_nGeneration ) {
			m_nRetrySourceGeneration[index] = 0u;
			m_nRetryAfterUsec[index] = 0u;
			m_nRetryAttempts[index] = 0u;
		}
		if ( m_nRetryAfterUsec[index] != 0u ) {
			if ( t3dSteadyClockUsec() < m_nRetryAfterUsec[index] ) {
				continue;
			}
			m_nRetryAfterUsec[index] = 0u;
		}
		if ( region != NULL && m_nFailedSourceGenerations[index] != 0u && m_nFailedSourceGenerations[index] == region->m_nGeneration ) {
			continue;
		}
		t3dTerrainMegaVTIncrementalCookRequest_t request{};
		if ( !rMegaVT.claimIncrementalCookHot( index, request, m_ticket ) ) {
			continue;
		}
		if ( !captureRegionCold( rInputs, request ) ) {
			retireClaimCold( rMegaVT, rHost, "MegaVT color source capture failed; retry scheduled", true );
			return;
		}
		m_done.store( false, T3D_MEMORY_ORDER_RELEASE );
		m_cancel.store( false, T3D_MEMORY_ORDER_RELEASE );
		m_bWorkerSucceeded = false;
		const uint64_t nRetained = getRetainedCpuBytesCold();
		if ( nRetained > m_nCpuByteLimit || m_nWorkerByteLimit > m_nCpuByteLimit - nRetained ) {
			retireClaimCold( rMegaVT, rHost, "MegaVT CPU budget rejected worker admission; retry scheduled", true );
			return;
		}
		m_nPeakCpuByteEstimate = MAX( m_nPeakCpuByteEstimate, nRetained + m_nWorkerByteLimit );
		m_worker = Terrain3DThread( &Terrain3DIncrementalMegaVTCooker::workerEntry, this );
		trimCpuReservationCold();
		++m_nSubmitted;
		return;
	}
	// Only a completed batch requests expensive package composition / GPU work.
	if ( m_nStagedCount != 0u ) {
		requestStagedPublicationCold( rHost );
	}
	// No new texture work: remain idle until the source publication changes.
	m_nIdlePublicationGeneration = rMegaVT.getPublicationGeneration();
	m_nNextRetryUsec = 0u;
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		const uint64_t retry = m_nRetryAfterUsec[i];
		if ( retry != 0u && ( m_nNextRetryUsec == 0u || retry < m_nNextRetryUsec ) ) {
			m_nNextRetryUsec = retry;
		}
	}
}

void Terrain3DIncrementalMegaVTCooker::updateCold( const t3dMegaVTAuthoringInputs_t& rInputs, Terrain3DTerrainMegaVT& rMegaVT,
                                                   Terrain3DMegaVTCookHost& rHost )
{
	Terrain3DMemoryAccountScope authoring_scope( m_authoringMemoryAccount.get() );
	m_nCpuByteLimit = rInputs.m_nCpuByteLimit;
	if ( !rMegaVT.isReady() ) {
		return;
	}
	if ( m_nObservedTopologyGeneration != rMegaVT.getTopologyGeneration() ) {
		m_nObservedTopologyGeneration = rMegaVT.getTopologyGeneration();
		retryDirtyRegionsCold();
	}
	if ( m_bForceRequested ) {
		// Drain the old worker safely. Its unpublished candidate can be retired:
		// the force batch replaces it while retaining the last usable fragments.
		finishWorkerCold( rInputs, rMegaVT, rHost );
		if ( m_worker.joinable() ) {
			return;
		}
		if ( m_nStagedCount != 0u ) {
			discardStagedBatchCold( rMegaVT, rHost, String(), false );
		}

		m_bForceRequested = false;
		m_bForceActive = true;
		m_bForceFailed = false;
		m_nForceCookedCount = 0u;
		if ( m_bForceAllRequested ) {
			m_nForceRegionCount = rMegaVT.requestForceRecookAllRegionsCold();
		} else {
			// Dirty source generations survive editor restart. Reuse clean cached
			// fragments, and mark missing fragments as work only on manual request.
			m_nForceRegionCount = 0u;
			for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
				const auto *region = rMegaVT.getRegion( i );
				if ( region == NULL || ( region->m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) == 0u ) {
					continue;
				}
				uint32_t generation = 0u, mask = 0u;
				( void )rMegaVT.getPersistentEditorState( i, generation, mask );
				if ( mask == 0u && m_fragments[i].is_empty() ) {
					const uint32_t pages = ( uint32_t )region->m_nMip0WidthPages * region->m_nMip0HeightPages;
					mask = pages >= 32u ? UINT32_MAX : ( 1u << pages ) - 1u;
					( void )rMegaVT.importPersistentEditorStateHot( i, generation, mask );
				}
				if ( mask != 0u ) {
					++m_nForceRegionCount;
				}
			}
		}
		for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
			const auto *region = rMegaVT.getRegion( i );
			m_forceSourceGenerations[i] = region != NULL ? region->m_nGeneration : 0u;
		}
		retryDirtyRegionsCold();
		for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
			m_bCacheRestoreAttempted[i] = true;
		}
		rHost.persistMegaVTEditorStateCold();
	}
	if ( m_bForceActive && ( m_bForceFailed || m_nForceCookedCount == m_nForceRegionCount ) ) {
		return;
	}
	if ( !m_worker.joinable() && m_nStagedCount == 0u && m_nTexelsPerMeter == rMegaVT.getTexelsPerMeter() &&
	     m_nIdlePublicationGeneration == rMegaVT.getPublicationGeneration() &&
	     ( m_nNextRetryUsec == 0u || t3dSteadyClockUsec() < m_nNextRetryUsec ) ) {
		return;
	}
	if ( m_nTexelsPerMeter != rMegaVT.getTexelsPerMeter() ) {
		if ( m_nStagedCount != 0u ) {
			discardStagedBatchCold( rMegaVT, rHost, String(), false );
		}
		memset( m_nFailedSourceGenerations, 0, sizeof( m_nFailedSourceGenerations ) );
		m_nTexelsPerMeter = rMegaVT.getTexelsPerMeter();
		for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
			m_fragments[i].clear();
		}
		memset( m_bCacheRestoreAttempted, 0, sizeof( m_bCacheRestoreAttempted ) );
		m_pixelCache.clear();
		rHost.requestMegaVTPackageRecomposeCold();
	}
	bool bRecompose = false, bDirty = false;
	uint64_t nRestoreRetainedBytes = 0u;
	bool bRestoreBudgetReady = false;
	for ( uint32_t index = 0u; index < T3D_REGION_SLOTS; ++index ) {
		const t3dTerrainMegaVTRegion_t *const region = rMegaVT.getRegion( index );
		if ( region == NULL || ( region->m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) == 0u ) {
			if ( !m_fragments[index].is_empty() ) {
				m_fragments[index].clear();
				bRecompose = true;
			}
			m_bCacheRestoreAttempted[index] = false;
			if ( m_bStartupRecoveryRegion[index] ) {
				m_bStartupRecoveryRegion[index] = false;
				if ( m_nRecoveryOutstanding != 0u ) {
					--m_nRecoveryOutstanding;
				}
			}
			m_nFailedSourceGenerations[index] = 0u;
			m_nRetryAfterUsec[index] = 0u;
			m_nRetryAttempts[index] = 0u;
			m_nRetrySourceGeneration[index] = 0u;
			continue;
		}
		uint32_t generation = 0u, mask = 0u;
		( void )rMegaVT.getPersistentEditorState( index, generation, mask );
		if ( m_bStartupRecoveryRegion[index] && generation != m_nStartupRecoverySourceGeneration[index] ) {
			m_bStartupRecoveryRegion[index] = false;
			if ( m_nRecoveryOutstanding != 0u ) {
				--m_nRecoveryOutstanding;
			}
		}
		if ( mask == 0u && m_fragments[index].is_empty() && m_ticket.m_nClaimId == 0u ) {
			if ( !m_bCacheRestoreAttempted[index] ) {
				if ( !bRestoreBudgetReady ) {
					nRestoreRetainedBytes = getRetainedCpuBytesCold();
					bRestoreBudgetReady = true;
				}
				if ( restoreCachedRegionCold( rInputs, index, generation, mask, nRestoreRetainedBytes ) ) {
					bRecompose = true;
					continue;
				}
			}
			// Missing derived data is a recoverable cache miss, tracked separately
			// from authored edits and kept out of the persisted dirty-state file.
			if ( !m_bStartupRecoveryRegion[index] ) {
				m_bStartupRecoveryRegion[index] = true;
				m_nStartupRecoverySourceGeneration[index] = generation;
				++m_nRecoveryOutstanding;
				++m_nRecoveryTotal;
			}
			const uint32_t nMip0Pages = ( uint32_t )region->m_nMip0WidthPages * region->m_nMip0HeightPages;
			const uint32_t nDirtyMask = nMip0Pages >= 32u ? UINT32_MAX : ( 1u << nMip0Pages ) - 1u;
			bDirty |= rMegaVT.importPersistentEditorStateHot( index, generation, nDirtyMask );
		}
	}
	// Missing cache-derived dirty masks are transient until a successful
	// batch commits; do not rewrite the source's persisted editor dirty state here.
	( void )bDirty;
	if ( bRecompose ) {
		rHost.requestMegaVTPackageRecomposeCold();
	}
	finishWorkerCold( rInputs, rMegaVT, rHost );
	finishPublicationCold( rInputs, rMegaVT, rHost );
	trimCpuReservationCold();
	// Interactive edits should become visible after a bounded delay; startup
	// recovery fills batches, avoiding a publication for every cache miss.
	if ( m_nStagedCount != 0u && !m_worker.joinable() && !m_bBatchRequested && m_nRecoveryOutstanding == 0u &&
	     t3dSteadyClockUsec() - m_nBatchFirstUsec >= 500000u ) {
		requestStagedPublicationCold( rHost );
	}
	queueRegionCold( rInputs, rMegaVT, rHost );
}

void Terrain3DIncrementalMegaVTCooker::retryDirtyRegionsCold( void )
{
	memset( m_nFailedSourceGenerations, 0, sizeof( m_nFailedSourceGenerations ) );
	memset( m_nRetryAfterUsec, 0, sizeof( m_nRetryAfterUsec ) );
	memset( m_nRetryAttempts, 0, sizeof( m_nRetryAttempts ) );
	memset( m_nRetrySourceGeneration, 0, sizeof( m_nRetrySourceGeneration ) );
	m_nNextRetryUsec = 0u;
	m_nIdlePublicationGeneration = UINT64_MAX;
}

void Terrain3DIncrementalMegaVTCooker::shutdownCold( Terrain3DTerrainMegaVT& rMegaVT )
{
	Terrain3DMemoryAccountScope authoring_scope( m_authoringMemoryAccount.get() );
	m_cancel.store( true, T3D_MEMORY_ORDER_RELEASE );
	if ( m_worker.joinable() ) {
		m_worker.join();
	}
	( void )rMegaVT.cancelIncrementalCookHot( m_ticket );
	m_ticket = t3dTerrainMegaVTCookTicket_t{};
	m_capture = t3dMegaVTColorCapture_t{};
	m_pixels = t3dMegaVTRegionPixels_t{};
	m_workerFragment.clear();
	for ( uint32_t i = 0u; i < m_nStagedCount; ++i ) {
		( void )rMegaVT.cancelIncrementalCookHot( m_stagedRegions[i].m_ticket );
		m_stagedRegions[i] = t3dMegaVTStagedRegion_t{};
	}
	m_nStagedCount = 0u;
	m_bBatchRequested = false;
	m_nBatchFirstUsec = 0u;
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		m_fragments[i].clear();
	}
	memset( m_bCacheRestoreAttempted, 0, sizeof( m_bCacheRestoreAttempted ) );
	memset( m_bStartupRecoveryRegion, 0, sizeof( m_bStartupRecoveryRegion ) );
	m_nRecoveryOutstanding = 0u;
	m_nRecoveryTotal = 0u;
	m_nTexelsPerMeter = 0u;
	m_pixelCache.clear();
	m_nPendingFingerprint = 0u;
	m_nIdlePublicationGeneration = UINT64_MAX;
	m_nObservedTopologyGeneration = UINT64_MAX;
	memset( m_nRetrySourceGeneration, 0, sizeof( m_nRetrySourceGeneration ) );
	memset( m_nRetryAfterUsec, 0, sizeof( m_nRetryAfterUsec ) );
	memset( m_nRetryAttempts, 0, sizeof( m_nRetryAttempts ) );
	m_nNextRetryUsec = 0u;
	memset( m_nFailedSourceGenerations, 0, sizeof( m_nFailedSourceGenerations ) );
	m_cpuMemory.reset();
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		( void )rMegaVT.cancelIncrementalCookHot( m_forceTickets[i] );
		m_forceTickets[i] = t3dTerrainMegaVTCookTicket_t{};
		m_forceFragments[i].clear();
	}
	m_bForceActive = false;
	m_bForceRequested = false;
	m_bForceFailed = false;
}

bool Terrain3DIncrementalMegaVTCooker::composePackageCold( PackedByteArray& rPackage, uint32_t& rGeneration, uint64_t& rFingerprint,
                                                           Terrain3DSharedMemoryReservation *const pPackageMemory,
                                                           const PackedByteArray *const pApprovedComposedFragment )
{
	Terrain3DMemoryAccountScope authoring_scope( m_authoringMemoryAccount.get() );
	eastl::vector<PackedByteArray, Terrain3DEastlAllocator> packages;
	if ( !t3dContainerReserveCold( packages, T3D_REGION_SLOTS + 2u ) ) {
		m_lastError = "MegaVT package list allocation failed";
		return false;
	}
	if ( !rPackage.is_empty() ) {
		packages.push_back( rPackage );
	}
	uint64_t nSignature = rGeneration;
	for ( uint32_t index = 0u; index < T3D_REGION_SLOTS; ++index ) {
		const PackedByteArray& fragment =
		    isForceRecookReadyCold() && !m_forceFragments[index].is_empty() ? m_forceFragments[index] : m_fragments[index];
		const PackedByteArray *pFragment = &fragment;
		for ( uint32_t j = 0u; j < m_nStagedCount; ++j ) {
			if ( m_stagedRegions[j].m_ticket.m_nRegionMapIndex == index ) {
				pFragment = &m_stagedRegions[j].m_fragment;
				break;
			}
		}
		const PackedByteArray& selectedFragment = *pFragment;
		if ( selectedFragment.is_empty() ) {
			continue;
		}
		packages.push_back( selectedFragment );
		nSignature ^= t3dHashPublicationBytes( selectedFragment.ptr(), selectedFragment.size() );
	}
	// A canonically approved composed fragment has no private page allocator.
	// Append it to the same V1 package merge so stable-key conflicts, physical
	// format families, tail pages and the global budget are validated once.
	if ( pApprovedComposedFragment != NULL && !pApprovedComposedFragment->is_empty() ) {
		packages.push_back( *pApprovedComposedFragment );
		nSignature ^= t3dHashPublicationBytes( pApprovedComposedFragment->ptr(), pApprovedComposedFragment->size() );
	}
	if ( packages.size() <= ( rPackage.is_empty() ? 0u : 1u ) ) {
		return true;
	}
	const uint32_t nGeneration = t3dNextGeneration( ( uint32_t )( nSignature ^ ( nSignature >> 32u ) ) );
	PackedByteArray merged;
	Terrain3DSharedMemoryReservation mergedMemory;
	const uint64_t nRetained = getRetainedCpuBytesCold() + ( uint64_t )rPackage.size() +
	                           ( pApprovedComposedFragment != NULL ? ( uint64_t )pApprovedComposedFragment->size() : 0u );
	m_nCompositionWorkingBytes = 0u;
	m_nCompositionWorkingLimit = nRetained < m_nCpuByteLimit ? m_nCpuByteLimit - nRetained : 0u;
	Terrain3DMemoryAccount *const pAccount = g_t3dMemoryAccount;
	if ( pAccount != NULL ) {
		// The independent authoring account admits composition without consuming
		// runtime residency headroom. Working memory is separately reserved.
		const auto stats = pAccount->stats();
		m_nCompositionWorkingLimit =
		    MIN( t3dMemoryCompositionLimit( pAccount ), stats.committed < stats.limit ? stats.limit - stats.committed : 0u );
	}
	if ( pAccount == NULL && nRetained >= m_nCpuByteLimit ) {
		m_lastError = "MegaVT retained CPU bytes " + String::num_uint64( nRetained ) + " exceed composition ceiling " +
		              String::num_uint64( m_nCpuByteLimit );
		return false;
	}
	t3dVTMergeStatistics_t statistics;
	const uint64_t nStart = t3dSteadyClockUsec();
	const bool bMerged = t3d_vt_godot_merge_packages( packages.data(), ( uint32_t )packages.size(), nGeneration, merged,
	                                                  m_nCompositionWorkingLimit, &statistics, &mergedMemory );
	m_nMergeUsec = t3dSteadyClockUsec() - nStart;
	m_nPayloadComparisons = statistics.m_nPayloadComparisons;
	m_nCompositionWorkingBytes = statistics.m_nWorkingBytes;
	m_nPeakCpuByteEstimate = MAX( m_nPeakCpuByteEstimate, nRetained + statistics.m_nWorkingBytes );
	if ( !bMerged ) {
		if ( statistics.m_bWorkingLimitExceeded ) {
			if ( pAccount != NULL ) {
				pAccount->recordDenial();
			}
			m_lastError = "MegaVT composition requires working bytes " + String::num_uint64( statistics.m_nWorkingBytes ) + "; available " +
			              String::num_uint64( m_nCompositionWorkingLimit );
		} else {
			m_lastError = "MegaVT package composition failed (invalid input or shared memory admission denied)";
		}
		return false;
	}
	m_lastError = String();
	rPackage = merged;
	rGeneration = nGeneration;
	if ( pPackageMemory != NULL ) {
		*pPackageMemory = mergedMemory;
	}
	rFingerprint = t3dHashPublicationBytes( merged.ptr(), merged.size() );
	if ( rFingerprint == 0u ) {
		rFingerprint = 1u;
	}
	if ( m_bBatchRequested && m_nStagedCount != 0u ) {
		m_nPendingFingerprint = rFingerprint;
	}
	return true;
}

struct t3dMegaVTRetainedByteCounter_t
{
	const uint8_t *m_seen[T3D_REGION_SLOTS * 2u + T3D_MEGAVT_PUBLICATION_BATCH_REGION_LIMIT + 64u] = {};
	size_t m_nSeen = 0u;
	uint64_t m_nBytes = 0u;
	void add( const PackedByteArray& rBytes )
	{
		if ( rBytes.is_empty() ) {
			return;
		}
		const uint8_t *const pBytes = rBytes.ptr();
		for ( size_t i = 0u; i < m_nSeen; ++i ) {
			if ( m_seen[i] == pBytes ) {
				return;
			}
		}
		m_seen[m_nSeen++] = pBytes;
		// Godot exposes size, not backing capacity. Include allocation/header
		// slack and report this as an estimate; aliases count only once.
		m_nBytes += ( uint64_t )rBytes.size() + 64u;
	}
};

size_t Terrain3DIncrementalMegaVTCooker::getRetainedPayloadBytesCold( void ) const
{
	t3dMegaVTRetainedByteCounter_t counter;
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		counter.add( m_fragments[i] );
		counter.add( m_forceFragments[i] );
	}
	for ( size_t i = 0u; i < m_pixelCache.size(); ++i ) {
		counter.add( m_pixelCache[i].m_albedo );
		counter.add( m_pixelCache[i].m_pbr );
	}
	// Capture bytes are immutable until join. Mutable worker storage is covered
	// by its admitted reservation rather than read concurrently for diagnostics.
	for ( uint32_t i = 0u; i < 9u; ++i ) {
		counter.add( m_capture.m_neighbors[i].m_bytes );
	}
	if ( m_worker.joinable() ) {
		counter.m_nBytes += m_nWorkerByteLimit;
	} else {
		counter.add( m_pixels.m_albedo );
		counter.add( m_pixels.m_pbr );
		counter.add( m_workerFragment );
	}
	for ( uint32_t i = 0u; i < m_nStagedCount; ++i ) {
		counter.add( m_stagedRegions[i].m_fragment );
	}
	return counter.m_nBytes > SIZE_MAX ? SIZE_MAX : ( size_t )counter.m_nBytes;
}

size_t Terrain3DIncrementalMegaVTCooker::getRetainedCpuBytesCold( void ) const
{
	return getRetainedPayloadBytesCold() + sizeof( *this ) + m_pixelCache.capacity() * sizeof( t3dMegaVTRegionPixels_t );
}

void Terrain3DIncrementalMegaVTCooker::trimCpuReservationCold( void )
{
	// Call only after releasing bytes or after a previously admitted transition.
	// A live worker keeps its bounded allowance until it has joined.
	( void )m_cpuMemory.resize( getRetainedPayloadBytesCold() );
}

Dictionary Terrain3DIncrementalMegaVTCooker::getDiagnosticsCold( void ) const
{
	Dictionary result;
	result["worker_active"] = m_worker.joinable();
	result["publication_pending"] = m_bBatchRequested;
	result["staged_regions"] = ( int64_t )m_nStagedCount;
	result["batched_publications"] = ( int64_t )m_nBatchedPublications;
	result["unchanged_region_fragments"] = ( int64_t )m_nUnchangedFragments;
	result["startup_recovery_pending_regions"] = ( int64_t )m_nRecoveryOutstanding;
	result["startup_recovery_total_regions"] = ( int64_t )m_nRecoveryTotal;
	result["submitted"] = ( int64_t )m_nSubmitted;
	result["accepted"] = ( int64_t )m_nAccepted;
	result["stale_rejected"] = ( int64_t )m_nRejected;
	result["failures"] = ( int64_t )m_nFailures;
	result["last_error"] = m_lastError;
	result["pixel_cache_regions"] = ( int64_t )m_pixelCache.size();
	result["cache_restored_regions"] = ( int64_t )m_nCacheRestored;
	result["cache_rejected_regions"] = ( int64_t )m_nCacheRejected;
	uint32_t retrying = 0u;
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		retrying += m_nRetryAfterUsec[i] != 0u ? 1u : 0u;
	}
	result["transient_retry_regions"] = ( int64_t )retrying;
	result["retained_cpu_bytes"] = ( int64_t )getRetainedCpuBytesCold();
	result["reserved_cpu_bytes"] = ( int64_t )m_cpuMemory.bytes();
	if ( m_authoringMemoryAccount.get() != NULL ) {
		const auto stats = m_authoringMemoryAccount.get()->stats();
		result["authoring_account_committed_bytes"] = ( int64_t )stats.committed;
		result["authoring_account_peak_bytes"] = ( int64_t )stats.peak;
		result["authoring_account_denied_allocations"] = ( int64_t )stats.denied;
	}
	result["cpu_byte_limit"] = ( int64_t )m_nCpuByteLimit;
	result["peak_cpu_byte_estimate"] = ( int64_t )m_nPeakCpuByteEstimate;
	result["composition_usec"] = ( int64_t )m_nMergeUsec;
	result["payload_comparisons"] = ( int64_t )m_nPayloadComparisons;
	result["composition_required_working_bytes"] = ( int64_t )m_nCompositionWorkingBytes;
	result["composition_working_byte_limit"] = ( int64_t )m_nCompositionWorkingLimit;
	return result;
}

bool Terrain3DIncrementalMegaVTCooker::appendRuntimeRegionsCold( const t3dMegaVTAuthoringInputs_t& rInputs, PackedByteArray& rPackage,
                                                                 uint32_t& rGeneration, String& rError )
{
	// Synchronous export is independent of the runtime residency account.
	s_exportCacheCounters = t3dMegaVTExportCacheCounters_t{};
	Terrain3DMemoryAccountOwner export_account( rInputs.m_nCpuByteLimit );
	if ( export_account.get() == NULL ) {
		rError = "MegaVT export authoring account allocation failed";
		return false;
	}
	Terrain3DMemoryAccountScope authoring_scope( g_t3dMemoryAccount != NULL ? g_t3dMemoryAccount : export_account.get() );
	// The CPU limit is a ceiling. Charge retained Godot buffers by their actual
	// size; native cook/merge scratch is charged separately by its allocator.
	Terrain3DMemoryReservation buffer_memory;
	Terrain3DRegionSource *const pData = &rInputs.m_source;
	if ( pData == NULL ) {
		rError = "Terrain MegaVT export has no data source";
		return false;
	}
	const TypedArray<Vector2i> locations = pData->getRegionLocations();
	eastl::vector<PackedByteArray, Terrain3DEastlAllocator> packages;
	if ( !t3dContainerReserveCold( packages, ( size_t )locations.size() + 1u ) ) {
		rError = "Terrain MegaVT export package list allocation failed";
		return false;
	}
	if ( !rPackage.is_empty() ) {
		packages.push_back( rPackage );
	}
	t3dAtomicBool_t cancel{ false };
	uint64_t nSignature = rGeneration;
	uint64_t nRetainedBytes = rPackage.is_empty() ? 0u : ( uint64_t )rPackage.size() + 64u;
	if ( !locations.is_empty() && !buffer_memory.resize( nRetainedBytes ) ) {
		rError = "Total terrain memory budget rejected MegaVT export inputs";
		return false;
	}
	for ( int64_t i = 0; i < locations.size(); ++i ) {
		const Vector2i location = locations[i];
		t3dMegaVTColorCapture_t capture{};
		if ( !Terrain3DTerrainMegaVT::locationToRegionIndex( location.x, location.y, capture.m_nRegionIndex ) ) {
			rError = "Terrain MegaVT export location is outside the world";
			return false;
		}
		uint16_t pagesW = 0u, pagesH = 0u, mips = 0u;
		uint32_t pageCount = 0u;
		if ( !Terrain3DTerrainMegaVT::computeFootprint( rInputs.m_nTexelsPerMeter, pagesW, pagesH, mips, pageCount ) ) {
			rError = "Invalid configured MegaVT export texel density";
			return false;
		}
		capture.m_nExtent = T3D_REGION_WORLD_SIZE_METERS * rInputs.m_nTexelsPerMeter;
		capture.m_nGeneration = 1u;
		capture.m_nDirtyMip0Mask = T3D_TERRAIN_MEGAVT_ALL_MIP0_PAGES_MASK;
		t3dMegaVTRegionPixels_t pixels;
		PackedByteArray fragment;
		uint64_t nCaptureBytes = 0u;
		if ( !t3dMegaVTCaptureColors( rInputs, location, capture ) ) {
			rError = "Terrain MegaVT source capture failed";
			return false;
		}
		for ( uint32_t n = 0u; n < 9u; ++n ) {
			if ( !capture.m_neighbors[n].m_bytes.is_empty() ) {
				nCaptureBytes += ( uint64_t )capture.m_neighbors[n].m_bytes.size() + 64u;
			}
		}
		const uint64_t nPixelBytes = 2u * 512u * 512u * 4u;
		if ( nRetainedBytes > rInputs.m_nCpuByteLimit || nCaptureBytes + nPixelBytes > rInputs.m_nCpuByteLimit - nRetainedBytes ) {
			rError = "Terrain MegaVT export CPU budget exceeded";
			return false;
		}
		if ( !buffer_memory.resize( nRetainedBytes + nCaptureBytes + nPixelBytes ) ) {
			rError = "Total terrain memory budget rejected MegaVT export buffers";
			return false;
		}
		const uint64_t nAvailableBytes = rInputs.m_nCpuByteLimit - nRetainedBytes - nCaptureBytes - nPixelBytes;
		const uint64_t sourceFingerprint = t3dMegaVTSourceFingerprint( capture );
		// Export fragments use the same V1 source/signature codec, but must not
		// replace live incremental fragments carrying a runtime source generation.
		const String cacheRoot = t3dMegaVTCacheDirectory( rInputs );
		const String cacheDirectory = cacheRoot.is_empty() ? String() : cacheRoot.path_join( "export_v1" );
		const String fragmentPath = cacheDirectory.path_join( String::num_int64( capture.m_nRegionIndex ) + ".t3dvt" );
		const bool reused =
		    !cacheDirectory.is_empty() && t3dMegaVTLoadExportFragmentCold( fragmentPath, sourceFingerprint, capture.m_nRegionIndex,
			                                                               capture.m_nExtent, nAvailableBytes, fragment );
		if ( reused ) {
			++s_exportCacheCounters.m_nReused;
			s_exportCacheCounters.m_nReusedBytes += ( uint64_t )fragment.size();
		} else {
			if ( !t3dMegaVTBuildPixels( capture, pixels, cancel ) || !t3dMegaVTCookPixels( capture, pixels, fragment, nAvailableBytes ) ) {
				rError = "Terrain MegaVT region export failed";
				return false;
			}
			++s_exportCacheCounters.m_nCooked;
			s_exportCacheCounters.m_nCookedBytes += ( uint64_t )fragment.size();
			// The cache is derived and optional; a write failure must not
			// invalidate successfully cooked V1 shipping bytes.
			if ( !cacheDirectory.is_empty() && DirAccess::make_dir_recursive_absolute( cacheDirectory ) == OK ) {
				String cacheError;
				if ( t3dMegaVTPublishCachePairCold( fragmentPath, fragment, sourceFingerprint, cacheError ) )
					++s_exportCacheCounters.m_nCacheWrites;
				else
					++s_exportCacheCounters.m_nCacheWriteFailures;
			} else {
				++s_exportCacheCounters.m_nCacheWriteFailures;
			}
		}
		nSignature ^= t3dHashPublicationBytes( fragment.ptr(), fragment.size() );
		packages.push_back( fragment );
		nRetainedBytes += ( uint64_t )fragment.size() + 64u;
		if ( !buffer_memory.resize( nRetainedBytes + nCaptureBytes + nPixelBytes ) ) {
			rError = "Total terrain memory budget rejected MegaVT export fragment";
			return false;
		}
		// Release the snapshots/pixels before admitting the next region or merge.
		capture = t3dMegaVTColorCapture_t{};
		pixels = t3dMegaVTRegionPixels_t{};
		if ( !buffer_memory.resize( nRetainedBytes ) ) {
			rError = "Total terrain memory budget rejected retained MegaVT export fragments";
			return false;
		}
	}
	if ( locations.is_empty() ) {
		return true;
	}
	const uint32_t nGeneration = t3dNextGeneration( ( uint32_t )( nSignature ^ ( nSignature >> 32u ) ) );
	PackedByteArray merged;
	if ( nRetainedBytes >= rInputs.m_nCpuByteLimit ||
	     !t3d_vt_godot_merge_packages( packages.data(), ( uint32_t )packages.size(), nGeneration, merged,
	                                   rInputs.m_nCpuByteLimit - nRetainedBytes ) ) {
		rError = "Terrain MegaVT export composition failed";
		return false;
	}
	rPackage = merged;
	rGeneration = nGeneration;
	s_exportCacheCounters.m_nMergedBytes = ( uint64_t )merged.size();
	s_exportCacheCounters.m_nLastGeneration = nGeneration;
	return true;
}

bool Terrain3DIncrementalMegaVTCooker::finishForceRecookAllRegionsCold(
    const t3dMegaVTAuthoringInputs_t& rInputs, Terrain3DTerrainMegaVT& rMegaVT, Terrain3DMegaVTCookHost& rHost, const bool bCommit,
    const String *const pShippingPath, const PackedByteArray *const pShippingBytes, const String *const pDemandPath,
    const PackedByteArray *const pDemandBytes, const String *const pV1Path, const PackedByteArray *const pV1Bytes )
{
	Terrain3DMemoryAccountScope authoring_scope( m_authoringMemoryAccount.get() );
	bool success = bCommit && isForceRecookReadyCold();
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS && success; ++i ) {
		if ( m_forceTickets[i].m_nClaimId != 0u && !rMegaVT.validateIncrementalCookCompletionHot( m_forceTickets[i] ) ) {
			success = false;
		}
	}
	// Stage the shipping package and all region artifacts before publishing any of them.
	// Backups remain available until every publication has succeeded.
	eastl::vector<Terrain3DFilePublication, Terrain3DEastlAllocator> publications;
	if ( success && ( pShippingPath == NULL || pShippingBytes == NULL || pShippingBytes->is_empty() ) ) {
		m_lastError = "Missing staged MegaVT shipping package";
		success = false;
	}
	if ( success && !t3dContainerReserveCold( publications, ( size_t )m_nForceRegionCount * 2u + 3u ) ) {
		m_lastError = "MegaVT batch publication staging allocation failed";
		success = false;
	}
	auto stage = [&]( const String& path, const PackedByteArray& bytes ) -> bool {
		if ( t3dMegaVTFileEqualsBytesCold( path, bytes ) )
			return true;
		publications.emplace_back( path, path + String( ".force_staged" ), path + String( ".force_previous" ) );
		Terrain3DFilePublication& publication = publications.back();
		if ( publication.stagePacked( bytes ) == OK ) {
			return true;
		}
		m_lastError = publication.getError();
		return false;
	};
	if ( success ) {
		success = stage( *pShippingPath, *pShippingBytes );
	}
	if ( success && pDemandPath != NULL && pDemandBytes != NULL && !pDemandPath->is_empty() && !pDemandBytes->is_empty() ) {
		success = stage( *pDemandPath, *pDemandBytes );
	}
	if ( success && pV1Path && pV1Bytes && !pV1Path->is_empty() && !pV1Bytes->is_empty() ) {
		if ( !pDemandPath || !pDemandBytes || pDemandPath->is_empty() || pDemandBytes->is_empty() || *pV1Path == *pDemandPath ||
		     *pV1Path == *pShippingPath ) {
			m_lastError = "Invalid T3VD v1 sidecar transaction arguments";
			success = false;
		} else
			success = stage( *pV1Path, *pV1Bytes );
	}
	const String directory = t3dMegaVTCacheDirectory( rInputs );
	if ( success && DirAccess::make_dir_recursive_absolute( directory ) != OK ) {
		m_lastError = "Cannot create MegaVT region cache directory";
		success = false;
	}
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS && success; ++i ) {
		if ( m_forceTickets[i].m_nClaimId == 0u ) {
			continue;
		}
		const String path = directory.path_join( String::num_int64( i ) + ".t3dvt" );
		const PackedByteArray signature = t3dMegaVTSourceSignatureBytes( m_forceSourceFingerprints[i] );
		success = signature.size() == 16 && stage( path + String( ".source" ), signature ) && stage( path, m_forceFragments[i] );
		if ( !success && m_lastError.is_empty() ) {
			m_lastError = "MegaVT batch source signature allocation failed";
		}
	}
	if ( success ) {
		for ( size_t i = 0u; i < publications.size(); ++i ) {
			if ( publications[i].publish() != OK ) {
				m_lastError = publications[i].getError();
				success = false;
				break;
			}
		}
	}
	if ( !success ) {
		for ( size_t i = publications.size(); i > 0u; --i ) {
			if ( publications[i - 1u].rollback() != OK ) {
				m_lastError += String( "; rollback: " ) + publications[i - 1u].getError();
			}
		}
	} else {
		// An accepted ticket has already passed the full batch snapshot validation.
		// All on-disk artifacts are available before acknowledging dirty generations.
		for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
			if ( m_forceTickets[i].m_nClaimId == 0u ) {
				continue;
			}
			if ( !rMegaVT.acceptIncrementalCookCompletionHot( m_forceTickets[i] ) ) {
				m_lastError = "MegaVT forced batch source changed while acknowledging completion";
				success = false;
				break;
			}
			m_fragments[i] = m_forceFragments[i];
			m_nRetryAfterUsec[i] = 0u;
			m_nRetryAttempts[i] = 0u;
			++m_nAccepted;
		}
		if ( success ) {
			// Removing previous files is cleanup after the transaction is already visible.
			for ( size_t i = 0u; i < publications.size(); ++i ) {
				if ( publications[i].commit() != OK ) {
					m_lastError = publications[i].getError();
				}
			}
		} else {
			for ( size_t i = publications.size(); i > 0u; --i ) {
				( void )publications[i - 1u].rollback();
			}
		}
	}
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		if ( !success && m_forceTickets[i].m_nClaimId != 0u ) {
			( void )rMegaVT.cancelIncrementalCookHot( m_forceTickets[i] );
		}
		m_forceFragments[i].clear();
		m_forceTickets[i] = t3dTerrainMegaVTCookTicket_t{};
		m_forceSourceFingerprints[i] = 0u;
	}
	if ( !success ) {
		retryDirtyRegionsCold();
	}
	m_ticket = t3dTerrainMegaVTCookTicket_t{};
	m_bForceActive = false;
	m_bForceFailed = false;
	m_bForceRequested = false;
	m_nIdlePublicationGeneration = rMegaVT.getPublicationGeneration();
	trimCpuReservationCold();
	rHost.persistMegaVTEditorStateCold();
	if ( success ) {
		m_lastError = String();
	}
	return success;
}

bool Terrain3DIncrementalMegaVTCooker::validateForceRecookSnapshotCold( Terrain3DTerrainMegaVT& rMegaVT ) const
{
	if ( !isForceRecookReadyCold() ) {
		return false;
	}
	for ( uint32_t i = 0u; i < T3D_REGION_SLOTS; ++i ) {
		if ( m_forceTickets[i].m_nClaimId != 0u && !rMegaVT.validateIncrementalCookCompletionHot( m_forceTickets[i] ) ) {
			return false;
		}
	}
	return true;
}

// -------- Consolidated from authoring/vt/build/terrain_3d_megatexture_cooker.cpp --------
// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.
uint32_t t3d_megatexture_cooker_page_count( uint32_t nWidth, uint32_t nHeight )
{
	if ( nWidth == 0u || nHeight == 0u ) {
		return 0u;
	}

	uint64_t nPageCount = 0u;
	const uint32_t nMipCount = t3d_vt_page_builder_mip_count( nWidth, nHeight );
	for ( uint32_t iMip = 0u; iMip < nMipCount; ++iMip ) {
		nPageCount += ( uint64_t )t3d_vt_page_builder_pages_wide( nWidth ) * t3d_vt_page_builder_pages_high( nHeight );
		if ( nPageCount > UINT32_MAX ) {
			return 0u;
		}
		nWidth = nWidth > 1u ? nWidth >> 1u : 1u;
		nHeight = nHeight > 1u ? nHeight >> 1u : 1u;
	}
	return ( uint32_t )nPageCount;
}

bool t3d_megatexture_cooker_build_page_u8( const uint8_t *pMipPixels, const uint32_t nMipWidth, const uint32_t nMipHeight,
                                           const uint32_t nComponents, const t3dMegatexturePageAddress_t& rAddress,
                                           const t3dVTCookWrapMode_t eWrapMode, uint8_t *pDestination, const uint64_t nDestinationBytes )
{
	return t3d_vt_page_builder_build_tile_u8( pMipPixels, nMipWidth, nMipHeight, nComponents, rAddress.m_nPageX, rAddress.m_nPageY,
	                                          eWrapMode, pDestination, nDestinationBytes );
}
