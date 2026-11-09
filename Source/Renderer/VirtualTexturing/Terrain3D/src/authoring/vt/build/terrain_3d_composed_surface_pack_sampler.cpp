#include "authoring/vt/build/terrain_3d_composed_surface_pack_sampler.h"

#include <cmath>
#include <cstring>
#include <limits>

static uint32_t t3d_composed_surface_pack_wrap( int64_t n, uint32_t extent )
{
	const int64_t m = int64_t( extent );
	return uint32_t( ( n % m + m ) % m );
}

bool t3d_composed_surface_pack_find_material_allocation( const t3dVTPackBaselineView_t& pack, uint32_t material_id, uint32_t& out )
{
	out = UINT32_MAX;
	if ( !pack.m_pHeader.m_bValid || !pack.m_pAllocationKeys.m_pBytes || !pack.m_pAllocations.m_pBytes || material_id == UINT32_MAX )
		return false;
	const uint64_t key = t3d_vt_terrain_material_stable_key( material_id );
	uint32_t lo = 0u, hi = pack.m_nAllocationCount;
	while ( lo < hi ) {
		const uint32_t mid = lo + ( hi - lo ) / 2u;
		const auto candidate = pack.m_pAllocationKeys[mid];
		if ( candidate.m_nStableKey < key )
			lo = mid + 1u;
		else
			hi = mid;
	}
	if ( lo >= pack.m_nAllocationCount )
		return false;
	const auto found = pack.m_pAllocationKeys[lo];
	if ( found.m_nStableKey != key || found.m_nAllocationId != lo || pack.m_pAllocations[lo].m_nConsumer != T3D_VT_CONSUMER_CLIPMAP )
		return false;
	out = lo;
	return true;
}

bool t3d_composed_surface_pack_open_source_pack( const void *pPackage, uint64_t nPackageBytes, t3dVTPackBaselineView_t& view,
                                                 t3dComposedSurfacePackSampler_t& sampler, t3dComposedSurfacePackDecodeFn decode,
                                                 void *decoder_user, uint8_t *scratch, uint32_t slots )
{
	view = {};
	sampler = {};
	if ( !t3d_vt_pack_open_baseline_view( pPackage, nPackageBytes, view ) ||
	     !t3d_composed_surface_pack_sampler_init( sampler, view, decode, decoder_user, scratch, slots ) ) {
		view = {};
		sampler = {};
		return false;
	}
	return true;
}

bool t3d_composed_surface_pack_sampler_init( t3dComposedSurfacePackSampler_t& r, const t3dVTPackBaselineView_t& pack,
                                             t3dComposedSurfacePackDecodeFn decode, void *user, uint8_t *scratch, uint32_t slots )
{
	r = {};
	if ( !pack.m_pHeader.m_bValid || !pack.m_pPayloadData || !pack.m_pPages.m_pBytes || !pack.m_pChannelAllocations.m_pBytes ||
	     !pack.m_pAllocations.m_pBytes || !pack.m_pPayloads.m_pBytes || !pack.m_pClusters.m_pBytes || !pack.m_pTails.m_pBytes || !decode ||
	     !scratch || slots == 0u || slots > T3D_COMPOSED_SURFACE_PACK_CACHE_MAX_SLOTS )
		return false;
	r.m_pPack = &pack;
	r.m_pDecode = decode;
	r.m_pDecoderUser = user;
	r.m_pCacheBytes = scratch;
	r.m_nCacheSlots = slots;
	for ( uint32_t i = 0u; i < slots; ++i )
		r.m_Entries[i].m_nPayloadId = UINT32_MAX;
	return true;
}

static bool t3d_composed_surface_pack_bridge_page( void *user, const t3dComposedSurfaceCookPage_t& page,
                                                   t3dComposedStaticFootprint_t& foot )
{
	auto& bridge = *static_cast<t3dComposedSurfacePackProducerBridge_t *>( user );
	return bridge.m_pOriginalPage( bridge.m_pOriginalUser, page, foot );
}

static bool t3d_composed_surface_pack_bridge_control( void *user, int32_t x, int32_t y, uint32_t& control )
{
	auto& bridge = *static_cast<t3dComposedSurfacePackProducerBridge_t *>( user );
	return bridge.m_pOriginalControl( bridge.m_pOriginalUser, x, y, control );
}

static bool t3d_composed_surface_pack_bridge_surface( void *user, uint32_t x, uint32_t y, float& height, float& normal_y )
{
	auto& bridge = *static_cast<t3dComposedSurfacePackProducerBridge_t *>( user );
	return bridge.m_pOriginalSurface && bridge.m_pOriginalSurface( bridge.m_pOriginalUser, x, y, height, normal_y );
}

