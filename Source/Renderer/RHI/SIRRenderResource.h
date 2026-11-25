#ifndef __SIR_RENDER_RESOURCE_H__
#define __SIR_RENDER_RESOURCE_H__

#include "Core/Platform/SIRCompiler.h"
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "Core/Memory/Pointers/SIRSharedPtr.h"
#include <Renderer/RHI/SIRRenderDevice.h>
#include <Renderer/SIRGraphicsTypes.h>

enum class SIRRenderResourceType : uint8_t
{
	SIR_RENDER_RESOURCE_VERTEX_BUFFER,
	SIR_RENDER_RESOURCE_INDEX_BUFFER,
	SIR_RENDER_RESOURCE_SHADER_STORAGE_BUFFER,
	SIR_RENDER_RESOURCE_TEXTURE_BUFFER,
	SIR_RENDER_RESOURCE_SHADER,
	SIR_RENDER_RESOURCE_FRAMEBUFFER,
};

class SIRRenderResource
{
public:
	constexpr explicit SIRRenderResource( const SIRRenderResource& ) noexcept = delete;
	constexpr explicit SIRRenderResource( SIRRenderResource&& ) noexcept = delete;
	virtual ~SIRRenderResource() noexcept = default;

	const SIRRenderResource& operator=( const SIRRenderResource& ) = delete;
	const SIRRenderResource& operator=( SIRRenderResource&& ) = delete;

	const SIRWeakPtr<SIRRenderDevice>& getDevice( void ) const noexcept;

	SIRSharedPtr<SIRRenderDevice> lockDevice( void ) const noexcept;

	uint32_t getDeviceGeneration( void ) const noexcept;

	virtual SIRRenderResourceType getType( void ) const noexcept = 0;
	virtual bool isValid( void ) const noexcept = 0;

protected:
	explicit SIRRenderResource( const SIRWeakPtr<SIRRenderDevice>& pDevice, uint32_t nDeviceGeneration ) noexcept;

private:
	SIRWeakPtr<SIRRenderDevice> m_pDevice;
	uint32_t m_nDeviceGeneration;
};

SIR_FORCEINLINE SIRRenderResource::SIRRenderResource( const SIRWeakPtr<SIRRenderDevice>& pDevice, uint32_t nDeviceGeneration ) noexcept
	: m_pDevice( pDevice ), m_nDeviceGeneration( nDeviceGeneration )
{
}

SIR_FORCEINLINE const SIRWeakPtr<SIRRenderDevice>& SIRRenderResource::getDevice( void ) const noexcept
{
	return m_pDevice;
}

SIR_FORCEINLINE SIRSharedPtr<SIRRenderDevice> SIRRenderResource::lockDevice( void ) const noexcept
{
	return m_pDevice.lock();
}

SIR_FORCEINLINE uint32_t SIRRenderResource::getDeviceGeneration( void ) const noexcept
{
	return m_nDeviceGeneration;
}

#endif // __SIR_RENDER_RESOURCE_H__
