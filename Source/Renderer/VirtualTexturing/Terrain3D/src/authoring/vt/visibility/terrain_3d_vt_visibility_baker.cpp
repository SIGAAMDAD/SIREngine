#include "authoring/vt/visibility/terrain_3d_vt_visibility_baker.h"
#include "rendering/rd/core/terrain_3d_rd_context.h"
#include "rendering/rd/shaders/terrain_3d_rd_shader_catalog.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state_attachment.hpp>
#include <godot_cpp/classes/rd_pipeline_depth_stencil_state.hpp>
#include <godot_cpp/classes/rd_pipeline_multisample_state.hpp>
#include <godot_cpp/classes/rd_pipeline_rasterization_state.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <math.h>
#include <string.h>

using namespace godot;

namespace
{

struct t3dVTProbeGpuVertex_t
{
	float m_position[4];
	float m_uvMaterial[4];
};
static_assert( sizeof( t3dVTProbeGpuVertex_t ) == 32u, "T02 vertex std430 ABI" );

struct t3dVTProbePush_t
{
	float m_eye[4];
	float m_right[4];
	float m_up[4];
	float m_forward[4];
	uint32_t m_parameters[4];
};
static_assert( sizeof( t3dVTProbePush_t ) == 80u, "T02 push constant ABI" );

class t3dVTLocalBakeDevice_t
{
public:
	RenderingDevice *m_rd = nullptr;
	std::vector<RID> m_live;
	~t3dVTLocalBakeDevice_t()
	{
		if ( m_rd ) {
			for ( size_t i = m_live.size(); i > 0u; --i ) {
				if ( m_live[i - 1u].is_valid() )
					m_rd->free_rid( m_live[i - 1u] );
			}
			memdelete( m_rd );
		}
	}
	RID hold( const RID& id )
	{
		if ( id.is_valid() )
			m_live.push_back( id );
		return id;
	}
	void releaseAfter( const size_t first )
	{
		while ( m_live.size() > first ) {
			if ( m_live.back().is_valid() )
				m_rd->free_rid( m_live.back() );
			m_live.pop_back();
		}
	}
};

bool t3dVTProbeValid( const t3dVTVisibilityStaticSceneView_t& scene, const t3dVTDemandVisibilityCookConfig_t& config )
{
	if ( !scene.m_pProbes || !scene.m_pTriangles || !scene.m_pVertices || !scene.m_pMaterialKeys || scene.m_nSourceSignature == 0u ||
	     config.m_nSourceSignature != scene.m_nSourceSignature || config.m_nPackageSignature == 0u || scene.m_nMaterialCount == 0u ||
	     scene.m_nMaterialCount > T3D_VT_VISIBILITY_MAX_MATERIALS || scene.m_nProbeCount == 0u ||
	     scene.m_nProbeCount > T3D_VT_VISIBILITY_MAX_PROBES || scene.m_nVertexCount < 3u || scene.m_nVertexCount > 1048576u ||
	     scene.m_nTriangleCount == 0u || scene.m_nTriangleCount > 1048576u || config.m_nNominalTextureSize < 16u ||
	     config.m_nNominalTextureSize > 16384u )
		return false;
	for ( uint32_t i = 0u; i < scene.m_nMaterialCount; ++i ) {
		if ( scene.m_pMaterialKeys[i] == 0u || ( i > 0u && scene.m_pMaterialKeys[i] <= scene.m_pMaterialKeys[i - 1u] ) )
			return false;
	}
	for ( uint32_t i = 0u; i < scene.m_nProbeCount; ++i ) {
		const auto& p = scene.m_pProbes[i];
		if ( p.m_nSourceRevision == 0u || p.m_nNominalRenderSize < 64u || p.m_nNominalRenderSize > 512u || !isfinite( p.m_fWorldX ) ||
		     !isfinite( p.m_fWorldY ) || !isfinite( p.m_fWorldZ ) || !isfinite( p.m_fFovDegrees ) || p.m_fFovDegrees < 30.0f ||
		     p.m_fFovDegrees > 120.0f || p.m_nTriangleCount == 0u || p.m_nTriangleCount > T3D_VT_VISIBILITY_MAX_TRIANGLES_PER_PROBE ||
		     p.m_nFirstTriangle > scene.m_nTriangleCount || p.m_nTriangleCount > scene.m_nTriangleCount - p.m_nFirstTriangle )
			return false;
	}
	for ( uint32_t i = 0u; i < scene.m_nTriangleCount; ++i ) {
		const auto& t = scene.m_pTriangles[i];
		if ( t.m_nMaterialKeyIndex >= scene.m_nMaterialCount )
			return false;
		for ( uint32_t v = 0u; v < 3u; ++v ) {
			if ( t.m_nIndices[v] >= scene.m_nVertexCount )
				return false;
		}
	}
	for ( uint32_t i = 0u; i < scene.m_nVertexCount; ++i ) {
		const auto& v = scene.m_pVertices[i];
		if ( !isfinite( v.m_fWorldX ) || !isfinite( v.m_fWorldY ) || !isfinite( v.m_fWorldZ ) || !isfinite( v.m_fU ) ||
		     !isfinite( v.m_fV ) )
			return false;
	}
	return true;
}

bool t3dVTProbeShaderSource( Ref<RDShaderSource>& out )
{
	const String text = FileAccess::get_file_as_string( t3dRdVisibilityBakeSourcePath() );
	const int32_t vertexTag = text.find( "#[vertex]" );
	const int32_t fragmentTag = text.find( "#[fragment]" );
	if ( vertexTag < 0 || fragmentTag <= vertexTag )
		return false;
	const int32_t vertexStart = vertexTag + 9;
	const int32_t fragmentStart = fragmentTag + 11;
	Ref<RDShaderSource> shader;
	shader.instantiate();
	shader->set_language( RenderingDevice::SHADER_LANGUAGE_GLSL );
	shader->set_stage_source( RenderingDevice::SHADER_STAGE_VERTEX, text.substr( vertexStart, fragmentTag - vertexStart ) );
	shader->set_stage_source( RenderingDevice::SHADER_STAGE_FRAGMENT, text.substr( fragmentStart ) );
	out = shader;
	return true;
}

RID t3dVTProbeTexture( t3dVTLocalBakeDevice_t& dev, RenderingDevice::DataFormat format, uint32_t size,
                       RenderingDevice::TextureUsageBits usage )
{
	Ref<RDTextureFormat> tf;
	tf.instantiate();
	tf->set_format( format );
	tf->set_width( size );
	tf->set_height( size );
	tf->set_texture_type( RenderingDevice::TEXTURE_TYPE_2D );
	tf->set_array_layers( 1u );
	tf->set_mipmaps( 1u );
	tf->set_samples( RenderingDevice::TEXTURE_SAMPLES_1 );
	tf->set_usage_bits( usage );
	Ref<RDTextureView> tv;
	tv.instantiate();
	return dev.hold( dev.m_rd->texture_create( tf, tv ) );
}

void t3dVTProbeView( t3dVTProbePush_t& pc, const t3dVTVisibilityProbeInput_t& probe, uint32_t face, uint32_t textureSize,
                     uint32_t materialCount )
{
	memset( &pc, 0, sizeof( pc ) );
	pc.m_eye[0] = probe.m_fWorldX;
	pc.m_eye[1] = probe.m_fWorldY;
	pc.m_eye[2] = probe.m_fWorldZ;
	// 8 horizontal yaw probes include all six cubemap directions with extra
	// diagonal views; +Y/-Y contributions are conservatively copied to 8 bins.
	pc.m_eye[3] = 1.0f / tanf( probe.m_fFovDegrees * 0.008726646259971648f );
	float dx, dy, dz;
	if ( face < 8u ) {
		dx = cosf( ( float )face * 0.7853981633974483f );
		dy = 0.0f;
		dz = sinf( ( float )face * 0.7853981633974483f );
		pc.m_right[0] = -dz;
		pc.m_right[2] = dx;
		pc.m_up[1] = 1.0f;
	} else {
		dx = dz = 0.0f;
		dy = face == 8u ? 1.0f : -1.0f;
		pc.m_right[0] = 1.0f;
		pc.m_up[2] = face == 8u ? -1.0f : 1.0f;
	}
	pc.m_forward[0] = dx;
	pc.m_forward[1] = dy;
	pc.m_forward[2] = dz;
	pc.m_parameters[0] = face;
	pc.m_parameters[1] = textureSize;
	pc.m_parameters[2] = materialCount;
}

PackedByteArray t3dVTProbeBytes( const void *p, const size_t count )
{
	PackedByteArray b;
	b.resize( ( int64_t )count );
	if ( count != 0u )
		memcpy( b.ptrw(), p, count );
	return b;
}

} // namespace