static bool t3d_composed_surface_pack_bridge_sample( void *user, uint32_t allocation, const float *uv, const float *dx, const float *dy,
                                                     float bias, uint32_t filter, t3dComposedStaticSample_t& sample )
{
	auto& bridge = *static_cast<t3dComposedSurfacePackProducerBridge_t *>( user );
	return t3d_composed_surface_pack_sample_static( bridge.m_pSampler, allocation, uv, dx, dy, bias, filter, sample );
}

bool t3d_composed_surface_pack_bind_static_source( t3dComposedSurfacePackSampler_t& sampler, t3dComposedSurfacePackProducerBridge_t& bridge,
                                                   t3dComposedStaticSource_t& source )
{
	if ( !sampler.m_pPack || !sampler.m_pDecode || !sampler.m_pCacheBytes || !source.m_pReadControl || !source.m_pResolvePage ||
	     ( source.m_nVtFiltering != 1u && source.m_nVtFiltering != 3u ) )
		return false;
	bridge = {};
	bridge.m_pSampler = &sampler;
	bridge.m_pOriginalUser = source.m_pUser;
	bridge.m_pOriginalPage = source.m_pResolvePage;
	bridge.m_pOriginalControl = source.m_pReadControl;
	bridge.m_pOriginalSurface = source.m_pReadTerrainSurface;
	source.m_pUser = &bridge;
	source.m_pResolvePage = t3d_composed_surface_pack_bridge_page;
	source.m_pReadControl = t3d_composed_surface_pack_bridge_control;
	if ( source.m_pReadTerrainSurface )
		source.m_pReadTerrainSurface = t3d_composed_surface_pack_bridge_surface;
	source.m_pSampleSource = t3d_composed_surface_pack_bridge_sample;
	return true;
}

// Payload offsets refer to a cluster's decoded stream. V1's current page
// payloads are codec NONE; reject outer-compressed clusters until a verified
// cluster decoder is provided rather than interpreting compressed bytes.
static bool t3d_composed_surface_pack_tile( t3dComposedSurfacePackSampler_t& r, uint32_t payload_id, const uint8_t *& result )
{
	const auto& pack = *r.m_pPack;
	if ( payload_id >= pack.m_nPayloadCount )
		return false;
	for ( uint32_t i = 0u; i < r.m_nCacheSlots; ++i ) {
		if ( r.m_Entries[i].m_nPayloadId == payload_id ) {
			r.m_Entries[i].m_nTimestamp = ++r.m_nClock;
			++r.m_nCacheHits;
			result = r.m_pCacheBytes + size_t( i ) * T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES;
			return true;
		}
	}
	const auto p = pack.m_pPayloads[payload_id];
	if ( p.m_nClusterId >= pack.m_nClusterCount || p.m_nCodec != T3D_VT_PACK_CODEC_NONE || p.m_nStoredBytes == 0u ||
	     p.m_nStoredBytes != p.m_nDecodedBytes || p.m_nDecodedBytes != t3d_vt_pack_expected_physical_tile_bytes( p.m_nFormat ) )
		return false;
	const auto c = pack.m_pClusters[p.m_nClusterId];
	if ( c.m_nCodec != T3D_VT_PACK_CODEC_NONE || c.m_nStoredBytes != c.m_nDecodedBytes ||
	     c.m_nPayloadDataOffset > pack.m_nPayloadDataBytes || c.m_nDecodedBytes > pack.m_nPayloadDataBytes - c.m_nPayloadDataOffset ||
	     p.m_nClusterOffset > c.m_nDecodedBytes || p.m_nStoredBytes > c.m_nDecodedBytes - p.m_nClusterOffset )
		return false;
	const uint8_t *cluster = pack.m_pPayloadData + c.m_nPayloadDataOffset;
	// Cache cluster verification can be added at the cold open boundary; here
	// validate the exact page bytes on every miss, including reused payload IDs.
	const uint8_t *bytes = cluster + p.m_nClusterOffset;
	if ( t3d_comp_surf_crc32( bytes, p.m_nStoredBytes ) != p.m_nChecksum ) {
		++r.m_nChecksumFailures;
		return false;
	}
	uint32_t victim = 0u;
	for ( uint32_t i = 0u; i < r.m_nCacheSlots; ++i ) {
		if ( r.m_Entries[i].m_nPayloadId == UINT32_MAX ) {
			victim = i;
			break;
		}
		if ( r.m_Entries[i].m_nTimestamp < r.m_Entries[victim].m_nTimestamp )
			victim = i;
	}
	uint8_t *output = r.m_pCacheBytes + size_t( victim ) * T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES;
	if ( !r.m_pDecode( r.m_pDecoderUser, p.m_nFormat, bytes, p.m_nDecodedBytes, output, T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES ) )
		return false;
	r.m_Entries[victim].m_nPayloadId = payload_id;
	r.m_Entries[victim].m_nTimestamp = ++r.m_nClock;
	++r.m_nDecodeCalls;
	result = output;
	return true;
}

