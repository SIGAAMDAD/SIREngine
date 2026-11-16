#ifndef TERRAIN3D_CHECKED_MATH_H
#define TERRAIN3D_CHECKED_MATH_H

#include "core/config/terrain_3d_compiler.h"

template <typename T>
EA_FORCE_INLINE constexpr T t3dSaturatingAddUnsigned( const T nA, const T nB )
{
	static_assert( ( T )-1 > ( T )0 && ( T )2 != ( T ) true, "Unsigned integer required" );
	return nA > ( T ) ~( T )0 - nB ? ( T ) ~( T )0 : nA + nB;
}

template <typename T>
EA_FORCE_INLINE constexpr bool t3dIsPowerOfTwo( const T nValue )
{
	return nValue != 0 && ( nValue & ( nValue - 1 ) ) == 0;
}

// Unsigned size/count arithmetic. Failure leaves the output unchanged, including
// when it aliases an input. Saturating or sentinel policies stay with callers.
template <typename T>
EA_FORCE_INLINE constexpr bool t3dCheckedAdd( const T nA, const T nB, T& rValue )
{
	static_assert( ( T )-1 > ( T )0 && ( T )2 != ( T ) true, "Unsigned integer required" );
	if ( nA > ( T ) ~( T )0 - nB ) {
		return false;
	}
	rValue = nA + nB;
	return true;
}

template <typename T>
EA_FORCE_INLINE constexpr bool t3dCheckedMultiply( const T nA, const T nB, T& rValue )
{
	static_assert( ( T )-1 > ( T )0 && ( T )2 != ( T ) true, "Unsigned integer required" );
	if ( nA != 0 && nB > ( T ) ~( T )0 / nA ) {
		return false;
	}
	rValue = nA * nB;
	return true;
}

template <typename T>
EA_FORCE_INLINE constexpr bool t3dCheckedAlignUp( const T nValue, const T nAlignment, T& rAligned )
{
	if ( !t3dIsPowerOfTwo( nAlignment ) ) {
		return false;
	}
	T sum = 0;
	if ( !t3dCheckedAdd( nValue, ( T )( nAlignment - 1 ), sum ) ) {
		return false;
	}
	rAligned = sum & ~( nAlignment - 1 );
	return true;
}

// Zero is the caller-selected sentinel for invalid or overflowing alignment.
template <typename T>
EA_FORCE_INLINE constexpr T t3dAlignUpOrZero( const T nValue, const T nAlignment )
{
	T aligned = 0;
	return t3dCheckedAlignUp( nValue, nAlignment, aligned ) ? aligned : 0;
}

template <typename T>
EA_FORCE_INLINE constexpr bool t3dByteRangeFits( const T nOffset, const T nBytes, const T nLimit )
{
	static_assert( ( T )-1 > ( T )0 && ( T )2 != ( T ) true, "Unsigned integer required" );
	return nOffset <= nLimit && nBytes <= nLimit - nOffset;
}

#endif // TERRAIN3D_CHECKED_MATH_H
