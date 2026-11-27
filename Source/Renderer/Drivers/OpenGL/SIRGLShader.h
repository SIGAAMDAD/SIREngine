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

#ifndef __SIR_GL_SHADER_H__
#define __SIR_GL_SHADER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include "SIRGLShared.h"
#include <Renderer/RHI/SIRRenderShader.h>

class SIRGLShader final : public SIRRenderShader
{
public:
	explicit SIRGLShader( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderShaderCode& description ) noexcept;
	~SIRGLShader() noexcept override;

	virtual bool isValid( void ) const noexcept override;

private:

	GLuint m_hShaderID;
};

#endif // __SIR_GL_SHADER_H__