// Resolve the authored mip0 dimensions from the frozen tail chain, not merely
// ceil(mip0/128) page counts (which overestimate 64x64 source textures).
static bool t3d_composed_surface_pack_dimensions( const t3dVTPackBaselineView_t& pack, uint32_t allocation, uint32_t channel,
                                                  uint32_t& width, uint32_t& height )
{
	width = height = 0u;
	if ( allocation >= pack.m_nAllocationCount || channel >= T3D_VT_LOGICAL_CHANNEL_COUNT ||
	     allocation > UINT32_MAX / T3D_VT_LOGICAL_CHANNEL_COUNT )
		return false;
	const auto a = pack.m_pChannelAllocations[allocation * T3D_VT_LOGICAL_CHANNEL_COUNT + channel];
	if ( a.m_nMipCount == 0u || a.m_nTailIndex >= pack.m_nTailCount )
		return false;
	const auto tail = pack.m_pTails[a.m_nTailIndex];
	if ( tail.m_nChannel != channel || tail.m_nCanonicalAllocationId != allocation || tail.m_nFirstMip >= a.m_nMipCount ||
	     tail.m_nFirstMip >= 31u || tail.m_nFirstMipWidth == 0u || tail.m_nFirstMipHeight == 0u ||
	     uint32_t( tail.m_nFirstMipWidth ) > UINT32_MAX >> tail.m_nFirstMip ||
	     uint32_t( tail.m_nFirstMipHeight ) > UINT32_MAX >> tail.m_nFirstMip )
		return false;
	width = uint32_t( tail.m_nFirstMipWidth ) << tail.m_nFirstMip;
	height = uint32_t( tail.m_nFirstMipHeight ) << tail.m_nFirstMip;
	if ( ( uint64_t( width ) + 127u ) / 128u != a.m_nMip0WidthPages || ( uint64_t( height ) + 127u ) / 128u != a.m_nMip0HeightPages )
		return false;
	return true;
}

// Resolve a logical channel and mip to a physical page or packed tail tile.
static bool t3d_composed_surface_pack_texel( t3dComposedSurfacePackSampler_t& r, uint32_t allocation, uint32_t channel, uint32_t mip,
                                             int64_t x, int64_t y, float *rgba )
{
	const auto& pack = *r.m_pPack;
	if ( allocation >= pack.m_nAllocationCount || channel >= T3D_VT_LOGICAL_CHANNEL_COUNT ||
	     allocation > UINT32_MAX / T3D_VT_LOGICAL_CHANNEL_COUNT )
		return false;
	const uint32_t index = allocation * T3D_VT_LOGICAL_CHANNEL_COUNT + channel;
	const auto a = pack.m_pChannelAllocations[index];
	if ( a.m_nMipCount == 0u || mip >= a.m_nMipCount || a.m_nTailIndex >= pack.m_nTailCount )
		return false;
	const auto tail = pack.m_pTails[a.m_nTailIndex];
	if ( tail.m_nChannel != channel || tail.m_nCanonicalAllocationId != allocation || tail.m_nFirstMip >= a.m_nMipCount )
		return false;
	uint32_t width = 0u, height = 0u;
	if ( !t3d_composed_surface_pack_dimensions( pack, allocation, channel, width, height ) )
		return false;
	const uint32_t mip_width = width >> mip ? width >> mip : 1u;
	const uint32_t mip_height = height >> mip ? height >> mip : 1u;
	const uint32_t tx = t3d_composed_surface_pack_wrap( x, mip_width );
	const uint32_t ty = t3d_composed_surface_pack_wrap( y, mip_height );
	uint32_t payload = UINT32_MAX, local_x = 0u, local_y = 0u;
	if ( mip < tail.m_nFirstMip ) {
		uint32_t base = a.m_nBaseVirtualPage;
		for ( uint32_t i = 0u; i < mip; ++i ) {
			const uint32_t iw = ( ( ( width >> i ) ? ( width >> i ) : 1u ) + 127u ) / 128u;
			const uint32_t ih = ( ( ( height >> i ) ? ( height >> i ) : 1u ) + 127u ) / 128u;
			if ( iw != 0u && ih > ( UINT32_MAX - base ) / iw )
				return false;
			base += iw * ih;
		}
		const uint32_t pages_wide = ( mip_width + 127u ) / 128u;
		const uint32_t page_x = tx / 128u, page_y = ty / 128u;
		const uint64_t page_id = uint64_t( base ) + uint64_t( page_y ) * pages_wide + page_x;
		if ( page_id >= pack.m_nPageCount || page_id >= uint64_t( a.m_nBaseVirtualPage ) + a.m_nPageCount )
			return false;
		const auto rec = pack.m_pPages[uint32_t( page_id )];
		if ( rec.m_nVirtualPage != page_id || rec.m_nAllocationId != allocation || rec.m_nChannel != channel || rec.m_nMip != mip ||
		     rec.m_nPageX != page_x || rec.m_nPageY != page_y )
			return false;
		payload = rec.m_nPayloadId;
		local_x = T3D_VT_PAGE_GUTTER + tx % T3D_VT_PAGE_INTERIOR;
		local_y = T3D_VT_PAGE_GUTTER + ty % T3D_VT_PAGE_INTERIOR;
	} else {
		payload = tail.m_nPayloadId;
		local_x = T3D_VT_PAGE_GUTTER;
		for ( uint32_t i = tail.m_nFirstMip; i < mip; ++i ) {
			const uint32_t mw = width >> i ? width >> i : 1u;
			local_x += mw;
		}
		local_x += tx;
		local_y = T3D_VT_PAGE_GUTTER + ty;
	}
	if ( local_x >= T3D_VT_PHYSICAL_TILE || local_y >= T3D_VT_PHYSICAL_TILE )
		return false;
	const uint8_t *tile = nullptr;
	if ( !t3d_composed_surface_pack_tile( r, payload, tile ) )
		return false;
	const uint8_t *texel = tile + 4u * ( local_y * T3D_VT_PHYSICAL_TILE + local_x );
	for ( uint32_t i = 0u; i < 4u; ++i )
		rgba[i] = float( texel[i] ) * ( 1.0f / 255.0f );
	return true;
}

