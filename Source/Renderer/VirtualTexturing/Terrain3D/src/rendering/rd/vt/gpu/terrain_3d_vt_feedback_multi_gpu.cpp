// Copyright (c) 2023-2026 Terrain3D contributors.
#include "rendering/rd/vt/gpu/terrain_3d_vt_feedback_multi_gpu.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/core/terrain_3d_rd_context.h"
#include "rendering/rd/core/terrain_3d_gpu_transfer_budget.h"
#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_descriptor_recipe.h"
#include "rendering/rd/infrastructure/descriptors/terrain_3d_rd_global_descriptor_cache.h"
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <string.h>

using namespace godot;

static bool t3d_vt_feedback_multi_texture_valid( RenderingDevice *pRd, const RID& rTexture, const RenderingDevice::TextureType eType,
                                                 const uint32_t nWidth, const uint32_t nHeight, const uint32_t nLayers )
{
	if ( !pRd || !pRd->texture_is_valid( rTexture ) )
		return false;
	const Ref<RDTextureFormat> format = pRd->texture_get_format( rTexture );
	return format.is_valid() && format->get_format() == RenderingDevice::DATA_FORMAT_R32_UINT && format->get_texture_type() == eType &&
	       format->get_width() == nWidth && format->get_height() == nHeight && format->get_array_layers() == nLayers &&
	       format->get_mipmaps() == 1u && format->get_samples() == RenderingDevice::TEXTURE_SAMPLES_1 &&
	       ( ( uint64_t )format->get_usage_bits() & RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT ) != 0u;
}

bool Terrain3DVTFeedbackMultiGpu::initializeCold( Terrain3DRdContext& rContext, const t3dVTFeedbackGridLayout_t& rLayout, const RID& rPages,
                                                  const RID& rCounts, const uint32_t nViewportWidth, const uint32_t nViewportHeight,
                                                  const uint32_t nSourceLayers, const uint32_t nPageCount )
{
	RenderingDevice *const rd = rContext.getDevice();
	if ( !Terrain3DRdContext::isOnRenderThread() || !rd || ready() || m_shader.isValid() || m_output.isValid() || m_pipeline.isValid() ||
	     !t3d_vt_feedback_multi_params( rLayout, nViewportWidth, nViewportHeight, 0u, nPageCount, nSourceLayers, m_params ) ||
	     !t3d_vt_feedback_multi_texture_valid( rd, rPages, RenderingDevice::TEXTURE_TYPE_2D_ARRAY, nViewportWidth, nViewportHeight,
	                                           nSourceLayers ) ||
	     !t3d_vt_feedback_multi_texture_valid( rd, rCounts, RenderingDevice::TEXTURE_TYPE_2D, nViewportWidth, nViewportHeight, 1u ) )
		return false;

	m_layout = rLayout;
	m_nDeviceGeneration = rContext.getDeviceGeneration();
	if ( !m_shader.createTracked( rContext, T3D_RD_CONTEXT_OWNER_TERRAIN, "T3D.VT.MultiFeedback.Shader",
	                              T3D_RD_SHADER_RETIRED_VT_FEEDBACK_MULTI ) ||
	     !m_pipeline.createTrackedCompute( rContext, T3D_RD_CONTEXT_OWNER_TERRAIN, "T3D.VT.MultiFeedback.Pipeline", m_shader.rid() ) ) {
		shutdownCold( rContext );
		return false;
	}
	Ref<RDSamplerState> samplerState;
	samplerState.instantiate();
	samplerState->set_min_filter( RenderingDevice::SAMPLER_FILTER_NEAREST );
	samplerState->set_mag_filter( RenderingDevice::SAMPLER_FILTER_NEAREST );
	if ( !m_sampler.createTracked( rContext, T3D_RD_CONTEXT_OWNER_TERRAIN, "T3D.VT.MultiFeedback.Sampler", samplerState ) ) {
		shutdownCold( rContext );
		return false;
	}
	t3dRdGpuBufferSpec_t bufferSpec;
	bufferSpec.m_pName = "T3D.VT.MultiFeedback.Records";
	bufferSpec.m_owner = T3D_RD_CONTEXT_OWNER_TERRAIN;
	bufferSpec.m_type = T3D_RD_CONTEXT_RESOURCE_STORAGE_BUFFER;
	bufferSpec.m_memory = T3D_RD_CONTEXT_MEMORY_STORAGE;
	if ( !m_output.configure( bufferSpec ) || !m_output.createTracked( rContext, rLayout.m_nTotalBytes ) ) {
		shutdownCold( rContext );
		return false;
	}
	Terrain3DRdGlobalDescriptorCache *const cache = rContext.tryGetGlobalDescriptorCacheHot();
	if ( !cache ) {
		shutdownCold( rContext );
		return false;
	}
	const t3dRdDescriptorDeviceKey_t device{ T3D_RD_DESCRIPTOR_GLOBAL_DEVICE_ID, rContext.getDeviceGeneration(),
	                                         T3D_RD_DESCRIPTOR_DEVICE_GLOBAL };
	t3dRdDescriptorRecipe_t recipe{};
	const RID pages[2] = { m_sampler.rid(), rPages };
	const RID counts[2] = { m_sampler.rid(), rCounts };
	const RID destination[1] = { m_output.rid() };
	if ( t3dRdDescriptorRecipeBeginCold( recipe, device, m_shader.rid(), 0u ) != T3D_RD_DESCRIPTOR_OK ||
	     t3dRdDescriptorRecipeAddBindingCold( recipe, 0u, RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, pages, 2u ) !=
	         T3D_RD_DESCRIPTOR_OK ||
	     t3dRdDescriptorRecipeAddBindingCold( recipe, 1u, RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, counts, 2u ) !=
	         T3D_RD_DESCRIPTOR_OK ||
	     t3dRdDescriptorRecipeAddBindingCold( recipe, 2u, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER, destination, 1u ) !=
	         T3D_RD_DESCRIPTOR_OK ||
	     cache->acquireDescriptorCold( recipe, T3D_RD_CONTEXT_OWNER_TERRAIN, m_lease ) != T3D_RD_DESCRIPTOR_OK ) {
		shutdownCold( rContext );
		return false;
	}
	m_headerBytes.resize( sizeof( t3dVTFeedbackGridHeader_t ) );
	m_pushBytes.resize( sizeof( t3dVTFeedbackMultiParams_t ) );
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
		m_slots[i].reset();
		m_readback[i].resize( rLayout.m_nSlotBytes );
		if ( m_readback[i].size() != rLayout.m_nSlotBytes ) {
			shutdownCold( rContext );
			return false;
		}
	}
	m_nNextSlot = 0u;
	m_sourcePages = rPages;
	m_sourceCounts = rCounts;
	return true;
}

