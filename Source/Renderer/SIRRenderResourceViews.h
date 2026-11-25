#ifndef __SIR_RENDERRESOURCEVIEWS_H__
#define __SIR_RENDERRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRShaderResourceView final : public SIRBlobResourceView { public: explicit SIRShaderResourceView( const SIRResourceView& rView ) noexcept : SIRBlobResourceView( rView ) {} SIRResourceResult getEntryPoint( uint32_t nStageId, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRRenderTextureView final : public SIRImageResourceView { public: explicit SIRRenderTextureView( const SIRResourceView& rView ) noexcept : SIRImageResourceView( rView ) {} SIRResourceResult getUploadPlane( uint32_t iMip, uint32_t iLayer, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRRenderMeshView final : public SIRGeometryResourceView { public: explicit SIRRenderMeshView( const SIRResourceView& rView ) noexcept : SIRGeometryResourceView( rView ) {} SIRResourceResult getDrawIndices( uint32_t iLod, uint32_t iSubmesh, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRMaterialView final : public SIRCompositionResourceView { public: explicit SIRMaterialView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getMaterialInput( uint32_t iInput, SIRResourceDependency *pDependency ) const noexcept; };
static_assert( sizeof( SIRRenderMeshView ) == sizeof( SIRResourceView ), "Render mesh view owns data" );
static_assert( sizeof( SIRRenderTextureView ) == sizeof( SIRResourceView ), "Render texture view owns data" );
#endif
