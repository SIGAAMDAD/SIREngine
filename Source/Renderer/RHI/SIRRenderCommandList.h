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

#ifndef __SIR_RENDER_COMMAND_LIST_H__
#define __SIR_RENDER_COMMAND_LIST_H__

#include "Renderer/SIRGraphicsTypes.h"
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Platform-independent command recording contract.
class SIRRenderCommandList
{
public:
	constexpr explicit SIRRenderCommandList( void ) noexcept = default;

	virtual SIRGraphicsQueue getQueue( void ) const noexcept = 0;
	virtual SIRGraphicsResult reset( void ) noexcept = 0;
	virtual SIRGraphicsResult barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept = 0;
	virtual SIRGraphicsResult copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource, uint64_t nSrcOffset, uint64_t nBytes ) noexcept = 0;
	virtual SIRGraphicsResult copyBufferTexture( const sirGraphicsBufferTextureCopy_t& copy ) noexcept = 0;
	virtual SIRGraphicsResult beginRenderPass( const sirGraphicsRenderPass_t& pass ) noexcept = 0;
	virtual SIRGraphicsResult endRenderPass( void ) noexcept = 0;
	virtual SIRGraphicsResult bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept = 0;
	virtual SIRGraphicsResult bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept = 0;
	virtual SIRGraphicsResult bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint32_t nStrideBytes ) noexcept = 0;
	virtual SIRGraphicsResult bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, bool bIndex32 ) noexcept = 0;
	virtual SIRGraphicsResult setViewport( const sirGraphicsViewport_t& viewport, const sirGraphicsRect_t& scissor ) noexcept = 0;
	virtual SIRGraphicsResult draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex, uint32_t nFirstInstance ) noexcept = 0;
	virtual SIRGraphicsResult drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset, uint32_t nFirstInstance ) noexcept = 0;
	virtual SIRGraphicsResult dispatch( uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept = 0;
	virtual SIRGraphicsResult close( void ) noexcept = 0;

protected:
	virtual ~SIRRenderCommandList() noexcept = default;
};

#endif // __SIR_RENDER_COMMAND_LIST_H__
