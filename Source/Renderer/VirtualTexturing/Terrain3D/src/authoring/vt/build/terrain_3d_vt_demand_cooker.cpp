// Copyright (c) 2023-2026 Terrain3D contributors.
#include "authoring/vt/build/terrain_3d_vt_demand_cooker.h"
#include "authoring/vt/visibility/terrain_3d_vt_visibility_scene_cooker.h"
#include <godot_cpp/classes/project_settings.hpp>
#include "assets/catalog/terrain_3d_assets.h"
#include "authoring/vt/io/terrain_3d_vt_pack_writer.h"
#include "data/model/terrain_3d_data.h"
#include "data/persistence/terrain_3d_file_publication.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "Terrain3DConstants.h"
#include <algorithm>
#include <string.h>
#include <vector>

using namespace godot;

static bool t3dDemandHasKey( const t3dVTPackBaselineView_t& rPack, const uint64_t nKey )
{
	uint32_t low = 0u, high = rPack.m_pHeader->m_nAllocationCount;
	while ( low < high ) {
		const uint32_t mid = low + ( high - low ) / 2u;
		if ( rPack.m_pAllocationKeys[mid].m_nStableKey < nKey ) {
			low = mid + 1u;
		} else {
			high = mid;
		}
	}
	return low < rPack.m_pHeader->m_nAllocationCount && rPack.m_pAllocationKeys[low].m_nStableKey == nKey;
}

