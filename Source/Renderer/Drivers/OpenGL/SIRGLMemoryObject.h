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

#ifndef __SIR_GL_MEMORY_OBJECT_H__
#define __SIR_GL_MEMORY_OBJECT_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRGLShared.h"
#include <Renderer/RHI/SIRRenderResource.h>

class SIRGLMemoryObject final : public SIRRenderResource
{
public:
	explicit SIRGLMemoryObject( const SIRSharedPtr<SIRRenderDevice>& device ) noexcept;
	~SIRGLMemoryObject() noexcept;

	virtual bool isValid( void ) const noexcept override;

private:
	GLuint m_hMemoryObjectID;
};

#endif // __SIR_GL_MEMORY_OBJECT_H__
