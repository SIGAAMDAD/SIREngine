#ifndef __SIR_AUDIORESOURCEVIEWS_H__
#define __SIR_AUDIORESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRAudioBankResourceView final : public SIRBlobResourceView { public: explicit SIRAudioBankResourceView( const SIRResourceView& rView ) noexcept : SIRBlobResourceView( rView ) {} SIRResourceResult getBankMemory( SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRAudioClipView final : public SIRSampleStreamResourceView { public: explicit SIRAudioClipView( const SIRResourceView& rView ) noexcept : SIRSampleStreamResourceView( rView ) {} SIRResourceResult getEncodedAudioBlock( uint32_t nBlockId, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRAudioEventView final : public SIRCompositionResourceView { public: explicit SIRAudioEventView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getSoundSource( uint32_t iSource, SIRResourceDependency *pDependency ) const noexcept; };
#endif