static bool t3d_composed_surface_pack_channel_sample( t3dComposedSurfacePackSampler_t& r, uint32_t allocation, uint32_t channel,
                                                      uint32_t mip, const float *uv, bool nearest, float *rgba )
{
	const auto& pack = *r.m_pPack;
	if ( allocation >= pack.m_nAllocationCount )
		return false;
	const auto a = pack.m_pChannelAllocations[allocation * T3D_VT_LOGICAL_CHANNEL_COUNT + channel];
	uint32_t width = 0u, height = 0u;
	if ( !t3d_composed_surface_pack_dimensions( pack, allocation, channel, width, height ) || mip >= a.m_nMipCount || mip >= 31u )
		return false;
	width = ( width >> mip ) ? ( width >> mip ) : 1u;
	height = ( height >> mip ) ? ( height >> mip ) : 1u;
	const float px = uv[0] * float( width ) - 0.5f;
	const float py = uv[1] * float( height ) - 0.5f;
	if ( !std::isfinite( px ) || !std::isfinite( py ) || fabsf( px ) > 1.0e12f || fabsf( py ) > 1.0e12f )
		return false;
	const int64_t x = int64_t( floorf( px ) ), y = int64_t( floorf( py ) );
	if ( nearest )
		return t3d_composed_surface_pack_texel( r, allocation, channel, mip, int64_t( floorf( px + 0.5f ) ), int64_t( floorf( py + 0.5f ) ),
		                                        rgba );
	float s00[4], s10[4], s01[4], s11[4];
	if ( !t3d_composed_surface_pack_texel( r, allocation, channel, mip, x, y, s00 ) ||
	     !t3d_composed_surface_pack_texel( r, allocation, channel, mip, x + 1, y, s10 ) ||
	     !t3d_composed_surface_pack_texel( r, allocation, channel, mip, x, y + 1, s01 ) ||
	     !t3d_composed_surface_pack_texel( r, allocation, channel, mip, x + 1, y + 1, s11 ) )
		return false;
	const float fx = px - floorf( px ), fy = py - floorf( py );
	for ( uint32_t i = 0u; i < 4u; ++i )
		rgba[i] = ( s00[i] * ( 1.0f - fx ) + s10[i] * fx ) * ( 1.0f - fy ) + ( s01[i] * ( 1.0f - fx ) + s11[i] * fx ) * fy;
	return true;
}

