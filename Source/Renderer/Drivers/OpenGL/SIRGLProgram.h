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

#ifndef __SIR_GL_PROGRAM_H__
#define __SIR_GL_PROGRAM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include "SIRGLShared.h"
#include <Renderer/RHI/SIRRenderProgram.h>
#include <Renderer/RHI/SIRRenderDevice.h>
#include <Core/Containers/SIRString.h>

class SIRGLProgram final : public SIRRenderProgram
{
public:
	explicit SIRGLProgram( const SIRSharedPtr<SIRRenderDevice>& device, const SIRString&  ) noexcept;
	~SIRGLProgram() noexcept override;

private:
	GLuint m_hProgramId;
};


#endif
