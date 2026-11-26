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

#include <Renderer/Drivers/OpenGL/SIRGLBuffers.h>
#include <stdint.h>

static constexpr uint32_t SIR_GL_NO_ERROR = 0;
static constexpr uint32_t SIR_GL_OUT_OF_MEMORY = 0x0505;
static constexpr uint32_t SIR_GL_BUFFER_USES = SIRGraphicsUsage::SIR_GRAPHICS_USAGE_VERTEX | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_INDEX | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_CONSTANT |
                                               SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_SOURCE |
                                               SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_INDIRECT;

static SIRGraphicsResult sirGLMapError( uint32_t nError ) noexcept
{
	return nError == SIR_GL_OUT_OF_MEMORY ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
}

SIRGraphicsResult sirGLLoadBufferDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLBufferDispatch_t *pAPI ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr || pAPI == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGLBufferDispatch_t result {};
	void *pCreate = pLookup( pContext, "glCreateBuffers" );
	void *pStorage = pLookup( pContext, "glNamedBufferStorage" );
	void *pDelete = pLookup( pContext, "glDeleteBuffers" );
	void *pError = pLookup( pContext, "glGetError" );
	if ( !sirGLIsUsableProcedure( pCreate ) || !sirGLIsUsableProcedure( pStorage ) || !sirGLIsUsableProcedure( pDelete ) ||
	     !sirGLIsUsableProcedure( pError ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	result.pCreateBuffers = reinterpret_cast<void ( * )( int32_t, uint32_t * )>( pCreate );
	result.pNamedBufferStorage = reinterpret_cast<void ( * )( uint32_t, intptr_t, const void *, uint32_t )>( pStorage );
	result.pDeleteBuffers = reinterpret_cast<void ( * )( int32_t, const uint32_t * )>( pDelete );
	result.pGetError = reinterpret_cast<uint32_t ( * )( void )>( pError );
	*pAPI = result;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLCreateNativeBuffer( const sirGLBufferDispatch_t *pAPI, const sirGraphicsBufferDesc_t *pDesc,
                                             uint32_t *pName ) noexcept
{
	if ( pAPI == nullptr || pDesc == nullptr || pName == nullptr || pAPI->pCreateBuffers == nullptr ||
	     pAPI->pNamedBufferStorage == nullptr || pAPI->pDeleteBuffers == nullptr || pAPI->pGetError == nullptr || pDesc->nSizeBytes == 0 ||
	     pDesc->nSizeBytes > static_cast<uint64_t>(INTPTR_MAX) || pDesc->nUsage == 0 || ( pDesc->nUsage & ~SIR_GL_BUFFER_USES ) != 0 ||
	     ( pDesc->nAlignmentBytes != 0 && ( pDesc->nAlignmentBytes & ( pDesc->nAlignmentBytes - 1 ) ) != 0 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	// Never attribute a previously queued GL error to a new allocation.
	if ( pAPI->pGetError() != SIR_GL_NO_ERROR ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
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
	pAPI->pNamedBufferStorage( nName, static_cast<intptr_t>(pDesc->nSizeBytes), nullptr, 0 );
	const uint32_t nStorageError = pAPI->pGetError();
	if ( nStorageError != SIR_GL_NO_ERROR ) {
		pAPI->pDeleteBuffers( 1, &nName );
		return sirGLMapError( nStorageError );
	}
	*pName = nName;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLDeleteNativeBuffer( const sirGLBufferDispatch_t *pAPI, uint32_t nName ) noexcept
{
	if ( pAPI == nullptr || pAPI->pDeleteBuffers == nullptr || pAPI->pGetError == nullptr || nName == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_NO_ERROR ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	pAPI->pDeleteBuffers( 1, &nName );
	const uint32_t nError = pAPI->pGetError();
	return nError == SIR_GL_NO_ERROR ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : sirGLMapError( nError );
}
