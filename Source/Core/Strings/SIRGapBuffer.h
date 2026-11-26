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

#ifndef __SIR_GAPBUFFER_H__
#define __SIR_GAPBUFFER_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Single-writer bounded editor buffer over caller-owned memory. Logical text is
// split around a movable gap. Input/output must not overlap backing storage.
class SIRGapBuffer final
{
public:
	explicit SIRGapBuffer() noexcept;
	explicit SIRGapBuffer( char *pStorage, uint32_t nCapacity ) noexcept;
	explicit SIRGapBuffer( const SIRGapBuffer& ) = delete;
	explicit SIRGapBuffer( SIRGapBuffer&& ) = delete;
	~SIRGapBuffer() noexcept;
	SIRGapBuffer& operator=( const SIRGapBuffer& ) = delete;
	SIRGapBuffer& operator=( SIRGapBuffer&& ) = delete;

	[[nodiscard]] uint32_t getByteLength( void ) const noexcept;
	[[nodiscard]] sirStringView_t getPrefix( void ) const noexcept;
	[[nodiscard]] sirStringView_t getSuffix( void ) const noexcept;
	[[nodiscard]] uint32_t getGapSize( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryGetByte( uint32_t iByte, char& nByte ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInsert( uint32_t iPosition, sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryErase( uint32_t iPosition, uint32_t nBytes ) noexcept;
	[[nodiscard]] SIRContainerResult tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept;
	[[nodiscard]] SIRContainerResult tryInitializeVirtual( uint32_t nCapacity ) noexcept;
	void clear( void ) noexcept;

private:
	void moveGap( uint32_t iPosition ) noexcept;
	void moveBytes( char *pDestination, char *pSource, uint32_t nBytes ) noexcept;
	size_t m_nMappedBytes{};
	char *m_pStorage;
	uint32_t m_nCapacity;
	uint32_t m_iGapBegin{};
	uint32_t m_iGapEnd;
};

#endif // __SIR_GAPBUFFER_H__
