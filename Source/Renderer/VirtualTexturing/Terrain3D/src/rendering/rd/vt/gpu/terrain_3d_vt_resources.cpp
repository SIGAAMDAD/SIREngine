// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "rendering/rd/vt/gpu/terrain_3d_vt_resources.h"
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <limits.h>
#include <string.h>

using namespace godot;

static uint32_t t3d_vt_channel_page_count( const t3dWorldLimits_t& pLimits, t3dVTResourceChannel_t pChannel )
{
	switch ( pChannel ) {
	case T3D_VT_RESOURCE_ALBEDO:
		return pLimits.nMaxVtPhysicalPageSets;
	case T3D_VT_RESOURCE_NORMAL:
		return pLimits.nMaxVtPhysicalPageSets;
	case T3D_VT_RESOURCE_SURFACE:
		return pLimits.nMaxVtPhysicalPageSets;
	case T3D_VT_RESOURCE_HEIGHT:
	case T3D_VT_RESOURCE_HEIGHT16:
		return pLimits.nMaxVtPhysicalPageSets;
	default:
		return 0u;
	}
}

static RenderingDevice::DataFormat t3d_vt_channel_format( t3dVTResourceChannel_t pChannel )
{
	switch ( pChannel ) {
	case T3D_VT_RESOURCE_ALBEDO:
		// VT V1 physical family 0 is shared by ALBEDO (sRGB semantic) and ORM (linear semantic).
		// Keep the cache itself linear and perform ALBEDO sRGB decode explicitly in the shader.
		return RenderingDevice::DATA_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case T3D_VT_RESOURCE_NORMAL:
		return RenderingDevice::DATA_FORMAT_BC5_UNORM_BLOCK;
	case T3D_VT_RESOURCE_SURFACE:
		return RenderingDevice::DATA_FORMAT_BC7_UNORM_BLOCK;
	case T3D_VT_RESOURCE_HEIGHT:
		return RenderingDevice::DATA_FORMAT_R8_UNORM;
	case T3D_VT_RESOURCE_HEIGHT16:
		return RenderingDevice::DATA_FORMAT_R16_UNORM;
	default:
		return RenderingDevice::DATA_FORMAT_MAX;
	}
}

static const char *t3d_vt_buffer_name( const t3dVTGpuBufferId_t pBuffer )
{
	switch ( pBuffer ) {
	case T3D_VT_GPU_BUFFER_ALLOCATION_TABLE:
		return "VT allocations";
	case T3D_VT_GPU_BUFFER_PAGE_TABLE:
		return "VT page table";
	case T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE:
		return "VT mip layout";
	case T3D_VT_GPU_BUFFER_TAIL_METADATA:
		return "VT tail metadata";
	case T3D_VT_GPU_BUFFER_FRAGMENT_HASH:
		return "VT fragment request hash";
	case T3D_VT_GPU_BUFFER_FRAGMENT_COUNTERS:
		return "VT fragment counters";
	case T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS:
		return "VT fragment params";
	case T3D_VT_GPU_BUFFER_FEEDBACK_GRID:
		return "VT coarse feedback ring";
	case T3D_VT_GPU_BUFFER_ANCESTOR_HINTS:
		return "VT generation-validated ancestor hints";
	default:
		return "VT buffer";
	}
}

Terrain3DVTResourceImage::Terrain3DVTResourceImage()
{
	m_pContext = NULL;
	m_pRenderingDevice = NULL;
	memset( m_nVtBankCount, 0, sizeof( m_nVtBankCount ) );
	memset( m_nVtBankLayers, 0, sizeof( m_nVtBankLayers ) );
	memset( &m_Plan, 0, sizeof( m_Plan ) );
	memset( &m_Limits, 0, sizeof( m_Limits ) );
	memset( &m_InitializeProfile, 0, sizeof( m_InitializeProfile ) );
	memset( &m_Stats, 0, sizeof( m_Stats ) );
	m_nInitializeBufferCursor = 0u;
	m_nInitializeChannelCursor = 0u;
	m_nInitializeStage = 0u;
	m_bInitializePending = false;
	m_nHotSealCreateEpoch = 0u;
	m_nHotSealFreeEpoch = 0u;
	m_bInitialized = false;
	m_bHotSealed = false;
}

