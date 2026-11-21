#ifndef TERRAIN3D_SHARED_VT_RUNTIME_H
#define TERRAIN3D_SHARED_VT_RUNTIME_H

#include "Terrain3DTypes.h"

#include "rendering/rd/vt/gpu/terrain_3d_vt_resources.h"
#include "rendering/rd/vt/runtime/terrain_3d_virtual_texture.h"
#include "rendering/rd/vt/gpu/terrain_3d_vt_shared_bindings.h"
#include "rendering/rd/world/core/terrain_3d_world_contract.h"

#include <godot_cpp/variant/callable.hpp>
#include "memory/arena/terrain_3d_memory_zone.h"

// Shared VT ownership retained after removal of the World scene renderer.
class Terrain3DSharedVTRuntime
{
public:
	Terrain3DSharedVTRuntime();
	~Terrain3DSharedVTRuntime();
	bool bindRuntimeAllocator( Terrain3DMemoryZone *pZone );
	// A composed region, when present in the V1 package, uses the same
	// residency, typed BC banks, page table, transfer budget and eviction.
	// Only cold/startup callers may query a package stable key.
	bool findComposedRegionAllocationCold( uint32_t nRegionMapIndex, uint32_t& rAllocation ) const;
	Terrain3DVirtualTexture& virtualTexture()
	{
		return m_VirtualTexture;
	}
	const Terrain3DVirtualTexture& virtualTexture() const
	{
		return m_VirtualTexture;
	}
	bool prepare_shared_vt_cpu_cold( t3dVTSharedResourceClass_t eResourceClass = T3D_VT_SHARED_RESOURCES_PRODUCTION,
	                                 uint32_t nPreviewMaterialCapacity = 0u, uint32_t nPreviewVirtualPageCapacity = 0u,
	                                 const t3dWorldResourceLimits_t *pProductionWorldLimits = NULL );
	bool prepare_shared_vt_storage_cold( Terrain3DRdContext& rContext );
	bool service_shared_vt_storage_cold( Terrain3DRdContext& rContext, uint32_t nMaxWorkUnits );
	bool bind_shared_vt_executable_cold();
	bool service_shared_vt_executable_cold( uint32_t nMaxWorkUnits );
	bool publish_shared_vt_cold();
	bool service_shared_vt_publication_cold( uint64_t nMaxBytes, uint32_t nMaxWorkUnits );
	bool ensure_shared_resources_cold( Terrain3DRdContext& rContext,
	                                   t3dVTSharedResourceClass_t eResourceClass = T3D_VT_SHARED_RESOURCES_PRODUCTION,
	                                   uint32_t nPreviewMaterialCapacity = 0u, uint32_t nPreviewVirtualPageCapacity = 0u,
	                                   const t3dWorldResourceLimits_t *pProductionWorldLimits = NULL );
	// A cold resource replacement closes acquisition and invalidates all VT-backed
	// descriptor recipes before any backing RID can be freed. Only the render
	// thread may run these transitions.
	bool beginVtResourceReplacementCold( Terrain3DRdContext& rContext );
	bool commitVtResourceReplacementCold( Terrain3DRdContext& rContext );
	void abortVtResourceReplacementCold();
	void shutdown_cold( bool bPreserveTailStaging = false );
	// Render-thread capacity fallback; graphics consumers retain their pipelines.
	bool replacePackageResourcesCold( Terrain3DRdContext& rContext, const void *pBytes, uint64_t nBytes, bool bNeutral,
	                                  t3dVTSharedResourceClass_t eResourceClass,
	                                  const t3dWorldResourceLimits_t *pProductionWorldLimits = NULL );
	bool stagePackageWarm( const void *pBytes, uint64_t nBytes );
	bool stagePackageFileWarm( const godot::String& rPath );
	bool servicePackageRepublishWarm( uint64_t nMaxBytes, uint32_t nMaxWorkUnits );
	void abortPackageRepublishWarm();
	bool are_shared_resources_ready() const;
	t3dVTSharedResourceClass_t get_shared_resource_class() const;
	const t3dWorldLimits_t& get_limits() const;
	const Terrain3DVTResourceImage& get_resources() const;
	t3dVTAcquireStatus_t acquirePublishedBindingsHot( t3dVTSharedResourceClass_t eRequestedClass, t3dSharedVTBindings_t& rBindings ) const;

private:
	bool refresh_shared_vt_bindings_hot();
	void invalidateBoundDescriptorsCold( Terrain3DRdContext& rContext );
	Terrain3DVTResourceImage m_Resources;
	t3dSharedVTBindings_t m_SharedVTBindings = {};
	uint64_t m_nPublishedDeviceGeneration = 0u;
	t3dWorldLimits_t m_Limits = {};
	t3dVTGpuProfile_t m_Profile = {};
	Terrain3DMemoryZone *m_pRuntimeAllocatorZone = NULL;
	Terrain3DVirtualTexture m_VirtualTexture;
	t3dVTSharedResourceClass_t m_eSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
	t3dVTSharedResourceClass_t m_ePreparingSharedResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
	bool m_bSharedResourcesInitialized = false;
	bool m_bResourceReplacementInProgress = false;
};

#endif // TERRAIN3D_SHARED_VT_RUNTIME_H