bool t3d_vt_demand_cook_asset_sidecar_cold( const Terrain3DData& rData, const Ref<Terrain3DAssets>& rAssets,
                                            const PackedByteArray& rFinalPackage, PackedByteArray& rSidecar, String& rPath, String& rError,
                                            PackedByteArray *const pV1Sidecar, String *const pV1Path )
{
	if ( pV1Sidecar )
		pV1Sidecar->clear();
	if ( pV1Path )
		*pV1Path = String();
	rSidecar.clear();
	rPath = String();
	rError = String();
	if ( rAssets.is_null() || !rAssets->resolveVtDemandIndexPathCold( rPath, &rError ) ) {
		return false;
	}
	if ( rFinalPackage.is_empty() ) {
		rError = "Cannot bake demand without final VT package";
		return false;
	}
	t3dVTPackBaselineView_t pack{};
	if ( !t3d_vt_pack_open_baseline_view( rFinalPackage.ptr(), ( uint64_t )rFinalPackage.size(), pack ) || pack.m_pHeader == nullptr ||
	     pack.m_pHeader->m_nContentSignature == 0u ) {
		rError = "Final VT package is invalid";
		return false;
	}
	const TypedArray<Vector2i> locations = rData.getRegionLocations();
	std::vector<Vector2i> ordered;
	ordered.reserve( locations.size() );
	for ( int i = 0; i < locations.size(); ++i ) {
		ordered.push_back( locations[i] );
	}
	std::sort( ordered.begin(), ordered.end(), []( const Vector2i& a, const Vector2i& b ) { return a.x != b.x ? a.x < b.x : a.y < b.y; } );
	std::vector<t3dVTDemandCell_t> cells;
	std::vector<t3dVTDemandStableMaterialRecord_t> records;
	const uint32_t nTerrainMaterials = ( uint32_t )MIN( rAssets->getTextureCount(), T3D_MAX_TEXTURE_ASSETS );
	for ( const Vector2i& location : ordered ) {
		uint32_t counts[T3D_MAX_TEXTURE_ASSETS]{};
		uint32_t nCoverageTotal = 0u;
		bool incomplete = false;
		const Ref<Terrain3DRegion> region = rData.getRegion( location );
		const Ref<Image> control = region.is_valid() ? region->getControlMap() : Ref<Image>();
		if ( control.is_valid() && !control->is_empty() && control->get_format() == Image::FORMAT_RF &&
		     control->get_width() == T3D_REGION_SIZE && control->get_height() == T3D_REGION_SIZE ) {
			const PackedByteArray bytes = control->get_data();
			const uint8_t *const data = bytes.ptr();
			if ( data != nullptr && bytes.size() == T3D_REGION_SIZE * T3D_REGION_SIZE * 4 ) {
				// Full authored control coverage; samples represent 128 m cells exactly.
				for ( uint32_t texel = 0u; texel < T3D_REGION_SIZE * T3D_REGION_SIZE; ++texel ) {
					uint32_t word = 0u;
					memcpy( &word, data + ( size_t )texel * 4u, 4u );
					if ( t3dIsHole( word ) ) {
						continue;
					}
					if ( t3dIsAuto( word ) ) {
						incomplete = true;
						continue;
					}
					const uint32_t a = t3dGetBase( word ), b = t3dGetOverlay( word );
					if ( a < nTerrainMaterials ) {
						++counts[a];
					}
					if ( b < nTerrainMaterials && b != a ) {
						++counts[b];
					}
					++nCoverageTotal;
				}
			} else {
				incomplete = true;
			}
		} else {
			incomplete = true;
		}
		if ( incomplete ) {
			// Auto terrain and unloaded authored control are conservatively covered.
			// Dynamic shader material selection remains on the reactive fallback path.
			for ( uint32_t m = 0u; m < nTerrainMaterials; ++m ) {
				counts[m] = MAX( counts[m], 1u );
			}
			nCoverageTotal = MAX( nCoverageTotal, 1u );
		}
		std::vector<t3dVTDemandStableMaterialRecord_t> local;
		auto append = [&]( const uint64_t key, const uint32_t coverage ) {
			if ( !t3dDemandHasKey( pack, key ) ) {
				return;
			}
			t3dVTDemandStableMaterialRecord_t item{};
			item.nStableMaterialKey = key;
			item.Footprint.nMip = 0u;
			item.Footprint.nCoverage =
			    ( uint16_t )MAX( 1u, MIN( 65535u, ( uint32_t )( ( uint64_t )coverage * 65535u / MAX( 1u, nCoverageTotal ) ) ) );
			item.Footprint.nUvMaxX = item.Footprint.nUvMaxY = 65535u;
			item.Footprint.nViewMask = 255u;
			item.nFlags = incomplete ? 1u : 0u;
			local.push_back( item );
		};
		// These are the actual stable IDs in the final cooked package.
		const int mapX = location.x + T3D_REGION_MAP_SIZE / 2;
		const int mapZ = location.y + T3D_REGION_MAP_SIZE / 2;
		if ( mapX >= 0 && mapX < T3D_REGION_MAP_SIZE && mapZ >= 0 && mapZ < T3D_REGION_MAP_SIZE ) {
			append( t3d_vt_terrain_region_stable_key( ( uint32_t )( mapZ * T3D_REGION_MAP_SIZE + mapX ) ), nCoverageTotal );
		}
		for ( uint32_t m = 0u; m < nTerrainMaterials; ++m ) {
			if ( counts[m] != 0u ) {
				append( t3d_vt_terrain_material_stable_key( m ), counts[m] );
			}
		}
		std::sort( local.begin(), local.end(),
		           []( const t3dVTDemandStableMaterialRecord_t& a, const t3dVTDemandStableMaterialRecord_t& b ) {
			           if ( a.Footprint.nCoverage != b.Footprint.nCoverage ) {
				           return a.Footprint.nCoverage > b.Footprint.nCoverage;
			           }
			           return a.nStableMaterialKey < b.nStableMaterialKey;
		           } );
		if ( local.empty() ) {
			continue;
		} // Uncovered cell: runtime retains full fallback.
		if ( cells.size() == T3D_VT_DEMAND_MAX_CELLS || records.size() + local.size() > T3D_VT_DEMAND_MAX_RECORDS ||
		     local.size() > T3D_VT_DEMAND_MAX_RECORDS_PER_CELL ) {
			rError = "VT demand index exceeds bounded cell/record capacity";
			return false;
		}
		t3dVTDemandCell_t cell{};
		cell.nX = location.x;
		cell.nZ = location.y;
		cell.nFirstRecord = ( uint32_t )records.size();
		cell.nCount = ( uint16_t )local.size();
		cells.push_back( cell );
		records.insert( records.end(), local.begin(), local.end() );
	}
	if ( records.empty() ) {
		// Mesh-only or completely empty terrain has no static terrain footprint.
		// Publish the VT package normally; all materials use reactive discovery.
		rPath = String();
		return true;
	}
	const CharString utf8 = rPath.utf8();
	const uint64_t nAssetHash = t3d_vt_demand_hash( ( const uint8_t * )utf8.get_data(), ( uint64_t )utf8.length() );
	std::vector<uint8_t> output;
	if ( !t3d_vt_demand_write_stable_material( cells.data(), ( uint32_t )cells.size(), records.data(), ( uint32_t )records.size(),
	                                           pack.m_pHeader->m_nContentSignature, nAssetHash, output ) ) {
		rError = "Serialized T3VD v2 failed round-trip validation";
		return false;
	}
	rSidecar.resize( ( int )output.size() );
	memcpy( rSidecar.ptrw(), output.data(), output.size() );
	// Opt-in authoring-only GPU cook; v2 always remains the canonical fallback.
	// Failure (including no Vulkan device) cannot destroy an otherwise sound
	// v2/VT package publication and cannot leak a partially encoded v1 file.
	ProjectSettings *const settings = ProjectSettings::get_singleton();
	const bool tryGpu = settings && ( bool )settings->get_setting( "terrain3d/vt_gpu_visibility_cook_enabled", false );
	if ( tryGpu && pV1Sidecar && pV1Path ) {
		std::vector<uint64_t> keys;
		keys.reserve( pack.m_pHeader->m_nAllocationCount );
		for ( uint32_t i = 0u; i < pack.m_pHeader->m_nAllocationCount; ++i )
			keys.push_back( pack.m_pAllocationKeys[i].m_nStableKey );
		PackedByteArray candidate;
		t3dVTVisibilityAggregateStats_t stats{};
		String gpuError;
		if ( t3d_vt_visibility_cook_cold( rData, keys.data(), ( uint32_t )keys.size(), pack.m_pHeader->m_nContentSignature, nAssetHash,
		                                  candidate, stats, gpuError ) &&
		     !candidate.is_empty() ) {
			// Never replace the v2 sidecar during the T03/T04 transition.
			*pV1Path = rPath + String( ".v1" );
			*pV1Sidecar = candidate;
			UtilityFunctions::print( "T3VD v1 GPU histogram: ", ( int64_t )stats.m_nCells, " cells, ", ( int64_t )stats.m_nProbes,
			                         " probes, ", ( int64_t )stats.m_nRecords, " records, ", ( int64_t )stats.m_nPartialCells,
			                         " partial cells" );
		} else {
			UtilityFunctions::push_warning( "T3VD v1 GPU cook fell back to v2: ", gpuError );
		}
	}
	// A previous successful v1 bake may belong to different authored heights
	// or controls even when the VT package signature is unchanged. If the GPU
	// baker is disabled/unavailable, invalidate that older histogram in the
	// same package+v2 transaction. Eight bytes intentionally fail the v1
	// header check, leaving v2 and reactive feedback authoritative.
	if ( pV1Sidecar && pV1Path && pV1Sidecar->is_empty() ) {
		const String previousV1 = rPath + String( ".v1" );
		if ( FileAccess::file_exists( previousV1 ) ) {
			static const uint8_t invalidatedV1[8] = { 'T', '3', 'V', 'D', 'O', 'F', 'F', 0u };
			if ( pV1Sidecar->resize( 8 ) != OK ) {
				rError = "Failed to invalidate stale T3VD v1 during VT recook";
				return false;
			}
			memcpy( pV1Sidecar->ptrw(), invalidatedV1, sizeof( invalidatedV1 ) );
			*pV1Path = previousV1;
		}
	}
	return true;
}

