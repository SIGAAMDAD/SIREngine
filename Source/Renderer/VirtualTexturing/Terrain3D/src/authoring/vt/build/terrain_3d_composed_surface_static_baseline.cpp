#include "authoring/vt/build/terrain_3d_composed_surface_static_baseline.h"

#include <cmath>
#include <string.h>
#include <float.h>

static float t3d_comp_surf_limit( const float x, const float low, const float high )
{
	return fminf( fmaxf( x, low ), high );
}

static bool t3d_comp_surf_finite( const float x )
{
	return std::isfinite( x );
}

bool t3d_comp_surf_static_baseline_eligible( const t3dComposedStaticSource_t& s )
{
	if ( !s.m_pResolvePage || !s.m_pReadControl || !s.m_pSampleSource || !s.m_pLayers || s.m_nLayerCount == 0u ||
	     s.m_nLayerCount > T3D_COMP_SURF_STATIC_MAX_LAYERS ||
	     ( s.m_nSamplingMode != T3D_COMP_SURF_STATIC_SAMPLE_NEAREST && s.m_nSamplingMode != T3D_COMP_SURF_STATIC_SAMPLE_BILINEAR &&
	       s.m_nSamplingMode != T3D_COMP_SURF_STATIC_SAMPLE_TOP_TWO ) ||
	     ( s.m_bAutoShader &&
	       ( !s.m_pReadTerrainSurface || s.m_nAutoBaseTexture >= s.m_nLayerCount || s.m_nAutoOverlayTexture >= s.m_nLayerCount ||
	         !t3d_comp_surf_finite( s.m_fAutoSlope ) || !t3d_comp_surf_finite( s.m_fAutoHeightReduction ) ) ) ||
	     ( s.m_bDetiling && ( s.m_nDetilingMode == 2u || s.m_nDetilingMode > 5u ) ) || s.m_bDualScale || s.m_bSideProjection ||
	     s.m_bAdaptiveSampling || s.m_bImplicitControls || s.m_bViewDependentMipBias || !t3d_comp_surf_finite( s.m_fStaticMipBias ) ||
	     !t3d_comp_surf_finite( s.m_fBlendSharpness ) || s.m_fBlendSharpness < 0.0f || s.m_fBlendSharpness > 1.0f )
		return false;
	for ( uint32_t i = 0u; i < s.m_nLayerCount; ++i ) {
		const auto& layer = s.m_pLayers[i];
		if ( layer.m_nAllocation == T3D_COMP_SURF_INVALID_SLOT )
			return false;
		for ( float x : layer.m_fColor )
			if ( !t3d_comp_surf_finite( x ) )
				return false;
		for ( float x : layer.m_fPbr0 )
			if ( !t3d_comp_surf_finite( x ) )
				return false;
		for ( float x : layer.m_fPbr1 )
			if ( !t3d_comp_surf_finite( x ) )
				return false;
	}
	return true;
}

struct t3dCompSurfMaterial_t
{
	float albedo[3];
	float normal[3];
	float ao, roughness, metallic, height, ao_affect;
};

static void t3d_comp_surf_accumulate( t3dCompSurfMaterial_t& dst, const t3dCompSurfMaterial_t& src, float w )
{
	for ( unsigned i = 0; i < 3u; ++i ) {
		dst.albedo[i] += src.albedo[i] * w;
		dst.normal[i] += src.normal[i] * w;
	}
	dst.ao += src.ao * w;
	dst.roughness += src.roughness * w;
	dst.metallic += src.metallic * w;
	dst.height += src.height * w;
	dst.ao_affect += src.ao_affect * w;
}

static void t3d_comp_surf_normalize( float *v )
{
	float sq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	if ( sq > 1.0e-14f ) {
		float inv = 1.0f / sqrtf( sq );
		for ( int i = 0; i != 3; ++i )
			v[i] *= inv;
	} else {
		v[0] = 0.0f;
		v[1] = 1.0f;
		v[2] = 0.0f;
	}
}

