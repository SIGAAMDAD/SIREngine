/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/SIRSharedGPUBackingOwner.h>
#include <Renderer/SIRGraphicsValidation.h>

#include <stdlib.h>

SIRSharedGPUBackingOwner::~SIRSharedGPUBackingOwner( void ) noexcept
{
	for ( uint32_t i = 0; i < MAX_NATIVE_BACKINGS; ++i ) {
		if ( m_szBackings[i].bActive ) {
			abort();
		}
	}
}

SIRResourceResult SIRSharedGPUBackingOwner::initialize( SIRResourceDerivedRegistry& rRegistry, SIRRenderingDevice& rDevice,
                                                        uint64_t nPhysicalDeviceId, uint32_t nProviderId ) noexcept
{
	if ( m_pRegistry || !rRegistry.isReady() || !nPhysicalDeviceId || !nProviderId || !rDevice.getDeviceGeneration() ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const SIRResourceResult eRegister =
	    rRegistry.registerProvider( nProviderId, rDevice.getDeviceGeneration(), SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT, { this, releaseNative } );
	if ( eRegister != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eRegister;
	}
	m_pRegistry = &rRegistry;
	m_pDevice = &rDevice;
	m_nPhysicalDeviceId = nPhysicalDeviceId;
	m_nProviderId = nProviderId;
	m_nDeviceGeneration = rDevice.getDeviceGeneration();
	return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
}

SIRSharedGPUBackingOwner::NativeBacking *SIRSharedGPUBackingOwner::findNative( uint64_t nNativeToken ) noexcept
{
	if ( !nNativeToken ) {
		return nullptr;
	}
	for ( uint32_t i = 0; i < MAX_NATIVE_BACKINGS; ++i ) {
		if ( m_szBackings[i].bActive && m_szBackings[i].nToken == nNativeToken ) {
			return &m_szBackings[i];
		}
	}
	return nullptr;
}

static bool sameSharedNativeHandle( const sirGraphicsHandle_t& a, const sirGraphicsHandle_t& b ) noexcept
{
	return a.nSlot == b.nSlot && a.nGeneration == b.nGeneration && a.nDeviceGeneration == b.nDeviceGeneration && a.eType == b.eType;
}

bool SIRSharedGPUBackingOwner::releaseNative( void *pContext, uint64_t nNativeToken ) noexcept
{
	return pContext && static_cast<SIRSharedGPUBackingOwner *>(pContext)->retireNative( nNativeToken );
}

bool SIRSharedGPUBackingOwner::retireNative( uint64_t nNativeToken ) noexcept
{
	NativeBacking *pNative = findNative( nNativeToken );
	if ( !pNative || !m_pDevice || m_pDevice->getDeviceGeneration() != m_nDeviceGeneration ) {
		return false;
	}
	// The GRDG coordinator already proved every reader/provider completion.
	// No new synthetic fence is invented for native queue retirement.
	const sirGraphicsCompletionSet_t completed = m_LastNativeCompletion;
	if ( m_pDevice->retireResource( pNative->hNative, completed ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pNative->bActive = false;
	pNative->hNative = {};
	pNative->hBacking = {};
	pNative->nToken = 0;
	return true;
}

SIRResourceResult SIRSharedGPUBackingOwner::createOrReuseBuffer( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
                                                                 const sirGraphicsBufferDesc_t& rBuffer, uint32_t nStorageLayoutId,
                                                                 uint32_t nStorageFormatId, sirNativeGPUBufferBacking_t *pBacking ) noexcept
{
	if ( !m_pRegistry || !m_pDevice || !pBacking || m_pDevice->getDeviceGeneration() != m_nDeviceGeneration ||
	     sirGraphicsValidateBuffer( rBuffer ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS || rBuffer.nSizeBytes > UINT64_MAX / 2 ||
	     rBuffer.nAlignmentBytes > UINT32_MAX || !nStorageLayoutId || !nStorageFormatId ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pBacking = {};
	const sirSharedDeviceBackingDesc_t desc = { m_nPhysicalDeviceId,
	                                            m_nDeviceGeneration,
	                                            nStorageLayoutId,
	                                            nStorageFormatId,
	                                            rBuffer.nUsage,
	                                            static_cast<uint32_t>( rBuffer.nAlignmentBytes ? rBuffer.nAlignmentBytes : 1 ),
	                                            1,
	                                            m_nProviderId,
	                                            rBuffer.nSizeBytes,
	                                            rBuffer.nSizeBytes };
	sirSharedDeviceBackingReservation_t reservation {};
	const SIRResourceResult eReserve = m_pRegistry->reserveSharedDeviceBacking( rScope, rSource, desc, &reservation );
	if ( eReserve != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eReserve;
	}
	if ( !reservation.bNeedsNativeCreation ) {
		// A reservation for a resident backing MUST refer to a physical resource
		// this sole device owner can resolve, never an unverified native token.
		for ( uint32_t i = 0; i < MAX_NATIVE_BACKINGS; ++i ) {
			if ( m_szBackings[i].bActive && m_szBackings[i].hBacking.m_nSlot == reservation.hBacking.m_nSlot &&
			     m_szBackings[i].hBacking.m_nGeneration == reservation.hBacking.m_nGeneration ) {
				*pBacking = { reservation.hBacking, m_szBackings[i].hNative, m_szBackings[i].nToken };
				return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
			}
		}
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	uint32_t iFree = MAX_NATIVE_BACKINGS;
	for ( uint32_t i = 0; i < MAX_NATIVE_BACKINGS; ++i ) {
		if ( !m_szBackings[i].bActive ) {
			iFree = i;
			break;
		}
	}
	if ( iFree == MAX_NATIVE_BACKINGS || m_szBackings[iFree].nGeneration == UINT32_MAX ) {
		static_cast<void>(m_pRegistry->cancelSharedDeviceBacking( reservation.hBacking ));
		return SIRResourceResult::SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	sirGraphicsHandle_t native {};
	if ( m_pDevice->createBuffer( rBuffer, &native ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ||
	     !sirGraphicsIsHandleValid( native, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, m_nDeviceGeneration ) ) {
		static_cast<void>(m_pRegistry->cancelSharedDeviceBacking( reservation.hBacking ));
		return SIRResourceResult::SIR_RESOURCE_RESULT_FAILED;
	}
	NativeBacking& record = m_szBackings[iFree];
	++record.nGeneration;
	const uint64_t nNativeToken = ( static_cast<uint64_t>(record.nGeneration) << 32 ) | ( iFree + 1 );
	record = { native, reservation.hBacking, nNativeToken, record.nGeneration, true };
	const SIRResourceResult ePublish = m_pRegistry->publishSharedDeviceBacking( reservation.hBacking, nNativeToken );
	if ( ePublish != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		if ( !retireNative( nNativeToken ) ) {
			abort();
		}
		static_cast<void>(m_pRegistry->cancelSharedDeviceBacking( reservation.hBacking ));
		return ePublish;
	}
	*pBacking = { reservation.hBacking, native, nNativeToken };
	return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRSharedGPUBackingOwner::acquireBuffer( const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
                                                           const sirSharedDeviceViewRequest_t& rRequest,
                                                           sirNativeGPUBufferLease_t *pLease ) noexcept
{
	if ( !m_pRegistry || !m_pDevice || !pLease || rRequest.nPhysicalDeviceId != m_nPhysicalDeviceId ||
	     rRequest.nDeviceGeneration != m_nDeviceGeneration || m_pDevice->getDeviceGeneration() != m_nDeviceGeneration ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pLease = {};
	sirSharedDeviceView_t view {};
	const SIRResourceResult eBorrow = m_pRegistry->acquireSharedDeviceView( rScope, rSource, rRequest, &view );
	if ( eBorrow != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eBorrow;
	}
	NativeBacking *pNative = findNative( view.nNativeResourceToken );
	if ( !pNative ) {
		// Failed resolution must not orphan the registry borrow. The absence
		// of a matching native record is an internal ownership violation.
		abort();
	}
	*pLease = { view, pNative->hNative };
	return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRSharedGPUBackingOwner::validateBufferLease( const sirNativeGPUBufferLease_t& rLease ) const noexcept
{
	if ( !m_pRegistry || !m_pDevice || m_pDevice->getDeviceGeneration() != m_nDeviceGeneration ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( !sirGraphicsIsHandleValid( rLease.hNative, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, m_nDeviceGeneration ) ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	for ( const NativeBacking& rBacking : m_szBackings ) {
		if ( !rBacking.bActive || rBacking.nToken != rLease.view.nNativeResourceToken ) {
			continue;
		}
		if ( !sameSharedNativeHandle( rBacking.hNative, rLease.hNative ) ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		return m_pRegistry->validateSharedDeviceView( rLease.view );
	}
	return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
}

SIRResourceResult SIRSharedGPUBackingOwner::releaseBuffer( const sirNativeGPUBufferLease_t& rLease,
                                                           const sirProviderCompletionSet_t& rRequired ) noexcept
{
	const SIRResourceResult eValid = validateBufferLease( rLease );
	if ( eValid != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eValid;
	}
	return m_pRegistry->releaseSharedDeviceView( rLease.view, rRequired );
}

SIRResourceResult SIRSharedGPUBackingOwner::retireBuffer( const sirNativeGPUBufferBacking_t& rBacking,
                                                          const sirProviderCompletionSet_t& rRequired ) noexcept
{
	NativeBacking *pNative = findNative( rBacking.nNativeToken );
	if ( !m_pRegistry || !pNative || pNative->hBacking.m_nSlot != rBacking.hBacking.m_nSlot ||
	     pNative->hBacking.m_nGeneration != rBacking.hBacking.m_nGeneration ||
	     !sameSharedNativeHandle( pNative->hNative, rBacking.hNative ) ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	return m_pRegistry->retireRepresentation( rBacking.hBacking, rRequired );
}

SIRResourceResult SIRSharedGPUBackingOwner::pollAndObserve( const sirGraphicsCompletionSet_t& rCompleted ) noexcept
{
	if ( !m_pRegistry || !m_pDevice || !sirGraphicsIsCompletionSetValid( rCompleted, m_nDeviceGeneration ) ||
	     m_pDevice->getDeviceGeneration() != m_nDeviceGeneration || rCompleted.nQueueMask == 0 ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const sirProviderCompletionSet_t observed = {
	    m_nProviderId,
	    m_nDeviceGeneration,
	    rCompleted.nQueueMask,
	    SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT,
	    { rCompleted.szQueueValues[0], rCompleted.szQueueValues[1], rCompleted.szQueueValues[2], 0 } };
	for ( uint32_t iQueue = 0; iQueue < SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		if ( ( rCompleted.nQueueMask & ( 1u << iQueue ) ) == 0 ) {
			continue;
		}
		const sirGraphicsCompletion_t completion = { m_nDeviceGeneration, static_cast<SIRGraphicsQueue>(iQueue), rCompleted.szQueueValues[iQueue] };
		bool bFinished = false;
		if ( m_pDevice->poll( completion, &bFinished ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS || !bFinished ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
	}
	m_LastNativeCompletion.nDeviceGeneration = m_nDeviceGeneration;
	m_LastNativeCompletion.nQueueMask |= rCompleted.nQueueMask;
	for ( uint32_t iQueue = 0; iQueue < SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		if ( ( rCompleted.nQueueMask & ( 1u << iQueue ) ) != 0 &&
		     rCompleted.szQueueValues[iQueue] > m_LastNativeCompletion.szQueueValues[iQueue] ) {
			m_LastNativeCompletion.szQueueValues[iQueue] = rCompleted.szQueueValues[iQueue];
		}
	}
	return m_pRegistry->updateProviderCompletion( observed );
}
