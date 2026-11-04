/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRSYSTEMRESOURCEVIEWS_H__
#define __SIRENGINE_SIRSYSTEMRESOURCEVIEWS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRResourceViews.h"

// Derived projections add behavior only. No virtual tables or owning payload members.
// The subsystem decides what its methods mean; GRDG retains the data and identity.

class SIRTextResourceView final : public SIRBlobResourceView
{
public:
	explicit SIRTextResourceView( const SIRResourceView &rView ) noexcept : SIRBlobResourceView( rView )
	{
	}
	SIRResourceResult getUtf8( SIRResourceChunkId id, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRShaderResourceView final : public SIRBlobResourceView
{
public:
	explicit SIRShaderResourceView( const SIRResourceView &rView ) noexcept : SIRBlobResourceView( rView )
	{
	}
	SIRResourceResult getEntryPoint( uint32_t nStageId, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRScriptResourceView final : public SIRBlobResourceView
{
public:
	explicit SIRScriptResourceView( const SIRResourceView &rView ) noexcept : SIRBlobResourceView( rView )
	{
	}
	SIRResourceResult getModule( SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRAudioBankResourceView final : public SIRBlobResourceView
{
public:
	explicit SIRAudioBankResourceView( const SIRResourceView &rView ) noexcept : SIRBlobResourceView( rView )
	{
	}
	SIRResourceResult getBankMemory( SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRRenderTextureView final : public SIRImageResourceView
{
public:
	explicit SIRRenderTextureView( const SIRResourceView &rView ) noexcept : SIRImageResourceView( rView )
	{
	}
	SIRResourceResult getUploadPlane( uint32_t iMip, uint32_t iLayer, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRRenderMeshView final : public SIRGeometryResourceView
{
public:
	explicit SIRRenderMeshView( const SIRResourceView &rView ) noexcept : SIRGeometryResourceView( rView )
	{
	}
	SIRResourceResult getDrawIndices( uint32_t iLod, uint32_t iSubmesh, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRCollisionMeshView final : public SIRGeometryResourceView
{
public:
	explicit SIRCollisionMeshView( const SIRResourceView &rView ) noexcept : SIRGeometryResourceView( rView )
	{
	}
	SIRResourceResult getCollisionIndices( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRNavigationGeometryView final : public SIRGeometryResourceView
{
public:
	explicit SIRNavigationGeometryView( const SIRResourceView &rView ) noexcept : SIRGeometryResourceView( rView )
	{
	}
	SIRResourceResult getWalkableIndices( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRSkinnedMeshView final : public SIRGeometryResourceView
{
public:
	explicit SIRSkinnedMeshView( const SIRResourceView &rView ) noexcept : SIRGeometryResourceView( rView )
	{
	}
	SIRResourceResult getJointInfluences( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRHeightFieldView : public SIRFieldResourceView
{
public:
	explicit SIRHeightFieldView( const SIRResourceView &rView ) noexcept : SIRFieldResourceView( rView )
	{
	}
	SIRResourceResult getHeights( uint32_t iMip, uint32_t nTileId, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRTerrainHeightView final : public SIRHeightFieldView
{
public:
	explicit SIRTerrainHeightView( const SIRResourceView &rView ) noexcept : SIRHeightFieldView( rView )
	{
	}
	SIRResourceResult getHeightPatch( uint32_t iLod, uint32_t nTileId, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRCollisionHeightView final : public SIRHeightFieldView
{
public:
	explicit SIRCollisionHeightView( const SIRResourceView &rView ) noexcept : SIRHeightFieldView( rView )
	{
	}
	SIRResourceResult getCollisionPatch( uint32_t iLod, uint32_t nTileId, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRVolumeFieldView final : public SIRFieldResourceView
{
public:
	explicit SIRVolumeFieldView( const SIRResourceView &rView ) noexcept : SIRFieldResourceView( rView )
	{
	}
	SIRResourceResult getBrick( uint32_t iMip, uint32_t nBrickId, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRParameterSetView : public SIRTableResourceView
{
public:
	explicit SIRParameterSetView( const SIRResourceView &rView ) noexcept : SIRTableResourceView( rView )
	{
	}
	SIRResourceResult findParameter( uint64_t nParameterId, SIRResourceConstBytes *pValue ) const noexcept;
};

class SIRPhysicsMaterialView final : public SIRParameterSetView
{
public:
	explicit SIRPhysicsMaterialView( const SIRResourceView &rView ) noexcept : SIRParameterSetView( rView )
	{
	}
	SIRResourceResult getPhysicalProperties( SIRResourceConstBytes *pProperties ) const noexcept;
};

class SIRNavigationGraphView final : public SIRGraphResourceView
{
public:
	explicit SIRNavigationGraphView( const SIRResourceView &rView ) noexcept : SIRGraphResourceView( rView )
	{
	}
	SIRResourceResult getAdjacency( uint32_t nNodeId, SIRResourceConstBytes *pAdjacentNodes ) const noexcept;
};

class SIRSkeletonView final : public SIRHierarchyResourceView
{
public:
	explicit SIRSkeletonView( const SIRResourceView &rView ) noexcept : SIRHierarchyResourceView( rView )
	{
	}
	SIRResourceResult getBindPose( SIRResourceConstBytes *pPose ) const noexcept;
};

class SIRAnimationClipView final : public SIRTimeSeriesResourceView
{
public:
	explicit SIRAnimationClipView( const SIRResourceView &rView ) noexcept : SIRTimeSeriesResourceView( rView )
	{
	}
	SIRResourceResult getAnimationTrack( uint32_t iTrack, SIRResourceConstBytes *pKeys ) const noexcept;
};

class SIRAudioClipView final : public SIRSampleStreamResourceView
{
public:
	explicit SIRAudioClipView( const SIRResourceView &rView ) noexcept : SIRSampleStreamResourceView( rView )
	{
	}
	SIRResourceResult getEncodedAudioBlock( uint32_t nBlockId, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRSceneView final : public SIRCompositionResourceView
{
public:
	explicit SIRSceneView( const SIRResourceView &rView ) noexcept : SIRCompositionResourceView( rView )
	{
	}
	SIRResourceResult getSceneChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept;
};

class SIRPrefabView final : public SIRCompositionResourceView
{
public:
	explicit SIRPrefabView( const SIRResourceView &rView ) noexcept : SIRCompositionResourceView( rView )
	{
	}
	SIRResourceResult getPrefabChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept;
};

class SIRWorldCellView final : public SIRCompositionResourceView
{
public:
	explicit SIRWorldCellView( const SIRResourceView &rView ) noexcept : SIRCompositionResourceView( rView )
	{
	}
	SIRResourceResult getResidentChild( uint32_t iChild, SIRResourceDependency *pDependency ) const noexcept;
};

class SIRTerrainTileView final : public SIRCompositionResourceView
{
public:
	explicit SIRTerrainTileView( const SIRResourceView &rView ) noexcept : SIRCompositionResourceView( rView )
	{
	}
	SIRResourceResult getTerrainLayer( uint32_t iLayer, SIRResourceDependency *pDependency ) const noexcept;
};

class SIRMaterialView final : public SIRCompositionResourceView
{
public:
	explicit SIRMaterialView( const SIRResourceView &rView ) noexcept : SIRCompositionResourceView( rView )
	{
	}
	SIRResourceResult getMaterialInput( uint32_t iInput, SIRResourceDependency *pDependency ) const noexcept;
};

class SIRAudioEventView final : public SIRCompositionResourceView
{
public:
	explicit SIRAudioEventView( const SIRResourceView &rView ) noexcept : SIRCompositionResourceView( rView )
	{
	}
	SIRResourceResult getSoundSource( uint32_t iSource, SIRResourceDependency *pDependency ) const noexcept;
};

// All view specializations must carry only the two borrowed base pointers.
static_assert( sizeof( SIRImageResourceView ) == sizeof( SIRResourceView ), "Image view owns data" );
static_assert( sizeof( SIRGeometryResourceView ) == sizeof( SIRResourceView ), "Geometry view owns data" );
static_assert( sizeof( SIRFieldResourceView ) == sizeof( SIRResourceView ), "Field view owns data" );
static_assert( sizeof( SIRRenderMeshView ) == sizeof( SIRResourceView ), "Render mesh view owns data" );
static_assert( sizeof( SIRCollisionMeshView ) == sizeof( SIRResourceView ), "Collision mesh view owns data" );
static_assert( sizeof( SIRTerrainHeightView ) == sizeof( SIRResourceView ), "Terrain height view owns data" );
static_assert( sizeof( SIRAudioBankResourceView ) == sizeof( SIRResourceView ), "Audio bank view owns data" );
static_assert( sizeof( SIRWorldCellView ) == sizeof( SIRResourceView ), "World cell view owns data" );

#endif // __SIRENGINE_SIRSYSTEMRESOURCEVIEWS_H__