bool t3d_vt_demand_publish_triplet_cold( const String& packagePath, const PackedByteArray& package, const String& demandPath,
                                         const PackedByteArray& demand, const String& v1Path, const PackedByteArray& v1, String& error )
{
	// Publication objects retain their previous bytes until every staged
	// artifact is published. Any failure rolls back in reverse order.
	if ( packagePath.is_empty() || package.is_empty() ) {
		error = "Cannot publish empty VT package";
		return false;
	}
	if ( demandPath.is_empty() || demand.is_empty() ) {
		if ( !v1Path.is_empty() || !v1.is_empty() ) {
			error = "T3VD v1 publication requires a v2 fallback";
			return false;
		}
		return t3dPublishPackedFile( packagePath, package, &error ) == OK;
	}
	const bool hasV1 = !v1Path.is_empty() && !v1.is_empty();
	if ( ( v1Path.is_empty() != v1.is_empty() ) || packagePath == demandPath ||
	     ( hasV1 && ( v1Path == demandPath || v1Path == packagePath ) ) ) {
		error = "VT sidecar publication paths/bytes mismatch";
		return false;
	}
	Terrain3DFilePublication pack( packagePath, packagePath + String( ".staged" ), packagePath + String( ".previous" ) );
	Terrain3DFilePublication fallback( demandPath, demandPath + String( ".staged" ), demandPath + String( ".previous" ) );
	Terrain3DFilePublication hist( v1Path, v1Path + String( ".staged" ), v1Path + String( ".previous" ) );
	if ( pack.stagePacked( package ) != OK ) {
		error = pack.getError();
		return false;
	}
	if ( fallback.stagePacked( demand ) != OK ) {
		error = fallback.getError();
		( void )pack.rollback();
		return false;
	}
	if ( hasV1 && hist.stagePacked( v1 ) != OK ) {
		error = hist.getError();
		( void )fallback.rollback();
		( void )pack.rollback();
		return false;
	}
	if ( pack.publish() != OK || fallback.publish() != OK || ( hasV1 && hist.publish() != OK ) ) {
		error = hasV1 && !hist.getError().is_empty() ? hist.getError()
		                                             : ( !fallback.getError().is_empty() ? fallback.getError() : pack.getError() );
		if ( hasV1 )
			( void )hist.rollback();
		( void )fallback.rollback();
		( void )pack.rollback();
		return false;
	}
	// All three new files are visible before backups are cleaned.
	const Error eV1 = hasV1 ? hist.commit() : OK;
	const Error eV2 = fallback.commit();
	const Error ePack = pack.commit();
	if ( eV1 != OK || eV2 != OK || ePack != OK ) {
		error = eV1 != OK ? hist.getError() : ( eV2 != OK ? fallback.getError() : pack.getError() );
		return false;
	}
	return true;
}

