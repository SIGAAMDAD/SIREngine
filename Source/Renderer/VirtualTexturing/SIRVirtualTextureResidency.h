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

#ifndef __SIRENGINE_SIRVIRTUALTEXTURERESIDENCY_H__
#define __SIRENGINE_SIRVIRTUALTEXTURERESIDENCY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>
#include <Resources/SIRResourceTypes.h>

typedef enum sirVirtualTextureState_e : uint8_t
{
	SIR_VT_FREE = 0,
	SIR_VT_REQUESTED = 1,
	SIR_VT_UPLOADING = 2,
	SIR_VT_RESIDENT = 3,
	SIR_VT_RETIRING = 4
} sirVirtualTextureState_t;

typedef struct sirVirtualTexturePageKey_s
{
	sirResourceKey_t source;
	uint64_t nRevision;
	uint32_t nX;
	uint32_t nY;
	uint16_t nMip;
	uint16_t nLayer;
} sirVirtualTexturePageKey_t;

typedef struct sirVirtualTextureHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint32_t nDeviceGeneration;
} sirVirtualTextureHandle_t;

typedef struct sirVirtualTextureSlot_s
{
	sirVirtualTexturePageKey_t key;
	sirGraphicsCompletionSet_t completion;
	uint64_t nLastTouch;
	uint32_t nGeneration;
	sirVirtualTextureState_t eState;
} sirVirtualTextureSlot_t;

typedef struct sirVirtualTextureStorage_s
{
	sirVirtualTextureSlot_t *pSlots;
	uint32_t *pBuckets;
	uint32_t nSlotCapacity;
	uint32_t nBucketCapacity;
	uint32_t nDeviceGeneration;
} sirVirtualTextureStorage_t;

// Single-writer state machine. Storage is fully caller-owned and preallocated.
// Slots index physical cache pages; the caller owns GPU images, page tables,
// uploads and page-table invalidation. A slot cannot be reused until its
// retire completion set (including outstanding upload work) is satisfied.
class SIRVirtualTextureResidency
{
public:
	SIRVirtualTextureResidency( void ) noexcept = default;
	SIRVirtualTextureResidency( const SIRVirtualTextureResidency& ) = delete;
	~SIRVirtualTextureResidency( void ) noexcept = default;

	SIRVirtualTextureResidency& operator=( const SIRVirtualTextureResidency& ) = delete;

	uint32_t getCapacity( void ) const noexcept;
	uint32_t getActiveCount( void ) const noexcept;
	uint32_t getResidentCount( void ) const noexcept;
	uint32_t getRetiringCount( void ) const noexcept;

	sirGraphicsResult_t initialize( const sirVirtualTextureStorage_t& storage ) noexcept;
	sirGraphicsResult_t reserve( const sirVirtualTexturePageKey_t& key, sirVirtualTextureHandle_t *pHandle, bool *pCreated ) noexcept;
	sirGraphicsResult_t find( const sirVirtualTexturePageKey_t& key, sirVirtualTextureHandle_t *pHandle ) const noexcept;
	sirGraphicsResult_t queryState( sirVirtualTextureHandle_t handle, sirVirtualTextureState_t *pState ) const noexcept;
	sirGraphicsResult_t submitUpload( sirVirtualTextureHandle_t handle, const sirGraphicsCompletionSet_t& completion ) noexcept;
	sirGraphicsResult_t collect( const sirGraphicsCompletionSet_t& completed, uint32_t *pTransitions ) noexcept;
	sirGraphicsResult_t retire( sirVirtualTextureHandle_t handle, const sirGraphicsCompletionSet_t& lastUse ) noexcept;
	sirGraphicsResult_t cancel( sirVirtualTextureHandle_t handle ) noexcept;
	sirGraphicsResult_t touch( sirVirtualTextureHandle_t handle ) noexcept;
	sirGraphicsResult_t findOldest( sirVirtualTextureHandle_t *pHandle ) const noexcept;

protected:
	bool isHandleValid( sirVirtualTextureHandle_t handle ) const noexcept;

private:
	uint32_t findBucket( const sirVirtualTexturePageKey_t& key ) const noexcept;
	uint32_t findInsertionBucket( const sirVirtualTexturePageKey_t& key ) const noexcept;
	uint32_t findFreeSlot( void ) const noexcept;
	void removeMapping( const sirVirtualTexturePageKey_t& key ) noexcept;
	void releaseSlot( uint32_t nSlot ) noexcept;
	bool completionReached( const sirGraphicsCompletionSet_t& required, const sirGraphicsCompletionSet_t& completed ) const noexcept;
	bool isCompletionValid( const sirGraphicsCompletionSet_t& completion, bool bRequireWork ) const noexcept;

	sirVirtualTextureStorage_t m_storage = {};
	uint64_t m_nTouchSerial = 0;
	uint32_t m_nActive = 0;
	uint32_t m_nResident = 0;
	uint32_t m_nRetiring = 0;
	uint32_t m_nFreeHint = 0;
};

static_assert( sizeof( sirVirtualTextureHandle_t ) == 12, "virtual texture handles must remain compact" );
static_assert( sizeof( sirVirtualTexturePageKey_t ) == 40, "virtual page keys must maintain their established layout" );

#endif // __SIRENGINE_SIRVIRTUALTEXTURERESIDENCY_H__
