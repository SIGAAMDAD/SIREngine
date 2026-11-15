/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRVGPHYSICALBACKING_H__
#define __SIRENGINE_SIRVGPHYSICALBACKING_H__

#include <SIRPCH.hpp>
#include <Renderer/SIRRenderingDevice.h>
#include <Resources/SIRResourceGraph.h>

// A single device-native page buffer, interned once in GRDG by source
// resource/revision/provider. The upload buffer is transient, not canonical
// source state; all downstream GPU consumers borrow the registered backing.
typedef struct sirVGPhysicalBackingConfig_s
{
	SIRRenderingDevice *pDevice;
	SIRResourceDerivedRegistry *pRegistry;
	const SIRResourceReadScope *pScope;
	const SIRResourceView *pSource;
	SIRDerivedResourceKey keyRepresentation;
	// Resources issues the global physical backing ID at registration.
	// It is retrieved through getBackingId after successful initialize.
	uint64_t nCapacityBytes;
	uint64_t nUploadBytes;
	uint32_t nMemoryDomain;
} sirVGPhysicalBackingConfig_t;

class SIRVGPhysicalBacking
{
public:
	SIRVGPhysicalBacking( void ) noexcept;
	SIRVGPhysicalBacking( const SIRVGPhysicalBacking& ) = delete;
	SIRVGPhysicalBacking& operator=( const SIRVGPhysicalBacking& ) = delete;

	sirGraphicsResult_t initialize( const sirVGPhysicalBackingConfig_t& config ) noexcept;
	SIRResourceResult acquire( SIRDerivedResourceLease *pLease ) noexcept;
	SIRResourceResult release( const SIRDerivedResourceLease& lease ) noexcept;
	sirGraphicsResult_t resolve( const SIRDerivedResourceLease& lease, sirGraphicsHandle_t *pBacking ) const noexcept;
	sirGraphicsResult_t getUploadBuffer( sirGraphicsHandle_t *pUpload ) const noexcept;
	sirGraphicsResult_t getBackingId( uint64_t *pBackingId ) const noexcept;
	sirGraphicsResult_t getBackingBuffer( sirGraphicsHandle_t *pBuffer ) const noexcept;
	SIRResourceResult retire( uint64_t nDirectFence ) noexcept;
	SIRResourceResult observeRetirement( uint64_t nDirectFence ) noexcept;

protected:
private:
	static bool releaseNative( void *pContext, uint64_t nNativeToken ) noexcept;
	bool retireBuffers( uint64_t nNativeToken ) noexcept;

	SIRRenderingDevice *m_pDevice;
	SIRResourceDerivedRegistry *m_pRegistry;
	SIRDerivedResourceKey m_Key;
	SIRDerivedResourceHandle m_Handle;
	sirGraphicsHandle_t m_hBacking;
	sirGraphicsHandle_t m_hUpload;
	uint64_t m_nPhysicalBackingId;
	uint64_t m_nNativeToken;
	uint64_t m_nLastDirectFence;
	uint32_t m_nDeviceGeneration;
	bool m_bPublished;
	bool m_bRetiring;
};

#endif // __SIRENGINE_SIRVGPHYSICALBACKING_H__