// Mirrors hash21/apply_detiling in terrain_clipmap_fragment_inc.glslinc.
// World-local material transforms are deterministic even for negative cells.
static float t3d_comp_surf_fract( const float x )
{
	return x - floorf( x );
}
static float t3d_comp_surf_hash21( const float x, const float y )
{
	float a = t3d_comp_surf_fract( x * 0.1031f );
	float b = t3d_comp_surf_fract( y * 0.1031f );
	float c = t3d_comp_surf_fract( x * 0.1031f );
	const float k = a * ( b + 33.33f ) + b * ( c + 33.33f ) + c * ( a + 33.33f );
	a += k;
	b += k;
	c += k;
	return t3d_comp_surf_fract( ( a + b ) * c );
}

// GLSL mat2(c, -s, s, c) rotates the UV and tangent frame by -angle.
static void t3d_comp_surf_detile( const t3dComposedStaticSource_t& s, const t3dComposedStaticLayer_t& layer, uint32_t id,
                                  const float *world, float *uv, float *dx, float *dy, float& rRotation )
{
	rRotation = 0.0f;
	if ( !s.m_bDetiling || s.m_nDetilingMode == 2u )
		return;
	const float k = fmaxf( layer.m_fPbr0[0], 0.000001f ) * 0.25f;
	const float hash = t3d_comp_surf_hash21( floorf( world[0] * k ) + float( id ), floorf( world[1] * k ) + 19.0f );
	const float shift = layer.m_fPbr1[3];
	if ( s.m_nDetilingMode == 3u ) {
		uv[0] += ( hash - 0.5f ) * shift;
		uv[1] += ( hash - 0.5f ) * shift;
		return;
	}
	const uint32_t sectors = s.m_nDetilingMode == 4u ? 4u : ( s.m_nDetilingMode == 5u ? 16u : 8u );
	rRotation = floorf( hash * float( sectors ) ) * ( 6.2831853071795864769f / float( sectors ) ) * layer.m_fPbr1[2];
	const float cs = cosf( rRotation ), sn = sinf( rRotation );
	const float ox = uv[0], odx = dx[0], ody = dy[0];
	uv[0] = cs * ox + sn * uv[1];
	uv[1] = -sn * ox + cs * uv[1];
	dx[0] = cs * odx + sn * dx[1];
	dx[1] = -sn * odx + cs * dx[1];
	dy[0] = cs * ody + sn * dy[1];
	dy[1] = -sn * ody + cs * dy[1];
	uv[0] += ( t3d_comp_surf_fract( hash * 7.13f ) - 0.5f ) * shift;
	uv[1] += ( t3d_comp_surf_fract( hash * 13.71f ) - 0.5f ) * shift;
}

