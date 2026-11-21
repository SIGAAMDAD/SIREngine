#include "authoring/vt/visibility/terrain_3d_vt_visibility_aggregator.h"
#include <algorithm>
#include <limits.h>
#include <math.h>

namespace
{
struct Candidate
{
	t3dVTDemandBakedVisibilityRecord_t record;
	uint64_t score = 0u;
};
static int32_t group4( const int32_t x )
{
	return x >= 0 ? x / 4 : ( int32_t )( -( ( -( int64_t )x + 3 ) / 4 ) );
}
static bool cellBefore( const t3dVTVisibilityProbeResult_t& a, const t3dVTVisibilityProbeResult_t& b )
{
	const int32_t ax = group4( a.m_nCellX ), bx = group4( b.m_nCellX );
	const int32_t az = group4( a.m_nCellZ ), bz = group4( b.m_nCellZ );
	if ( ax != bx )
		return ax < bx;
	if ( az != bz )
		return az < bz;
	if ( a.m_nCellX != b.m_nCellX )
		return a.m_nCellX < b.m_nCellX;
	return a.m_nCellZ < b.m_nCellZ;
}
static uint32_t saturate32( const uint64_t x )
{
	return x > UINT32_MAX ? UINT32_MAX : ( uint32_t )x;
}
}

