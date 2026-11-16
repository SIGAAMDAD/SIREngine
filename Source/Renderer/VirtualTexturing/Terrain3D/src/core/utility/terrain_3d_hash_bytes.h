#ifndef TERRAIN3D_HASH_BYTES_H
#define TERRAIN3D_HASH_BYTES_H

#include "core/config/terrain_3d_compiler.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

EA_FORCE_INLINE uint64_t t3dHashCombine64( const uint64_t nHash, const uint64_t nValue )
{
	return nHash ^ ( nValue + 0x9e3779b97f4a7c15ULL + ( nHash << 6u ) + ( nHash >> 2u ) );
}

inline uint64_t t3dHashFnv1a64LittleU32( uint64_t nHash, const uint32_t nValue )
{
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		nHash = ( nHash ^ ( uint8_t )( nValue >> ( i * 8u ) ) ) * 1099511628211ull;
	}
	return nHash;
}

// Integer avalanche only. Coordinate mixing, seeds and normalization are
// domain policy and remain with each baker.
EA_FORCE_INLINE uint32_t t3dHashAvalanche32( uint32_t nValue )
{
	nValue ^= nValue >> 16u;
	nValue *= 0x7feb352du;
	nValue ^= nValue >> 15u;
	nValue *= 0x846ca68bu;
	nValue ^= nValue >> 16u;
	return nValue;
}

// Seed is always explicit: existing asset/cache formats use different bases.
// This operation consumes bytes verbatim and performs no endian conversion.
inline uint64_t t3dHashFnv1a64( const void *pData, const uint64_t nSize, uint64_t nHash )
{
	const uint8_t *pBytes = ( const uint8_t * )( pData );
	for ( uint64_t i = 0u; i < nSize; ++i ) {
		nHash ^= ( uint64_t )( pBytes[i] );
		nHash *= 1099511628211ull;
	}
	return nHash;
}

inline uint64_t t3dHashPublicationBytes( const void *pData, const size_t nSize )
{
	return t3dHashFnv1a64( pData, nSize, 1469598103934665603ULL );
}

inline uint32_t t3dHashFnv1a32( const void *pData, const uint64_t nSize, uint32_t nHash )
{
	const uint8_t *pBytes = ( const uint8_t * )( pData );
	for ( uint64_t i = 0u; i < nSize; ++i ) {
		nHash = ( nHash ^ pBytes[i] ) * 16777619u;
	}
	return nHash;
}

// Serialized integer signatures explicitly consume low byte first, regardless
// of host byte order. Do not substitute the native-order helpers below.
inline uint64_t t3dHashFnv1a64LittleU64( uint64_t nHash, const uint64_t nValue )
{
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		nHash = ( nHash ^ ( uint8_t )( nValue >> ( i * 8u ) ) ) * 1099511628211ull;
	}
	return nHash;
}

inline void t3dHashFnv1a64Append( uint64_t& nHash, const void *pData, const size_t nSize )
{
	nHash = t3dHashFnv1a64( pData, nSize, nHash );
}

// These named native-order helpers preserve the existing cache signatures.
// They are not a portable little-endian serialization contract.
EA_FORCE_INLINE void t3dHashFnv1a64AppendNativeU32( uint64_t& nHash, const uint32_t nValue )
{
	t3dHashFnv1a64Append( nHash, &nValue, sizeof( nValue ) );
}

EA_FORCE_INLINE void t3dHashFnv1a64AppendNativeU64( uint64_t& nHash, const uint64_t nValue )
{
	t3dHashFnv1a64Append( nHash, &nValue, sizeof( nValue ) );
}

EA_FORCE_INLINE void t3dHashFnv1a64AppendNativeF32( uint64_t& nHash, const float fValue )
{
	uint32_t bits = 0u;
	static_assert( sizeof( bits ) == sizeof( fValue ), "32-bit float required by native cache signatures" );
	memcpy( &bits, &fValue, sizeof( bits ) );
	t3dHashFnv1a64AppendNativeU32( nHash, bits );
}

// Existing VT package seed and null-input policy; persisted signatures stay unchanged.
inline uint64_t t3dHashStableBytes( const void *pBytes, const uint64_t nBytes )
{
	const uint8_t *const pData = ( const uint8_t * )pBytes;
	uint64_t nHash = 1469598103934665603ull;
	if ( pData == NULL ) {
		return nBytes == 0u ? nHash : 0u;
	}
	return t3dHashFnv1a64( pData, nBytes, nHash );
}

#endif // TERRAIN3D_HASH_BYTES_H
