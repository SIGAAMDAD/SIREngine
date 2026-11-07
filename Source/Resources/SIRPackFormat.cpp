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

#include <Resources/SIRPackFormat.h>

#include <string.h>

uint16_t sirPackRead16( const uint8_t *pBytes ) noexcept
{
	return ( uint16_t )( ( uint16_t )pBytes[0] | ( ( uint16_t )pBytes[1] << 8 ) );
}

uint32_t sirPackRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

uint64_t sirPackRead64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirPackRead32( pBytes ) | ( ( uint64_t )sirPackRead32( pBytes + 4 ) << 32 );
}

void sirPackWrite16( uint8_t *pBytes, uint16_t nValue ) noexcept
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

void sirPackWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pBytes[i] = ( uint8_t )( nValue >> ( 8 * i ) );
	}
}

void sirPackWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	sirPackWrite32( pBytes, ( uint32_t )nValue );
	sirPackWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

typedef struct sirPackHashState_s
{
	uint32_t nState[8];
	uint8_t szBlock[64];
	uint64_t nTotal;
	uint32_t nBuffered;
} sirPackHashState_t;

static uint32_t sirPackRotR( uint32_t nValue, uint32_t nShift ) noexcept
{
	return ( nValue >> nShift ) | ( nValue << ( 32 - nShift ) );
}

static void sirPackHashBlock( sirPackHashState_t *pState, const uint8_t *pBlock ) noexcept
{
	static constexpr uint32_t s_nK[64] = {
	    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
	    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
	    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
	    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
	    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
	    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U };
	uint32_t nW[64] = {};
	for ( uint32_t i = 0; i < 16; ++i ) {
		nW[i] = ( ( uint32_t )pBlock[4 * i] << 24 ) | ( ( uint32_t )pBlock[4 * i + 1] << 16 ) | ( ( uint32_t )pBlock[4 * i + 2] << 8 ) |
		        ( uint32_t )pBlock[4 * i + 3];
	}
	for ( uint32_t i = 16; i < 64; ++i ) {
		const uint32_t nS0 = sirPackRotR( nW[i - 15], 7 ) ^ sirPackRotR( nW[i - 15], 18 ) ^ ( nW[i - 15] >> 3 );
		const uint32_t nS1 = sirPackRotR( nW[i - 2], 17 ) ^ sirPackRotR( nW[i - 2], 19 ) ^ ( nW[i - 2] >> 10 );
		nW[i] = nW[i - 16] + nS0 + nW[i - 7] + nS1;
	}
	uint32_t a = pState->nState[0];
	uint32_t b = pState->nState[1];
	uint32_t c = pState->nState[2];
	uint32_t d = pState->nState[3];
	uint32_t e = pState->nState[4];
	uint32_t f = pState->nState[5];
	uint32_t g = pState->nState[6];
	uint32_t h = pState->nState[7];
	for ( uint32_t i = 0; i < 64; ++i ) {
		const uint32_t nS1 = sirPackRotR( e, 6 ) ^ sirPackRotR( e, 11 ) ^ sirPackRotR( e, 25 );
		const uint32_t nChoice = ( e & f ) ^ ( ~e & g );
		const uint32_t nTemp1 = h + nS1 + nChoice + s_nK[i] + nW[i];
		const uint32_t nS0 = sirPackRotR( a, 2 ) ^ sirPackRotR( a, 13 ) ^ sirPackRotR( a, 22 );
		const uint32_t nMajority = ( a & b ) ^ ( a & c ) ^ ( b & c );
		const uint32_t nTemp2 = nS0 + nMajority;
		h = g;
		g = f;
		f = e;
		e = d + nTemp1;
		d = c;
		c = b;
		b = a;
		a = nTemp1 + nTemp2;
	}
	const uint32_t nNew[8] = { a, b, c, d, e, f, g, h };
	for ( uint32_t i = 0; i < 8; ++i ) {
		pState->nState[i] += nNew[i];
	}
}

static void sirPackHashInit( sirPackHashState_t *pState ) noexcept
{
	const uint32_t nInitial[8] = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U };
	memcpy( pState->nState, nInitial, sizeof( nInitial ) );
	pState->nTotal = 0;
	pState->nBuffered = 0;
}

static void sirPackHashUpdate( sirPackHashState_t *pState, const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	pState->nTotal += nBytes;
	while ( nBytes != 0 ) {
		if ( pState->nBuffered == 0 && nBytes >= 64 ) {
			sirPackHashBlock( pState, pBytes );
			pBytes += 64;
			nBytes -= 64;
			continue;
		}
		const uint32_t nRoom = 64 - pState->nBuffered;
		const uint32_t nCount = nBytes < nRoom ? ( uint32_t )nBytes : nRoom;
		memcpy( pState->szBlock + pState->nBuffered, pBytes, nCount );
		pBytes += nCount;
		nBytes -= nCount;
		pState->nBuffered += nCount;
		if ( pState->nBuffered == 64 ) {
			sirPackHashBlock( pState, pState->szBlock );
			pState->nBuffered = 0;
		}
	}
}

static void sirPackHashFinish( sirPackHashState_t *pState, uint8_t *pDigest ) noexcept
{
	const uint64_t nBits = pState->nTotal * 8;
	uint8_t szPadding[128] = {};
	szPadding[0] = 0x80;
	const uint32_t nPadding = pState->nBuffered < 56 ? 56 - pState->nBuffered : 120 - pState->nBuffered;
	sirPackHashUpdate( pState, szPadding, nPadding );
	uint8_t szLength[8] = {};
	for ( uint32_t i = 0; i < 8; ++i ) {
		szLength[7 - i] = ( uint8_t )( nBits >> ( 8 * i ) );
	}
	sirPackHashUpdate( pState, szLength, 8 );
	for ( uint32_t i = 0; i < 8; ++i ) {
		pDigest[4 * i] = ( uint8_t )( pState->nState[i] >> 24 );
		pDigest[4 * i + 1] = ( uint8_t )( pState->nState[i] >> 16 );
		pDigest[4 * i + 2] = ( uint8_t )( pState->nState[i] >> 8 );
		pDigest[4 * i + 3] = ( uint8_t )pState->nState[i];
	}
}

void sirPackSha256( const uint8_t *pBytes, uint64_t nBytes, uint8_t *pDigest ) noexcept
{
	if ( pDigest == nullptr || ( pBytes == nullptr && nBytes != 0 ) ) {
		return;
	}
	sirPackHashState_t state = {};
	sirPackHashInit( &state );
	if ( nBytes != 0 ) {
		sirPackHashUpdate( &state, pBytes, nBytes );
	}
	sirPackHashFinish( &state, pDigest );
}

void sirPackCatalogDigest( const uint8_t *pBytes, uint64_t nBytes, uint8_t *pDigest ) noexcept
{
	if ( pBytes == nullptr || pDigest == nullptr || nBytes < SIR_PACK_HEADER_BYTES ) {
		return;
	}
	static constexpr uint8_t s_szZeroDigest[32] = {};
	sirPackHashState_t state = {};
	sirPackHashInit( &state );
	sirPackHashUpdate( &state, pBytes, 88 );
	sirPackHashUpdate( &state, s_szZeroDigest, 32 );
	sirPackHashUpdate( &state, pBytes + 120, nBytes - 120 );
	sirPackHashFinish( &state, pDigest );
}