// P31d: tracked and reusable replay textures. These do not originate from the
// Forward+ raster pass; they can be fed by a verified offline capture.
bool Terrain3DVTFeedbackMultiGpu::initializeOwnedCold( Terrain3DRdContext& rContext, const t3dVTFeedbackGridLayout_t& rLayout,
                                                       const uint32_t nViewportWidth, const uint32_t nViewportHeight,
                                                       const uint32_t nSourceLayers, const uint32_t nPageCount )
{
	uint64_t pixelBytes = 0u, totalBytes = 0u;
	t3dVTFeedbackMultiParams_t params{};
	if ( ready() || m_ownedPages.isValid() || m_ownedCounts.isValid() || !Terrain3DRdContext::isOnRenderThread() ||
	     !t3d_vt_feedback_multi_params( rLayout, nViewportWidth, nViewportHeight, 0u, nPageCount, nSourceLayers, params ) ||
	     !t3d_vt_feedback_multi_capture_bytes( nViewportWidth, nViewportHeight, nSourceLayers, pixelBytes, totalBytes ) )
		return false;
	const uint64_t usage = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
	t3dRdTextureSpec_t pages{};
	pages.m_name = "T3D.VT.MultiFeedback.Replay.Pages";
	pages.m_owner = T3D_RD_CONTEXT_OWNER_TERRAIN;
	pages.m_memory = T3D_RD_CONTEXT_MEMORY_TERRAIN_TEXTURE;
	// Optional replay allocation failures must not fail the shipping renderer.
	pages.m_bFailureFatal = false;
	pages.m_layout.m_format = RenderingDevice::DATA_FORMAT_R32_UINT;
	pages.m_layout.m_type = RenderingDevice::TEXTURE_TYPE_2D_ARRAY;
	pages.m_layout.m_nWidth = nViewportWidth;
	pages.m_layout.m_nHeight = nViewportHeight;
	pages.m_layout.m_nLayers = nSourceLayers;
	pages.m_layout.m_nMipmaps = 1u;
	pages.m_layout.m_nBytes = pixelBytes * nSourceLayers;
	pages.m_layout.m_nUsageBits = usage;
	t3dRdTextureSpec_t counts = pages;
	counts.m_name = "T3D.VT.MultiFeedback.Replay.Counts";
	counts.m_layout.m_type = RenderingDevice::TEXTURE_TYPE_2D;
	counts.m_layout.m_nLayers = 1u;
	counts.m_layout.m_nBytes = pixelBytes;
	if ( !m_ownedPages.createTracked( rContext, pages ) || !m_ownedCounts.createTracked( rContext, counts ) ) {
		shutdownCold( rContext );
		return false;
	}
	for ( uint32_t i = 0u; i < nSourceLayers; ++i ) {
		m_pageUpload[i].resize( ( int64_t )pixelBytes );
		if ( m_pageUpload[i].size() != ( int64_t )pixelBytes ) {
			shutdownCold( rContext );
			return false;
		}
	}
	m_countUpload.resize( ( int64_t )pixelBytes );
	if ( m_countUpload.size() != ( int64_t )pixelBytes || !initializeCold( rContext, rLayout, m_ownedPages.rid(), m_ownedCounts.rid(),
	                                                                       nViewportWidth, nViewportHeight, nSourceLayers, nPageCount ) ) {
		shutdownCold( rContext );
		return false;
	}
	return true;
}

