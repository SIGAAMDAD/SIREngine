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

#include "SIRVulkanShader.h"
#include "SIRVulkanDevice.h"

SIRVulkanShader::SIRVulkanShader( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderShaderCode& description )
	: SIRRenderShader( device, description )
{
	if ( device.get() != nullptr && device->getBackend() == SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN &&
	     description.eFormat == SIRRenderShaderFormat::SIR_RENDER_SHADER_SPIRV ) {
		SIRVulkanDevice& nativeDevice = getDevice<SIRVulkanDevice>();
		static_cast<void>( nativeDevice.createShaderModule( description.code.getData(), description.code.getSize(),
		                                               description.pszEntryPoint, &m_hModule ) );
	}
}

SIRVulkanShader::~SIRVulkanShader() noexcept
{
	if ( m_hModule != VK_NULL_HANDLE && getDevice().get() != nullptr ) {
		SIRVulkanDevice& nativeDevice = getDevice<SIRVulkanDevice>();
		static_cast<void>( nativeDevice.destroyShaderModule( &m_hModule ) );
	}
}