Terrain3DVTResourceImage::~Terrain3DVTResourceImage()
{
	// Ownership is intentionally explicit. The composition root must call shutdown_cold().
}

bool Terrain3DVTResourceImage::initialize_cold( Terrain3DRdContext& rContext, const t3dWorldLimits_t& pLimits,
                                                const t3dVTGpuProfile_t& pProfile )
{
	return initialize_internal_cold( rContext, pLimits, pProfile );
}

bool Terrain3DVTResourceImage::initialize_internal_cold( Terrain3DRdContext& rContext, const t3dWorldLimits_t& pLimits,
                                                         const t3dVTGpuProfile_t& pProfile )
{
	if ( !begin_initialize_cold( rContext, pLimits, pProfile ) ) {
		return false;
	}
	while ( is_initialize_pending_cold() ) {
		if ( !service_initialize_cold( UINT32_MAX ) ) {
			return false;
		}
	}
	return m_bInitialized;
}

bool Terrain3DVTResourceImage::begin_initialize_cold( Terrain3DRdContext& rContext, const t3dWorldLimits_t& pLimits,
                                                      const t3dVTGpuProfile_t& pProfile, const uint32_t nActiveTailAtlasHeight )
{
	RenderingDevice *const pRenderingDevice = rContext.getDevice();
	if ( m_bInitialized || m_bInitializePending || pRenderingDevice == NULL ) {
		return false;
	}
	if ( !t3d_vt_compute_resource_plan( pLimits, pProfile, m_Plan, nActiveTailAtlasHeight ) ) {
		return false;
	}
	m_pContext = &rContext;
	m_pRenderingDevice = pRenderingDevice;
	m_Limits = pLimits;
	m_InitializeProfile = pProfile;
	if ( !validate_device_cold( pLimits, pProfile ) ) {
		m_pRenderingDevice = NULL;
		m_pContext = NULL;
		memset( &m_Plan, 0, sizeof( m_Plan ) );
		memset( &m_Limits, 0, sizeof( m_Limits ) );
		memset( &m_InitializeProfile, 0, sizeof( m_InitializeProfile ) );
		return false;
	}
	m_nInitializeBufferCursor = 0u;
	m_nInitializeChannelCursor = 0u;
	m_nInitializeStage = 0u;
	m_bInitializePending = true;
	return true;
}

bool Terrain3DVTResourceImage::service_initialize_cold( const uint32_t nMaxWorkUnits )
{
	if ( !m_bInitializePending || m_bInitialized || m_pRenderingDevice == NULL || nMaxWorkUnits == 0u ) {
		return false;
	}
	uint32_t nUnits = 0u;
	while ( m_bInitializePending && nUnits < nMaxWorkUnits ) {
		bool bReady = true;
		if ( m_nInitializeStage == 0u ) {
			if ( m_nInitializeBufferCursor < T3D_VT_GPU_BUFFER_COUNT ) {
				const uint32_t iBuffer = m_nInitializeBufferCursor++;
				if ( m_Plan.nBufferBytes[iBuffer] != 0u ) {
					bReady = create_storage_buffer_cold( ( t3dVTGpuBufferId_t )iBuffer, m_Plan.nBufferBytes[iBuffer] );
				}
				++nUnits;
			} else {
				m_nInitializeStage = 1u;
				continue;
			}
		} else if ( m_nInitializeStage == 1u ) {
			if ( m_nInitializeChannelCursor < T3D_VT_CHANNEL_COUNT ) {
				const t3dVTResourceChannel_t Channel = ( t3dVTResourceChannel_t )m_nInitializeChannelCursor++;
				bReady = create_vt_channel_cold( Channel, t3d_vt_channel_page_count( m_Limits, Channel ),
				                                 m_InitializeProfile.nVtPhysicalTileSize, t3d_vt_channel_format( Channel ) ) &&
				         create_vt_tail_channel_cold( Channel, t3d_vt_channel_format( Channel ) );
				++nUnits;
			} else {
				m_nInitializeStage = 2u;
				continue;
			}
		} else if ( m_nInitializeStage == 2u ) {
			bReady = true;
			++nUnits;
			if ( bReady ) {
				m_nInitializeStage = 3u;
				m_bInitializePending = false;
				m_bInitialized = true;
				++m_Stats.nResourceGeneration;
			}
		}
		if ( !bReady ) {
			rollback_failed_initialize_cold();
			return false;
		}
	}
	return true;
}

