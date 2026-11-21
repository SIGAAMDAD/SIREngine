// Copyright 2026 Terrain3D contributors.
#if defined( T3D_EDITOR_NATIVE )
#include "authoring/vt/build/terrain_3d_st_godot_region_cooker.h"
#include "authoring/vt/build/terrain_3d_st_region_source_cooker.h"
#include "data/model/terrain_3d_data.h"
#include "data/model/terrain_3d_region.h"
#include "data/formats/terrain_3d_vt_pack_format.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "Terrain3DConstants.h"
#include <godot_cpp/classes/image.hpp>
#include <string.h>
#include <vector>

struct t3dStAuthoringSnapshot_t
{
	godot::PackedByteArray m_height, m_control, m_color;
	t3dStAuthoredRegionSource_t m_input;
};

static bool t3dStFindComposedFromPack( const t3dVTPackBaselineView_t& rPack, const uint32_t nRegion, uint32_t& rBase,
                                       uint32_t& rGeneration )
{
	const uint64_t key = t3d_vt_composed_region_stable_key( nRegion );
	uint32_t first = 0u;
	uint32_t last = rPack.m_pHeader->m_nAllocationCount;
	while ( first < last ) {
		const uint32_t mid = first + ( last - first ) / 2u;
		if ( rPack.m_pAllocationKeys[mid].m_nStableKey < key )
			first = mid + 1u;
		else
			last = mid;
	}
	if ( first >= rPack.m_pHeader->m_nAllocationCount || rPack.m_pAllocationKeys[first].m_nStableKey != key )
		return false;
	const uint32_t id = rPack.m_pAllocationKeys[first].m_nAllocationId;
	if ( id >= rPack.m_pHeader->m_nAllocationCount || rPack.m_pAllocations[id].m_nConsumer != T3D_VT_CONSUMER_COMPOSED ||
	     rPack.m_pAllocations[id].m_nGeneration == 0u )
		return false;
	rBase = t3d_vt_runtime_allocation_base( id );
	rGeneration = rPack.m_pAllocations[id].m_nGeneration;
	return true;
}

bool t3d_st_cook_region_package_cold( Terrain3DData& rData, const godot::PackedByteArray& rFinalV1, godot::PackedByteArray& rRegionPackage,
                                      godot::String& rError )
{
	rError = godot::String();
	t3dVTPackBaselineView_t pack{};
	if ( rFinalV1.is_empty() || !t3d_vt_pack_open_baseline_view( rFinalV1.ptr(), ( uint64_t )rFinalV1.size(), pack ) ||
	     pack.m_pHeader == nullptr || pack.m_pHeader->m_nPackageGeneration == 0u ) {
		rError = "SuperTerrain cannot cook from an invalid COMPOSED V1 package";
		return false;
	}
	const godot::TypedArray<godot::Vector2i> locations = rData.getRegionLocations();
	if ( locations.is_empty() || locations.size() > 1024 ) {
		rError = "SuperTerrain has no valid authored regions";
		return false;
	}
	std::vector<t3dStAuthoringSnapshot_t> snapshots( ( size_t )locations.size() );
	std::vector<t3dStAuthoredRegionSource_t> inputs;
	inputs.reserve( ( size_t )locations.size() );
	for ( int i = 0; i < locations.size(); ++i ) {
		const godot::Vector2i location = locations[i];
		if ( location.x < -T3D_REGION_MAP_HALF || location.x >= T3D_REGION_MAP_HALF || location.y < -T3D_REGION_MAP_HALF ||
		     location.y >= T3D_REGION_MAP_HALF || !rData.ensureCpuRegionResident( location ) ) {
			rError = "SuperTerrain region is outside the frozen map or its CPU source is unavailable";
			return false;
		}
		Terrain3DRegion *const region = rData.getRegionPtr( location );
		if ( region == nullptr || region->isDeleted() ) {
			rError = "SuperTerrain region disappeared during cold cook";
			return false;
		}
		const uint32_t index =
		    ( uint32_t )( location.y + T3D_REGION_MAP_HALF ) * T3D_REGION_MAP_SIZE + ( uint32_t )( location.x + T3D_REGION_MAP_HALF );
		uint32_t allocation = 0u, generation = 0u;
		if ( !t3dStFindComposedFromPack( pack, index, allocation, generation ) ) {
			rError = "SuperTerrain region lacks a canonical composed allocation";
			return false;
		}
		const godot::Ref<godot::Image> height = region->getHeightMap();
		const godot::Ref<godot::Image> control = region->getControlMap();
		const godot::Ref<godot::Image> color = region->getColorMap();
		if ( height.is_null() || control.is_null() || color.is_null() || height->get_format() != godot::Image::FORMAT_R16 ||
		     control->get_format() != godot::Image::FORMAT_RF || color->get_format() != godot::Image::FORMAT_RGBA8 ||
		     height->get_width() != 128 || height->get_height() != 128 || control->get_width() != 128 || control->get_height() != 128 ||
		     color->get_width() != 128 || color->get_height() != 128 ) {
			rError = "SuperTerrain requires exact authored R16/RF/RGBA8 128x128 sources";
			return false;
		}
		t3dStAuthoringSnapshot_t& snapshot = snapshots[( size_t )i];
		snapshot.m_height = height->get_data();
		snapshot.m_control = control->get_data();
		snapshot.m_color = color->get_data();
		if ( snapshot.m_height.size() < 32768 || snapshot.m_control.size() < 65536 || snapshot.m_color.size() < 65536 ) {
			rError = "SuperTerrain authored source bytes are incomplete";
			return false;
		}
		snapshot.m_input.m_pHeightR16 = snapshot.m_height.ptr();
		snapshot.m_input.m_pControlRF = snapshot.m_control.ptr();
		snapshot.m_input.m_pColorRgba8 = snapshot.m_color.ptr();
		snapshot.m_input.m_nRegionIndex = index;
		snapshot.m_input.m_nComposedAllocation = allocation;
		snapshot.m_input.m_nComposedGeneration = generation;
		inputs.push_back( snapshot.m_input );
	}
	std::vector<uint8_t> bytes;
	if ( !Terrain3DStRegionSourceCooker::cookCold( inputs.data(), ( uint32_t )inputs.size(), pack.m_pHeader->m_nPackageGeneration,
	                                               bytes ) ) {
		rError = "SuperTerrain VHM/cutout transaction cook failed";
		return false;
	}
	godot::PackedByteArray staged;
	if ( staged.resize( ( int64_t )bytes.size() ) != godot::OK ) {
		rError = "SuperTerrain VHM/cutout transaction allocation failed";
		return false;
	}
	memcpy( staged.ptrw(), bytes.data(), bytes.size() );
	rRegionPackage = staged;
	return true;
}
#endif // T3D_EDITOR_NATIVE
