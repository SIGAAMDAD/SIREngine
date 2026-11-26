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

#ifndef __SIR_TERRAINRESOURCEVIEWS_H__
#define __SIR_TERRAINRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRTerrainHeightView final : public SIRHeightFieldView { public: explicit SIRTerrainHeightView( const SIRResourceView& rView ) noexcept : SIRHeightFieldView( rView ) {} SIRResourceResult getHeightPatch( uint32_t iLod, uint32_t nTileId, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRTerrainTileView final : public SIRCompositionResourceView { public: explicit SIRTerrainTileView( const SIRResourceView& rView ) noexcept : SIRCompositionResourceView( rView ) {} SIRResourceResult getTerrainLayer( uint32_t iLayer, SIRResourceDependency *pDependency ) const noexcept; };
#endif