bool Terrain3DVTVisibilityBaker::selectCellProbes( const t3dVTVisibilityStaticVertex_t *vertices, const uint32_t nVertices,
                                                   const t3dVTVisibilityStaticTriangle_t *triangles, const uint32_t firstTriangle,
                                                   const uint32_t nTriangles, const int32_t cellX, const int32_t cellZ,
                                                   const uint32_t revision, const uint32_t renderSize,
                                                   std::vector<t3dVTVisibilityProbeInput_t>& probes )
{
	probes.clear();
	if ( !vertices || !triangles || nVertices < 3u || nVertices > 1048576u || nTriangles == 0u ||
	     nTriangles > T3D_VT_VISIBILITY_MAX_TRIANGLES_PER_PROBE || firstTriangle > 1048576u || nTriangles > 1048576u - firstTriangle ||
	     revision == 0u || renderSize < 64u || renderSize > 512u )
		return false;
	// Fixed ordering makes probe selection independent of allocation or
	// triangle traversal order. Every accepted camera is 1.7m above a known
	// opaque walkable surface; unknown cells remain reactive, never guessed.
	static const float positions[5][2] = { { 0.5f, 0.5f }, { 0.25f, 0.25f }, { 0.75f, 0.25f }, { 0.25f, 0.75f }, { 0.75f, 0.75f } };
	std::vector<t3dVTVisibilityProbeInput_t> selected;
	selected.reserve( 5u );
	for ( uint32_t i = 0u; i < 5u; ++i ) {
		const float x = ( ( float )cellX + positions[i][0] ) * 128.0f;
		const float z = ( ( float )cellZ + positions[i][1] ) * 128.0f;
		float highest = -INFINITY;
		for ( uint32_t j = 0u; j < nTriangles; ++j ) {
			const auto& t = triangles[firstTriangle + j];
			if ( t.m_nFlags != 0u || t.m_nIndices[0] >= nVertices || t.m_nIndices[1] >= nVertices || t.m_nIndices[2] >= nVertices )
				continue;
			const auto& a = vertices[t.m_nIndices[0]];
			const auto& b = vertices[t.m_nIndices[1]];
			const auto& c = vertices[t.m_nIndices[2]];
			if ( !isfinite( a.m_fWorldX ) || !isfinite( a.m_fWorldY ) || !isfinite( a.m_fWorldZ ) || !isfinite( b.m_fWorldX ) ||
			     !isfinite( b.m_fWorldY ) || !isfinite( b.m_fWorldZ ) || !isfinite( c.m_fWorldX ) || !isfinite( c.m_fWorldY ) ||
			     !isfinite( c.m_fWorldZ ) )
				continue;
			const float ux = b.m_fWorldX - a.m_fWorldX;
			const float uy = b.m_fWorldY - a.m_fWorldY;
			const float uz = b.m_fWorldZ - a.m_fWorldZ;
			const float vx = c.m_fWorldX - a.m_fWorldX;
			const float vy = c.m_fWorldY - a.m_fWorldY;
			const float vz = c.m_fWorldZ - a.m_fWorldZ;
			const float nx = uy * vz - uz * vy;
			const float ny = uz * vx - ux * vz;
			const float nz = ux * vy - uy * vx;
			const float normalSq = nx * nx + ny * ny + nz * nz;
			if ( !isfinite( normalSq ) || normalSq < 0.000001f || ny * ny < 0.3025f * normalSq )
				continue;
			// Barycentric intersection on XZ; accept either triangle winding.
			const float denominator = ( b.m_fWorldZ - c.m_fWorldZ ) * ( a.m_fWorldX - c.m_fWorldX ) +
			                          ( c.m_fWorldX - b.m_fWorldX ) * ( a.m_fWorldZ - c.m_fWorldZ );
			if ( fabsf( denominator ) < 0.00001f )
				continue;
			const float u =
			    ( ( b.m_fWorldZ - c.m_fWorldZ ) * ( x - c.m_fWorldX ) + ( c.m_fWorldX - b.m_fWorldX ) * ( z - c.m_fWorldZ ) ) / denominator;
			const float v =
			    ( ( c.m_fWorldZ - a.m_fWorldZ ) * ( x - c.m_fWorldX ) + ( a.m_fWorldX - c.m_fWorldX ) * ( z - c.m_fWorldZ ) ) / denominator;
			const float w = 1.0f - u - v;
			if ( u < -0.00001f || v < -0.00001f || w < -0.00001f )
				continue;
			const float height = u * a.m_fWorldY + v * b.m_fWorldY + w * c.m_fWorldY;
			if ( isfinite( height ) && height > highest )
				highest = height;
		}
		if ( !isfinite( highest ) )
			continue;
		t3dVTVisibilityProbeInput_t probe;
		probe.m_nCellX = cellX;
		probe.m_nCellZ = cellZ;
		probe.m_fWorldX = x;
		probe.m_fWorldY = highest + 1.7f;
		probe.m_fWorldZ = z;
		probe.m_nSourceRevision = revision;
		probe.m_nFirstTriangle = firstTriangle;
		probe.m_nTriangleCount = nTriangles;
		probe.m_nNominalRenderSize = renderSize;
		selected.push_back( probe );
	}
	if ( selected.empty() )
		return false;
	probes = eastl::move( selected );
	return true;
}