bool Terrain3DVTResourceImage::is_initialize_pending_cold() const
{
	return m_bInitializePending;
}

void Terrain3DVTResourceImage::shutdown_cold()
{
	m_bHotSealed = false;

	if ( m_pRenderingDevice == NULL || m_pContext == NULL ) {
		m_bInitializePending = false;
		m_bInitialized = false;
		return;
	}

	for ( int32_t iChannel = ( int32_t )T3D_VT_CHANNEL_COUNT - 1; iChannel >= 0; --iChannel ) {
		if ( m_vtTail[iChannel].isValid() ) {
			const uint64_t bytes = m_vtTail[iChannel].bytes();
			m_vtTail[iChannel].releaseTracked( *m_pContext );
			++m_Stats.nGpuFreeCount;
			if ( m_Stats.nLiveResourceCount != 0u )
				--m_Stats.nLiveResourceCount;
			m_Stats.nLiveTextureBytes = m_Stats.nLiveTextureBytes >= bytes ? m_Stats.nLiveTextureBytes - bytes : 0u;
		}
	}
	for ( int32_t iChannel = ( int32_t )T3D_VT_CHANNEL_COUNT - 1; iChannel >= 0; --iChannel ) {
		for ( int32_t iBank = ( int32_t )m_nVtBankCount[iChannel] - 1; iBank >= 0; --iBank ) {
			if ( m_vtPhysical[iChannel][iBank].isValid() ) {
				const uint64_t bytes = m_vtPhysical[iChannel][iBank].bytes();
				m_vtPhysical[iChannel][iBank].releaseTracked( *m_pContext );
				++m_Stats.nGpuFreeCount;
				if ( m_Stats.nLiveResourceCount != 0u )
					--m_Stats.nLiveResourceCount;
				m_Stats.nLiveTextureBytes = m_Stats.nLiveTextureBytes >= bytes ? m_Stats.nLiveTextureBytes - bytes : 0u;
			}
		}
	}

	for ( int32_t iBuffer = ( int32_t )T3D_VT_GPU_BUFFER_COUNT - 1; iBuffer >= 0; --iBuffer ) {
		if ( m_buffers[iBuffer].isValid() ) {
			m_buffers[iBuffer].releaseTracked( *m_pContext );
			++m_Stats.nGpuFreeCount;
			if ( m_Stats.nLiveResourceCount != 0u )
				--m_Stats.nLiveResourceCount;
			const uint64_t bytes = m_Plan.nBufferBytes[iBuffer];
			m_Stats.nLiveBufferBytes = m_Stats.nLiveBufferBytes >= bytes ? m_Stats.nLiveBufferBytes - bytes : 0u;
		}
	}

	memset( m_nVtBankCount, 0, sizeof( m_nVtBankCount ) );
	memset( m_nVtBankLayers, 0, sizeof( m_nVtBankLayers ) );
	memset( &m_Plan, 0, sizeof( m_Plan ) );
	memset( &m_Limits, 0, sizeof( m_Limits ) );
	memset( &m_InitializeProfile, 0, sizeof( m_InitializeProfile ) );
	m_nInitializeBufferCursor = 0u;
	m_nInitializeChannelCursor = 0u;
	m_nInitializeStage = 0u;
	m_bInitializePending = false;
	m_pRenderingDevice = NULL;
	m_pContext = NULL;
	m_bInitialized = false;
	++m_Stats.nResourceGeneration;
}

void Terrain3DVTResourceImage::seal_hot_runtime()
{
	if ( !m_bInitialized ) {
		return;
	}
	m_nHotSealCreateEpoch = m_Stats.nGpuCreateCount;
	m_nHotSealFreeEpoch = m_Stats.nGpuFreeCount;
	m_bHotSealed = true;
}

void Terrain3DVTResourceImage::unseal_hot_runtime_cold()
{
	m_bHotSealed = false;
}

bool Terrain3DVTResourceImage::service_idle_frame_hot()
{
	if ( !m_bInitialized || !m_bHotSealed ) {
		return false;
	}
	if ( hot_epoch_unchanged() ) {
		return true;
	}

	++m_Stats.nHotSealViolationCount;
	return false;
}

