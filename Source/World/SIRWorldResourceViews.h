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

#ifndef __SIR_WORLDRESOURCEVIEWS_H__
#define __SIR_WORLDRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRSceneView final : public SIRCompositionResourceView { public: explicit SIRSceneView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getSceneChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept; };
class SIRPrefabView final : public SIRCompositionResourceView { public: explicit SIRPrefabView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getPrefabChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept; };
class SIRWorldCellView final : public SIRCompositionResourceView { public: explicit SIRWorldCellView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getResidentChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept; };
#endif
