#ifndef __SIR_WORLDRESOURCEVIEWS_H__
#define __SIR_WORLDRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRSceneView final : public SIRCompositionResourceView { public: explicit SIRSceneView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getSceneChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept; };
class SIRPrefabView final : public SIRCompositionResourceView { public: explicit SIRPrefabView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getPrefabChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept; };
class SIRWorldCellView final : public SIRCompositionResourceView { public: explicit SIRWorldCellView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getResidentChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept; };
#endif
