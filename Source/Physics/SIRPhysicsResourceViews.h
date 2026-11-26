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

#ifndef __SIR_PHYSICSRESOURCEVIEWS_H__
#define __SIR_PHYSICSRESOURCEVIEWS_H__
#include <Resources/SIRResourceViews.h>
class SIRCollisionMeshView final : public SIRGeometryResourceView { public: explicit SIRCollisionMeshView( const SIRResourceView& rView ) noexcept : SIRGeometryResourceView( rView ) {} SIRResourceResult getCollisionIndices( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRCollisionHeightView final : public SIRHeightFieldView { public: explicit SIRCollisionHeightView( const SIRResourceView& rView ) noexcept : SIRHeightFieldView( rView ) {} SIRResourceResult getCollisionPatch( uint32_t iLod, uint32_t nTileId, SIRResourceConstBytes *pBytes ) const noexcept; };
class SIRPhysicsMaterialView final : public SIRParameterSetView { public: explicit SIRPhysicsMaterialView( const SIRResourceView& rView ) noexcept : SIRParameterSetView( rView ) {} SIRResourceResult getPhysicalProperties( SIRResourceConstBytes *pProperties ) const noexcept; };
static_assert( sizeof( SIRCollisionMeshView ) == sizeof( SIRResourceView ), "Collision mesh view owns data" );
#endif
