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

#ifndef __SIR_RENDER_TEXTURE_H__
#define __SIR_RENDER_TEXTURE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>
#include "Renderer/SIRGraphicsTypes.h"

// One GPU image/texture; array, mip, aspect and format views are separate.
// Nonowning, zero-allocation typed façade; the device owns resource lifetime.
class SIRRenderTexture
{
public:
	SIRRenderTexture( void ) noexcept = default;
	explicit SIRRenderTexture( sirGraphicsHandle_t hHandle ) noexcept : m_hHandle( hHandle )
	{
	}

	sirGraphicsHandle_t getHandle( void ) const noexcept
	{
		return m_hHandle;
	}
	bool isValid( void ) const noexcept
	{
		return m_hHandle.nGeneration != 0 && m_hHandle.nDeviceGeneration != 0 && m_hHandle.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE;
	}

	uint32_t getWidth( void ) const noexcept;
	uint32_t getHeight( void ) const noexcept;
	uint32_t getMipLevels( void ) const noexcept;
	SIRGraphicsFormat getFormat( void ) const noexcept;
	SIRGraphicsTextureDimension getTextureType( void ) const noexcept;

protected:
	sirGraphicsHandle_t m_hHandle = {};
	uint32_t m_nWidth = {};
	uint32_t m_nHeight = {};
	uint32_t m_nMipLevels = {};
	SIRGraphicsFormat m_eFormat = {};
	SIRGraphicsTextureDimension m_eType = {};
};

#endif // __SIR_RENDER_TEXTURE_H__