static bool t3d_comp_surf_material( const t3dComposedStaticSource_t& s, const uint32_t id, const float *world, const float *grad_x,
                                    const float *grad_y, float scale, uint32_t rotation, t3dCompSurfMaterial_t& dst )
{
	if ( id >= s.m_nLayerCount )
		return false;
	const auto& layer = s.m_pLayers[id];
	const float angle = float( rotation ) * 0.3926990816987241548f;
	const float cs = cosf( angle ), sn = sinf( angle );
	const float k = fmaxf( scale, 0.1f ) * fmaxf( layer.m_fPbr0[0], 0.000001f );
	float uv[2] = { ( cs * world[0] - sn * world[1] ) * k, ( sn * world[0] + cs * world[1] ) * k };
	float dx[2] = { ( cs * grad_x[0] - sn * grad_x[1] ) * k, ( sn * grad_x[0] + cs * grad_x[1] ) * k };
	float dy[2] = { ( cs * grad_y[0] - sn * grad_y[1] ) * k, ( sn * grad_y[0] + cs * grad_y[1] ) * k };
	float detile_rotation = 0.0f;
	t3d_comp_surf_detile( s, layer, id, world, uv, dx, dy, detile_rotation );
	t3dComposedStaticSample_t sample{};
	if ( !s.m_pSampleSource( s.m_pUser, layer.m_nAllocation, uv, dx, dy, s.m_fStaticMipBias, s.m_nVtFiltering, sample ) )
		return false;
	for ( float x : sample.m_fAlbedo )
		if ( !t3d_comp_surf_finite( x ) )
			return false;
	for ( float x : sample.m_fPbr )
		if ( !t3d_comp_surf_finite( x ) )
			return false;
	if ( !t3d_comp_surf_finite( sample.m_fHeight ) )
		return false;
	memset( &dst, 0, sizeof( dst ) );
	for ( unsigned i = 0u; i < 3u; ++i )
		dst.albedo[i] = sample.m_fAlbedo[i] * layer.m_fColor[i];
	float nx = sample.m_fPbr[0] * 2.0f - 1.0f;
	float nz = sample.m_fPbr[1] * 2.0f - 1.0f;
	float len2 = nx * nx + nz * nz;
	if ( len2 > 1.0f ) {
		float inv = 1.0f / sqrtf( len2 );
		nx *= inv;
		nz *= inv;
		len2 = 1.0f;
	}
	// Same canonical V1 XZ normal transform as sample_material_canonical_v1.
	// Tangent normal follows detile_rotation * authored_control_rotation.
	const float normal_angle = angle - detile_rotation;
	const float nc = cosf( normal_angle ), ns = sinf( normal_angle );
	dst.normal[0] = ( nc * nx - ns * nz ) * layer.m_fPbr0[1];
	dst.normal[1] = sqrtf( fmaxf( 1.0f - len2, 0.0f ) );
	dst.normal[2] = ( ns * nx + nc * nz ) * layer.m_fPbr0[1];
	t3d_comp_surf_normalize( dst.normal );
	dst.height = sample.m_fHeight;
	dst.ao = 1.0f + ( sample.m_fPbr[3] - 1.0f ) * t3d_comp_surf_limit( layer.m_fPbr1[0], 0.0f, 1.0f );
	dst.roughness = t3d_comp_surf_limit( sample.m_fPbr[2] * fmaxf( 1.0f + layer.m_fPbr0[2], 0.0f ), 0.0f, 1.0f );
	dst.metallic = t3d_comp_surf_limit( layer.m_fPbr0[3], 0.0f, 1.0f );
	dst.ao_affect = layer.m_fPbr1[1];
	return true;
}

static bool t3d_comp_surf_control_pair( const t3dComposedStaticSource_t& s, const uint32_t control, const float *world, const float *dx,
                                        const float *dy, t3dCompSurfMaterial_t& dst )
{
	const uint32_t base = ( control >> 27u ) & 31u;
	const uint32_t overlay = ( control >> 22u ) & 31u;
	const uint32_t rotation = ( control >> 10u ) & 15u;
	const uint32_t packed_scale = ( control >> 7u ) & 7u;
	const float scale = 0.9f - float( ( ( packed_scale + 3u ) % 8u ) + 1u ) * 0.1f;
	t3dCompSurfMaterial_t first{};
	if ( !t3d_comp_surf_material( s, base, world, dx, dy, scale, rotation, first ) )
		return false;
	if ( base == overlay ) {
		dst = first;
		return true;
	}
	t3dCompSurfMaterial_t second{};
	if ( !t3d_comp_surf_material( s, overlay, world, dx, dy, scale, rotation, second ) )
		return false;
	const float blend = float( ( control >> 14u ) & 255u ) * ( 1.0f / 255.0f );
	const float sharp = fmaf( 60.0f, s.m_fBlendSharpness, 4.0f );
	const float w_base = powf( fmaxf( 1.0f - blend + first.height, 1.0e-8f ), sharp );
	const float w_overlay = powf( fmaxf( blend + second.height, 1.0e-8f ), sharp );
	const float overlay_weight = w_overlay / fmaxf( w_base + w_overlay, 1.0e-5f );
	memset( &dst, 0, sizeof( dst ) );
	t3d_comp_surf_accumulate( dst, first, 1.0f - overlay_weight );
	t3d_comp_surf_accumulate( dst, second, overlay_weight );
	return true;
}

