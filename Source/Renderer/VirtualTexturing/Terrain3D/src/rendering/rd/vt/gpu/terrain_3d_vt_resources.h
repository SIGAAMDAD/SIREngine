// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_RESOURCES_H
#define TERRAIN3D_VT_RESOURCES_H

#include "Terrain3DTypes.h"

#include "rendering/rd/vt/planning/terrain_3d_vt_resource_plan.h"
#include "rendering/rd/core/terrain_3d_rd_context.h"
#include "rendering/rd/infrastructure/buffers/terrain_3d_rd_gpu_buffer.h"
#include "rendering/rd/infrastructure/resources/terrain_3d_rd_texture.h"

#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/rid.hpp>

class Terrain3DVTResourceImage
{
public:
	Terrain3DVTResourceImage();
	~Terrain3DVTResourceImage();

	bool initialize_cold( Terrain3DRdContext& rContext, const t3dWorldLimits_t& pLimits, const t3dVTGpuProfile_t& pProfile );
	bool begin_initialize_cold( Terrain3DRdContext& rContext, const t3dWorldLimits_t& pLimits, const t3dVTGpuProfile_t& pProfile,
	                            uint32_t nActiveTailAtlasHeight = 0u );
	bool service_initialize_cold( uint32_t nMaxWorkUnits );
	bool is_initialize_pending_cold() const;
	void shutdown_cold();

	void seal_hot_runtime();
	void unseal_hot_runtime_cold();
	bool service_idle_frame_hot();
	bool validate_idle_plateau_reference_debug( uint32_t pFrameCount );

	bool is_initialized() const;
	bool is_hot_sealed() const;
	const t3dVTGpuResourcePlan_t& get_plan() const;
	const t3dWorldLimits_t& get_limits() const;
	const t3dVTGpuResourceStats_t& get_stats() const;
	const godot::RID& get_buffer_rid( t3dVTGpuBufferId_t pBuffer ) const;
	const Terrain3DRdGpuBuffer *get_buffer( t3dVTGpuBufferId_t pBuffer ) const;
	uint32_t get_vt_bank_count( t3dVTResourceChannel_t pChannel ) const;
	uint32_t get_vt_bank_layer_count( t3dVTResourceChannel_t pChannel, uint32_t pBank ) const;
	const godot::RID& get_vt_bank_rid( t3dVTResourceChannel_t pChannel, uint32_t pBank ) const;
	const godot::RID& get_vt_tail_rid( t3dVTResourceChannel_t pChannel ) const;
	bool update_vt_bank_layer( t3dVTResourceChannel_t pChannel, uint32_t pBank, uint32_t pLayer,
	                           const godot::PackedByteArray& rData ) const;
	bool update_vt_tail_layer( t3dVTResourceChannel_t pChannel, uint32_t pLayer, const godot::PackedByteArray& rData ) const;
	Terrain3DRdContext *get_rd_context() const;

private:
	bool initialize_internal_cold( Terrain3DRdContext& rContext, const t3dWorldLimits_t& pLimits, const t3dVTGpuProfile_t& pProfile );
	bool validate_device_cold( const t3dWorldLimits_t& pLimits, const t3dVTGpuProfile_t& pProfile ) const;
	bool create_storage_buffer_cold( t3dVTGpuBufferId_t pBuffer, uint64_t pBytes );
	bool create_vt_channel_cold( t3dVTResourceChannel_t pChannel, uint32_t pPageCount, uint32_t pTileSize,
	                             godot::RenderingDevice::DataFormat pFormat );
	bool create_vt_tail_channel_cold( t3dVTResourceChannel_t pChannel, godot::RenderingDevice::DataFormat pFormat );
	void rollback_failed_initialize_cold();
	bool hot_epoch_unchanged() const;

	Terrain3DRdContext *m_pContext;
	godot::RenderingDevice *m_pRenderingDevice;
	godot::RID m_hInvalid;
	Terrain3DRdGpuBuffer m_buffers[T3D_VT_GPU_BUFFER_COUNT];
	Terrain3DRdTexture m_vtPhysical[T3D_VT_CHANNEL_COUNT][T3D_VT_MAX_TEXTURE_BANKS];
	Terrain3DRdTexture m_vtTail[T3D_VT_CHANNEL_COUNT];
	uint32_t m_nVtBankCount[T3D_VT_CHANNEL_COUNT];
	uint32_t m_nVtBankLayers[T3D_VT_CHANNEL_COUNT][T3D_VT_MAX_TEXTURE_BANKS];
	t3dVTGpuResourcePlan_t m_Plan;
	t3dWorldLimits_t m_Limits;
	t3dVTGpuProfile_t m_InitializeProfile;
	t3dVTGpuResourceStats_t m_Stats;
	uint32_t m_nInitializeBufferCursor;
	uint32_t m_nInitializeChannelCursor;
	uint32_t m_nInitializeStage;
	bool m_bInitializePending;
	uint64_t m_nHotSealCreateEpoch;
	uint64_t m_nHotSealFreeEpoch;
	bool m_bInitialized;
	bool m_bHotSealed;
};

#endif // TERRAIN3D_VT_RESOURCES_H
