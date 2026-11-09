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

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Renderer/SIRGraphicsTypes.h>
#include <Renderer/SIRSharedGPUBackingOwner.h>

// One semantic frame product per view/eye/quality/epoch; zero means unkeyed
// transient scratch, not an authoritative scene product.
typedef struct sirFrameGraphProductKey_s
{
	uint64_t nViewId;
	uint64_t nFrameEpoch;
	uint32_t nSemanticId;
	uint32_t nEyeId;
	uint32_t nQualityId;
} sirFrameGraphProductKey_t;

typedef struct sirFrameGraphResource_s
{
	sirGraphicsHandle_t hImported;
	sirGraphicsHandleType_t eType;
	sirGraphicsState_t eInitialState;
	sirGraphicsQueue_t eInitialQueue;
	uint32_t nMipLevels;
	uint32_t nArrayLayers;
	bool bImported;
	// Nonzero only for a GRDG-backed immutable buffer import. These are IDs,
	// not second storage handles; the caller retains the provider borrow.
	uint64_t nCanonicalBackingId;
	uint64_t nCanonicalNativeToken;
	sirFrameGraphProductKey_t product;
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

// An output exports an explicitly initialized subresource interval.
typedef struct sirFrameGraphOutput_s
{
	uint32_t nResource;
	uint32_t nBaseMip;
	uint32_t nMipCount;
	uint32_t nBaseLayer;
	uint32_t nLayerCount;
} sirFrameGraphOutput_t;

// CPU recording only; callers must stage reversible work, not submit GPU queues.
typedef sirGraphicsResult_t ( *sirFrameGraphRecordFn_t )( uint32_t nPass, void *pContext ) noexcept;
typedef void ( *sirFrameGraphRollbackFn_t )( uint32_t nPass, void *pContext ) noexcept;

typedef struct sirFrameGraphCallbacks_s
{
	sirFrameGraphRecordFn_t pRecord;
	sirFrameGraphRollbackFn_t pRollback;
	void *pContext;
} sirFrameGraphCallbacks_t;

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

// Source-pinned GPU reads are tracked through compile as independent consumer /
// provider dependencies, without copying the canonical physical storage.
typedef struct sirFrameGraphSharedRead_s
{
	uint32_t nPass;
	uint32_t nResource;
	uint32_t nConsumerId;
	uint32_t nCompletionProviderId;
	uint32_t nBorrowSlot;
	uint32_t nBorrowGeneration;
	uint64_t nPhysicalBackingId;
	uint64_t nNativeResourceToken;
	uint64_t nOffsetBytes;
	uint64_t nBytes;
} sirFrameGraphSharedRead_t;

// Shared per-frame graph hard limits; no runtime allocation in graph compilation.
constexpr uint32_t SIR_FRAMEGRAPH_MAX_PASSES = 64;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_OUTPUTS = 64;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_RESOURCES = 128;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_ACCESSES = 512;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_TRANSITIONS = 512;
constexpr uint32_t SIR_FRAMEGRAPH_MAX_DEPENDENCIES = 512;
constexpr uint32_t SIR_FRAMEGRAPH_INVALID_INDEX = UINT32_MAX;

typedef struct sirFrameGraphCompiled_s
{
	const void *pOwner;
	uint64_t nOwnerIdentity;
	uint64_t nRevision;
	uint32_t nOriginalPassCount;
	uint32_t nPassCount;
	uint8_t szLivePasses[SIR_FRAMEGRAPH_MAX_PASSES];
	uint32_t nTransitionCount;
	uint32_t nDependencyCount;
	uint32_t nSharedReadCount;
	sirFrameGraphSharedRead_t szSharedReads[SIR_FRAMEGRAPH_MAX_ACCESSES];
	uint32_t szPassOrder[SIR_FRAMEGRAPH_MAX_PASSES];
	uint32_t szFirstUse[SIR_FRAMEGRAPH_MAX_RESOURCES];
	uint32_t szLastUse[SIR_FRAMEGRAPH_MAX_RESOURCES];
	sirFrameGraphTransition_t szTransitions[SIR_FRAMEGRAPH_MAX_TRANSITIONS];
	sirFrameGraphDependency_t szDependencies[SIR_FRAMEGRAPH_MAX_DEPENDENCIES];
} sirFrameGraphCompiled_t;

typedef struct sirFrameGraphTransientInput_s sirFrameGraphTransientInput_t;
typedef struct sirFrameGraphTransientPlan_s sirFrameGraphTransientPlan_t;

class SIRFrameGraph
{
public:
	explicit SIRFrameGraph( const SIRSharedGPUBackingOwner *pSharedOwner = nullptr ) noexcept;
	~SIRFrameGraph( void ) noexcept = default;

	uint32_t getPassCount( void ) const noexcept;
	uint32_t getResourceCount( void ) const noexcept;
	bool isCullingEnabled( void ) const noexcept;
	void setCullingEnabled( bool bEnabled ) noexcept;

	void reset( void ) noexcept;
	sirGraphicsResult_t addResource( const sirFrameGraphResource_t& resource, uint32_t *pIndex ) noexcept;
	// Imports a verified GRDG-native buffer lease. Repeated subrange imports
	// from one physical backing always return the same FrameGraph resource.
	sirGraphicsResult_t importSharedBuffer( const sirNativeGPUBufferLease_t& rLease, sirGraphicsState_t eInitialState,
											sirGraphicsQueue_t eInitialQueue, uint32_t *pIndex ) noexcept;
	// Subrange declarations must remain within the caller's pinned read-only
	// GRDG view. The FrameGraph only stores metadata and does not own/clone bytes.
	sirGraphicsResult_t addSharedBufferRead( uint32_t nPass, uint32_t nResource, const sirNativeGPUBufferLease_t& rLease,
											 uint64_t nOffsetBytes, uint64_t nBytes, sirGraphicsState_t eState ) noexcept;
	sirGraphicsResult_t addPass( const sirFrameGraphPass_t& pass, uint32_t *pIndex ) noexcept;
	sirGraphicsResult_t addAccess( const sirFrameGraphAccess_t& access ) noexcept;
	sirGraphicsResult_t addDependency( uint32_t nBefore, uint32_t nAfter ) noexcept;
	sirGraphicsResult_t addOutput( const sirFrameGraphOutput_t& output ) noexcept;
	sirGraphicsResult_t compile( const sirGraphicsCapabilities_t& caps, sirFrameGraphCompiled_t *pCompiled ) const noexcept;
	sirGraphicsResult_t planTransients( const sirFrameGraphCompiled_t& compiled, const sirFrameGraphTransientInput_t& input,
										sirFrameGraphTransientPlan_t *pPlan ) const noexcept;
	sirGraphicsResult_t record( const sirFrameGraphCompiled_t& compiled, const sirFrameGraphCallbacks_t& callbacks ) const noexcept;

protected:
private:
	// The public graph API cannot invent physical GRDG imports or bypass the
	// lease-backed subrange path by registering an unpinned raw access.
	sirGraphicsResult_t addResourceImpl( const sirFrameGraphResource_t& resource, uint32_t *pIndex, bool bSharedLease ) noexcept;
	sirGraphicsResult_t addAccessImpl( const sirFrameGraphAccess_t& access, bool bSharedLease ) noexcept;
	static bool overlaps( const sirFrameGraphAccess_t& a, const sirFrameGraphAccess_t& b ) noexcept;
	static bool covers( const sirFrameGraphAccess_t& a, const sirFrameGraphAccess_t& b ) noexcept;
	static bool isWritableState( sirGraphicsState_t eState ) noexcept;
	static bool isReadableState( sirGraphicsState_t eState ) noexcept;
	bool requiresOrdering( const sirFrameGraphAccess_t& previous, const sirFrameGraphAccess_t& current ) const noexcept;
	sirGraphicsResult_t markProducers( const sirFrameGraphAccess_t& target, uint32_t nBeforePass, uint8_t *pLive,
									   uint32_t *pCellBudget ) const noexcept;
	sirGraphicsResult_t findLivePasses( uint8_t *pLive ) const noexcept;
	sirGraphicsResult_t validateCapabilities( const sirGraphicsCapabilities_t& caps, const uint8_t *pLive ) const noexcept;
	sirGraphicsResult_t buildTransitions( uint8_t *pszEdges, const uint8_t *pLive, sirFrameGraphCompiled_t *pCompiled ) const noexcept;
	sirGraphicsResult_t buildSchedule( const uint8_t *pszEdges, const uint8_t *pLive, sirFrameGraphCompiled_t *pCompiled ) const noexcept;
	void calculateLifetimes( sirFrameGraphCompiled_t *pCompiled ) const noexcept;

	sirFrameGraphResource_t m_szResources[SIR_FRAMEGRAPH_MAX_RESOURCES] = {};
	sirFrameGraphPass_t m_szPasses[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	sirFrameGraphAccess_t m_szAccesses[SIR_FRAMEGRAPH_MAX_ACCESSES] = {};
	sirFrameGraphSharedRead_t m_szSharedByteAccesses[SIR_FRAMEGRAPH_MAX_ACCESSES] = {};
	sirNativeGPUBufferLease_t m_szSharedReadLeases[SIR_FRAMEGRAPH_MAX_ACCESSES] = {};
	uint32_t m_nSharedByteAccessCount = 0;
	uint8_t m_szExplicitEdges[SIR_FRAMEGRAPH_MAX_PASSES * SIR_FRAMEGRAPH_MAX_PASSES] = {};
	sirFrameGraphOutput_t m_szOutputs[SIR_FRAMEGRAPH_MAX_OUTPUTS] = {};
	uint64_t m_nRevision = 1;
	uint64_t m_nIdentity = 0;
	const SIRSharedGPUBackingOwner *m_pSharedOwner = nullptr;
	uint32_t m_nOutputCount = 0;
	bool m_bCullingEnabled = false;
	uint32_t m_nResourceCount = 0;
	uint32_t m_nPassCount = 0;
	uint32_t m_nAccessCount = 0;
};

#endif // __SIRENGINE_SIRFRAMEGRAPH_H__