bool t3d_vt_demand_publish_quartet_cold( const String& packagePath, const PackedByteArray& package, const String& demandPath,
                                         const PackedByteArray& demand, const String& v1Path, const PackedByteArray& v1,
                                         const String& surfacePath, const PackedByteArray& surface, String& error )
{
	const bool hasDemand = !demandPath.is_empty() && !demand.is_empty();
	const bool hasV1 = !v1Path.is_empty() && !v1.is_empty();
	if ( packagePath.is_empty() || package.is_empty() || surfacePath.is_empty() || surface.is_empty() ||
	     ( demandPath.is_empty() != demand.is_empty() ) || ( v1Path.is_empty() != v1.is_empty() ) || ( hasV1 && !hasDemand ) ||
	     packagePath == surfacePath || ( hasDemand && ( demandPath == packagePath || demandPath == surfacePath ) ) ||
	     ( hasV1 && ( v1Path == packagePath || v1Path == surfacePath || v1Path == demandPath ) ) ) {
		error = "SuperTerrain package publication requires non-overlapping complete artifacts";
		return false;
	}
	Terrain3DFilePublication pack( packagePath, packagePath + String( ".staged" ), packagePath + String( ".previous" ) );
	Terrain3DFilePublication surf( surfacePath, surfacePath + String( ".staged" ), surfacePath + String( ".previous" ) );
	Terrain3DFilePublication fallback( demandPath, demandPath + String( ".staged" ), demandPath + String( ".previous" ) );
	Terrain3DFilePublication hist( v1Path, v1Path + String( ".staged" ), v1Path + String( ".previous" ) );
	if ( pack.stagePacked( package ) != OK ) {
		error = pack.getError();
		return false;
	}
	if ( surf.stagePacked( surface ) != OK ) {
		error = surf.getError();
		( void )pack.rollback();
		return false;
	}
	if ( hasDemand && fallback.stagePacked( demand ) != OK ) {
		error = fallback.getError();
		( void )surf.rollback();
		( void )pack.rollback();
		return false;
	}
	if ( hasV1 && hist.stagePacked( v1 ) != OK ) {
		error = hist.getError();
		if ( hasDemand )
			( void )fallback.rollback();
		( void )surf.rollback();
		( void )pack.rollback();
		return false;
	}
	if ( pack.publish() != OK || surf.publish() != OK || ( hasDemand && fallback.publish() != OK ) || ( hasV1 && hist.publish() != OK ) ) {
		error = hasV1 && !hist.getError().is_empty() ? hist.getError()
		                                             : ( hasDemand && !fallback.getError().is_empty()
		                                                     ? fallback.getError()
		                                                     : ( !surf.getError().is_empty() ? surf.getError() : pack.getError() ) );
		if ( hasV1 )
			( void )hist.rollback();
		if ( hasDemand )
			( void )fallback.rollback();
		( void )surf.rollback();
		( void )pack.rollback();
		return false;
	}
	const Error eHist = hasV1 ? hist.commit() : OK;
	const Error eDemand = hasDemand ? fallback.commit() : OK;
	const Error eSurface = surf.commit();
	const Error ePackage = pack.commit();
	if ( eHist != OK || eDemand != OK || eSurface != OK || ePackage != OK ) {
		error = eHist != OK ? hist.getError() : eDemand != OK ? fallback.getError() : eSurface != OK ? surf.getError() : pack.getError();
		return false;
	}
	return true;
}

bool t3d_vt_demand_publish_pair_cold( const String& packagePath, const PackedByteArray& package, const String& demandPath,
                                      const PackedByteArray& demand, String& error )
{
	return t3d_vt_demand_publish_triplet_cold( packagePath, package, demandPath, demand, String(), PackedByteArray(), error );
}
