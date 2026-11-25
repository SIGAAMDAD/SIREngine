#ifndef __SIR_NAVIGATIONRESOURCEVIEWS_H__
#define __SIR_NAVIGATIONRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRNavigationGeometryView final : public SIRGeometryResourceView { public: explicit SIRNavigationGeometryView( const SIRResourceView& rView ) noexcept : SIRGeometryResourceView( rView ) {} SIRResourceResult getWalkableIndices( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRNavigationGraphView final : public SIRGraphResourceView { public: explicit SIRNavigationGraphView( const SIRResourceView& rView ) noexcept : SIRGraphResourceView( rView ) {} SIRResourceResult getAdjacency( uint32_t nNodeId, SIRResourceConstBytes *pAdjacentNodes ) const noexcept; };
#endif
