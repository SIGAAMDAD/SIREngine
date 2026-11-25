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

#ifndef __SIR_BINARY_WRITER_H__
#define __SIR_BINARY_WRITER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>

class SIRBinaryWriter
{
public:
	constexpr explicit SIRBinaryWriter( void ) noexcept = default;
	constexpr explicit SIRBinaryWriter( const SIRBinaryWriter& ) noexcept = delete;
	constexpr explicit SIRBinaryWriter( SIRBinaryWriter&& ) noexcept = delete;
	SIRBinaryWriter( const SIRSpan<uint8_t>& memory ) noexcept;
	~SIRBinaryWriter() noexcept = default;

	const SIRBinaryWriter& operator=( const SIRBinaryWriter& ) noexcept = delete;
	const SIRBinaryWriter& operator=( SIRBinaryWriter&& ) noexcept = delete;

	bool writeU8( uint8_t value ) noexcept;
	bool writeU16( uint16_t value ) noexcept;
	bool writeU32( uint32_t value ) noexcept;
	bool writeU64( uint64_t value ) noexcept;

	bool writeI8( int8_t value ) noexcept;
	bool writeI16( int16_t value ) noexcept;
	bool writeI32( int32_t value ) noexcept;
	bool writeI64( int64_t value ) noexcept;

	bool writeBytes( const SIRSpan<uint8_t>& bytes ) noexcept;

	const SIRSpan<uint8_t>& getSpan( void ) const noexcept;
	size_t getSize( void ) const noexcept;
	size_t getOffset( void ) const noexcept;
	size_t getRemainingBytes( void ) const noexcept;
	SIRBinaryWriter getSlice( size_t nOffset, size_t nCount ) const noexcept;

private:
	SIRSpan<uint8_t> m_BufferRange;
	size_t m_nOffset;
};

SIR_FORCEINLINE SIRBinaryWriter::SIRBinaryWriter( const SIRSpan<uint8_t>& memory ) noexcept
    : m_BufferRange( memory ),
      m_nOffset( 0 )
{
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeU8( uint8_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint8_t ) ) {
		return false;
	}

	m_BufferRange[m_nOffset] = value;
	m_nOffset += sizeof( uint8_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeU16( uint16_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint16_t ) ) {
		return false;
	}

	*reinterpret_cast<uint16_t *>( &m_BufferRange[m_nOffset] ) = value;
	m_nOffset += sizeof( uint16_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeU32( uint32_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint32_t ) ) {
		return false;
	}

	*reinterpret_cast<uint32_t *>( &m_BufferRange[m_nOffset] ) = value;
	m_nOffset += sizeof( uint32_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeU64( uint64_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint64_t ) ) {
		return false;
	}

	*reinterpret_cast<uint64_t *>( &m_BufferRange[m_nOffset] ) = value;
	m_nOffset += sizeof( uint64_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeI8( int8_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( int8_t ) ) {
		return false;
	}

	m_BufferRange[m_nOffset] = static_cast<uint8_t>( value );
	m_nOffset += sizeof( int8_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeI16( int16_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( int16_t ) ) {
		return false;
	}

	*reinterpret_cast<int16_t *>( &m_BufferRange[m_nOffset] ) = value;
	m_nOffset += sizeof( int16_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeI32( int32_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( int32_t ) ) {
		return false;
	}

	*reinterpret_cast<int32_t *>( &m_BufferRange[m_nOffset] ) = value;
	m_nOffset += sizeof( int32_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeI64( int64_t value ) noexcept
{
	if ( getRemainingBytes() < sizeof( int64_t ) ) {
		return false;
	}

	*reinterpret_cast<int64_t *>( &m_BufferRange[m_nOffset] ) = value;
	m_nOffset += sizeof( int64_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryWriter::writeBytes( const SIRSpan<uint8_t>& bytes ) noexcept
{
	if ( getRemainingBytes() < bytes.getSize() ) {
		return false;
	}

	memcpy( m_BufferRange.getData() + m_nOffset, bytes.getData(), bytes.getSize() );
	m_nOffset += bytes.getSize();
	return true;
}

SIR_FORCEINLINE const SIRSpan<uint8_t>& SIRBinaryWriter::getSpan( void ) const noexcept
{
	return m_BufferRange;
}

SIR_FORCEINLINE size_t SIRBinaryWriter::getSize( void ) const noexcept
{
	return m_BufferRange.getSize();
}

SIR_FORCEINLINE size_t SIRBinaryWriter::getOffset( void ) const noexcept
{
	return m_nOffset;
}

SIR_FORCEINLINE size_t SIRBinaryWriter::getRemainingBytes( void ) const noexcept
{
	return m_BufferRange.getSize() - m_nOffset;
}

SIR_FORCEINLINE SIRBinaryWriter SIRBinaryWriter::getSlice( size_t nOffset, size_t nCount ) const noexcept
{
	return SIRBinaryWriter( m_BufferRange.subSpan( nOffset, nCount ) );
}

#endif // _SIR_BINARY_WRITER_H__
