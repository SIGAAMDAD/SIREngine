// Copyright (c) 2023-2026 Terrain3D contributors.
#include "rendering/rd/vt/streaming/terrain_3d_vt_demand_bundle_store.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <limits.h>
#include <string.h>

using namespace godot;

Terrain3DVTDemandBundleStore::~Terrain3DVTDemandBundleStore()
{
	resetCold();
}

void Terrain3DVTDemandBundleStore::resetCold()
{
	{
		Terrain3DScopedLock lock( m_mutex );
		m_bStop = true;
		m_wake.notifyAll();
	}
	if ( m_worker.joinable() )
		m_worker.join();
	Terrain3DScopedLock lock( m_mutex );
	++m_nEpoch;
	if ( !m_nEpoch )
		++m_nEpoch;
	m_directory.resetCold();
	m_path = String();
	for ( Slot& s : m_slots ) {
		s.records.clear();
		s.records.shrink_to_fit();
		s.nBundle = UINT32_MAX;
		s.nPriority = 0u;
		s.nAge = s.nEpoch = 0u;
		s.state = FREE;
	}
	m_bOpen = false;
	m_bStop = false;
	m_nUseClock = 0u;
	m_stats = t3dVTDemandBundleStats_t{};
}

bool Terrain3DVTDemandBundleStore::openCold( const String& path, const uint64_t packageSignature, const uint64_t assetIdentity )
{
	resetCold();
	if ( path.is_empty() || !packageSignature || !assetIdentity )
		return false;
	Ref<FileAccess> file = FileAccess::open( path, FileAccess::READ );
	if ( file.is_null() )
		return false;
	const uint64_t size = file->get_length();
	if ( size < T3D_VT_DEMAND_BAKED_VISIBILITY_HEADER_BYTES || size > T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_FILE_BYTES )
		return false;
	const PackedByteArray head = file->get_buffer( T3D_VT_DEMAND_BAKED_VISIBILITY_HEADER_BYTES );
	if ( head.size() != T3D_VT_DEMAND_BAKED_VISIBILITY_HEADER_BYTES )
		return false;
	// Read directory length from the header, then let the independent codec
	// validate every count, signature, offset and directory checksum.
	uint64_t directoryBytes = 0u;
	for ( uint32_t i = 0u; i < 8u; ++i )
		directoryBytes |= ( uint64_t )head[104 + i] << ( i * 8u );
	if ( directoryBytes < T3D_VT_DEMAND_BAKED_VISIBILITY_HEADER_BYTES || directoryBytes > size ||
	     directoryBytes > T3D_VT_DEMAND_BAKED_VISIBILITY_HEADER_BYTES +
	                          ( uint64_t )T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_CELLS *
	                              ( T3D_VT_DEMAND_BAKED_VISIBILITY_CELL_BYTES + T3D_VT_DEMAND_BAKED_VISIBILITY_BUNDLE_BYTES ) )
		return false;
	file->seek( 0u );
	const PackedByteArray directory = file->get_buffer( ( int64_t )directoryBytes );
	if ( directory.size() != ( int64_t )directoryBytes ||
	     !m_directory.loadCold( directory.ptr(), directoryBytes, size, packageSignature, assetIdentity ) )
		return false;
	for ( Slot& s : m_slots )
		s.records.resize( T3D_VT_BUNDLE_MAX_RECORDS );
	m_path = path;
	m_bOpen = true;
	m_worker = Terrain3DThread( [this]() { workerMain(); } );
	return true;
}

bool Terrain3DVTDemandBundleStore::validFor( const uint64_t signature ) const
{
	return m_bOpen && m_directory.validFor( signature );
}

int32_t Terrain3DVTDemandBundleStore::findSlotLocked( const uint32_t bundle ) const
{
	for ( uint32_t i = 0u; i < T3D_VT_BUNDLE_CACHE_SLOTS; ++i )
		if ( m_slots[i].state != FREE && m_slots[i].nBundle == bundle && m_slots[i].nEpoch == m_nEpoch )
			return ( int32_t )i;
	return -1;
}

void Terrain3DVTDemandBundleStore::requestCellHot( const int32_t x, const int32_t z, const uint8_t priority )
{
	uint32_t cell = 0u;
	if ( !m_bOpen || !m_directory.findCellHot( x, z, cell ) )
		return;
	const uint32_t bundle = m_directory.cells()[cell].nBundle;
	Terrain3DScopedLock lock( m_mutex );
	if ( m_bStop )
		return;
	const int32_t previous = findSlotLocked( bundle );
	if ( previous >= 0 ) {
		Slot& s = m_slots[previous];
		if ( s.state != FAILED ) {
			s.nPriority = priority > s.nPriority ? priority : s.nPriority;
			s.nAge = ++m_nUseClock;
			++m_stats.nCoalesced;
			return;
		}
	}
	int32_t victim = -1;
	for ( uint32_t i = 0u; i < T3D_VT_BUNDLE_CACHE_SLOTS; ++i ) {
		if ( m_slots[i].state == FREE || m_slots[i].state == FAILED ) {
			victim = ( int32_t )i;
			break;
		}
		if ( m_slots[i].state == READY && ( victim < 0 || m_slots[i].nAge < m_slots[victim].nAge ) )
			victim = ( int32_t )i;
	}
	if ( victim < 0 )
		return; // all slots pinned by queued/worker jobs
	Slot& s = m_slots[victim];
	if ( s.state == READY )
		++m_stats.nEvictions;
	s.nBundle = bundle;
	s.nPriority = priority;
	s.nAge = ++m_nUseClock;
	s.nEpoch = m_nEpoch;
	s.state = QUEUED;
	m_wake.notifyOne();
}