bool Terrain3DVTFeedbackMultiGpu::publishOwnedCaptureHot( Terrain3DRdContext& rContext, const t3dVTFeedbackMultiCaptureView_t& rCapture,
                                                          const t3dVTFeedbackMultiSourceProof_t& rProof )
{
	m_bOwnedCapturePublished = false;
	if ( !Terrain3DRdContext::isOnRenderThread() || !ready() || !hasOwnedSource() || !rContext.getDevice() ||
	     m_nDeviceGeneration != rContext.getDeviceGeneration() || rProof.m_nFrame == 0u ||
	     !t3d_vt_feedback_multi_source_complete( rProof, m_params, rProof.m_nFeedbackEpoch, rProof.m_nDatabaseGeneration,
	                                             ( uint32_t )m_nDeviceGeneration, rProof.m_nViewport ) ||
	     !t3d_vt_feedback_multi_capture_valid( rCapture, m_params ) )
		return false;
	// This source pair is single-buffered. Reusing it while a GPU capture or
	// readback owns the previous contents is prohibited.
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i )
		if ( m_slots[i].m_eStatus != T3D_VT_FEEDBACK_SLOT_FREE )
			return false;
	const uint64_t pixelBytes = ( uint64_t )m_params.m_nViewportWidth * m_params.m_nViewportHeight * sizeof( uint32_t );
	const uint64_t pixels = pixelBytes / sizeof( uint32_t );
	for ( uint32_t layer = 0u; layer < m_params.m_nSourceLayers; ++layer ) {
		memcpy( m_pageUpload[layer].ptrw(), rCapture.m_pPages + layer * pixels, ( size_t )pixelBytes );
		if ( !m_ownedPages.updateLayerTracked( rContext, layer, m_pageUpload[layer] ) )
			return false;
	}
	memcpy( m_countUpload.ptrw(), rCapture.m_pCounts, ( size_t )pixelBytes );
	if ( !m_ownedCounts.updateLayerTracked( rContext, 0u, m_countUpload ) )
		return false;
	m_publishedProof = rProof;
	m_bOwnedCapturePublished = true;
	return true;
}

