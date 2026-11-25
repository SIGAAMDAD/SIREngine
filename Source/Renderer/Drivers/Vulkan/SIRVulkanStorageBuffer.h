#ifndef __SIR_VULKAN_STORAGE_BUFFER_H__
#define __SIR_VULKAN_STORAGE_BUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/RHI/SIRRenderStorageBuffer.h>

class SIRVulkanStorageBuffer final : public SIRRenderStorageBuffer
{
public:
	explicit SIRVulkanStorageBuffer() noexcept;
	~SIRVulkanStorageBuffer() noexcept override;

protected:

private:
};

#endif // __SIR_VULKAN_STORAGE_BUFFER_H__