bool Terrain3DVTResourceImage::validate_idle_plateau_reference_debug( uint32_t pFrameCount )
{
	if ( !m_bInitialized || !m_bHotSealed || pFrameCount == 0u ) {
		return false;
	}
	for ( uint32_t iFrame = 0u; iFrame < pFrameCount; ++iFrame ) {
		if ( !service_idle_frame_hot() ) {
			return false;
		}
	}
	return true;
}

bool Terrain3DVTResourceImage::is_initialized() const
{
	return m_bInitialized;
}

bool Terrain3DVTResourceImage::is_hot_sealed() const
{
	return m_bHotSealed;
}

const t3dVTGpuResourcePlan_t& Terrain3DVTResourceImage::get_plan() const
{
	return m_Plan;
}

const t3dWorldLimits_t& Terrain3DVTResourceImage::get_limits() const
{
	return m_Limits;
}

const t3dVTGpuResourceStats_t& Terrain3DVTResourceImage::get_stats() const
{
	return m_Stats;
}

const RID& Terrain3DVTResourceImage::get_buffer_rid( t3dVTGpuBufferId_t pBuffer ) const
{
	if ( ( uint32_t )pBuffer >= T3D_VT_GPU_BUFFER_COUNT ) {
		return m_hInvalid;
	}
	return m_buffers[pBuffer].rid();
}

const Terrain3DRdGpuBuffer *Terrain3DVTResourceImage::get_buffer( t3dVTGpuBufferId_t pBuffer ) const
{
	return ( uint32_t )pBuffer < T3D_VT_GPU_BUFFER_COUNT ? &m_buffers[pBuffer] : NULL;
}

uint32_t Terrain3DVTResourceImage::get_vt_bank_count( t3dVTResourceChannel_t pChannel ) const
{
	if ( ( uint32_t )pChannel >= T3D_VT_CHANNEL_COUNT ) {
		return 0u;
	}
	return m_nVtBankCount[pChannel];
}

uint32_t Terrain3DVTResourceImage::get_vt_bank_layer_count( t3dVTResourceChannel_t pChannel, uint32_t pBank ) const
{
	if ( ( uint32_t )pChannel >= T3D_VT_CHANNEL_COUNT ) {
		return 0u;
	}
	if ( pBank >= m_nVtBankCount[pChannel] ) {
		return 0u;
	}
	return m_nVtBankLayers[pChannel][pBank];
}

const RID& Terrain3DVTResourceImage::get_vt_bank_rid( t3dVTResourceChannel_t pChannel, uint32_t pBank ) const
{
	if ( ( uint32_t )pChannel >= T3D_VT_CHANNEL_COUNT ) {
		return m_hInvalid;
	}
	if ( pBank >= m_nVtBankCount[pChannel] ) {
		return m_hInvalid;
	}
	return m_vtPhysical[pChannel][pBank].rid();
}

const RID& Terrain3DVTResourceImage::get_vt_tail_rid( t3dVTResourceChannel_t pChannel ) const
{
	if ( ( uint32_t )pChannel >= T3D_VT_CHANNEL_COUNT ) {
		return m_hInvalid;
	}
	return m_vtTail[pChannel].rid();
}

bool Terrain3DVTResourceImage::update_vt_bank_layer( t3dVTResourceChannel_t pChannel, uint32_t pBank, uint32_t pLayer,
                                                     const PackedByteArray& rData ) const
{
	return m_pContext && ( uint32_t )pChannel < T3D_VT_CHANNEL_COUNT && pBank < m_nVtBankCount[pChannel] &&
	       pLayer < m_nVtBankLayers[pChannel][pBank] && m_vtPhysical[pChannel][pBank].updateLayerTracked( *m_pContext, pLayer, rData );
}

bool Terrain3DVTResourceImage::update_vt_tail_layer( t3dVTResourceChannel_t pChannel, uint32_t pLayer, const PackedByteArray& rData ) const
{
	return m_pContext && ( uint32_t )pChannel < T3D_VT_CHANNEL_COUNT &&
	       m_vtTail[pChannel].updateLayerTracked( *m_pContext, pLayer, rData, T3D_GPU_PUBLICATION );
}

Terrain3DRdContext *Terrain3DVTResourceImage::get_rd_context() const
{
	return m_pContext;
}