int32_t Terrain3DVTDemandBundleStore::selectQueuedLocked() const
{
	int32_t selected = -1;
	for ( uint32_t i = 0u; i < T3D_VT_BUNDLE_CACHE_SLOTS; ++i ) {
		const Slot& s = m_slots[i];
		if ( s.state != QUEUED )
			continue;
		if ( selected < 0 || s.nPriority > m_slots[selected].nPriority ||
		     ( s.nPriority == m_slots[selected].nPriority && s.nAge < m_slots[selected].nAge ) )
			selected = ( int32_t )i;
	}
	return selected;
}

void Terrain3DVTDemandBundleStore::workerMain()
{
	// Worker allocations, FileAccess and decompression are isolated from
	// all RenderingDevice callbacks. One read at a time, four decoded slots.
	std::vector<uint8_t> encoded( T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_BUNDLE_BYTES );
	std::vector<uint8_t> scratch( T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_BUNDLE_BYTES );
	for ( ;; ) {
		int32_t chosen = -1;
		uint32_t bundle = UINT32_MAX;
		uint64_t epoch = 0u;
		{
			Terrain3DUniqueLock lock( m_mutex );
			while ( !m_bStop && ( chosen = selectQueuedLocked() ) < 0 )
				m_wake.wait( lock );
			if ( m_bStop )
				return;
			Slot& slot = m_slots[chosen];
			slot.state = LOADING;
			bundle = slot.nBundle;
			epoch = slot.nEpoch;
		}
		const auto& entry = m_directory.bundles()[bundle];
		Ref<FileAccess> file = FileAccess::open( m_path, FileAccess::READ );
		bool ok = false;
		if ( file.is_valid() && file->get_length() == m_directory.fileBytes() ) {
			file->seek( entry.nOffset );
			const PackedByteArray bytes = file->get_buffer( entry.nEncodedBytes );
			if ( bytes.size() == entry.nEncodedBytes ) {
				memcpy( encoded.data(), bytes.ptr(), entry.nEncodedBytes );
				ok = t3d_vt_demand_decode_bundle( encoded.data(), entry.nEncodedBytes, entry, scratch.data(), ( uint32_t )scratch.size(),
				                                  m_slots[chosen].records.data(), T3D_VT_BUNDLE_MAX_RECORDS );
			}
		}
		// Validate per-cell key/view ordering before publishing any decoded
		// records, including compressed groups from a damaged payload.
		if ( ok ) {
			for ( const auto& c : m_directory.cells() ) {
				if ( c.nBundle != bundle )
					continue;
				const uint32_t first = c.nByteOffset / T3D_VT_DEMAND_BAKED_VISIBILITY_RECORD_BYTES;
				for ( uint32_t k = 1u; k < c.nCount; ++k ) {
					const auto& a = m_slots[chosen].records[first + k - 1u];
					const auto& b = m_slots[chosen].records[first + k];
					if ( a.m_nStableMaterialKey > b.m_nStableMaterialKey ||
					     ( a.m_nStableMaterialKey == b.m_nStableMaterialKey && a.m_nViewBin >= b.m_nViewBin ) ) {
						ok = false;
						break;
					}
				}
				if ( !ok )
					break;
			}
		}
		{
			Terrain3DScopedLock lock( m_mutex );
			Slot& slot = m_slots[chosen];
			++m_stats.nReads;
			if ( ok )
				m_stats.nBytes += entry.nEncodedBytes;
			else
				++m_stats.nInvalid;
			// Slot storage is never recycled during LOADING; reset first
			// joins this thread, and epoch prevents stale publication.
			if ( m_bStop || slot.nEpoch != epoch ) {
				slot.state = FREE;
				++m_stats.nCancelled;
			} else
				slot.state = ok ? READY : FAILED;
		}
	}
}

const t3dVTDemandBakedVisibilityRecord_t *Terrain3DVTDemandBundleStore::findCellHot( const int32_t x, const int32_t z, uint32_t& count,
                                                                                     uint32_t& flags )
{
	count = 0u;
	flags = 0u;
	uint32_t id = 0u;
	if ( !m_bOpen || !m_directory.findCellHot( x, z, id ) )
		return nullptr;
	const auto& c = m_directory.cells()[id];
	Terrain3DScopedLock lock( m_mutex );
	const int32_t slotId = findSlotLocked( c.nBundle );
	if ( slotId < 0 || m_slots[slotId].state != READY ) {
		++m_stats.nCacheMisses;
		return nullptr;
	}
	Slot& s = m_slots[slotId];
	++m_stats.nCacheHits;
	s.nAge = ++m_nUseClock;
	flags = c.nFlags;
	count = c.nCount;
	return s.records.data() + c.nByteOffset / T3D_VT_DEMAND_BAKED_VISIBILITY_RECORD_BYTES;
}

t3dVTDemandBundleStats_t Terrain3DVTDemandBundleStore::stats() const
{
	Terrain3DScopedLock lock( m_mutex );
	t3dVTDemandBundleStats_t stats = m_stats;
	for ( const Slot& s : m_slots ) {
		stats.nQueued += s.state == QUEUED || s.state == LOADING;
		stats.nResident += s.state == READY;
	}
	return stats;
}
