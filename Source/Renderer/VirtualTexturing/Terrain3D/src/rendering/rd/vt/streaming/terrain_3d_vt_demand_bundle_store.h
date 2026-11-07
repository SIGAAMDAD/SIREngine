// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_DEMAND_BUNDLE_STORE_H
#define TERRAIN3D_VT_DEMAND_BUNDLE_STORE_H

#include "core/utility/terrain_3d_sync.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.h"
#include <godot_cpp/variant/string.hpp>

// T04: one immutable T3VD v1 directory + four bounded asynchronous 4x4-cell
// payloads. The hot path only queues and reads already-decoded records.
#define T3D_VT_BUNDLE_CACHE_SLOTS 4u
#define T3D_VT_BUNDLE_MAX_RECORDS ( T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_BUNDLE_BYTES / T3D_VT_DEMAND_BAKED_VISIBILITY_RECORD_BYTES )

struct t3dVTDemandBundleStats_t
{
	uint64_t nReads = 0u, nBytes = 0u, nInvalid = 0u;
	uint64_t nCacheHits = 0u, nCacheMisses = 0u, nCoalesced = 0u;
	uint64_t nCancelled = 0u, nEvictions = 0u;
	uint32_t nQueued = 0u, nResident = 0u;
};

class Terrain3DVTDemandBundleStore
{
public:
	Terrain3DVTDemandBundleStore() = default;
	~Terrain3DVTDemandBundleStore();
	Terrain3DVTDemandBundleStore( const Terrain3DVTDemandBundleStore& ) = delete;
	Terrain3DVTDemandBundleStore& operator=( const Terrain3DVTDemandBundleStore& ) = delete;

	bool openCold( const godot::String& rPath, uint64_t nPackageSignature, uint64_t nAssetIdentityHash );
	void resetCold(); // join worker; no slot recycled while worker owns bytes
	bool validFor( uint64_t nPackageSignature ) const;
	const t3dVTDemandVisibilityCookConfig_t& configCold() const
	{
		return m_directory.config();
	}
	void requestCellHot( int32_t nX, int32_t nZ, uint8_t nPriority );
	// Valid until the next requestCellHot()/resetCold() on the owner lane.
	const t3dVTDemandBakedVisibilityRecord_t *findCellHot( int32_t nX, int32_t nZ, uint32_t& rCount, uint32_t& rFlags );
	t3dVTDemandBundleStats_t stats() const;

private:
	enum State : uint8_t
	{
		FREE,
		QUEUED,
		LOADING,
		READY,
		FAILED
	};
	struct Slot
	{
		std::vector<t3dVTDemandBakedVisibilityRecord_t> records;
		uint32_t nBundle = UINT32_MAX;
		uint32_t nPriority = 0u;
		uint64_t nAge = 0u;
		uint64_t nEpoch = 0u;
		State state = FREE;
	};
	void workerMain();
	int32_t selectQueuedLocked() const;
	int32_t findSlotLocked( uint32_t nBundle ) const;
	Terrain3DVTDemandDirectory m_directory;
	godot::String m_path;
	Slot m_slots[T3D_VT_BUNDLE_CACHE_SLOTS];
	mutable Terrain3DMutex m_mutex;
	Terrain3DCondition m_wake;
	Terrain3DThread m_worker;
	uint64_t m_nEpoch = 1u, m_nUseClock = 0u;
	bool m_bStop = false, m_bOpen = false;
	t3dVTDemandBundleStats_t m_stats{};
};
#endif
