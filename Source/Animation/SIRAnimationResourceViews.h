#ifndef __SIR_ANIMATIONRESOURCEVIEWS_H__
#define __SIR_ANIMATIONRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRSkinnedMeshView final : public SIRGeometryResourceView { public: explicit SIRSkinnedMeshView( const SIRResourceView& rView ) noexcept : SIRGeometryResourceView( rView ) {} SIRResourceResult getJointInfluences( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRSkeletonView final : public SIRHierarchyResourceView { public: explicit SIRSkeletonView( const SIRResourceView& rView ) noexcept : SIRHierarchyResourceView( rView ) {} SIRResourceResult getBindPose( SIRResourceConstBytes *pPose ) const noexcept; };
class SIRAnimationClipView final : public SIRTimeSeriesResourceView { public: explicit SIRAnimationClipView( const SIRResourceView& rView ) noexcept : SIRTimeSeriesResourceView( rView ) {} SIRResourceResult getAnimationTrack( uint32_t iTrack, SIRResourceConstBytes *pKeys ) const noexcept; };
#endif