bool Terrain3DVTFeedbackMultiGpu::dispatchHot( Terrain3DRdContext& rContext, const t3dVTFeedbackMultiSourceProof_t& rProof,
                                               const uint32_t nFrame, const uint32_t nEpoch, const uint32_t nDatabase,
                                               const uint32_t nDevice, const uint64_t nViewport, uint32_t& rSlot )
{
	RenderingDevice *const rd = rContext.getDevice();
	rSlot = UINT32_MAX;
	if ( !Terrain3DRdContext::isOnRenderThread() || !rd || !ready() || rContext.getDeviceGeneration() != m_nDeviceGeneration ||
	     ( uint32_t )m_nDeviceGeneration != nDevice ||
	     !t3d_vt_feedback_multi_texture_valid( rd, m_sourcePages, RenderingDevice::TEXTURE_TYPE_2D_ARRAY, m_params.m_nViewportWidth,
	                                           m_params.m_nViewportHeight, m_params.m_nSourceLayers ) ||
	     !t3d_vt_feedback_multi_texture_valid( rd, m_sourceCounts, RenderingDevice::TEXTURE_TYPE_2D, m_params.m_nViewportWidth,
	                                           m_params.m_nViewportHeight, 1u ) ||
	     ( hasOwnedSource() && ( !m_bOwnedCapturePublished || !t3d_vt_feedback_multi_same_source_frame( rProof, m_publishedProof ) ) ) ||
	     rProof.m_nFrame != nFrame || !t3d_vt_feedback_multi_source_complete( rProof, m_params, nEpoch, nDatabase, nDevice, nViewport ) )
		return false;
	uint32_t slot = UINT32_MAX;
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
		const uint32_t candidate = ( i + m_nNextSlot ) % T3D_VT_FEEDBACK_GRID_SLOT_COUNT;
		if ( m_slots[candidate].m_eStatus == T3D_VT_FEEDBACK_SLOT_FREE ) {
			slot = candidate;
			break;
		}
	}
	if ( slot == UINT32_MAX ) {
		++m_nSkipped;
		return false;
	}
	m_params.m_nSlot = slot;
	const t3dVTFeedbackGridHeader_t header = t3d_vt_feedback_multi_header( m_params, nFrame, nEpoch, nDatabase, nDevice, nViewport );
	memcpy( m_headerBytes.ptrw(), &header, sizeof( header ) );
	memcpy( m_pushBytes.ptrw(), &m_params, sizeof( m_params ) );
	if ( rd->buffer_update( m_output.rid(), slot * m_layout.m_nSlotBytes, sizeof( header ), m_headerBytes ) != OK ) {
		++m_nSkipped;
		return false;
	}
	Terrain3DRdGlobalDescriptorCache *const cache = rContext.tryGetGlobalDescriptorCacheHot();
	RID set;
	if ( !cache || cache->resolveDescriptorHot( m_lease, cache->currentSubmissionEpochHot(), set ) != T3D_RD_DESCRIPTOR_OK ) {
		++m_nSkipped;
		return false;
	}
	const int64_t list = rd->compute_list_begin();
	if ( list == RenderingDevice::INVALID_ID ) {
		++m_nSkipped;
		return false;
	}
	rd->compute_list_bind_compute_pipeline( list, m_pipeline.rid() );
	rd->compute_list_bind_uniform_set( list, set, 0u );
	rd->compute_list_set_push_constant( list, m_pushBytes, sizeof( m_params ) );
	rd->compute_list_dispatch( list, ( m_layout.m_nWidth + 7u ) / 8u, ( m_layout.m_nHeight + 7u ) / 8u, 1u );
	rd->compute_list_add_barrier( list );
	rd->compute_list_end();
	if ( !m_slots[slot].prepare( header ) || !m_slots[slot].markDrawn() ) {
		++m_nSkipped;
		return false;
	}
	m_nNextSlot = ( slot + 1u ) % T3D_VT_FEEDBACK_GRID_SLOT_COUNT;
	if ( hasOwnedSource() )
		m_bOwnedCapturePublished = false;
	rSlot = slot;
	return true;
}

bool Terrain3DVTFeedbackMultiGpu::requestReadbackHot( Terrain3DRdContext& rContext, const uint32_t nSlot, const Callable& rCallback )
{
	RenderingDevice *const rd = rContext.getDevice();
	if ( !Terrain3DRdContext::isOnRenderThread() || !rd || !ready() || rContext.getDeviceGeneration() != m_nDeviceGeneration ||
	     nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT || m_slots[nSlot].m_eStatus != T3D_VT_FEEDBACK_SLOT_DRAWN || !rCallback.is_valid() )
		return false;
	t3dGpuTransferTicket_t ticket;
	if ( !Terrain3DGpuTransferBudget::get().reserve( T3D_GPU_TRANSFER_FEEDBACK, m_layout.m_nSlotBytes, 1u, ticket ) ) {
		++m_nSkipped;
		return false;
	}
	const bool accepted =
	    rd->buffer_get_data_async( m_output.rid(), rCallback, nSlot * m_layout.m_nSlotBytes, m_layout.m_nSlotBytes ) == OK;
	m_slots[nSlot].submit( accepted );
	if ( !accepted ) {
		Terrain3DGpuTransferBudget::get().refund( ticket, m_layout.m_nSlotBytes, 1u );
		++m_nSkipped;
	}
	return accepted;
}

