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

#ifndef __SIR_BUFFER_H__
#define __SIR_BUFFER_H__

#include "Core/Platform/SIRCompiler.h"
#include "Core/Systems/SIREngineSystem.h"
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>

class SIRBuffer
{
public:
	constexpr SIRBuffer( void ) noexcept = default;
	constexpr SIRBuffer( uint8_t *pBytes, size_t nSize ) noexcept;
	constexpr SIRBuffer( const SIRSpan<uint8_t>& rStorage ) noexcept;
	constexpr SIRBuffer( const SIRBuffer& ) noexcept = default;
	constexpr SIRBuffer( SIRBuffer&& ) noexcept = default;
	constexpr ~SIRBuffer() noexcept = default;

	constexpr size_t getSize( void ) const noexcept;
	constexpr void clear( void ) const noexcept;

	void appendBytes( const SIRBuffer& buffer ) noexcept;
	void assign( size_t nOffset, size_t nCount, uint8_t value ) noexcept;
	void assign( size_t nOffset, const SIRBuffer& buffer ) noexcept;

private:
	SIRSpan<uint8_t> m_Storage;
	size_t m_nSize;
};

SIR_FORCEINLINE constexpr SIRBuffer::SIRBuffer( uint8_t *pBytes, size_t nSize ) noexcept
	: m_Storage( pBytes, nSize )
{
}

SIR_FORCEINLINE constexpr SIRBuffer::SIRBuffer( const SIRSpan<uint8_t>& rStorage ) noexcept
	: m_Storage( rStorage )
{
}

SIR_FORCEINLINE constexpr size_t SIRBuffer::getSize( void ) const noexcept
{
	return m_nSize;
}

#endif // __SIR_BUFFER_H__
