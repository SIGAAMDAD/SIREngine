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

#include <Renderer/Drivers/OpenGL/SIRGLBuffers.h>
#include <stdint.h>

static constexpr uint32_t SIR_GL_NO_ERROR = 0;
static constexpr uint32_t SIR_GL_OUT_OF_MEMORY = 0x0505;
static constexpr uint32_t SIR_GL_BUFFER_USES = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_INDEX | SIR_GRAPHICS_USAGE_CONSTANT |
                                               SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COPY_SOURCE |
                                               SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_INDIRECT;

static sirGraphicsResult_t sirGLMapError( uint32_t nError ) noexcept
{
	return nError == SIR_GL_OUT_OF_MEMORY ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t sirGLLoadBufferDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLBufferDispatch_t *pAPI ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr || pAPI == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLBufferDispatch_t result = {};
	void *pCreate = pLookup( pContext, "glCreateBuffers" );
	void *pStorage = pLookup( pContext, "glNamedBufferStorage" );
	void *pDelete = pLookup( pContext, "glDeleteBuffers" );
	void *pError = pLookup( pContext, "glGetError" );
	if ( !sirGLIsUsableProcedure( pCreate ) || !sirGLIsUsableProcedure( pStorage ) || !sirGLIsUsableProcedure( pDelete ) ||
	     !sirGLIsUsableProcedure( pError ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	result.pCreateBuffers = ( decltype( result.pCreateBuffers ) )pCreate;
	result.pNamedBufferStorage = ( decltype( result.pNamedBufferStorage ) )pStorage;
	result.pDeleteBuffers = ( decltype( result.pDeleteBuffers ) )pDelete;
	result.pGetError = ( decltype( result.pGetError ) )pError;
	*pAPI = result;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLCreateNativeBuffer( const sirGLBufferDispatch_t *pAPI, const sirGraphicsBufferDesc_t *pDesc,
                                             uint32_t *pName ) noexcept
{
	if ( pAPI == nullptr || pDesc == nullptr || pName == nullptr || pAPI->pCreateBuffers == nullptr ||
	     pAPI->pNamedBufferStorage == nullptr || pAPI->pDeleteBuffers == nullptr || pAPI->pGetError == nullptr || pDesc->nSizeBytes == 0 ||
	     pDesc->nSizeBytes > ( uint64_t )INTPTR_MAX || pDesc->nUsage == 0 || ( pDesc->nUsage & ~SIR_GL_BUFFER_USES ) != 0 ||
	     ( pDesc->nAlignmentBytes != 0 && ( pDesc->nAlignmentBytes & ( pDesc->nAlignmentBytes - 1 ) ) != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	// Never attribute a previously queued GL error to a new allocation.
	if ( pAPI->pGetError() != SIR_GL_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	uint32_t nName = 0;
	pAPI->pCreateBuffers( 1, &nName );
	const uint32_t nCreateError = pAPI->pGetError();
	if ( nCreateError != SIR_GL_NO_ERROR || nName == 0 ) {
		if ( nName != 0 ) {
			pAPI->pDeleteBuffers( 1, &nName );
		}
		return sirGLMapError( nCreateError );
	}
	pAPI->pNamedBufferStorage( nName, ( intptr_t )pDesc->nSizeBytes, nullptr, 0 );
	const uint32_t nStorageError = pAPI->pGetError();
	if ( nStorageError != SIR_GL_NO_ERROR ) {
		pAPI->pDeleteBuffers( 1, &nName );
		return sirGLMapError( nStorageError );
	}
	*pName = nName;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLDeleteNativeBuffer( const sirGLBufferDispatch_t *pAPI, uint32_t nName ) noexcept
{
	if ( pAPI == nullptr || pAPI->pDeleteBuffers == nullptr || pAPI->pGetError == nullptr || nName == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pAPI->pDeleteBuffers( 1, &nName );
	const uint32_t nError = pAPI->pGetError();
	return nError == SIR_GL_NO_ERROR ? SIR_GRAPHICS_SUCCESS : sirGLMapError( nError );
}