bool Terrain3DVTVisibilityBaker::bake( const t3dVTVisibilityStaticSceneView_t& scene, const t3dVTDemandVisibilityCookConfig_t& config,
                                       std::vector<t3dVTVisibilityProbeResult_t>& results, t3dVTVisibilityBakeStats_t& stats ) const
{
	results.clear();
	stats = t3dVTVisibilityBakeStats_t{};
	if ( !t3dVTProbeValid( scene, config ) )
		return false;
	t3dVTLocalBakeDevice_t dev;
	dev.m_rd = Terrain3DRdContext::createLocalDevice();
	if ( !dev.m_rd )
		return false;
	Ref<RDShaderSource> source;
	if ( !t3dVTProbeShaderSource( source ) )
		return false;
	const Ref<RDShaderSPIRV> spirv = dev.m_rd->shader_compile_spirv_from_source( source );
	if ( spirv.is_null() )
		return false;
	const RID shader = dev.hold( dev.m_rd->shader_create_from_spirv( spirv, "T3D.T02.VisibilityProbe" ) );
	if ( !shader.is_valid() )
		return false;

	Ref<RDPipelineRasterizationState> raster;
	raster.instantiate();
	raster->set_cull_mode( RenderingDevice::POLYGON_CULL_DISABLED );
	Ref<RDPipelineMultisampleState> multisample;
	multisample.instantiate();
	Ref<RDPipelineDepthStencilState> depthState;
	depthState.instantiate();
	depthState->set_enable_depth_test( true );
	depthState->set_enable_depth_write( true );
	depthState->set_depth_compare_operator( RenderingDevice::COMPARE_OP_LESS );
	Ref<RDPipelineDepthStencilState> histogramDepth;
	histogramDepth.instantiate();
	histogramDepth->set_enable_depth_test( true );
	histogramDepth->set_enable_depth_write( false );
	histogramDepth->set_depth_compare_operator( RenderingDevice::COMPARE_OP_EQUAL );
	Ref<RDPipelineColorBlendState> blend;
	blend.instantiate();
	Ref<RDPipelineColorBlendStateAttachment> attachment;
	attachment.instantiate();
	TypedArray<Ref<RDPipelineColorBlendStateAttachment>> attachments;
	attachments.push_back( attachment );
	blend->set_attachments( attachments );

	const uint32_t nCounters = scene.m_nMaterialCount * 8u * T3D_VT_DEMAND_BAKED_VISIBILITY_MIP_BINS;
	const uint32_t nCounterBytes = nCounters * sizeof( uint32_t );
	const RID histogram = dev.hold( dev.m_rd->storage_buffer_create( nCounterBytes ) );
	if ( !histogram.is_valid() )
		return false;

	std::vector<t3dVTVisibilityProbeResult_t> output;
	output.reserve( scene.m_nProbeCount );
	for ( uint32_t probeIndex = 0u; probeIndex < scene.m_nProbeCount; ++probeIndex ) {
		const auto& probe = scene.m_pProbes[probeIndex];
		std::vector<t3dVTProbeGpuVertex_t> vertices;
		vertices.reserve( ( size_t )probe.m_nTriangleCount * 3u );
		uint32_t unsupported = 0u;
		for ( uint32_t ti = 0u; ti < probe.m_nTriangleCount; ++ti ) {
			const auto& tri = scene.m_pTriangles[probe.m_nFirstTriangle + ti];
			if ( tri.m_nFlags != 0u ) {
				++unsupported;
				continue;
			}
			for ( uint32_t v = 0u; v < 3u; ++v ) {
				const auto& in = scene.m_pVertices[tri.m_nIndices[v]];
				t3dVTProbeGpuVertex_t expanded{};
				expanded.m_position[0] = in.m_fWorldX;
				expanded.m_position[1] = in.m_fWorldY;
				expanded.m_position[2] = in.m_fWorldZ;
				expanded.m_position[3] = 1.0f;
				expanded.m_uvMaterial[0] = in.m_fU;
				expanded.m_uvMaterial[1] = in.m_fV;
				expanded.m_uvMaterial[2] = ( float )tri.m_nMaterialKeyIndex;
				vertices.push_back( expanded );
			}
		}
		t3dVTVisibilityProbeResult_t r;
		r.m_nCellX = probe.m_nCellX;
		r.m_nCellZ = probe.m_nCellZ;
		r.m_nSourceRevision = probe.m_nSourceRevision;
		r.m_nMaterialCount = scene.m_nMaterialCount;
		r.m_nNominalRenderSize = probe.m_nNominalRenderSize;
		r.m_nNominalTextureSize = config.m_nNominalTextureSize;
		r.m_nOpaqueTriangles = ( uint32_t )( vertices.size() / 3u );
		r.m_nUncapturedTriangles = unsupported;
		r.m_bPartial = unsupported != 0u;
		r.m_counts.resize( nCounters, 0u );
		stats.m_nUncapturedTriangles += unsupported;
		if ( !vertices.empty() ) {
			const size_t firstProbeRid = dev.m_live.size();
			// All attached resources belong to this local RD and are released
			// before its lifetime ends. Cold probe work may allocate.
			const RID geometry =
			    dev.hold( dev.m_rd->storage_buffer_create( ( uint32_t )( vertices.size() * sizeof( vertices[0] ) ),
				                                           t3dVTProbeBytes( vertices.data(), vertices.size() * sizeof( vertices[0] ) ) ) );
			if ( !geometry.is_valid() )
				return false;
			TypedArray<Ref<RDUniform>> uniforms;
			for ( uint32_t binding = 0u; binding < 2u; ++binding ) {
				Ref<RDUniform> uniform;
				uniform.instantiate();
				uniform->set_binding( ( int32_t )binding );
				uniform->set_uniform_type( RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER );
				uniform->add_id( binding == 0u ? geometry : histogram );
				uniforms.push_back( uniform );
			}
			const RID uniformSet = dev.hold( dev.m_rd->uniform_set_create( uniforms, shader, 0u ) );
			if ( !uniformSet.is_valid() )
				return false;
			const RID color = t3dVTProbeTexture( dev, RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM, probe.m_nNominalRenderSize,
			                                     RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT );
			const RID depth = t3dVTProbeTexture( dev, RenderingDevice::DATA_FORMAT_D32_SFLOAT, probe.m_nNominalRenderSize,
			                                     RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT );
			if ( !color.is_valid() || !depth.is_valid() )
				return false;
			TypedArray<RID> textures;
			textures.push_back( color );
			textures.push_back( depth );
			const RID framebuffer = dev.hold( dev.m_rd->framebuffer_create( textures ) );
			if ( !framebuffer.is_valid() )
				return false;
			const RID depthPipeline = dev.hold( dev.m_rd->render_pipeline_create(
			    shader, dev.m_rd->framebuffer_get_format( framebuffer ), RenderingDevice::INVALID_FORMAT_ID,
			    RenderingDevice::RENDER_PRIMITIVE_TRIANGLES, raster, multisample, depthState, blend ) );
			const RID histogramPipeline = dev.hold( dev.m_rd->render_pipeline_create(
			    shader, dev.m_rd->framebuffer_get_format( framebuffer ), RenderingDevice::INVALID_FORMAT_ID,
			    RenderingDevice::RENDER_PRIMITIVE_TRIANGLES, raster, multisample, histogramDepth, blend ) );
			if ( !depthPipeline.is_valid() || !histogramPipeline.is_valid() )
				return false;
			if ( dev.m_rd->buffer_clear( histogram, 0u, nCounterBytes ) != godot::OK )
				return false;
			PackedColorArray clears;
			clears.push_back( Color( 0, 0, 0, 0 ) );
			for ( uint32_t face = 0u; face < 10u; ++face ) {
				t3dVTProbePush_t pc{};
				t3dVTProbeView( pc, probe, face, config.m_nNominalTextureSize, scene.m_nMaterialCount );
				const int64_t draw = dev.m_rd->draw_list_begin( framebuffer, RenderingDevice::DRAW_DEFAULT_ALL, clears, 1.0f );
				dev.m_rd->draw_list_bind_uniform_set( draw, uniformSet, 0u );
				// Pass 1: finish depth writes for every opaque triangle.
				pc.m_parameters[3] = 1u;
				const PackedByteArray depthPush = t3dVTProbeBytes( &pc, sizeof( pc ) );
				dev.m_rd->draw_list_bind_render_pipeline( draw, depthPipeline );
				dev.m_rd->draw_list_set_push_constant( draw, depthPush, sizeof( pc ) );
				dev.m_rd->draw_list_draw( draw, false, 1u, ( uint32_t )vertices.size() );
				// Pass 2: depth-equal, no depth writes. Only final visible
				// fragments contribute to the histogram.
				pc.m_parameters[3] = 0u;
				const PackedByteArray histogramPush = t3dVTProbeBytes( &pc, sizeof( pc ) );
				dev.m_rd->draw_list_bind_render_pipeline( draw, histogramPipeline );
				dev.m_rd->draw_list_set_push_constant( draw, histogramPush, sizeof( pc ) );
				dev.m_rd->draw_list_draw( draw, false, 1u, ( uint32_t )vertices.size() );
				dev.m_rd->draw_list_end();
				stats.m_nGpuPasses += 2u;
			}
			dev.m_rd->submit();
			dev.m_rd->sync(); // local RD only, cold bake completion
			const PackedByteArray bytes = dev.m_rd->buffer_get_data( histogram, 0u, nCounterBytes );
			if ( ( uint32_t )bytes.size() != nCounterBytes )
				return false;
			memcpy( r.m_counts.data(), bytes.ptr(), nCounterBytes );
			stats.m_nReadbackBytes += nCounterBytes;
			for ( const uint32_t count : r.m_counts ) {
				stats.m_nGpuVisibleSamples += count;
				r.m_bHasVisibleSamples |= count != 0u;
				// Exact saturation is impossible at <=512x512x10 per probe,
				// but an unexpected full counter is treated as an error.
				if ( count == UINT32_MAX )
					++stats.m_nOverflowedCounters;
			}
			if ( stats.m_nOverflowedCounters != 0u )
				return false;
			dev.releaseAfter( firstProbeRid );
		}
		output.push_back( eastl::move( r ) );
		++stats.m_nCapturedProbes;
	}
	results = eastl::move( output );
	return true;
}
