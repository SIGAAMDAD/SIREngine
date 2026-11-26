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

#ifndef __SIR_AUDIORESOURCEVIEWS_H__
#define __SIR_AUDIORESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRAudioBankResourceView final : public SIRBlobResourceView { public: explicit SIRAudioBankResourceView( const SIRResourceView& rView ) noexcept : SIRBlobResourceView( rView ) {} SIRResourceResult getBankMemory( SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRAudioClipView final : public SIRSampleStreamResourceView { public: explicit SIRAudioClipView( const SIRResourceView& rView ) noexcept : SIRSampleStreamResourceView( rView ) {} SIRResourceResult getEncodedAudioBlock( uint32_t nBlockId, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRAudioEventView final : public SIRCompositionResourceView { public: explicit SIRAudioEventView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getSoundSource( uint32_t iSource, SIRResourceDependency *pDependency ) const noexcept; };
#endif