bool Terrain3DVTResourceImage::validate_device_cold( const t3dWorldLimits_t& pLimits, const t3dVTGpuProfile_t& pProfile ) const
{
	if ( m_pRenderingDevice == NULL ) {
		return false;
	}

	for ( uint32_t iBuffer = 0u; iBuffer < T3D_VT_GPU_BUFFER_COUNT; ++iBuffer ) {
		// godot-cpp 10.0.0-stable / Godot 4.7 exposes uint32_t buffer sizes.
		if ( m_Plan.nBufferBytes[iBuffer] > UINT32_MAX ) {
			return false;
		}
	}

	const uint64_t nMaxTextureSize = m_pRenderingDevice->limit_get( RenderingDevice::LIMIT_MAX_TEXTURE_SIZE_2D );
	const uint64_t nMaxArrayLayers = m_pRenderingDevice->limit_get( RenderingDevice::LIMIT_MAX_TEXTURE_ARRAY_LAYERS );
	if ( nMaxTextureSize == 0u || nMaxArrayLayers == 0u ) {
		return false;
	}
	if ( pProfile.nVtPhysicalTileSize > nMaxTextureSize || m_Plan.nVtTailAtlasWidth > nMaxTextureSize ||
	     m_Plan.nVtTailAtlasHeight > nMaxTextureSize ) {
		return false;
	}

	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
		const uint32_t nPages = t3d_vt_channel_page_count( pLimits, ( t3dVTResourceChannel_t )iChannel );
		t3dVTBankLayout_t Layout = {};
		if ( !t3d_vt_compute_bank_layout( nPages, ( uint32_t )nMaxArrayLayers, Layout ) ) {
			return false;
		}
	}

	const BitField<RenderingDevice::TextureUsageBits> VtUsage =
	    ( BitField<RenderingDevice::TextureUsageBits> )( RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
		                                                 RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
		                                                 RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT |
		                                                 RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT );

	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
		if ( !Terrain3DRdTexture::supportsFormatUsage( m_pRenderingDevice, t3d_vt_channel_format( ( t3dVTResourceChannel_t )iChannel ),
		                                               VtUsage ) ) {
			return false;
		}
	}

	return true;
}

bool Terrain3DVTResourceImage::create_storage_buffer_cold( t3dVTGpuBufferId_t pBuffer, uint64_t pBytes )
{
	if ( m_pRenderingDevice == NULL || m_pContext == NULL ) {
		return false;
	}
	if ( ( uint32_t )pBuffer >= T3D_VT_GPU_BUFFER_COUNT ) {
		return false;
	}
	if ( pBytes == 0u || pBytes > UINT32_MAX ) {
		return false;
	}

	t3dRdGpuBufferSpec_t spec;
	spec.m_pName = t3d_vt_buffer_name( pBuffer );
	spec.m_owner = T3D_RD_CONTEXT_OWNER_VT;
	spec.m_type =
	    pBuffer == T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS ? T3D_RD_CONTEXT_RESOURCE_UNIFORM_BUFFER : T3D_RD_CONTEXT_RESOURCE_STORAGE_BUFFER;
	spec.m_memory = T3D_RD_CONTEXT_MEMORY_STORAGE;
	spec.m_growth = T3D_GPU_EXACT;
	if ( !m_buffers[pBuffer].configure( spec ) || !m_buffers[pBuffer].createTracked( *m_pContext, ( uint32_t )pBytes ) ) {
		return false;
	}

	++m_Stats.nGpuCreateCount;
	++m_Stats.nLiveResourceCount;
	m_Stats.nLiveBufferBytes += pBytes;
	return true;
}

