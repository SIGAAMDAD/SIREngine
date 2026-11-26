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

#ifndef __SIR_ANIMATIONRESOURCEVIEWS_H__
#define __SIR_ANIMATIONRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRSkinnedMeshView final : public SIRGeometryResourceView { public: explicit SIRSkinnedMeshView( const SIRResourceView& rView ) noexcept : SIRGeometryResourceView( rView ) {} SIRResourceResult getJointInfluences( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRSkeletonView final : public SIRHierarchyResourceView { public: explicit SIRSkeletonView( const SIRResourceView& rView ) noexcept : SIRHierarchyResourceView( rView ) {} SIRResourceResult getBindPose( SIRResourceConstBytes *pPose ) const noexcept; };
class SIRAnimationClipView final : public SIRTimeSeriesResourceView { public: explicit SIRAnimationClipView( const SIRResourceView& rView ) noexcept : SIRTimeSeriesResourceView( rView ) {} SIRResourceResult getAnimationTrack( uint32_t iTrack, SIRResourceConstBytes *pKeys ) const noexcept; };
#endif
