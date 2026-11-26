/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_SCRIPTRESOURCEVIEWS_H__
#define __SIR_SCRIPTRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRScriptResourceView final : public SIRBlobResourceView { public: explicit SIRScriptResourceView( const SIRResourceView& rView ) noexcept : SIRBlobResourceView( rView ) {} SIRResourceResult getModule( SIRResourceConstBytes *pBytes ) const noexcept; };
#endif