// GLSL auto_control: the authored auto bit is resolved per output pixel,
// using the same sampled terrain height/normal for all four control corners.
static bool t3d_comp_surf_resolve_auto( const t3dComposedStaticSource_t& s, uint32_t *pControls, uint32_t count, uint32_t px, uint32_t py )
{
	if ( !s.m_bAutoShader )
		return true;
	bool needed = false;
	for ( uint32_t i = 0u; i < count; ++i )
		needed |= ( pControls[i] & 1u ) != 0u;
	if ( !needed )
		return true;
	float height = 0.0f, normal_y = 1.0f;
	if ( !s.m_pReadTerrainSurface( s.m_pUser, px, py, height, normal_y ) || !t3d_comp_surf_finite( height ) ||
	     !t3d_comp_surf_finite( normal_y ) || normal_y < -1.0f || normal_y > 1.0f )
		return false;
	const float blend =
	    t3d_comp_surf_limit( fmaf( s.m_fAutoSlope * 2.0f, normal_y - 1.0f, 1.0f ) - s.m_fAutoHeightReduction * 0.01f * height, 0.0f, 1.0f );
	const uint32_t base = s.m_nAutoBaseTexture;
	const uint32_t overlay = s.m_nAutoOverlayTexture;
	const bool swap = base > overlay;
	const uint32_t weight = uint32_t( floorf( ( swap ? 1.0f - blend : blend ) * 255.0f + 0.5f ) );
	const uint32_t replacement = ( ( swap ? overlay : base ) << 27u ) | ( ( swap ? base : overlay ) << 22u ) | ( ( weight & 255u ) << 14u );
	for ( uint32_t i = 0u; i < count; ++i )
		if ( ( pControls[i] & 1u ) != 0u )
			pControls[i] = replacement;
	return true;
}