bool Terrain3DVTResourceImage::create_vt_channel_cold( t3dVTResourceChannel_t pChannel, uint32_t pPageCount, uint32_t pTileSize,
                                                       RenderingDevice::DataFormat pFormat )
{
	if ( m_pRenderingDevice == NULL || m_pContext == NULL || ( uint32_t )pChannel >= T3D_VT_CHANNEL_COUNT || pPageCount == 0u ) {
		return false;
	}
	const uint32_t nMaxArrayLayers = ( uint32_t )m_pRenderingDevice->limit_get( RenderingDevice::LIMIT_MAX_TEXTURE_ARRAY_LAYERS );
	t3dVTBankLayout_t Layout = {};
	if ( !t3d_vt_compute_bank_layout( pPageCount, nMaxArrayLayers, Layout ) ) {
		return false;
	}
	const uint64_t usage = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
	                       RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	uint32_t nRemainingPages = pPageCount;
	uint64_t nRemainingBytes = m_Plan.nVtPhysicalBytes[pChannel];
	const uint64_t nBytesPerLayer = m_Plan.nVtPhysicalBytes[pChannel] / pPageCount;
	for ( uint32_t nBank = 0u; nBank < Layout.nBankCount; ++nBank ) {
		const uint32_t nLayers = Layout.nLayers[nBank];
		const uint64_t nBankBytes = nRemainingPages == nLayers ? nRemainingBytes : nBytesPerLayer * nLayers;
		t3dRdTextureSpec_t spec;
		spec.m_name =
		    String( "Terrain3D VT channel " ) + String::num_uint64( ( uint64_t )pChannel ) + " bank " + String::num_uint64( nBank );
		spec.m_owner = T3D_RD_CONTEXT_OWNER_VT;
		spec.m_memory = T3D_RD_CONTEXT_MEMORY_TERRAIN_TEXTURE;
		spec.m_layout.m_format = pFormat;
		spec.m_layout.m_type = RenderingDevice::TEXTURE_TYPE_2D_ARRAY;
		spec.m_layout.m_nWidth = pTileSize;
		spec.m_layout.m_nHeight = pTileSize;
		spec.m_layout.m_nLayers = nLayers;
		spec.m_layout.m_nMipmaps = 1u;
		spec.m_layout.m_nUsageBits = usage;
		spec.m_layout.m_nBytes = nBankBytes;
		if ( !m_vtPhysical[pChannel][nBank].createTracked( *m_pContext, spec ) ) {
			return false;
		}
		++m_Stats.nGpuCreateCount;
		++m_Stats.nLiveResourceCount;
		m_Stats.nLiveTextureBytes += nBankBytes;
		m_nVtBankLayers[pChannel][nBank] = nLayers;
		nRemainingBytes -= nBankBytes;
		nRemainingPages -= nLayers;
		m_nVtBankCount[pChannel] = nBank + 1u;
	}
	return nRemainingPages == 0u && nRemainingBytes == 0u;
}

bool Terrain3DVTResourceImage::create_vt_tail_channel_cold( t3dVTResourceChannel_t pChannel, RenderingDevice::DataFormat pFormat )
{
	if ( m_pRenderingDevice == NULL || m_pContext == NULL || ( uint32_t )pChannel >= T3D_VT_CHANNEL_COUNT ||
	     m_Plan.nVtTailBytes[pChannel] == 0u ) {
		return false;
	}
	t3dRdTextureSpec_t spec;
	spec.m_name = String( "Terrain3D VT tail channel " ) + String::num_uint64( ( uint64_t )pChannel );
	spec.m_owner = T3D_RD_CONTEXT_OWNER_VT;
	spec.m_memory = T3D_RD_CONTEXT_MEMORY_TERRAIN_TEXTURE;
	spec.m_layout.m_format = pFormat;
	spec.m_layout.m_type = RenderingDevice::TEXTURE_TYPE_2D;
	spec.m_layout.m_nWidth = m_Plan.nVtTailAtlasWidth;
	spec.m_layout.m_nHeight = m_Plan.nVtTailAtlasHeight;
	spec.m_layout.m_nLayers = 1u;
	spec.m_layout.m_nMipmaps = 1u;
	spec.m_layout.m_nUsageBits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
	                             RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	spec.m_layout.m_nBytes = m_Plan.nVtTailBytes[pChannel];
	if ( !m_vtTail[pChannel].createTracked( *m_pContext, spec ) ) {
		return false;
	}
	++m_Stats.nGpuCreateCount;
	++m_Stats.nLiveResourceCount;
	m_Stats.nLiveTextureBytes += spec.m_layout.m_nBytes;
	return true;
}

void Terrain3DVTResourceImage::rollback_failed_initialize_cold()
{
	shutdown_cold();
	m_bInitialized = false;
}

bool Terrain3DVTResourceImage::hot_epoch_unchanged() const
{
	return m_Stats.nGpuCreateCount == m_nHotSealCreateEpoch && m_Stats.nGpuFreeCount == m_nHotSealFreeEpoch;
}
