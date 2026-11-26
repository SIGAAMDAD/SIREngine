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

#ifndef __SIR_RENDER_COMMAND_LIST_H__
#define __SIR_RENDER_COMMAND_LIST_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderResource.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderPipeline;
class SIRRenderBindingSet;
class SIRRenderStorageBuffer;
class SIRRenderTexture;
class SIRRenderQueryPool;

class SIRRenderCommandList : public SIRRenderResource
{
public:
	explicit SIRRenderCommandList( const SIRRenderCommandList& ) = delete;
	explicit SIRRenderCommandList( SIRRenderCommandList&& ) = delete;
	virtual ~SIRRenderCommandList() noexcept = default;
	SIRRenderCommandList& operator=( const SIRRenderCommandList& ) = delete;
	SIRRenderCommandList& operator=( SIRRenderCommandList&& ) = delete;

	SIRRenderQueueType getQueueType( void ) const noexcept
	{
		return m_eQueueType;
	}

	virtual SIRRenderResult begin( void ) noexcept = 0;
	virtual SIRRenderResult end( void ) noexcept = 0;
	virtual SIRRenderResult reset( void ) noexcept = 0;
	virtual SIRRenderResult beginRenderPass( void *pPass ) noexcept = 0;
	virtual SIRRenderResult endRenderPass( void ) noexcept = 0;
	virtual SIRRenderResult bindPipeline( const SIRRenderPipeline& pipeline ) noexcept = 0;
	virtual SIRRenderResult bindBindingSet( uint32_t nSetIndex, const SIRRenderBindingSet& bindingSet ) noexcept = 0;
	virtual SIRRenderResult bindVertexBuffer( uint32_t nSlot, const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nStrideBytes ) noexcept = 0;
	virtual SIRRenderResult bindIndexBuffer( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nIndexStrideBytes ) noexcept = 0;
	virtual SIRRenderResult setViewport( const sirRenderViewport_t& viewport, const sirRenderRect_t& scissor ) noexcept = 0;
	virtual SIRRenderResult setConstants( uint32_t nOffsetBytes, SIRSpan<const uint8_t> constants ) noexcept = 0;
	virtual SIRRenderResult draw( uint32_t nVertexCount, uint32_t nInstanceCount, uint32_t nFirstVertex, uint32_t nFirstInstance ) noexcept = 0;
	virtual SIRRenderResult drawIndexed( uint32_t nIndexCount, uint32_t nInstanceCount, uint32_t nFirstIndex, int32_t nVertexOffset, uint32_t nFirstInstance ) noexcept = 0;
	virtual SIRRenderResult drawIndirect( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nDrawCount, uint32_t nStrideBytes ) noexcept = 0;
	virtual SIRRenderResult drawIndexedIndirect( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nDrawCount, uint32_t nStrideBytes ) noexcept = 0;
	virtual SIRRenderResult copyBuffer( const SIRRenderStorageBuffer& destination,
	                                    const SIRRenderStorageBuffer& source,
	                                    const sirRenderBufferRange_t& destinationRange,
	                                    const sirRenderBufferRange_t& sourceRange ) noexcept = 0;
	virtual SIRRenderResult copyTexture( const SIRRenderTexture& destination,
	                                     const SIRRenderTexture& source,
	                                     const sirRenderSubresourceRange_t& destinationRange,
	                                     const sirRenderSubresourceRange_t& sourceRange ) noexcept = 0;
	virtual SIRRenderResult copyBufferToTexture( const SIRRenderStorageBuffer& source,
	                                             const SIRRenderTexture& destination,
	                                             const sirRenderBufferRange_t& sourceRange,
	                                             const sirRenderSubresourceRange_t& destinationRange ) noexcept = 0;
	virtual SIRRenderResult copyTextureToBuffer( const SIRRenderTexture& source,
	                                             const SIRRenderStorageBuffer& destination,
	                                             const sirRenderSubresourceRange_t& sourceRange,
	                                             const sirRenderBufferRange_t& destinationRange ) noexcept = 0;
	virtual SIRRenderResult fillBuffer( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint64_t nSizeBytes, uint32_t nValue ) noexcept = 0;
	virtual SIRRenderResult clearTexture( const SIRRenderTexture& texture ) noexcept = 0;
	virtual SIRRenderResult resolveTexture( const SIRRenderTexture& destination, const SIRRenderTexture& source ) noexcept = 0;
	virtual SIRRenderResult transition( SIRSpan<const uint8_t> barriers ) noexcept = 0;
	virtual SIRRenderResult dispatch( uint32_t nGroupCountX, uint32_t nGroupCountY, uint32_t nGroupCountZ ) noexcept = 0;
	virtual SIRRenderResult dispatchIndirect( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes ) noexcept = 0;
	virtual SIRRenderResult beginQuery( const SIRRenderQueryPool& queryPool, uint32_t iQuery ) noexcept = 0;
	virtual SIRRenderResult endQuery( const SIRRenderQueryPool& queryPool, uint32_t iQuery ) noexcept = 0;
	virtual SIRRenderResult writeTimestamp( const SIRRenderQueryPool& queryPool, uint32_t iQuery ) noexcept = 0;
	virtual SIRRenderResult resetQueries( const SIRRenderQueryPool& queryPool, uint32_t iFirstQuery, uint32_t nQueryCount ) noexcept = 0;
	virtual SIRRenderResult resolveQueries(
	    const SIRRenderQueryPool& queryPool, uint32_t iFirstQuery, uint32_t nQueryCount, const SIRRenderStorageBuffer& destination, uint64_t nDestinationOffsetBytes ) noexcept = 0;
	virtual SIRRenderResult dispatchMesh( uint32_t nGroupCountX, uint32_t nGroupCountY, uint32_t nGroupCountZ ) noexcept = 0;
	virtual SIRRenderResult buildAccelerationStructure( const void *pBuildDescription ) noexcept = 0;
	virtual SIRRenderResult copyAccelerationStructure( const void *pDestination, const void *pSource ) noexcept = 0;
	virtual SIRRenderResult traceRays( const void *pTraceDescription ) noexcept = 0;

protected:
	explicit SIRRenderCommandList( const SIRSharedPtr<SIRRenderDevice>& device, SIRRenderQueueType eQueueType ) noexcept
	    : SIRRenderResource( device ),
	      m_eQueueType( eQueueType )
	{
	}

private:
	SIRRenderQueueType m_eQueueType;
};
#endif // __SIR_RENDER_COMMAND_LIST_H__
