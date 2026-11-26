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

#include <Core/Strings/SIRCompressedString.h>
#include <Core/Strings/SIRStringOps.h>

SIRContainerResult SIRStringDictionary::tryInitialize( const sirStringView_t *pSymbols, uint32_t nCount ) noexcept
{
	if ( nCount > MAX_SYMBOLS || ( pSymbols == nullptr && nCount != 0 ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	// Preflight the entire table, including self-aliasing from an earlier dictionary.
	const sirStringView_t storage{ reinterpret_cast<const char *>( this ), static_cast<uint32_t>( sizeof( *this ) ) };
	for ( uint32_t iSymbol = 0; iSymbol < nCount; ++iSymbol ) {
		const sirStringView_t symbol = pSymbols[iSymbol];
		if ( !SIRStringOps::isValid( symbol ) || symbol.nBytes == 0 || symbol.nBytes > 8 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( SIRStringOps::overlaps( storage, symbol ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	memset( m_iHeads, 0, sizeof( m_iHeads ) );
	memset( m_nLengths, 0, sizeof( m_nLengths ) );
	m_nCount = nCount;
	for ( uint32_t iSymbol = 0; iSymbol < nCount; ++iSymbol ) {
		const sirStringView_t symbol = pSymbols[iSymbol];
		memset( m_szSymbols[iSymbol], 0, 8 );
		memcpy( m_szSymbols[iSymbol], symbol.pBytes, symbol.nBytes );
		m_nLengths[iSymbol] = static_cast<uint8_t>( symbol.nBytes );
		const uint8_t nFirst = static_cast<uint8_t>( symbol.pBytes[0] );
		m_iNext[iSymbol] = m_iHeads[nFirst];
		m_iHeads[nFirst] = static_cast<uint16_t>( iSymbol + 1 );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

uint32_t SIRStringDictionary::getMatch( sirStringView_t input ) const noexcept
{
	if ( input.pBytes == nullptr || input.nBytes == 0 ) {
		return 255;
	}
	uint32_t iBest = 255;
	uint32_t nBestLength = 0;
	uint16_t iLink = m_iHeads[static_cast<uint8_t>( input.pBytes[0] )];
	while ( iLink != 0 ) {
		const uint32_t iSymbol = iLink - 1;
		const uint32_t nLength = m_nLengths[iSymbol];
		if ( nLength > nBestLength && nLength <= input.nBytes ) {
			if ( memcmp( input.pBytes, m_szSymbols[iSymbol], nLength ) == 0 ) {
				iBest = iSymbol;
				nBestLength = nLength;
			}
		}
		iLink = m_iNext[iSymbol];
	}
	return iBest;
}

sirStringView_t SIRStringDictionary::getSymbol( uint32_t iCode ) const noexcept
{
	if ( iCode >= m_nCount ) {
		return { nullptr, 0 };
	}
	return { m_szSymbols[iCode], m_nLengths[iCode] };
}

uint32_t SIRCompressedString::getDecodedBytes( void ) const noexcept
{
	return m_nDecodedBytes;
}

sirStringView_t SIRCompressedString::getEncodedView( void ) const noexcept
{
	return m_Encoded;
}

SIRContainerResult SIRCompressedString::measureEncoded( const SIRStringDictionary& dictionary, sirStringView_t input, uint32_t& nBytes ) noexcept
{
	if ( !SIRStringOps::isValid( input ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint64_t nTotal = 0;
	uint32_t iInput = 0;
	while ( iInput < input.nBytes ) {
		const uint32_t iCode = dictionary.getMatch( { input.pBytes + iInput, input.nBytes - iInput } );
		if ( iCode == 255 ) {
			++iInput;
			nTotal += 2;
		} else {
			iInput += dictionary.getSymbol( iCode ).nBytes;
			++nTotal;
		}
	}
	if ( nTotal > UINT32_MAX ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	nBytes = static_cast<uint32_t>( nTotal );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCompressedString::measureDecoded( const SIRStringDictionary& dictionary, sirStringView_t encoded, uint32_t& nBytes ) noexcept
{
	if ( !SIRStringOps::isValid( encoded ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint64_t nTotal = 0;
	uint32_t iInput = 0;
	while ( iInput < encoded.nBytes ) {
		const uint8_t nCode = static_cast<uint8_t>( encoded.pBytes[iInput++] );
		if ( nCode == 255 ) {
			if ( iInput == encoded.nBytes ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
			++iInput;
			++nTotal;
		} else {
			const sirStringView_t symbol = dictionary.getSymbol( nCode );
			if ( symbol.nBytes == 0 ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
			nTotal += symbol.nBytes;
		}
	}
	if ( nTotal > UINT32_MAX ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	nBytes = static_cast<uint32_t>( nTotal );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCompressedString::tryEncode( const SIRStringDictionary& dictionary, sirStringView_t input, char *pOutput, uint32_t nCapacity ) noexcept
{
	uint32_t nEncoded = 0;
	const SIRContainerResult eResult = measureEncoded( dictionary, input, nEncoded );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	if ( nEncoded > nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	const sirStringView_t output{ pOutput, nEncoded };
	if ( !SIRStringOps::isValid( output ) || SIRStringOps::overlaps( input, output ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const sirStringView_t dictionaryStorage{ reinterpret_cast<const char *>( &dictionary ), static_cast<uint32_t>( sizeof( dictionary ) ) };
	if ( SIRStringOps::overlaps( dictionaryStorage, output ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iInput = 0;
	uint32_t iOutput = 0;
	while ( iInput < input.nBytes ) {
		const uint32_t iCode = dictionary.getMatch( { input.pBytes + iInput, input.nBytes - iInput } );
		const uint8_t nCode = static_cast<uint8_t>( iCode );
		memcpy( pOutput + iOutput++, &nCode, 1 );
		if ( iCode == 255 ) {
			pOutput[iOutput++] = input.pBytes[iInput++];
		} else {
			iInput += dictionary.getSymbol( iCode ).nBytes;
		}
	}
	m_bRaw = false;
	m_pDictionary = &dictionary;
	m_Encoded = output;
	m_nDecodedBytes = input.nBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCompressedString::tryBind( const SIRStringDictionary& dictionary, sirStringView_t encoded, uint32_t nDecodedBytes ) noexcept
{
	uint32_t nMeasured = 0;
	const SIRContainerResult eResult = measureDecoded( dictionary, encoded, nMeasured );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	if ( nMeasured != nDecodedBytes ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	m_bRaw = false;
	m_pDictionary = &dictionary;
	m_Encoded = encoded;
	m_nDecodedBytes = nDecodedBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCompressedString::tryDecode( char *pszOutput, uint32_t nCapacity ) const noexcept
{
	if ( pszOutput == nullptr || ( m_pDictionary == nullptr && !m_bRaw ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nCapacity <= m_nDecodedBytes ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( m_bRaw ) {
		return SIRStringOps::copyTerminated( m_Encoded, pszOutput, nCapacity );
	}
	const sirStringView_t output{ pszOutput, m_nDecodedBytes + 1 };
	const sirStringView_t dictionaryStorage{ reinterpret_cast<const char *>( m_pDictionary ), static_cast<uint32_t>( sizeof( SIRStringDictionary ) ) };
	if ( SIRStringOps::overlaps( output, m_Encoded ) || SIRStringOps::overlaps( output, dictionaryStorage ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t nMeasured = 0;
	const SIRContainerResult eResult = measureDecoded( *m_pDictionary, m_Encoded, nMeasured );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	if ( nMeasured != m_nDecodedBytes ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iInput = 0;
	uint32_t iOutput = 0;
	while ( iInput < m_Encoded.nBytes ) {
		const uint8_t nCode = static_cast<uint8_t>( m_Encoded.pBytes[iInput++] );
		if ( nCode == 255 ) {
			pszOutput[iOutput++] = m_Encoded.pBytes[iInput++];
		} else {
			const sirStringView_t symbol = m_pDictionary->getSymbol( nCode );
			if ( m_nDecodedBytes - iOutput >= 8 ) {
				memcpy( pszOutput + iOutput, symbol.pBytes, 8 );
			} else {
				memcpy( pszOutput + iOutput, symbol.pBytes, symbol.nBytes );
			}
			iOutput += symbol.nBytes;
		}
	}
	pszOutput[iOutput] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRCompressedString::SIRCompressedString() noexcept
    : m_bRaw( false )
{
}

bool SIRCompressedString::isRaw( void ) const noexcept
{
	return m_bRaw;
}

SIRContainerResult SIRCompressedString::tryBindRaw( sirStringView_t raw ) noexcept
{
	if ( !SIRStringOps::isValid( raw ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	m_bRaw = true;
	m_pDictionary = nullptr;
	m_Encoded = raw;
	m_nDecodedBytes = raw.nBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

uint32_t SIRStringDictionary::getOptimalMatch( sirStringView_t input, const uint32_t *pCosts ) const noexcept
{
	if ( input.pBytes == nullptr || input.nBytes == 0 || pCosts == nullptr ) {
		return 255;
	}
	uint32_t nBestCost = 2 + pCosts[1];
	uint32_t iBest = 255;
	uint16_t iLink = m_iHeads[static_cast<uint8_t>( input.pBytes[0] )];
	while ( iLink != 0 ) {
		const uint32_t iSymbol = iLink - 1;
		const uint32_t nLength = m_nLengths[iSymbol];
		if ( nLength <= input.nBytes && 1 + pCosts[nLength] < nBestCost ) {
			if ( memcmp( input.pBytes, m_szSymbols[iSymbol], nLength ) == 0 ) {
				nBestCost = 1 + pCosts[nLength];
				iBest = iSymbol;
			}
		}
		iLink = m_iNext[iSymbol];
	}
	return iBest;
}

SIRContainerResult SIRCompressedString::tryEncodeOptimal( const SIRStringDictionary& dictionary,
                                                          sirStringView_t input,
                                                          const sirStringEncodeOutput_t& output,
                                                          const sirStringEncodeWorkspace_t& workspace ) noexcept
{
	if ( !SIRStringOps::isValid( input ) || workspace.pCosts == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( input.nBytes >= UINT32_MAX / sizeof( uint32_t ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	if ( workspace.nCount <= input.nBytes ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	const sirStringView_t scratch{ reinterpret_cast<const char *>( workspace.pCosts ), ( input.nBytes + 1 ) * static_cast<uint32_t>( sizeof( uint32_t ) ) };
	const sirStringView_t destination{ output.pBytes, output.nCapacity };
	const sirStringView_t table{ reinterpret_cast<const char *>( &dictionary ), static_cast<uint32_t>( sizeof( dictionary ) ) };
	if ( !SIRStringOps::isValid( destination ) || SIRStringOps::overlaps( scratch, input ) || SIRStringOps::overlaps( scratch, destination ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( SIRStringOps::overlaps( scratch, table ) || SIRStringOps::overlaps( destination, table ) || SIRStringOps::overlaps( destination, input ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const sirStringView_t self{ reinterpret_cast<const char *>( this ), static_cast<uint32_t>( sizeof( *this ) ) };
	if ( SIRStringOps::overlaps( scratch, self ) || SIRStringOps::overlaps( destination, self ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t *pCosts = workspace.pCosts;
	pCosts[input.nBytes] = 0;
	for ( uint32_t i = input.nBytes; i != 0; ) {
		--i;
		const uint32_t iCode = dictionary.getOptimalMatch( { input.pBytes + i, input.nBytes - i }, pCosts + i );
		pCosts[i] = iCode == 255 ? 2 + pCosts[i + 1] : 1 + pCosts[i + dictionary.getSymbol( iCode ).nBytes];
	}
	// Raw fallback is selected only by this new API; legacy codec remains wire-compatible.
	if ( pCosts[0] >= input.nBytes ) {
		if ( input.nBytes > output.nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		if ( input.nBytes != 0 ) {
			memcpy( output.pBytes, input.pBytes, input.nBytes );
		}
		return tryBindRaw( { output.pBytes, input.nBytes } );
	}
	if ( pCosts[0] > output.nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	uint32_t iInput = 0;
	uint32_t iOutput = 0;
	while ( iInput < input.nBytes ) {
		const uint32_t iCode = dictionary.getOptimalMatch( { input.pBytes + iInput, input.nBytes - iInput }, pCosts + iInput );
		output.pBytes[iOutput++] = static_cast<char>( iCode );
		if ( iCode == 255 ) {
			output.pBytes[iOutput++] = input.pBytes[iInput++];
		} else {
			iInput += dictionary.getSymbol( iCode ).nBytes;
		}
	}
	m_bRaw = false;
	m_pDictionary = &dictionary;
	m_Encoded = { output.pBytes, iOutput };
	m_nDecodedBytes = input.nBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}
