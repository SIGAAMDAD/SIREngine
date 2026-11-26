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

#ifndef __SIR_IO_BUFFER_POOL_H__
#define __SIR_IO_BUFFER_POOL_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/IO/SIRIOTypes.h>

class SIRIOBufferPool final
{
public:
	SIRIOBufferPool( void ) noexcept;

	sirIOResult_t initialize( const sirIOBufferPoolConfig_t& config ) noexcept;
	sirIOResult_t acquire( uint64_t nBytes, uint32_t nAlignment, sirIOBufferLease_t *pLease ) noexcept;
	sirIOResult_t release( sirIOBufferHandle_t handle ) noexcept;

	uint64_t getCapacity( void ) const noexcept;
	uint64_t getUsedBytes( void ) const noexcept;
	sirIOResult_t shutdown( void ) noexcept;

private:
	uint8_t *m_pStorage;
	uint64_t m_nCapacity, m_nUsed;
	sirIOBufferLease_t *m_pSlots;
	uint32_t m_nSlotCapacity, m_nMaxAlignment;
	bool m_bInitialized;
};
#endif // __SIR_IO_BUFFER_POOL_H__