bool Terrain3DVTVisibilityAggregator::reduce( const t3dVTVisibilityProbeResult_t *const probes, const uint32_t probeCount,
                                              const uint64_t *const keys, const uint32_t materialCount, const uint32_t perCellLimit,
                                              const bool forcePartial, std::vector<t3dVTDemandVisibilityCell_t>& outCells,
                                              std::vector<t3dVTDemandBakedVisibilityRecord_t>& outRecords,
                                              t3dVTVisibilityAggregateStats_t& outStats )
{
	t3dVTVisibilityAggregateStats_t stats{};
	if ( !probes || !keys || !probeCount || probeCount > T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_CELLS * 5u || !materialCount ||
	     materialCount > T3D_VT_VISIBILITY_MAX_MATERIALS || !perCellLimit || perCellLimit > 1024u )
		return false;
	for ( uint32_t k = 0u; k < materialCount; ++k )
		if ( !keys[k] || ( k && keys[k] <= keys[k - 1u] ) )
			return false;
	const size_t countPerProbe = ( size_t )materialCount * 8u * 16u;
	std::vector<uint32_t> order( probeCount );
	for ( uint32_t i = 0u; i < probeCount; ++i ) {
		const auto& p = probes[i];
		if ( p.m_nSourceRevision == 0u || p.m_nMaterialCount != materialCount || p.m_counts.size() != countPerProbe ||
		     !p.m_nNominalRenderSize || !p.m_nNominalTextureSize )
			return false;
		order[i] = i;
	}
	std::sort( order.begin(), order.end(), [&]( const uint32_t a, const uint32_t b ) { return cellBefore( probes[a], probes[b] ); } );
	std::vector<t3dVTDemandVisibilityCell_t> cells;
	std::vector<t3dVTDemandBakedVisibilityRecord_t> records;
	std::vector<uint32_t> maximum( countPerProbe, 0u );
	std::vector<Candidate> candidates;
	candidates.reserve( ( size_t )materialCount * 8u );
	for ( size_t begin = 0u; begin < order.size(); ) {
		size_t end = begin + 1u;
		const auto& first = probes[order[begin]];
		while ( end < order.size() && probes[order[end]].m_nCellX == first.m_nCellX && probes[order[end]].m_nCellZ == first.m_nCellZ )
			++end;
		if ( cells.size() >= T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_CELLS )
			return false;
		std::fill( maximum.begin(), maximum.end(), 0u );
		bool partial = forcePartial;
		for ( size_t i = begin; i < end; ++i ) {
			const auto& probe = probes[order[i]];
			if ( probe.m_nSourceRevision != first.m_nSourceRevision || probe.m_nNominalTextureSize != first.m_nNominalTextureSize ||
			     probe.m_nNominalRenderSize != first.m_nNominalRenderSize )
				return false;
			partial |= probe.m_bPartial || probe.m_nUncapturedTriangles != 0u;
			for ( size_t k = 0u; k < maximum.size(); ++k )
				maximum[k] = std::max( maximum[k], probe.m_counts[k] );
		}
		candidates.clear();
		for ( uint32_t view = 0u; view < 8u; ++view ) {
			uint64_t viewTotal = 0u;
			for ( uint32_t m = 0u; m < materialCount; ++m )
				for ( uint32_t mip = 0u; mip < 16u; ++mip )
					viewTotal += maximum[( ( size_t )m * 8u + view ) * 16u + mip];
			if ( !viewTotal )
				continue;
			for ( uint32_t material = 0u; material < materialCount; ++material ) {
				uint64_t count = 0u;
				uint64_t score = 0u;
				const uint32_t *const bins = maximum.data() + ( ( size_t )material * 8u + view ) * 16u;
				for ( uint32_t mip = 0u; mip < 16u; ++mip ) {
					count += bins[mip];
					score += ( uint64_t )bins[mip] * ( 16u - mip );
				}
				if ( !count )
					continue;
				Candidate c;
				c.score = score;
				c.record.m_nStableMaterialKey = keys[material];
				c.record.m_nSampleCount = saturate32( count );
				c.record.m_nCoverageSampleCount = c.record.m_nSampleCount;
				c.record.m_nProbeCount = ( uint32_t )( end - begin );
				c.record.m_nSourceRevision = first.m_nSourceRevision;
				c.record.m_nViewBin = ( uint8_t )view;
				c.record.m_nCoverageConfidence = ( uint16_t )std::max( 1ull, std::min( 65535ull, count * 65535ull / viewTotal ) );
				c.record.m_nFlags = partial ? T3D_VT_VISIBILITY_PARTIAL : 0u;
				uint32_t minMip = 0u;
				while ( minMip < 16u && bins[minMip] == 0u )
					++minMip;
				c.record.m_nNominalMipBase = ( uint16_t )std::min( minMip, 15u );
				for ( uint32_t mip = 0u; mip < 16u; ++mip ) {
					c.record.m_szMipWeight[mip] =
					    bins[mip] ? ( uint16_t )std::max( 1ull, std::min( 65535ull, ( uint64_t )bins[mip] * 65535ull / count ) ) : 0u;
				}
				candidates.push_back( c );
			}
		}
		// Never fabricate a visible record for a wholly empty cell. Leaving
		// it absent retains the existing v2/reactive fallback for that cell.
		if ( !candidates.empty() ) {
			std::sort( candidates.begin(), candidates.end(), []( const Candidate& a, const Candidate& b ) {
				if ( a.score != b.score )
					return a.score > b.score;
				if ( a.record.m_nStableMaterialKey != b.record.m_nStableMaterialKey )
					return a.record.m_nStableMaterialKey < b.record.m_nStableMaterialKey;
				return a.record.m_nViewBin < b.record.m_nViewBin;
			} );
			const uint32_t keep = std::min( perCellLimit, ( uint32_t )candidates.size() );
			if ( keep < candidates.size() ) {
				stats.m_nDroppedRecords += ( uint32_t )candidates.size() - keep;
				partial = true;
			}
			t3dVTDemandVisibilityCell_t cell;
			cell.m_nX = first.m_nCellX;
			cell.m_nZ = first.m_nCellZ;
			cell.m_nFirstRecord = ( uint32_t )records.size();
			cell.m_nRecordCount = keep;
			cell.m_nFlags = partial ? 1u : 0u;
			if ( partial )
				++stats.m_nPartialCells;
			for ( uint32_t k = 0u; k < keep; ++k ) {
				t3dVTDemandBakedVisibilityRecord_t r = candidates[k].record;
				if ( partial )
					r.m_nFlags |= T3D_VT_VISIBILITY_PARTIAL;
				for ( uint32_t mip = 0u; mip < 16u; ++mip )
					stats.m_szMipSamples[mip] +=
					    maximum[( ( size_t )( std::lower_bound( keys, keys + materialCount, r.m_nStableMaterialKey ) - keys ) * 8u +
						          r.m_nViewBin ) *
						            16u +
						        mip];
				stats.m_nVisibleSamples += r.m_nSampleCount;
				records.push_back( r );
			}
			std::sort( records.begin() + cell.m_nFirstRecord, records.end(),
			           []( const t3dVTDemandBakedVisibilityRecord_t& a, const t3dVTDemandBakedVisibilityRecord_t& b ) {
				           if ( a.m_nStableMaterialKey != b.m_nStableMaterialKey )
					           return a.m_nStableMaterialKey < b.m_nStableMaterialKey;
				           return a.m_nViewBin < b.m_nViewBin;
			           } );
			cells.push_back( cell );
		}
		stats.m_nProbes += ( uint32_t )( end - begin );
		begin = end;
	}
	if ( records.empty() || records.size() > T3D_VT_DEMAND_BAKED_VISIBILITY_MAX_RECORDS )
		return false;
	stats.m_nCells = ( uint32_t )cells.size();
	stats.m_nRecords = ( uint32_t )records.size();
	outCells.swap( cells );
	outRecords.swap( records );
	outStats = stats;
	return true;
}
