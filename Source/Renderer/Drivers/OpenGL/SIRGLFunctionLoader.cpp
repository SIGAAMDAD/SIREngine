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

#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <stdint.h>

// WGL can return 1, 2, 3, or -1 for a missing function. These are not callable pointers.
bool sirGLIsUsableProcedure( const void *pAddress ) noexcept
{
	const uintptr_t nAddress = ( uintptr_t )pAddress;
	return nAddress > 3 && nAddress != UINTPTR_MAX;
}

// This is a minimum device-admission set, not an exhaustive feature dispatch table.
// Every actual operation must separately resolve and validate the functions it uses.
static constexpr const char *const SIR_GL46_REQUIRED_PROCEDURES[] = { "glGetStringi",
                                                                      "glGetInternalformativ",
                                                                      "glCreateBuffers",
                                                                      "glNamedBufferStorage",
                                                                      "glCreateTextures",
                                                                      "glTextureStorage2D",
                                                                      "glTextureStorage3D",
                                                                      "glCreateVertexArrays",
                                                                      "glCreateFramebuffers",
                                                                      "glNamedFramebufferTexture",
                                                                      "glCheckNamedFramebufferStatus",
                                                                      "glCreateSamplers",
                                                                      "glCreateShader",
                                                                      "glShaderSource",
                                                                      "glCompileShader",
                                                                      "glCreateProgram",
                                                                      "glLinkProgram",
                                                                      "glBindBufferRange",
                                                                      "glDispatchCompute",
                                                                      "glMemoryBarrier",
                                                                      "glFenceSync",
                                                                      "glClientWaitSync",
                                                                      "glDrawElementsInstancedBaseVertexBaseInstance",
                                                                      "glMultiDrawElementsIndirect",
                                                                      "glBindTextureUnit",
                                                                      "glGetProgramBinary",
                                                                      "glCreateQueries",
                                                                      "glGetQueryObjectui64v",
                                                                      "glClearNamedFramebufferfv",
                                                                      "glGetTextureSubImage" };

bool sirGLValidateRequired46Functions( sirGLProcedureLookup_t pLookup, void *pContext ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr ) {
		return false;
	}
	for ( const char *pszName : SIR_GL46_REQUIRED_PROCEDURES ) {
		if ( !sirGLIsUsableProcedure( pLookup( pContext, pszName ) ) ) {
			return false;
		}
	}
	return true;
}
