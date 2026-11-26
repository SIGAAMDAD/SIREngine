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

#ifndef __SIR_BINARY_READER_H__
#define __SIR_BINARY_READER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>

class SIRBinaryReader
{
public:
	constexpr SIRBinaryReader( void ) noexcept = default;
	constexpr SIRBinaryReader( const SIRBinaryReader& ) noexcept = delete;
	constexpr SIRBinaryReader( SIRBinaryReader&& ) noexcept = delete;
	SIRBinaryReader( const SIRSpan<uint8_t>& memory ) noexcept;
	~SIRBinaryReader() noexcept = default;

	const SIRBinaryReader& operator=( const SIRBinaryReader& ) noexcept = delete;
	const SIRBinaryReader& operator=( SIRBinaryReader&& ) noexcept = delete;

	bool readU8( uint8_t& rValue ) noexcept;
	bool readU16( uint16_t& rValue ) noexcept;
	bool readU32( uint32_t& rValue ) noexcept;
	bool readU64( uint64_t& rValue ) noexcept;

	bool readI8( int8_t& rValue ) noexcept;
	bool readI16( int16_t& rValue ) noexcept;
	bool readI32( int32_t& rValue ) noexcept;
	bool readI64( int64_t& rValue ) noexcept;

	const SIRSpan<uint8_t> readBytes( size_t nCount ) noexcept;

	const SIRSpan<uint8_t>& getSpan( void ) const noexcept;
	size_t getSize( void ) const noexcept;
	size_t getRemainingBytes( void ) const noexcept;
	SIRBinaryReader getSlice( size_t nOffset, size_t nCount ) const noexcept;

private:
	const SIRSpan<uint8_t> m_BufferRange;
	uintptr_t m_nOffset;
};

SIR_FORCEINLINE SIRBinaryReader::SIRBinaryReader( const SIRSpan<uint8_t>& memory ) noexcept
    : m_BufferRange( memory ),
      m_nOffset( 0 )
{
}

SIR_FORCEINLINE bool SIRBinaryReader::readU8( uint8_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint8_t ) ) {
		return false;
	}

	rValue = m_BufferRange[m_nOffset];
	m_nOffset += sizeof( uint8_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryReader::readU16( uint16_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint16_t ) ) {
		return false;
	}

	rValue = *reinterpret_cast<const uint16_t *>( &m_BufferRange[m_nOffset] );
	m_nOffset += sizeof( uint16_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryReader::readU32( uint32_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint32_t ) ) {
		return false;
	}

	rValue = *reinterpret_cast<const uint32_t *>( &m_BufferRange[m_nOffset] );
	m_nOffset += sizeof( uint32_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryReader::readU64( uint64_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( uint64_t ) ) {
		return false;
	}

	rValue = *reinterpret_cast<const uint64_t *>( &m_BufferRange[m_nOffset] );
	m_nOffset += sizeof( uint64_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryReader::readI8( int8_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( int8_t ) ) {
		return false;
	}

	rValue = static_cast<int8_t>( m_BufferRange[m_nOffset] );
	m_nOffset += sizeof( int8_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryReader::readI16( int16_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( int16_t ) ) {
		return false;
	}

	rValue = *reinterpret_cast<const int16_t *>( &m_BufferRange[m_nOffset] );
	m_nOffset += sizeof( int16_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryReader::readI32( int32_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( int32_t ) ) {
		return false;
	}

	rValue = *reinterpret_cast<const int32_t *>( &m_BufferRange[m_nOffset] );
	m_nOffset += sizeof( int32_t );
	return true;
}

SIR_FORCEINLINE bool SIRBinaryReader::readI64( int64_t& rValue ) noexcept
{
	if ( getRemainingBytes() < sizeof( int64_t ) ) {
		return false;
	}

	rValue = *reinterpret_cast<const int64_t *>( &m_BufferRange[m_nOffset] );
	m_nOffset += sizeof( int64_t );
	return true;
}

SIR_FORCEINLINE const SIRSpan<uint8_t> SIRBinaryReader::readBytes( size_t nCount ) noexcept
{
	const SIRSpan<uint8_t> value = m_BufferRange.subSpan( m_nOffset, nCount );
	m_nOffset += nCount;
	return SIRSpan<uint8_t>( value );
}

SIR_FORCEINLINE const SIRSpan<uint8_t>& SIRBinaryReader::getSpan( void ) const noexcept
{
	return m_BufferRange;
}

SIR_FORCEINLINE size_t SIRBinaryReader::getSize( void ) const noexcept
{
	return m_BufferRange.getSize();
}

SIR_FORCEINLINE size_t SIRBinaryReader::getRemainingBytes( void ) const noexcept
{
	return getSize() - m_nOffset;
}

SIR_FORCEINLINE SIRBinaryReader SIRBinaryReader::getSlice( size_t nOffset, size_t nCount ) const noexcept
{
	return SIRBinaryReader( m_BufferRange.subSpan( nOffset, nCount ) );
}

#endif