static bool t3d_composed_surface_pack_filtered( t3dComposedSurfacePackSampler_t& r, uint32_t allocation, uint32_t channel, const float *uv,
                                                const float *dx, const float *dy, float bias, uint32_t filtering, float *rgba )
{
	const auto& pack = *r.m_pPack;
	if ( allocation >= pack.m_nAllocationCount || channel >= T3D_VT_LOGICAL_CHANNEL_COUNT )
		return false;
	const auto a = pack.m_pChannelAllocations[allocation * T3D_VT_LOGICAL_CHANNEL_COUNT + channel];
	if ( a.m_nMipCount == 0u )
		return false;
	uint32_t full_width = 0u, full_height = 0u;
	if ( !t3d_composed_surface_pack_dimensions( pack, allocation, channel, full_width, full_height ) )
		return false;
	const float w = float( full_width );
	const float h = float( full_height );
	const float dx_len = hypotf( dx[0] * w, dx[1] * h ), dy_len = hypotf( dy[0] * w, dy[1] * h );
	const float rho = fmaxf( dx_len, dy_len );
	if ( !std::isfinite( rho ) )
		return false;
	const float lod = fminf( fmaxf( log2f( fmaxf( rho, 1.0e-8f ) ) + bias, 0.0f ), float( a.m_nMipCount - 1u ) );
	const uint32_t mip = uint32_t( floorf( lod ) );
	if ( !t3d_composed_surface_pack_channel_sample( r, allocation, channel, mip, uv, filtering == 3u, rgba ) )
		return false;
	if ( filtering == 1u && mip + 1u < a.m_nMipCount ) {
		const float frac = lod - float( mip );
		if ( frac > 0.0001f ) {
			float other[4];
			if ( !t3d_composed_surface_pack_channel_sample( r, allocation, channel, mip + 1u, uv, false, other ) )
				return false;
			for ( uint32_t i = 0u; i < 4u; ++i )
				rgba[i] += ( other[i] - rgba[i] ) * frac;
		}
	}
	return true;
}

static float t3d_composed_surface_pack_srgb( float c )
{
	return c <= 0.04045f ? c * ( 1.0f / 12.92f ) : powf( ( c + 0.055f ) * ( 1.0f / 1.055f ), 2.4f );
}

bool t3d_composed_surface_pack_sample_static( void *user, uint32_t allocation, const float *uv, const float *dx, const float *dy,
                                              float bias, uint32_t filtering, t3dComposedStaticSample_t& result )
{
	if ( !user || !uv || !dx || !dy || !std::isfinite( bias ) || ( filtering != 1u && filtering != 3u ) )
		return false;
	auto& r = *static_cast<t3dComposedSurfacePackSampler_t *>( user );
	if ( !r.m_pPack || allocation >= r.m_pPack->m_nAllocationCount )
		return false;
	const auto& pack = *r.m_pPack;
	if ( pack.m_pAllocations[allocation].m_nConsumer != T3D_VT_CONSUMER_CLIPMAP )
		return false;
	const auto base = pack.m_pChannelAllocations[allocation * T3D_VT_LOGICAL_CHANNEL_COUNT + T3D_VT_LOGICAL_CHANNEL_ALBEDO];
	const auto pbr = pack.m_pChannelAllocations[allocation * T3D_VT_LOGICAL_CHANNEL_COUNT + T3D_VT_LOGICAL_CHANNEL_PBR];
	if ( !base.m_nMipCount || !pbr.m_nMipCount )
		return false;
	float albedo[4], surface[4], height[4] = { float( T3D_VT_NEUTRAL_HEIGHT8 ) / 255.0f, 0, 0, 1 };
	if ( !t3d_composed_surface_pack_filtered( r, allocation, T3D_VT_LOGICAL_CHANNEL_ALBEDO, uv, dx, dy, bias, filtering, albedo ) ||
	     !t3d_composed_surface_pack_filtered( r, allocation, T3D_VT_LOGICAL_CHANNEL_PBR, uv, dx, dy, bias, filtering, surface ) )
		return false;
	const auto h = pack.m_pChannelAllocations[allocation * T3D_VT_LOGICAL_CHANNEL_COUNT + T3D_VT_LOGICAL_CHANNEL_HEIGHT];
	if ( h.m_nMipCount &&
	     !t3d_composed_surface_pack_filtered( r, allocation, T3D_VT_LOGICAL_CHANNEL_HEIGHT, uv, dx, dy, bias, filtering, height ) )
		return false;
	for ( uint32_t i = 0u; i < 3u; ++i )
		result.m_fAlbedo[i] = t3d_composed_surface_pack_srgb( albedo[i] );
	for ( uint32_t i = 0u; i < 4u; ++i )
		result.m_fPbr[i] = surface[i];
	result.m_fHeight = height[0];
	return true;
}
