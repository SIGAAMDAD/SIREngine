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

#ifndef __SIR_GL_STORAGE_BUFFER_H__
#define __SIR_GL_STORAGE_BUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include "SIRGLShared.h"
#include <Renderer/RHI/SIRRenderStorageBuffer.h>

class SIRGLStorageBuffer final : public SIRRenderStorageBuffer
{
public:
	~SIRGLStorageBuffer() noexcept override;

	// advanced pipeline features
	virtual SIRRenderResult map( uint64_t nOffsetBytes, uint64_t nSizeBytes, SIRRenderMappedBuffer *pMapping ) noexcept override;
	virtual SIRRenderResult unmap( void ) noexcept override;
	virtual SIRRenderResult flush( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept override;
	virtual SIRRenderResult invalidate( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept override;
	virtual uint64_t getDeviceAddress( void ) const noexcept override;

	virtual SIRRenderResult updateBufferRange( uint64_t nOffsetBytes, uint64_t nSizeBytes, const SIRSpan<uint8_t>& memory ) noexcept override;
	virtual SIRRenderResult copyBufferRange( SIRRenderStorageBuffer& copyTarget, uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept override;
	virtual SIRRenderResult clearBufferRange( uint64_t nOffsetBytes, uint64_t nSizeBytes ) noexcept override;
	virtual SIRRenderResult clear( void ) noexcept override;

	virtual SIRRenderResult bind( void ) const noexcept override;
	virtual SIRRenderResult unbind( void ) const noexcept override;

	virtual bool isValid( void ) const noexcept override;

private:
	explicit SIRGLStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderBufferDesc& desc ) noexcept;

	GLuint m_hBufferID;
	void *m_pMappedBuffer;
};

#endif // __SIR_GL_STORAGE_BUFFER_H__
