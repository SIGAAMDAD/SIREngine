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

#ifndef __SIR_RENDER_STORAGE_BUFFER_H__
#define __SIR_RENDER_STORAGE_BUFFER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class SIRRenderMemoryType : uint8_t {
	SIR_RENDER_MEMORY_TYPE_GPU_LOCAL,
	SIR_RENDER_MEMORY_TYPE_CPU_UPLOAD,
	SIR_RENDER_MEMORY_TYPE_CPU_READBACK,
	SIR_RENDER_MEMORY_TYPE_GPU_UPLOAD,
	SIR_RENDER_MEMORY_TYPE_TRANSIENT
};

#include <Renderer/SIRRenderTypes.h>

// One GPU buffer. Usage bits select vertex/index/constant/storage/indirect/transfer roles.
// Nonowning, zero-allocation typed façade; the device owns resource lifetime.
class SIRRenderStorageBuffer
{
public:
	explicit SIRRenderStorageBuffer( const SIRRenderStorageBuffer& ) noexcept = delete;
	explicit SIRRenderStorageBuffer( SIRRenderStorageBuffer&& ) noexcept = delete;

	bool isValid( void ) const noexcept;
	uint32_t getReservedSize( void ) const noexcept;
	uint32_t getCommittedBytes( void ) const noexcept;

protected:
	explicit SIRRenderStorageBuffer( void ) noexcept = default;
	virtual ~SIRRenderStorageBuffer() noexcept = default;

	uint32_t m_nReservedBytes;
	uint32_t m_nCommittedBytes;

	SIRRenderMemoryType m_eMemoryType;
};

#endif // __SIR_RENDER_STORAGE_BUFFER_H__
