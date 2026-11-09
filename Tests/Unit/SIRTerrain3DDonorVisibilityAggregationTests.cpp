// Runs the exact Terrain3D T03 offline visibility aggregation implementation.
#include "authoring/vt/visibility/terrain_3d_vt_visibility_aggregator.h"
#include <assert.h>
#include <vector>

int main( void )
{
	const uint64_t keys[] = { 10u, 20u, 30u };
	t3dVTVisibilityProbeResult_t probes[3]{};
	for ( uint32_t i = 0; i < 3; ++i ) {
		auto& r = probes[i];
		r.m_nCellX = i < 2 ? -1 : 4;
		r.m_nCellZ = i < 2 ? -5 : 0;
		r.m_nSourceRevision = 3;
		r.m_nMaterialCount = 3;
		r.m_nNominalTextureSize = 4096;
		r.m_nNominalRenderSize = 128;
		r.m_counts.resize( 3u * 8u * 16u );
		r.m_counts[( 0u * 8u + 0u ) * 16u + 1u] = 10 + i;
		r.m_counts[( 1u * 8u + 0u ) * 16u + 2u] = 20;
		r.m_counts[( 2u * 8u + 0u ) * 16u + 3u] = 30;
		r.m_bHasVisibleSamples = true;
	}
	probes[1].m_counts[1] = 90;
	std::vector<t3dVTDemandVisibilityCell_t> cells;
	std::vector<t3dVTDemandBakedVisibilityRecord_t> records;
	t3dVTVisibilityAggregateStats_t stats;
	assert( Terrain3DVTVisibilityAggregator::reduce( probes, 3, keys, 3, 2, false, cells, records, stats ) );
	assert( cells.size() == 2 && records.size() == 4 );
	assert( stats.m_nDroppedRecords == 2 && stats.m_nPartialCells == 2 );
	assert( cells[0].m_nX == -1 && cells[0].m_nZ == -5 );
	assert( records[0].m_nSourceRevision == 3 && records[0].m_nProbeCount == 2 );
	std::vector<t3dVTDemandVisibilityCell_t> secondCells;
	std::vector<t3dVTDemandBakedVisibilityRecord_t> secondRecords;
	t3dVTVisibilityAggregateStats_t secondStats;
	assert( Terrain3DVTVisibilityAggregator::reduce( probes, 3, keys, 3, 2, false, secondCells, secondRecords, secondStats ) );
	assert( secondCells.size() == cells.size() && secondRecords.size() == records.size() );
	for ( size_t i = 0; i < cells.size(); ++i ) {
		assert( secondCells[i].m_nX == cells[i].m_nX );
		assert( secondCells[i].m_nZ == cells[i].m_nZ );
		assert( secondCells[i].m_nRecordCount == cells[i].m_nRecordCount );
	}
	for ( size_t i = 0; i < records.size(); ++i ) {
		assert( secondRecords[i].m_nStableMaterialKey == records[i].m_nStableMaterialKey );
		assert( secondRecords[i].m_nProbeCount == records[i].m_nProbeCount );
	}
	return 0;
}
