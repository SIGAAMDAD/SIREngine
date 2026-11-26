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

#ifndef __SIR_ENDIAN_H__
#define __SIR_ENDIAN_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

class SIREndian
{
public:
	SIREndian( void ) = delete;

	static constexpr bool isLittleEndian( void ) noexcept;
	static constexpr bool isBigEndian( void ) noexcept;

	template <typename T>
	static T byteSwap( T value ) noexcept;

	template <typename T>
	static T readNative( const void *pData ) noexcept;

	template <typename T>
	static T readLittle( const void *pData ) noexcept;

	template <typename T>
	static T readBig( const void *pData ) noexcept;

	template <typename T>
	static void writeNative( void *pData, T value ) noexcept;

	template <typename T>
	static void writeLittle( void *pData, T value ) noexcept;

	template <typename T>
	static void writeBig( void *pData, T value ) noexcept;

private:
	template <typename T>
	static constexpr bool isSupportedType( void ) noexcept;
};

constexpr bool SIREndian::isLittleEndian( void ) noexcept
{
#ifdef SIR_ENDIAN_LITTLE
	return true;
#elif defined( SIR_PLATFORM_WIN32 )
	return true;
#else
#error Unsupported platform byte order
#endif
}

constexpr bool SIREndian::isBigEndian( void ) noexcept
{
#ifdef SIR_ENDIAN_BIG
	return true;
#elif defined( SIR_PLATFORM_WIN32 )
	return false;
#else
#error Unsupported platform byte order
#endif
}

template <typename T>
constexpr bool SIREndian::isSupportedType( void ) noexcept
{
	return std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<T, bool> &&
	       ( sizeof( T ) == 1 || sizeof( T ) == 2 || sizeof( T ) == 4 || sizeof( T ) == 8 );
}

template <typename T>
SIR_FORCEINLINE T SIREndian::byteSwap( T value ) noexcept
{
	static_assert( isSupportedType<T>(), "Unsupported endian type" );

	if constexpr ( sizeof( T ) == 1 ) {
		return value;
	}
#if defined( __clang__ ) || defined( __GNUC__ )

	else if constexpr ( sizeof( T ) == 2 ) {
		return static_cast<T>( __builtin_bswap16( value ) );
	} else if constexpr ( sizeof( T ) == 4 ) {
		return static_cast<T>( __builtin_bswap32( value ) );
	} else {
		return static_cast<T>( __builtin_bswap64( value ) );
	}

#elif defined( _MSC_VER )

	else if constexpr ( sizeof( T ) == 2 ) {
		return static_cast<T>( _byteswap_ushort( value ) );
	} else if constexpr ( sizeof( T ) == 4 ) {
		return static_cast<T>( _byteswap_ulong( value ) );
	} else {
		return static_cast<T>( _byteswap_uint64( value ) );
	}

#else

	else {
		T result = 0;

		for ( size_t i = 0; i < sizeof( T ); ++i ) {
			result = static_cast<T>( ( result << 8 ) | ( value & 0xff ) );

			value >>= 8;
		}

		return result;
	}

#endif
}

template <typename T>
SIR_FORCEINLINE T SIREndian::readNative( const void *pData ) noexcept
{
	static_assert( isSupportedType<T>(), "Unsupported endian type" );

	T value;
	memcpy( &value, pData, sizeof( T ) );

	return value;
}

template <typename T>
SIR_FORCEINLINE T SIREndian::readLittle( const void *pData ) noexcept
{
	static_assert( isLittleEndian() || isBigEndian(), "Unsupported mixed-endian platform" );

	T value = readNative<T>( pData );

	if constexpr ( isBigEndian() ) {
		return byteSwap( value );
	}

	return value;
}

template <typename T>
SIR_FORCEINLINE T SIREndian::readBig( const void *pData ) noexcept
{
	static_assert( isLittleEndian() || isBigEndian(), "Unsupported mixed-endian platform" );

	T value = readNative<T>( pData );

	if constexpr ( isLittleEndian() ) {
		return byteSwap( value );
	}

	return value;
}

template <typename T>
SIR_FORCEINLINE void SIREndian::writeNative( void *pData, T value ) noexcept
{
	static_assert( isSupportedType<T>(), "Unsupported endian type" );

	memcpy( pData, &value, sizeof( T ) );
}

template <typename T>
SIR_FORCEINLINE void SIREndian::writeLittle( void *pData, T value ) noexcept
{
	static_assert( isLittleEndian() || isBigEndian(), "Unsupported mixed-endian platform" );

	if constexpr ( isBigEndian() ) {
		value = byteSwap( value );
	}

	writeNative( pData, value );
}

template <typename T>
SIR_FORCEINLINE void SIREndian::writeBig( void *pData, T value ) noexcept
{
	static_assert( isLittleEndian() || isBigEndian(), "Unsupported mixed-endian platform" );

	if constexpr ( isLittleEndian() ) {
		value = byteSwap( value );
	}

	writeNative( pData, value );
}

#endif // __SIR_ENDIAN_H__