bool Terrain3DVTFeedbackMultiGpu::consumeReadbackHot( const PackedByteArray& rBytes )
{
	if ( rBytes.size() < ( int64_t )sizeof( t3dVTFeedbackGridHeader_t ) )
		return false;
	t3dVTFeedbackGridHeader_t header{};
	memcpy( &header, rBytes.ptr(), sizeof( header ) );
	if ( header.m_nMagic != T3D_VT_FEEDBACK_GRID_MAGIC || header.m_nVersion != T3D_VT_FEEDBACK_MULTI_VERSION )
		return false;
	if ( header.m_nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT )
		return true;
	t3dVTFeedbackMultiParams_t slotParams = m_params;
	slotParams.m_nSlot = header.m_nSlot;
	if ( rBytes.size() != m_layout.m_nSlotBytes ) {
		// A truncated callback is never admitted to the reducer. Retire only
		// the matching outstanding identity, not a newer reused slot.
		t3dVTFeedbackMultiSlot_t& slot = m_slots[header.m_nSlot];
		if ( slot.m_eStatus == T3D_VT_FEEDBACK_SLOT_READBACK_SUBMITTED && slot.m_Header.m_nFrame == header.m_nFrame &&
		     slot.m_Header.m_nFeedbackEpoch == header.m_nFeedbackEpoch && slot.m_Header.m_nViewportKey == header.m_nViewportKey )
			slot.reset();
		return true;
	}
	if ( !m_slots[header.m_nSlot].complete( header, slotParams ) )
		return true;
	memcpy( m_readback[header.m_nSlot].ptrw(), rBytes.ptr(), ( size_t )rBytes.size() );
	return true;
}

const uint8_t *Terrain3DVTFeedbackMultiGpu::peekReadyHot( const uint32_t nSlot, t3dVTFeedbackGridHeader_t& rHeader ) const
{
	if ( nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT || m_slots[nSlot].m_eStatus != T3D_VT_FEEDBACK_SLOT_READY )
		return NULL;
	rHeader = m_slots[nSlot].m_Header;
	return m_readback[nSlot].ptr();
}

bool Terrain3DVTFeedbackMultiGpu::retireReadyHot( const uint32_t nSlot )
{
	if ( nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT || m_slots[nSlot].m_eStatus != T3D_VT_FEEDBACK_SLOT_READY )
		return false;
	m_slots[nSlot].reset();
	return true;
}

void Terrain3DVTFeedbackMultiGpu::invalidateHot()
{
	m_bOwnedCapturePublished = false;
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i )
		m_slots[i].invalidate();
}

void Terrain3DVTFeedbackMultiGpu::shutdownCold( Terrain3DRdContext& rContext )
{
	invalidateHot();
	if ( Terrain3DRdGlobalDescriptorCache *const cache = rContext.tryGetGlobalDescriptorCacheHot() )
		cache->releaseDescriptorCold( m_lease );
	m_lease = t3dRdDescriptorLease_t{};
	m_output.releaseTracked( rContext );
	m_sampler.releaseTracked( rContext );
	m_pipeline.releaseTracked( rContext );
	m_shader.releaseTracked( rContext );
	m_ownedCounts.releaseTracked( rContext );
	m_ownedPages.releaseTracked( rContext );
	abandonDevice();
}

void Terrain3DVTFeedbackMultiGpu::abandonDevice()
{
	m_lease = t3dRdDescriptorLease_t{};
	m_output.abandon();
	m_sampler.abandon();
	m_pipeline.abandon();
	m_shader.abandon();
	m_ownedPages.abandon();
	m_ownedCounts.abandon();
	m_bOwnedCapturePublished = false;
	m_publishedProof = t3dVTFeedbackMultiSourceProof_t{};
	m_countUpload.clear();
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_MULTI_MAX_LAYERS; ++i )
		m_pageUpload[i].clear();
	m_layout = t3dVTFeedbackGridLayout_t{};
	m_params = t3dVTFeedbackMultiParams_t{};
	m_sourcePages = RID();
	m_sourceCounts = RID();
	m_nDeviceGeneration = 0u;
	m_nNextSlot = 0u;
	m_headerBytes.clear();
	m_pushBytes.clear();
	for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
		m_slots[i].reset();
		m_readback[i].clear();
	}
}
