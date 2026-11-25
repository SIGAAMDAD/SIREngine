#ifndef __SIR_TERRAINRESOURCEVIEWS_H__
#define __SIR_TERRAINRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRTerrainHeightView final : public SIRHeightFieldView { public: explicit SIRTerrainHeightView( const SIRResourceView& rView ) noexcept : SIRHeightFieldView( rView ) {} SIRResourceResult getHeightPatch( uint32_t iLod, uint32_t nTileId, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRTerrainTileView final : public SIRCompositionResourceView { public: explicit SIRTerrainTileView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getTerrainLayer( uint32_t iLayer, SIRResourceDependency *pDependency ) const noexcept; };
#endif
