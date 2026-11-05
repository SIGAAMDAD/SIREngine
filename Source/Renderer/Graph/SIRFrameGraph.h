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

#ifndef __SIRENGINE_SIRFRAMEGRAPH_H__
#define __SIRENGINE_SIRFRAMEGRAPH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

typedef struct sirFrameGraphResource_s
{
	sirGraphicsHandle_t hImported;
	sirGraphicsHandleType_t eType;
	sirGraphicsState_t eInitialState;
	sirGraphicsQueue_t eInitialQueue;
	uint32_t nMipLevels;
	uint32_t nArrayLayers;
	bool bImported;
} sirFrameGraphResource_t;

typedef struct sirFrameGraphPass_s
{
	sirGraphicsQueue_t eQueue;
	uint64_t nRequiredFeatures;
	bool bSideEffect;
} sirFrameGraphPass_t;

typedef struct sirFrameGraphAccess_s
{
	uint32_t nPass;
	uint32_t nResource;
	uint32_t nBaseMip;
	uint32_t nMipCount;
	uint32_t nBaseLayer;
	uint32_t nLayerCount;
	sirGraphicsState_t eState;
	bool bWrite;
} sirFrameGraphAccess_t;

typedef struct sirFrameGraphTransition_s
{
	uint32_t nResource;
	uint32_t nBeforePass;
	uint32_t nAfterPass;
	uint32_t nBaseMip;
	uint32_t nMipCount;
	uint32_t nBaseLayer;
	uint32_t nLayerCount;
	sirGraphicsQueue_t eFromQueue;
	sirGraphicsQueue_t eToQueue;
	sirGraphicsState_t eBefore;
	sirGraphicsState_t eAfter;
	bool bWriteHazard;
} sirFrameGraphTransition_t;

typedef struct sirFrameGraphDependency_s
{
	uint32_t nBeforePass;
	uint32_t nAfterPass;
	bool bCrossQueue;
} sirFrameGraphDependency_t;

// Shared per-frame graph hard limits; no runtime allocation in graph compilation.
constexpr uint32_t SIR_FRAMEGRAPH_MAX_PASSES = 64;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_RESOURCES = 128;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_ACCESSES = 512;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_TRANSITIONS = 512;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_DEPENDENCIES = 512;
constexpr uint32_t SIR_FRAMEGRAPH_INVALID_INDEX = UINT32_MAX;

typedef struct sirFrameGraphCompiled_s
{
	uint32_t nPassCount;
	uint32_t nTransitionCount;
	uint32_t nDependencyCount;
	uint32_t szPassOrder[SIR_FRAMEGRAPH_MAX_PASSES];
	uint32_t szFirstUse[SIR_FRAMEGRAPH_MAX_RESOURCES];
	uint32_t szLastUse[SIR_FRAMEGRAPH_MAX_RESOURCES];
	sirFrameGraphTransition_t szTransitions[SIR_FRAMEGRAPH_MAX_TRANSITIONS];
	sirFrameGraphDependency_t szDependencies[SIR_FRAMEGRAPH_MAX_DEPENDENCIES];
} sirFrameGraphCompiled_t;

class SIRFrameGraph
{
public:
	SIRFrameGraph( void ) noexcept = default;
	~SIRFrameGraph( void ) noexcept = default;

	uint32_t getPassCount( void ) const noexcept;
	uint32_t getResourceCount( void ) const noexcept;

	void reset( void ) noexcept;
	sirGraphicsResult_t addResource( const sirFrameGraphResource_t& resource, uint32_t *pIndex ) noexcept;
	sirGraphicsResult_t addPass( const sirFrameGraphPass_t& pass, uint32_t *pIndex ) noexcept;
	sirGraphicsResult_t addAccess( const sirFrameGraphAccess_t& access ) noexcept;
	sirGraphicsResult_t addDependency( uint32_t nBefore, uint32_t nAfter ) noexcept;
	sirGraphicsResult_t compile( const sirGraphicsCapabilities_t& caps, sirFrameGraphCompiled_t *pCompiled ) const noexcept;

protected:
private:
	static bool overlaps( const sirFrameGraphAccess_t& a, const sirFrameGraphAccess_t& b ) noexcept;
	static bool covers( const sirFrameGraphAccess_t& a, const sirFrameGraphAccess_t& b ) noexcept;
	static bool isWritableState( sirGraphicsState_t eState ) noexcept;
	static bool isReadableState( sirGraphicsState_t eState ) noexcept;
	bool requiresOrdering( const sirFrameGraphAccess_t& previous, const sirFrameGraphAccess_t& current ) const noexcept;
	sirGraphicsResult_t validateCapabilities( const sirGraphicsCapabilities_t& caps ) const noexcept;
	sirGraphicsResult_t buildTransitions( uint8_t *pszEdges, sirFrameGraphCompiled_t *pCompiled ) const noexcept;
	sirGraphicsResult_t buildSchedule( const uint8_t *pszEdges, sirFrameGraphCompiled_t *pCompiled ) const noexcept;
	void calculateLifetimes( sirFrameGraphCompiled_t *pCompiled ) const noexcept;

	sirFrameGraphResource_t m_szResources[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	sirFrameGraphPass_t m_szPasses[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	sirFrameGraphAccess_t m_szAccesses[SIR_FRAMEGRAPH_MAX_ACCESSES] = {};
	uint8_t m_szExplicitEdges[SIR_FRAMEGRAPH_MAX_PASSES * SIR_FRAMEGRAPH_MAX_PASSES] = {};
	uint32_t m_nResourceCount = 0;
	uint32_t m_nPassCount = 0;
	uint32_t m_nAccessCount = 0;
};

#endif // __SIRENGINE_SIRFRAMEGRAPH_H__
