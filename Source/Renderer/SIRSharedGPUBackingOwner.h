/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRSHAREDGPUBACKINGOWNER_H__
#define __SIRENGINE_SIRSHAREDGPUBACKINGOWNER_H__

#include <Renderer/SIRRenderingDevice.h>
#include <Resources/SIRResourceGraph.h>

// The sole native-buffer creation owner for each physical device. The
// graphics ABI handles stay backend-owned; consumer systems borrow GRDG views
// and may not create a second physical buffer for that canonical source.
struct sirNativeGPUBufferLease_t
{
	sirSharedDeviceView_t view;
	sirGraphicsHandle_t hNative;
};

struct sirNativeGPUBufferBacking_t
{
	SIRDerivedResourceHandle hBacking;
	sirGraphicsHandle_t hNative;
	uint64_t nNativeToken;
};

class SIRSharedGPUBackingOwner final
{
public:
	static constexpr uint32_t MAX_NATIVE_BACKINGS = 64;

	SIRSharedGPUBackingOwner( void ) noexcept = default;
	~SIRSharedGPUBackingOwner( void ) noexcept;
	SIRSharedGPUBackingOwner( const SIRSharedGPUBackingOwner& ) = delete;
	SIRSharedGPUBackingOwner& operator=( const SIRSharedGPUBackingOwner& ) = delete;

	SIRResourceResult initialize( SIRResourceDerivedRegistry& rRegistry, SIRRenderingDevice& rDevice, uint64_t nPhysicalDeviceId,
	                              uint32_t nProviderId ) noexcept;
	SIRResourceResult createOrReuseBuffer( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
	                                       const sirGraphicsBufferDesc_t& rBuffer, uint32_t nStorageLayoutId, uint32_t nStorageFormatId,
	                                       sirNativeGPUBufferBacking_t *pBacking ) noexcept;
	SIRResourceResult acquireBuffer( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
	                                 const sirSharedDeviceViewRequest_t& rRequest, sirNativeGPUBufferLease_t *pLease ) noexcept;
	// Validate an active issued borrow and the exact live native allocation identity.
	SIRResourceResult validateBufferLease( const sirNativeGPUBufferLease_t& rLease ) const noexcept;
	SIRResourceResult releaseBuffer( const sirNativeGPUBufferLease_t& rLease, const sirProviderCompletionSet_t& rRequired ) noexcept;
	SIRResourceResult retireBuffer( const sirNativeGPUBufferBacking_t& rBacking, const sirProviderCompletionSet_t& rRequired ) noexcept;
	// Drive completion changes with actual native queue polls, never fabricated fence values.
	SIRResourceResult pollAndObserve( const sirGraphicsCompletionSet_t& rCompleted ) noexcept;

protected:
private:
	struct NativeBacking
	{
		sirGraphicsHandle_t hNative;
		SIRDerivedResourceHandle hBacking;
		uint64_t nToken;
		uint32_t nGeneration;
		bool bActive;
	};
	NativeBacking m_szBackings[MAX_NATIVE_BACKINGS] = {};
	SIRResourceDerivedRegistry *m_pRegistry = nullptr;
	SIRRenderingDevice *m_pDevice = nullptr;
	uint64_t m_nPhysicalDeviceId = 0;
	uint32_t m_nProviderId = 0;
	uint32_t m_nDeviceGeneration = 0;
	sirGraphicsCompletionSet_t m_LastNativeCompletion = {};

	static bool releaseNative( void *pContext, uint64_t nNativeToken ) noexcept;
	bool retireNative( uint64_t nNativeToken ) noexcept;
	NativeBacking *findNative( uint64_t nNativeToken ) noexcept;
};

#endif // __SIRENGINE_SIRSHAREDGPUBACKINGOWNER_H__
