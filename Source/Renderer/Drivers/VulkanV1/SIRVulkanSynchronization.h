/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIR_VULKAN_SYNCHRONIZATION_H__
#define __SIR_VULKAN_SYNCHRONIZATION_H__

#include <Core/Platform/Shared/Threading/SIRScopedLock.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <Renderer/RHI/SIRRenderSynchronization.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

// A timeline semaphore belonging to the supplied, already initialized device.
// The device must outlive this object. Queue submissions must report their final
// completion through noteGpuUse before the semaphore may be destroyed.
class SIRVulkanSynchronization final : public SIRRenderSynchronization
{
public:
	explicit SIRVulkanSynchronization( SIRVulkanDevice& device ) noexcept;
	~SIRVulkanSynchronization( void ) noexcept override;

	SIRVulkanSynchronization( const SIRVulkanSynchronization& ) = delete;
	SIRVulkanSynchronization& operator=( const SIRVulkanSynchronization& ) = delete;

	SIRRenderResult signal( uint64_t nValue ) noexcept override;
	SIRRenderResult wait( uint64_t nValue, uint64_t nTimeoutNanoseconds ) noexcept override;
	uint64_t getCompletedValue( void ) const noexcept override;

	bool isValid( void ) const noexcept;
	SIRVulkanDevice& getDevice( void ) const noexcept { return m_Device; }
	VkSemaphore getNativeSemaphore( void ) const noexcept { return m_Semaphore; }
	uint64_t getPlannedSignalValue( void ) const noexcept;
	SIRRenderResult reserveSignalValue( uint64_t *pValue ) noexcept;
	SIRRenderResult reserveSignalValue( uint64_t nValue ) noexcept;
	void finishSignalReservation( uint64_t nValue, bool bSubmitted ) noexcept;
	bool noteGpuUse( const sirVulkanCompletion_t& completion ) const noexcept;

private:
	SIRVulkanDevice& m_Device;
	VkSemaphore m_Semaphore = VK_NULL_HANDLE;
	uint64_t m_nDeviceSerial = 0;
	uint64_t m_nPlannedSignalValue = 0;
	uint64_t m_nReservedSignalValue = 0;
	mutable uint64_t m_nLastGpuUse = 0;
	mutable SIRMutex m_Mutex;
};

#endif // __SIR_VULKAN_SYNCHRONIZATION_H__
