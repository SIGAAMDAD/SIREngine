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

#ifndef __SIR_GL_TEXTURE_H__
#define __SIR_GL_TEXTURE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include "SIRGLShared.h"
#include <Renderer/RHI/SIRRenderTexture.h>

class SIRGLTexture final : public SIRRenderTexture
{
public:
	explicit SIRGLTexture( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderTextureDesc& desc ) noexcept;
	~SIRGLTexture() noexcept override;

private:
	GLuint m_hTextureID;
};

#endif // _SIR_GL_TEXTURE_H__
