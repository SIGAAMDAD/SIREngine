/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include "SIRVulkanSynchronization.h"

#include <limits>

namespace {
SIRRenderResult sirVulkanSynchronizationResult( VkResult result ) noexcept
{
	switch ( result ) {
	case VK_SUCCESS: return SIRRenderResult::SIR_RENDER_SUCCESS;
	case VK_TIMEOUT: return SIRRenderResult::SIR_RENDER_TIMEOUT;
	case VK_ERROR_DEVICE_LOST: return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	case VK_ERROR_OUT_OF_HOST_MEMORY:
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return SIRRenderResult::SIR_RENDER_OUT_OF_MEMORY;
	default: return SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
	}
}
}

SIRVulkanSynchronization::SIRVulkanSynchronization( SIRVulkanDevice& device ) noexcept
    : SIRRenderSynchronization( SIRRenderSynchronizationType::SIR_RENDER_SYNC_TIMELINE ), m_Device( device )
{
	if ( !device.m_bReady || device.m_bDeviceLost || device.m_Device == VK_NULL_HANDLE || device.m_Procs.vkCreateSemaphore == nullptr ||
	     device.m_Procs.vkDestroySemaphore == nullptr || device.m_Procs.vkSignalSemaphore == nullptr ||
	     device.m_Procs.vkWaitSemaphores == nullptr || device.m_Procs.vkGetSemaphoreCounterValue == nullptr ) return;

	VkSemaphoreTypeCreateInfo typeInfo{};
	typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	typeInfo.initialValue = 0;
	VkSemaphoreCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	createInfo.pNext = &typeInfo;
	if ( device.m_Procs.vkCreateSemaphore( device.m_Device, &createInfo, nullptr, &m_Semaphore ) == VK_SUCCESS ) {
		m_nDeviceSerial = device.m_nDeviceSerial;
		++device.m_nLiveSynchronizations;
	}
}

SIRVulkanSynchronization::~SIRVulkanSynchronization( void ) noexcept
{
	if ( m_Semaphore == VK_NULL_HANDLE ) return;
	if ( m_nLastGpuUse != 0 && m_Device.m_bReady && !m_Device.m_bDeviceLost && m_nDeviceSerial == m_Device.m_nDeviceSerial ) {
		const sirVulkanCompletion_t completion{ m_nDeviceSerial, m_nLastGpuUse };
		if ( m_Device.waitCompletion( &completion, std::numeric_limits<uint64_t>::max() ) != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			// A failed wait gives no proof that GPU use has retired. Leave the
			// semaphore to VkDevice destruction instead of freeing it too early.
			--m_Device.m_nLiveSynchronizations;
			return;
		}
	}
	m_Device.m_Procs.vkDestroySemaphore( m_Device.m_Device, m_Semaphore, nullptr );
	m_Semaphore = VK_NULL_HANDLE;
	--m_Device.m_nLiveSynchronizations;
}

bool SIRVulkanSynchronization::isValid( void ) const noexcept
{
	return m_Semaphore != VK_NULL_HANDLE && m_Device.m_bReady && !m_Device.m_bDeviceLost &&
	       m_nDeviceSerial == m_Device.m_nDeviceSerial;
}

SIRRenderResult SIRVulkanSynchronization::signal( uint64_t nValue ) noexcept
{
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	if ( !isValid() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if ( nValue == 0 || nValue <= m_nPlannedSignalValue ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	if ( m_nReservedSignalValue != 0 ) return SIRRenderResult::SIR_RENDER_BUSY;

	uint64_t nCompleted = 0;
	const VkResult query = m_Device.m_Procs.vkGetSemaphoreCounterValue( m_Device.m_Device, m_Semaphore, &nCompleted );
	if ( query != VK_SUCCESS ) return sirVulkanSynchronizationResult( query );
	// Do not overtake a queued GPU signal on the same timeline.
	if ( nCompleted < m_nPlannedSignalValue ) return SIRRenderResult::SIR_RENDER_BUSY;
	if ( nValue <= nCompleted ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;

	VkSemaphoreSignalInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	info.semaphore = m_Semaphore;
	info.value = nValue;
	const VkResult result = m_Device.m_Procs.vkSignalSemaphore( m_Device.m_Device, &info );
	if ( result == VK_SUCCESS ) m_nPlannedSignalValue = nValue;
	return sirVulkanSynchronizationResult( result );
}

SIRRenderResult SIRVulkanSynchronization::wait( uint64_t nValue, uint64_t nTimeoutNanoseconds ) noexcept
{
	if ( !isValid() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if ( nValue == 0 ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	VkSemaphoreWaitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	info.semaphoreCount = 1;
	info.pSemaphores = &m_Semaphore;
	info.pValues = &nValue;
	return sirVulkanSynchronizationResult( m_Device.m_Procs.vkWaitSemaphores( m_Device.m_Device, &info, nTimeoutNanoseconds ) );
}

uint64_t SIRVulkanSynchronization::getCompletedValue( void ) const noexcept
{
	if ( !isValid() ) return 0;
	uint64_t nCompleted = 0;
	return m_Device.m_Procs.vkGetSemaphoreCounterValue( m_Device.m_Device, m_Semaphore, &nCompleted ) == VK_SUCCESS ? nCompleted : 0;
}

uint64_t SIRVulkanSynchronization::getPlannedSignalValue( void ) const noexcept
{
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	return isValid() ? m_nPlannedSignalValue : 0;
}

SIRRenderResult SIRVulkanSynchronization::reserveSignalValue( uint64_t *pValue ) noexcept
{
	if ( pValue == nullptr ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	*pValue = 0;
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	if ( !isValid() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if ( m_nReservedSignalValue != 0 ) return SIRRenderResult::SIR_RENDER_BUSY;
	if ( m_nPlannedSignalValue == std::numeric_limits<uint64_t>::max() ) return SIRRenderResult::SIR_RENDER_CAPACITY_EXCEEDED;
	m_nReservedSignalValue = m_nPlannedSignalValue + 1;
	*pValue = m_nReservedSignalValue;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanSynchronization::reserveSignalValue( uint64_t nValue ) noexcept
{
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	if ( !isValid() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if ( m_nReservedSignalValue != 0 ) return SIRRenderResult::SIR_RENDER_BUSY;
	if ( nValue <= m_nPlannedSignalValue ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	m_nReservedSignalValue = nValue;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

void SIRVulkanSynchronization::finishSignalReservation( uint64_t nValue, bool bSubmitted ) noexcept
{
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	if ( nValue == 0 || nValue != m_nReservedSignalValue ) return;
	if ( bSubmitted ) m_nPlannedSignalValue = nValue;
	m_nReservedSignalValue = 0;
}

bool SIRVulkanSynchronization::noteGpuUse( const sirVulkanCompletion_t& completion ) const noexcept
{
	SIRScopedLock<SIRMutex> lock( m_Mutex );
	if ( !isValid() || completion.nDeviceSerial != m_nDeviceSerial || completion.nValue == 0 ||
	     completion.nValue > m_Device.m_nLastTransferValue ) return false;
	if ( completion.nValue > m_nLastGpuUse ) m_nLastGpuUse = completion.nValue;
	return true;
}
