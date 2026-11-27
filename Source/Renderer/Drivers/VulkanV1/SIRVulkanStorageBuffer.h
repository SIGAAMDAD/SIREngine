/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_VULKAN_STORAGE_BUFFER_H__
#define __SIR_VULKAN_STORAGE_BUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderStorageBuffer.h>
#include <Renderer/RHI/SIRRenderTypes.h>
#include "SIRVulkanDevice.h"

class SIRVulkanStorageBuffer final : public SIRRenderStorageBuffer
{
public:
	explicit SIRVulkanStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderBufferDesc& description ) noexcept;
	~SIRVulkanStorageBuffer() noexcept override;

	virtual SIRRenderResult map( uint64_t nOffsetBytes, uint64_t nSizeBytes, SIRRenderBufferMapping *pMapping ) noexcept override;
	virtual SIRRenderResult unmap( void ) noexcept override;
	virtual SIRRenderResult flush( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept override;
	virtual SIRRenderResult invalidate( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept override;
	virtual uint64_t getDeviceAddress( void ) const noexcept override;
	const sirVulkanBuffer_t& getNativeBuffer( void ) const noexcept { return m_Native; }

private:
	sirVulkanBuffer_t m_Native{};
};

#endif // __SIR_VULKAN_STORAGE_BUFFER_H__
