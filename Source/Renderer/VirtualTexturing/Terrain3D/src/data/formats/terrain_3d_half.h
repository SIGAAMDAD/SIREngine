// IEEE-754 binary16 conversion shared by storage, material and geometry code.
#ifndef TERRAIN3D_HALF_H
#define TERRAIN3D_HALF_H

#include <stdint.h>
#include <string.h>

// Round to nearest, ties to even. Overflow produces signed infinity; NaNs
// retain their sign/payload and are quieted. Saturation belongs to callers.
inline uint16_t t3dHalfEncode( const float fValue )
{
	uint32_t bits = 0u;
	memcpy( &bits, &fValue, sizeof( bits ) );
	const uint16_t sign = ( uint16_t )( ( bits >> 16u ) & 0x8000u );
	uint32_t mantissa = bits & 0x007fffffu;
	const uint32_t source_exponent = ( bits >> 23u ) & 0xffu;

	if ( source_exponent == 0xffu ) {
		if ( mantissa == 0u ) {
			return ( uint16_t )( sign | 0x7c00u );
		}
		return ( uint16_t )( sign | 0x7e00u | ( uint16_t )( mantissa >> 13u ) );
	}

	int32_t exponent = ( int32_t )( source_exponent )-127 + 15;
	if ( exponent >= 31 ) {
		return ( uint16_t )( sign | 0x7c00u );
	}
	if ( exponent <= 0 ) {
		if ( exponent < -10 ) {
			return sign;
		}
		mantissa |= 0x00800000u;
		const uint32_t shift = ( uint32_t )( 14 - exponent );
		const uint32_t rounding = ( ( uint32_t )( 1 ) << ( shift - 1u ) ) - 1u + ( ( mantissa >> shift ) & 1u );
		return ( uint16_t )( sign | ( uint16_t )( ( mantissa + rounding ) >> shift ) );
	}

	mantissa += 0x00000fffu + ( ( mantissa >> 13u ) & 1u );
	if ( ( mantissa & 0x00800000u ) != 0u ) {
		mantissa = 0u;
		++exponent;
		if ( exponent >= 31 ) {
			return ( uint16_t )( sign | 0x7c00u );
		}
	}
	return ( uint16_t )( sign | ( uint16_t )( ( uint32_t )( exponent ) << 10u ) | ( uint16_t )( mantissa >> 13u ) );
}

inline float t3dHalfDecode( const uint16_t nValue )
{
	const uint32_t sign = ( uint32_t )( nValue & 0x8000u ) << 16u;
	uint32_t exponent = ( nValue >> 10u ) & 0x1fu;
	uint32_t mantissa = nValue & 0x03ffu;
	uint32_t bits = 0u;

	if ( exponent == 0u ) {
		if ( mantissa == 0u ) {
			bits = sign;
		} else {
			exponent = 1u;
			while ( ( mantissa & 0x0400u ) == 0u ) {
				mantissa <<= 1u;
				--exponent;
			}
			mantissa &= 0x03ffu;
			const uint32_t float_exponent = exponent + ( uint32_t )( 127 - 15 );
			bits = sign | ( float_exponent << 23u ) | ( mantissa << 13u );
		}
	} else if ( exponent == 0x1fu ) {
		bits = sign | 0x7f800000u | ( mantissa << 13u );
	} else {
		const uint32_t float_exponent = exponent + ( uint32_t )( 127 - 15 );
		bits = sign | ( float_exponent << 23u ) | ( mantissa << 13u );
	}

	float fResult = 0.0f;
	memcpy( &fResult, &bits, sizeof( fResult ) );
	return fResult;
}

#endif // TERRAIN3D_HALF_H