static bool t3d_comp_surf_pixel( const t3dComposedStaticSource_t& s, const t3dComposedStaticFootprint_t& foot, const uint32_t x,
                                 const uint32_t y, t3dCompSurfMaterial_t& rResult )
{
	const float fx = float( int32_t( x ) - int32_t( T3D_COMP_SURF_PAGE_GUTTER ) ) + 0.5f;
	const float fy = float( int32_t( y ) - int32_t( T3D_COMP_SURF_PAGE_GUTTER ) ) + 0.5f;
	float world[2] = { foot.m_fWorldOrigin[0] + fx * foot.m_fWorldStepX[0] + fy * foot.m_fWorldStepY[0],
	                   foot.m_fWorldOrigin[1] + fx * foot.m_fWorldStepX[1] + fy * foot.m_fWorldStepY[1] };
	float ct[2] = { foot.m_fControlOrigin[0] + fx * foot.m_fControlStepX[0] + fy * foot.m_fControlStepY[0],
	                foot.m_fControlOrigin[1] + fx * foot.m_fControlStepX[1] + fy * foot.m_fControlStepY[1] };
	// Guard float->integer conversion and +1 neighboring-control indexing.
	if ( !t3d_comp_surf_finite( ct[0] ) || !t3d_comp_surf_finite( ct[1] ) || ct[0] < -2147483000.0f || ct[0] > 2147483000.0f ||
	     ct[1] < -2147483000.0f || ct[1] > 2147483000.0f )
		return false;
	int32_t cell_x = int32_t( floorf( ct[0] ) ), cell_y = int32_t( floorf( ct[1] ) );
	uint32_t controls[4]{};
	if ( !s.m_pReadControl( s.m_pUser, cell_x, cell_y, controls[0] ) )
		return false;
	if ( s.m_nSamplingMode == T3D_COMP_SURF_STATIC_SAMPLE_NEAREST ) {
		if ( !t3d_comp_surf_resolve_auto( s, controls, 1u, x, y ) )
			return false;
		return t3d_comp_surf_control_pair( s, controls[0], world, foot.m_fWorldStepX, foot.m_fWorldStepY, rResult );
	}
	if ( !s.m_pReadControl( s.m_pUser, cell_x + 1, cell_y, controls[1] ) ||
	     !s.m_pReadControl( s.m_pUser, cell_x, cell_y + 1, controls[2] ) ||
	     !s.m_pReadControl( s.m_pUser, cell_x + 1, cell_y + 1, controls[3] ) || !t3d_comp_surf_resolve_auto( s, controls, 4u, x, y ) )
		return false;
	if ( controls[0] == controls[1] && controls[0] == controls[2] && controls[0] == controls[3] ) {
		return t3d_comp_surf_control_pair( s, controls[0], world, foot.m_fWorldStepX, foot.m_fWorldStepY, rResult );
	}
	float tx = ct[0] - floorf( ct[0] ), ty = ct[1] - floorf( ct[1] );
	float w[4] = { ( 1.0f - tx ) * ( 1.0f - ty ), tx * ( 1.0f - ty ), ( 1.0f - tx ) * ty, tx * ty };
	if ( s.m_nSamplingMode == T3D_COMP_SURF_STATIC_SAMPLE_TOP_TWO ) {
		uint32_t first = 0u, second = 1u;
		if ( w[1] > w[0] ) {
			first = 1u;
			second = 0u;
		}
		for ( uint32_t j = 2u; j < 4u; ++j ) {
			if ( w[j] > w[first] ) {
				second = first;
				first = j;
			} else if ( w[j] > w[second] )
				second = j;
		}
		for ( uint32_t j = 0u; j < 4u; ++j )
			if ( j != first && j != second )
				w[j] = 0.0f;
	}
	float sum = 0.0f;
	for ( float& weight : w ) {
		if ( weight <= 1.0e-5f )
			weight = 0.0f;
		sum += weight;
	}
	if ( sum <= 1.0e-5f )
		return false;
	memset( &rResult, 0, sizeof( rResult ) );
	// page reduction: duplicate authored control words share the *entire* material
	// evaluation within this page texel. Preserve each corner's blend weight.
	// This is cold/offline composition, never a new per-fragment cache.
	t3dCompSurfMaterial_t evaluated[4]{};
	bool ready[4]{};
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		if ( w[i] == 0.0f )
			continue;
		uint32_t source_index = i;
		for ( uint32_t j = 0u; j < i; ++j ) {
			if ( ready[j] && controls[j] == controls[i] ) {
				source_index = j;
				break;
			}
		}
		if ( source_index == i ) {
			if ( !t3d_comp_surf_control_pair( s, controls[i], world, foot.m_fWorldStepX, foot.m_fWorldStepY, evaluated[i] ) )
				return false;
			ready[i] = true;
		}
		t3d_comp_surf_accumulate( rResult, evaluated[source_index], w[i] / sum );
	}
	return true;
}

static uint8_t t3d_comp_surf_byte( const float x )
{
	return uint8_t( t3d_comp_surf_limit( floorf( x * 255.0f + 0.5f ), 0.0f, 255.0f ) );
}
static float t3d_comp_surf_linear_to_srgb( const float x )
{
	float linear = t3d_comp_surf_limit( x, 0.0f, 1.0f );
	return linear <= 0.0031308f ? linear * 12.92f : 1.055f * powf( linear, 1.0f / 2.4f ) - 0.055f;
}

// Exact byte packing shared by the single-channel reference and the
// fused three-channel cooker. Preserve the canonical output ordering.
static void t3d_comp_surf_pack_channel_bytes( t3dCompSurfMaterial_t& material, const uint32_t nChannel, uint8_t *pRgba,
                                              const uint32_t index )
{
	if ( nChannel == T3D_COMP_SURF_CHANNEL_ALBEDO ) {
		for ( uint32_t c = 0u; c < 3u; ++c )
			pRgba[index + c] = t3d_comp_surf_byte( t3d_comp_surf_linear_to_srgb( material.albedo[c] ) );
		pRgba[index + 3u] = 255u;
	} else if ( nChannel == T3D_COMP_SURF_CHANNEL_NORMAL ) {
		t3d_comp_surf_normalize( material.normal );
		pRgba[index] = t3d_comp_surf_byte( material.normal[0] * 0.5f + 0.5f );
		pRgba[index + 1u] = t3d_comp_surf_byte( material.normal[2] * 0.5f + 0.5f );
		pRgba[index + 2u] = 255u;
		pRgba[index + 3u] = 255u;
	} else {
		pRgba[index] = t3d_comp_surf_byte( material.ao );
		pRgba[index + 1u] = t3d_comp_surf_byte( material.roughness );
		pRgba[index + 2u] = t3d_comp_surf_byte( material.metallic );
		pRgba[index + 3u] = t3d_comp_surf_byte( material.ao_affect );
	}
}

