#ifndef __SIR_SCRIPTRESOURCEVIEWS_H__
#define __SIR_SCRIPTRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRScriptResourceView final : public SIRBlobResourceView { public: explicit SIRScriptResourceView( const SIRResourceView& rView ) noexcept : SIRBlobResourceView( rView ) {} SIRResourceResult getModule( SIRResourceConstBytes *pBytes ) const noexcept; };
#endif
