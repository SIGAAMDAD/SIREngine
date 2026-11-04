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

#ifndef __SIRENGINE_SIRRESOURCEVIEWS_H__
#define __SIRENGINE_SIRRESOURCEVIEWS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRResourceTypes.h"

class SIRResourceGraph;
struct SIRPublishedResourceRecord;

// The only reader lifetime root: individual views do not own or retain payloads.
class SIRResourceReadScope
{
private:
	const SIRResourceGraph *m_pGraph;
	uint64_t m_nReaderToken;

public:
	explicit SIRResourceReadScope( const SIRResourceGraph& rGraph ) noexcept;
	~SIRResourceReadScope( void ) noexcept;

	SIRResourceReadScope( const SIRResourceReadScope& ) = delete;
	SIRResourceReadScope& operator=( const SIRResourceReadScope& ) = delete;
	SIRResourceReadScope( SIRResourceReadScope&& ) = delete;
	SIRResourceReadScope& operator=( SIRResourceReadScope&& ) = delete;

	bool isActive( void ) const noexcept;

	friend class SIRResourceGraph;
};

// Lightweight, non-owning read-only projection: exactly two borrowed pointers.
class SIRResourceView
{
protected:
	const SIRResourceReadScope *m_pScope;
	const SIRPublishedResourceRecord *m_pRecord;

	SIRResourceView( const SIRResourceReadScope *pScope, const SIRPublishedResourceRecord *pRecord ) noexcept;

public:
	SIRResourceView( void ) noexcept;
	bool isValid( void ) const noexcept;
	SIRResourceKind getKind( void ) const noexcept;
	SIRResourceHandle getHandle( void ) const noexcept;
	uint64_t getRevision( void ) const noexcept;
	SIRResourceResult getInfo( SIRResourceInfo *pInfo ) const noexcept;
	SIRResourceResult getChunk( SIRResourceChunkId id, SIRResourceConstBytes *pBytes ) const noexcept;
	SIRResourceResult getChunkInfo( uint32_t iChunk, SIRResourceChunkInfo *pInfo ) const noexcept;
	SIRResourceResult getDependency( uint32_t iEdge, SIRResourceDependency *pEdge ) const noexcept;

	friend class SIRResourceGraph;
};

// The following descriptors carry no resource bytes or ownership.
typedef struct sirImageDesc_s
{
	uint32_t m_nWidth;
	uint32_t m_nHeight;
	uint32_t m_nDepth;
	uint32_t m_nLayers;
	uint32_t m_nMips;
	uint32_t m_nFormatId;
} sirImageDesc_t;
typedef sirImageDesc_t SIRImageDesc;

typedef struct sirGeometryDesc_s
{
	uint32_t m_nVertexCount;
	uint32_t m_nIndexCount;
	uint16_t m_nLodCount;
	uint16_t m_nSubmeshCount;
	uint32_t m_nFormatId;
} sirGeometryDesc_t;
typedef sirGeometryDesc_t SIRGeometryDesc;

typedef struct sirFieldDesc_s
{
	uint32_t m_nWidth;
	uint32_t m_nHeight;
	uint32_t m_nDepth;
	uint16_t m_nMipCount;
	uint16_t m_nChannelCount;
	uint32_t m_nFormatId;
} sirFieldDesc_t;
typedef sirFieldDesc_t SIRFieldDesc;

typedef struct sirStreamDesc_s
{
	uint64_t m_nSampleCount;
	uint32_t m_nSampleRate;
	uint32_t m_nChannelCount;
	uint32_t m_nEncodingId;
} sirStreamDesc_t;
typedef sirStreamDesc_t SIRStreamDesc;

typedef struct sirTableDesc_s
{
	uint64_t m_nRowCount;
	uint32_t m_nColumnCount;
	uint32_t m_nSchemaId;
} sirTableDesc_t;
typedef sirTableDesc_t SIRTableDesc;

typedef struct sirGraphDesc_s
{
	uint32_t m_nNodeCount;
	uint32_t m_nEdgeCount;
	uint32_t m_nSchemaId;
} sirGraphDesc_t;
typedef sirGraphDesc_t SIRGraphDesc;

