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

#ifndef __SIRENGINE_SIRD3D11HANDLES_H__
#define __SIRENGINE_SIRD3D11HANDLES_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/SIRGraphicsTypes.h>

// The table does not own COM references: the native device releases each
// extracted object exactly once. Handle generations are never resurrected.
typedef struct sirD3D11ObjectInfo_s
{
	void *pNativeObject;
	uint64_t nSizeBytes;
	sirGraphicsHandleType_t eType;
	sirGraphicsMemoryClass_t eMemory;
} sirD3D11ObjectInfo_t;

typedef struct sirD3D11ObjectSlot_s
{
	sirD3D11ObjectInfo_t object;
	uint64_t nMappedOffsetBytes;
	uint64_t nMappedBytes;
	uint32_t nGeneration;
	uint32_t nMapped;
} sirD3D11ObjectSlot_t;

class SIRD3D11Handles final
{
public:
	static constexpr uint32_t CAPACITY = 256;

	explicit SIRD3D11Handles( uint32_t nDeviceGeneration ) noexcept;
	~SIRD3D11Handles( void ) noexcept = default;

	uint32_t getDeviceGeneration( void ) const noexcept;
	uint32_t getActiveCount( void ) const noexcept;
	uint64_t getAllocatedBytes( void ) const noexcept;

	sirGraphicsResult_t create( const sirD3D11ObjectInfo_t& object, sirGraphicsHandle_t *pHandle ) noexcept;
	sirGraphicsResult_t lookup( sirGraphicsHandle_t handle, sirD3D11ObjectSlot_t **ppSlot ) noexcept;
	sirGraphicsResult_t destroy( sirGraphicsHandle_t handle, void **ppNativeObject ) noexcept;
	sirD3D11ObjectSlot_t *getSlot( uint32_t nIndex ) noexcept;
	void clear( void ) noexcept;

protected:
private:
	sirD3D11ObjectSlot_t m_szSlots[CAPACITY] = {};
	uint32_t m_nDeviceGeneration = 0;
	uint32_t m_nActiveCount = 0;
	uint64_t m_nAllocatedBytes = 0;
};

#endif // __SIRENGINE_SIRD3D11HANDLES_H__
