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

#include <Scripting/Core/SIRScriptSHA256.h>

#include <string.h>

// The fixed constants implement FIPS 180-4 SHA-256. No heap or external crypto dependency.
static constexpr uint32_t s_szSHA256Constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

typedef struct sirScriptSHA256State_s
{
	uint32_t szHash[8];
	uint8_t szBuffer[64];
	uint64_t nBytes;
	uint32_t nBuffered;
} sirScriptSHA256State_t;

static uint32_t scriptRotateRight( uint32_t nValue, uint32_t nBits ) noexcept
{
	return ( nValue >> nBits ) | ( nValue << ( 32 - nBits ) );
}

static uint32_t scriptReadBE32( const uint8_t *pData ) noexcept
{
	return ( ( uint32_t )pData[0] << 24 ) | ( ( uint32_t )pData[1] << 16 ) | ( ( uint32_t )pData[2] << 8 ) | pData[3];
}

static void scriptWriteBE32( uint8_t *pData, uint32_t nValue ) noexcept
{
	pData[0] = ( uint8_t )( nValue >> 24 );
	pData[1] = ( uint8_t )( nValue >> 16 );
	pData[2] = ( uint8_t )( nValue >> 8 );
	pData[3] = ( uint8_t )nValue;
}

static void scriptSHA256Transform( sirScriptSHA256State_t &state, const uint8_t *pBlock ) noexcept
{
	uint32_t szWords[64] = {};
	for ( uint32_t iWord = 0; iWord < 16; ++iWord ) {
		szWords[iWord] = scriptReadBE32( pBlock + 4 * iWord );
	}
	for ( uint32_t iWord = 16; iWord < 64; ++iWord ) {
		const uint32_t nA = szWords[iWord - 15];
		const uint32_t nB = szWords[iWord - 2];
		const uint32_t nSigma0 = scriptRotateRight( nA, 7 ) ^ scriptRotateRight( nA, 18 ) ^ ( nA >> 3 );
		const uint32_t nSigma1 = scriptRotateRight( nB, 17 ) ^ scriptRotateRight( nB, 19 ) ^ ( nB >> 10 );
		szWords[iWord] = szWords[iWord - 16] + nSigma0 + szWords[iWord - 7] + nSigma1;
	}

	uint32_t a = state.szHash[0];
	uint32_t b = state.szHash[1];
	uint32_t c = state.szHash[2];
	uint32_t d = state.szHash[3];
	uint32_t e = state.szHash[4];
	uint32_t f = state.szHash[5];
	uint32_t g = state.szHash[6];
	uint32_t h = state.szHash[7];
	for ( uint32_t iWord = 0; iWord < 64; ++iWord ) {
		const uint32_t nS1 = scriptRotateRight( e, 6 ) ^ scriptRotateRight( e, 11 ) ^ scriptRotateRight( e, 25 );
		const uint32_t nChoice = ( e & f ) ^ ( ~e & g );
		const uint32_t nStep1 = h + nS1 + nChoice + s_szSHA256Constants[iWord] + szWords[iWord];
		const uint32_t nS0 = scriptRotateRight( a, 2 ) ^ scriptRotateRight( a, 13 ) ^ scriptRotateRight( a, 22 );
		const uint32_t nMajority = ( a & b ) ^ ( a & c ) ^ ( b & c );
		const uint32_t nStep2 = nS0 + nMajority;
		h = g;
		g = f;
		f = e;
		e = d + nStep1;
		d = c;
		c = b;
		b = a;
		a = nStep1 + nStep2;
	}
	state.szHash[0] += a;
	state.szHash[1] += b;
	state.szHash[2] += c;
	state.szHash[3] += d;
	state.szHash[4] += e;
	state.szHash[5] += f;
	state.szHash[6] += g;
	state.szHash[7] += h;
}

static void scriptSHA256Update( sirScriptSHA256State_t &state, const uint8_t *pData, uint64_t nBytes ) noexcept
{
	state.nBytes += nBytes;
	while ( nBytes != 0 ) {
		const uint32_t nSpace = 64 - state.nBuffered;
		const uint32_t nCopy = nBytes < nSpace ? ( uint32_t )nBytes : nSpace;
		memcpy( state.szBuffer + state.nBuffered, pData, nCopy );
		state.nBuffered += nCopy;
		pData += nCopy;
		nBytes -= nCopy;
		if ( state.nBuffered == 64 ) {
			scriptSHA256Transform( state, state.szBuffer );
			state.nBuffered = 0;
		}
	}
}

void SIRScriptSHA256::digest( const void *pData, uint64_t nBytes, uint8_t szDigest[32] ) noexcept
{
	if ( szDigest == nullptr || ( nBytes != 0 && pData == nullptr ) ) {
		return;
	}
	sirScriptSHA256State_t state = {};
	state.szHash[0] = 0x6a09e667;
	state.szHash[1] = 0xbb67ae85;
	state.szHash[2] = 0x3c6ef372;
	state.szHash[3] = 0xa54ff53a;
	state.szHash[4] = 0x510e527f;
	state.szHash[5] = 0x9b05688c;
	state.szHash[6] = 0x1f83d9ab;
	state.szHash[7] = 0x5be0cd19;

	scriptSHA256Update( state, ( const uint8_t * )pData, nBytes );
	const uint64_t nBitLength = state.nBytes * 8;
	const uint8_t nMarker = 0x80;
	scriptSHA256Update( state, &nMarker, 1 );
	const uint8_t nZero = 0;
	while ( state.nBuffered != 56 ) {
		scriptSHA256Update( state, &nZero, 1 );
	}
	uint8_t szLength[8] = {};
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		szLength[7 - iByte] = ( uint8_t )( nBitLength >> ( iByte * 8 ) );
	}
	scriptSHA256Update( state, szLength, 8 );
	for ( uint32_t iWord = 0; iWord < 8; ++iWord ) {
		scriptWriteBE32( szDigest + iWord * 4, state.szHash[iWord] );
	}
}