// P09: evaluate the exact same bounded static-V1 material function for both
// legacy byte/BC sidecar cooking and a pre-encoding float reference. The float
// path does not set or require the authoritative visual-parity approval bit.
// It is an R&D capture endpoint, never a runtime surface publication path.
static bool t3d_comp_surf_static_baseline_evaluate_shared( void *pUser, const t3dComposedSurfaceCookPage_t& page, const uint32_t nChannel,
                                                           uint8_t *pRgba, float *pLinear, const uint32_t nOutputElements )
{
	if ( !pUser || ( pRgba == NULL ) == ( pLinear == NULL ) || nOutputElements != T3D_COMP_SURF_RGBA_SCRATCH_BYTES ||
	     nChannel >= T3D_COMP_SURF_CHANNEL_COUNT )
		return false;
	auto& adapter = *static_cast<t3dComposedStaticCookAdapter_t *>( pUser );
	const auto& s = adapter.m_source;
	if ( !t3d_comp_surf_static_baseline_eligible( s ) )
		return false;
	if ( pRgba && !( s.m_bCanonicalMaterialParityAccepted || s.m_bReferenceOnly ) )
		return false;
	t3dComposedStaticFootprint_t foot{};
	if ( !s.m_pResolvePage( s.m_pUser, page, foot ) )
		return false;
	const float *components = &foot.m_fWorldOrigin[0];
	for ( uint32_t i = 0u; i < sizeof( foot ) / sizeof( float ); ++i ) {
		if ( !t3d_comp_surf_finite( components[i] ) )
			return false;
	}
	for ( uint32_t y = 0u; y < T3D_COMP_SURF_PHYSICAL_TILE; ++y ) {
		for ( uint32_t x = 0u; x < T3D_COMP_SURF_PHYSICAL_TILE; ++x ) {
			t3dCompSurfMaterial_t material{};
			if ( !t3d_comp_surf_pixel( s, foot, x, y, material ) )
				return false;
			const uint32_t index = 4u * ( y * T3D_COMP_SURF_PHYSICAL_TILE + x );
			if ( pRgba ) {
				t3d_comp_surf_pack_channel_bytes( material, nChannel, pRgba, index );
				continue;
			}
			if ( nChannel == T3D_COMP_SURF_CHANNEL_ALBEDO ) {
				if ( pLinear ) {
					for ( uint32_t c = 0u; c < 3u; ++c )
						pLinear[index + c] = material.albedo[c];
					pLinear[index + 3u] = 1.0f;
				}
			} else if ( nChannel == T3D_COMP_SURF_CHANNEL_NORMAL ) {
				t3d_comp_surf_normalize( material.normal );
				if ( pLinear ) {
					pLinear[index] = material.normal[0];
					pLinear[index + 1u] = material.normal[1];
					pLinear[index + 2u] = material.normal[2];
					pLinear[index + 3u] = 1.0f;
				}
			} else {
				if ( pLinear ) {
					pLinear[index] = material.ao;
					pLinear[index + 1u] = material.roughness;
					pLinear[index + 2u] = material.metallic;
					pLinear[index + 3u] = material.ao_affect;
				}
			}
		}
	}
	return true;
}

