#ifndef __SIR_PHYSICSRESOURCEVIEWS_H__
#define __SIR_PHYSICSRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRCollisionMeshView final : public SIRGeometryResourceView { public: explicit SIRCollisionMeshView( const SIRResourceView& rView ) noexcept : SIRGeometryResourceView( rView ) {} SIRResourceResult getCollisionIndices( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRCollisionHeightView final : public SIRHeightFieldView { public: explicit SIRCollisionHeightView( const SIRResourceView& rView ) noexcept : SIRHeightFieldView( rView ) {} SIRResourceResult getCollisionPatch( uint32_t iLod, uint32_t nTileId, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRPhysicsMaterialView final : public SIRParameterSetView { public: explicit SIRPhysicsMaterialView( const SIRResourceView& rView ) noexcept : SIRParameterSetView( rView ) {} SIRResourceResult getPhysicalProperties( SIRResourceConstBytes *pProperties ) const noexcept; };
static_assert( sizeof( SIRCollisionMeshView ) == sizeof( SIRResourceView ), "Collision mesh view owns data" );
#endif
