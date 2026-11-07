// Exercises the actual, unmodified Terrain3D T3VD v2 serializer and runtime.
// This test is host-only: no Godot runtime or simulated VT algorithm.
#include "rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.h"
#include <assert.h>
#include <string.h>
#include <vector>

int main( void )
{
	t3dVTDemandCell_t cells[2]{};
	cells[0].nX = -2;
	cells[0].nZ = 4;
	cells[0].nFirstRecord = 0;
	cells[0].nCount = 2;
	cells[1].nX = 3;
	cells[1].nZ = 0;
	cells[1].nFirstRecord = 2;
	cells[1].nCount = 1;
	t3dVTDemandStableMaterialRecord_t records[3]{};
	records[0].nStableMaterialKey = 100;
	records[1].nStableMaterialKey = 200;
	records[2].nStableMaterialKey = 300;
	for ( auto& record : records ) {
		record.Footprint.nCoverage = 32768;
		record.Footprint.nViewMask = 255;
		record.Footprint.nUvMaxX = 65535;
		record.Footprint.nUvMaxY = 65535;
	}
	records[1].nFlags = 1;
	std::vector<uint8_t> bytes, again;
	assert( t3d_vt_demand_write_stable_material( cells, 2, records, 3, 777, 888, bytes ) );
	assert( t3d_vt_demand_write_stable_material( cells, 2, records, 3, 777, 888, again ) );
	assert( bytes == again );
	Terrain3DVTSpatialDemand reader;
	assert( reader.loadCold( bytes.data(), bytes.size() ) );
	assert( reader.getVersion() == T3D_VT_DEMAND_STABLE_MATERIAL_FORMAT_VERSION );
	assert( reader.getAssetIdentityHash() == 888 );
	assert( reader.isValidFor( 777 ) && !reader.isValidFor( 778 ) );
	assert( !reader.hasBoundPredictor() );
	const uint64_t keys[] = { 100, 200, 300 };
	const uint32_t allocations[] = { 24, 27, 38 };
	assert( reader.bindStableKeysCold( keys, allocations, 3 ) );
	assert( reader.hasBoundPredictor() );
	assert( reader.hasCoveredCellHot( -2, 4 ) );
	uint32_t count = 0;
	const t3dVTDemandRecord_t *record = reader.findCellHot( -2, 4, count );
	assert( record && count == 2 );
	assert( record[0].nAllocation == 24 && record[1].nAllocation == 27 );
	assert( reader.findCellHot( 3, 0, count ) != nullptr && count == 1 );
	assert( reader.findCellHot( 9, 9, count ) == nullptr && count == 0 );
	std::vector<uint8_t> bad = bytes;
	bad.back() ^= 0x42;
	assert( !reader.loadCold( bad.data(), bad.size() ) );
	assert( !reader.hasBoundPredictor() );
	assert( !reader.loadCold( bytes.data(), bytes.size() - 1 ) );
	return 0;
}