bool t3d_comp_surf_static_baseline_evaluate_all( void *pUser, const t3dComposedSurfaceCookPage_t& page, uint8_t *pRgbaAll,
                                                 const uint32_t nRgbaBytes )
{
	if ( !pUser || !pRgbaAll || nRgbaBytes != T3D_COMP_SURF_RGBA_SCRATCH_BYTES )
		return false;
	auto& adapter = *static_cast<t3dComposedStaticCookAdapter_t *>( pUser );
	const auto& s = adapter.m_source;
	if ( !t3d_comp_surf_static_baseline_eligible( s ) || !( s.m_bCanonicalMaterialParityAccepted || s.m_bReferenceOnly ) )
		return false;
	t3dComposedStaticFootprint_t foot{};
	if ( !s.m_pResolvePage( s.m_pUser, page, foot ) )
		return false;
	const float *components = &foot.m_fWorldOrigin[0];
	for ( uint32_t i = 0u; i < sizeof( foot ) / sizeof( float ); ++i )
		if ( !t3d_comp_surf_finite( components[i] ) )
			return false;
	for ( uint32_t y = 0u; y < T3D_COMP_SURF_PHYSICAL_TILE; ++y ) {
		for ( uint32_t x = 0u; x < T3D_COMP_SURF_PHYSICAL_TILE; ++x ) {
			t3dCompSurfMaterial_t material{};
			if ( !t3d_comp_surf_pixel( s, foot, x, y, material ) )
				return false;
			const uint32_t index = 4u * ( y * T3D_COMP_SURF_PHYSICAL_TILE + x );
			for ( uint32_t channel = 0u; channel < T3D_COMP_SURF_CHANNEL_COUNT; ++channel )
				t3d_comp_surf_pack_channel_bytes( material, channel, pRgbaAll + channel * nRgbaBytes, index );
		}
	}
	return true;
}

bool t3d_comp_surf_static_baseline_evaluate( void *pUser, const t3dComposedSurfaceCookPage_t& page, const uint32_t nChannel, uint8_t *pRgba,
                                             const uint32_t nBytes )
{
	return t3d_comp_surf_static_baseline_evaluate_shared( pUser, page, nChannel, pRgba, NULL, nBytes );
}

bool t3d_comp_surf_static_baseline_evaluate_linear( void *pUser, const t3dComposedSurfaceCookPage_t& page, const uint32_t nChannel,
                                                    float *pLinear, const uint32_t nFloatCount )
{
	return t3d_comp_surf_static_baseline_evaluate_shared( pUser, page, nChannel, NULL, pLinear, nFloatCount );
}

bool t3d_comp_surf_static_baseline_encode( void *pUser, const uint32_t nChannel, const uint8_t *pRgba, const uint32_t nRgbaBytes,
                                           uint8_t *pCompressed, const uint32_t nCapacity, uint32_t& rWritten )
{
	if ( !pUser )
		return false;
	auto& adapter = *static_cast<t3dComposedStaticCookAdapter_t *>( pUser );
	return adapter.m_pEncoder &&
	       adapter.m_pEncoder( adapter.m_pEncoderUser, nChannel, pRgba, nRgbaBytes, pCompressed, nCapacity, rWritten );
}

bool t3d_comp_surf_static_baseline_make_cook_source( t3dComposedStaticCookAdapter_t& adapter, const uint64_t nFingerprint,
                                                     t3dComposedSurfaceCookSource_t& rCook )
{
	rCook = {};
	if ( !nFingerprint || !adapter.m_pEncoder || !t3d_comp_surf_static_baseline_eligible( adapter.m_source ) ||
	     !( adapter.m_source.m_bCanonicalMaterialParityAccepted || adapter.m_source.m_bReferenceOnly ) )
		return false;
	rCook.m_pUser = &adapter;
	rCook.m_pEvaluate = t3d_comp_surf_static_baseline_evaluate;
	rCook.m_pEvaluateAll = t3d_comp_surf_static_baseline_evaluate_all;
	rCook.m_pEncode = t3d_comp_surf_static_baseline_encode;
	rCook.m_nSourceFingerprint = nFingerprint;
	rCook.m_bCanonicalMaterialParityAccepted = adapter.m_source.m_bCanonicalMaterialParityAccepted;
	rCook.m_bReferenceOnly = adapter.m_source.m_bReferenceOnly;
	return true;
}