typedef struct sirHierarchyDesc_s
{
	uint32_t m_nNodeCount;
	uint32_t m_nSchemaId;
} sirHierarchyDesc_t;
typedef sirHierarchyDesc_t SIRHierarchyDesc;

typedef struct sirTimeSeriesDesc_s
{
	uint32_t m_nTrackCount;
	uint32_t m_nTickRate;
	uint64_t m_nDurationTicks;
} sirTimeSeriesDesc_t;
typedef sirTimeSeriesDesc_t SIRTimeSeriesDesc;

// All subclasses add methods only, never payload storage or owning containers.
class SIRBlobResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_BLOB;
	explicit SIRBlobResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult getBytes( SIRResourceChunkId id, SIRResourceByteRange range, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRImageResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_IMAGE;
	explicit SIRImageResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeImage( SIRImageDesc *pDesc ) const noexcept;
	SIRResourceResult getImagePlane( uint32_t iMip, uint32_t iLayer, uint32_t iPlane, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRGeometryResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_GEOMETRY;
	explicit SIRGeometryResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeGeometry( SIRGeometryDesc *pDesc ) const noexcept;
	SIRResourceResult getVertexStream( uint32_t iLod, uint32_t nSemanticId, SIRResourceConstBytes *pBytes ) const noexcept;
	SIRResourceResult getIndexStream( uint32_t iLod, SIRResourceConstBytes *pBytes ) const noexcept;
	SIRResourceResult getSubmesh( uint32_t iLod, uint32_t iSubmesh, SIRResourceByteRange *pIndexRange ) const noexcept;
};

class SIRFieldResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_FIELD;
	explicit SIRFieldResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeField( SIRFieldDesc *pDesc ) const noexcept;
	SIRResourceResult getFieldTile( uint32_t iMip, uint32_t nTileId, uint32_t iChannel, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRTableResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_TABLE;
	explicit SIRTableResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeTable( SIRTableDesc *pDesc ) const noexcept;
	SIRResourceResult getColumn( uint32_t iColumn, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRGraphResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_GRAPH;
	explicit SIRGraphResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeGraph( SIRGraphDesc *pDesc ) const noexcept;
	SIRResourceResult getNodes( SIRResourceConstBytes *pNodes ) const noexcept;
	SIRResourceResult getEdges( SIRResourceConstBytes *pEdges ) const noexcept;
};

class SIRHierarchyResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_HIERARCHY;
	explicit SIRHierarchyResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeHierarchy( SIRHierarchyDesc *pDesc ) const noexcept;
	SIRResourceResult getParentIndices( SIRResourceConstBytes *pIndices ) const noexcept;
	SIRResourceResult getLocalTransforms( SIRResourceConstBytes *pTransforms ) const noexcept;
};

class SIRTimeSeriesResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_TIME_SERIES;
	explicit SIRTimeSeriesResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeTimeSeries( SIRTimeSeriesDesc *pDesc ) const noexcept;
	SIRResourceResult getTrack( uint32_t iTrack, SIRResourceConstBytes *pTrack ) const noexcept;
};

class SIRSampleStreamResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_SAMPLE_STREAM;
	explicit SIRSampleStreamResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult describeStream( SIRStreamDesc *pDesc ) const noexcept;
	SIRResourceResult getSampleBlock( uint32_t nBlockId, SIRResourceConstBytes *pBytes ) const noexcept;
};

class SIRCompositionResourceView : public SIRResourceView
{
public:
	static constexpr SIRResourceKind RESOURCE_KIND = SIR_RESOURCE_KIND_COMPOSITION;
	explicit SIRCompositionResourceView( const SIRResourceView& rView ) noexcept : SIRResourceView( rView )
	{
	}
	SIRResourceResult getChild( uint32_t iChild, SIRResourceDependency *pChild ) const noexcept;
};

// Checked typed conversion: never reinterpret one canonical payload as an unrelated family.
template <class T>
SIRResourceResult tryCastResourceView( const SIRResourceView& rView, T *pOut ) noexcept
{
	if ( pOut == nullptr ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( !rView.isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( rView.getKind() != T::RESOURCE_KIND ) {
		return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}
	*pOut = T( rView );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

#endif // __SIRENGINE_SIRRESOURCEVIEWS_H__
