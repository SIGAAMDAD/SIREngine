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

#include <Core/Strings/SIRStringCodec12.h>
#include <Core/Strings/SIRStringOps.h>

uint32_t SIRStringCodec12::getSymbolCount( void ) const noexcept
{
	return m_nCount;
}

uint64_t SIRStringCodec12::score( const sirStringSymbol12_t& symbol ) noexcept
{
	return static_cast<uint64_t>( symbol.nFrequency ) * ( symbol.nFrequency >= 2 && symbol.nBytes > 0 ? symbol.nBytes - 1 : 0 );
}

void SIRStringCodec12::addSymbol( const sirStringSymbol12_t& symbol ) noexcept
{
	const uint8_t nFirst = static_cast<uint8_t>( symbol.szBytes[0] );
	m_Symbols[m_nCount] = symbol;
	m_iNext[m_nCount] = m_iHeads[nFirst];
	m_iHeads[nFirst] = static_cast<uint16_t>( ++m_nCount );
}

SIRContainerResult SIRStringCodec12::tryInitialize( const sirStringSymbol12_t *pSymbols, uint32_t nCount ) noexcept
{
	if ( nCount > MAX_SYMBOLS || ( nCount != 0 && pSymbols == nullptr ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( pSymbols[i].nBytes < 2 || pSymbols[i].nBytes > 8 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	m_nCount = 0;
	memset( m_iHeads, 0, sizeof( m_iHeads ) );
	for ( uint32_t i = 0; i < nCount; ++i ) {
		addSymbol( pSymbols[i] );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRStringCodec12::tryTrain( const sirStringView_t *pCorpus, uint32_t nCount, sirStringSymbol12_t *pCandidates, uint32_t nCapacity ) noexcept
{
	if ( pCandidates == nullptr || nCapacity == 0 || ( nCount != 0 && pCorpus == nullptr ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( !SIRStringOps::isValid( pCorpus[i] ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t i = 0; i < nCapacity; ++i ) {
		pCandidates[i] = {};
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirStringView_t view = pCorpus[i];
		for ( uint32_t j = 0; j < view.nBytes; ++j ) {
			uint64_t nHash = 14695981039346656037ULL;
			for ( uint32_t k = 0; k < 8 && k < view.nBytes - j; ++k ) {
				nHash = ( nHash ^ static_cast<uint8_t>( view.pBytes[j + k] ) ) * 1099511628211ULL;
				if ( k == 0 ) {
					continue;
				}
				uint32_t iSlot = static_cast<uint32_t>( nHash % nCapacity );
				uint32_t nProbes = 0;
				while ( pCandidates[iSlot].nBytes != 0 ) {
					const sirStringSymbol12_t& candidate = pCandidates[iSlot];
					if ( candidate.nBytes == k + 1 && memcmp( candidate.szBytes, view.pBytes + j, k + 1 ) == 0 ) {
						break;
					}
					if ( ++nProbes == nCapacity ) {
						return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
					}
					iSlot = iSlot + 1 == nCapacity ? 0 : iSlot + 1;
				}
				sirStringSymbol12_t& candidate = pCandidates[iSlot];
				if ( candidate.nBytes == 0 ) {
					candidate.nBytes = k + 1;
					memcpy( candidate.szBytes, view.pBytes + j, k + 1 );
				}
				if ( candidate.nFrequency != UINT32_MAX ) {
					++candidate.nFrequency;
				}
			}
		}
	}
	m_nCount = 0;
	memset( m_iHeads, 0, sizeof( m_iHeads ) );
	// Offline deterministic ranking. Select only repeated symbols; workspace is disposable.
	for ( uint32_t i = 0; i < MAX_SYMBOLS && i < nCapacity; ++i ) {
		uint32_t iBest = i;
		for ( uint32_t j = i + 1; j < nCapacity; ++j ) {
			if ( score( pCandidates[j] ) > score( pCandidates[iBest] ) ) {
				iBest = j;
			}
		}
		if ( pCandidates[iBest].nFrequency < 2 ) {
			break;
		}
		const sirStringSymbol12_t selected = pCandidates[iBest];
		pCandidates[iBest] = pCandidates[i];
		pCandidates[i] = selected;
		addSymbol( selected );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult
SIRStringCodec12::tryEncode( sirStringView_t input, const sirStringCodec12Workspace_t& workspace, sirStringCodec12Output_t& output ) const noexcept
{
	return encodeImpl( input, workspace, output, false );
}

SIRContainerResult
SIRStringCodec12::tryEncodeAdaptive( sirStringView_t input, const sirStringCodec12Workspace_t& workspace, sirStringCodec12Output_t& output ) const noexcept
{
	return encodeImpl( input, workspace, output, true );
}

SIRContainerResult SIRStringCodec12::encodeImpl( sirStringView_t input,
                                                 const sirStringCodec12Workspace_t& workspace,
                                                 sirStringCodec12Output_t& output,
                                                 bool bAdaptive ) const noexcept
{
	if ( !SIRStringOps::isValid( input ) || workspace.pCosts == nullptr || workspace.pCodes == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( workspace.nCapacity <= input.nBytes ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	workspace.pCosts[input.nBytes] = 0;
	for ( uint32_t i = input.nBytes; i != 0; ) {
		--i;
		const uint8_t nLiteral = static_cast<uint8_t>( input.pBytes[i] );
		uint32_t nBest = 1 + workspace.pCosts[i + 1];
		uint16_t nCode = nLiteral;
		for ( uint16_t iLink = m_iHeads[nLiteral]; iLink != 0; iLink = m_iNext[iLink - 1] ) {
			const sirStringSymbol12_t& symbol = m_Symbols[iLink - 1];
			if ( symbol.nBytes <= input.nBytes - i && workspace.pCosts[i + symbol.nBytes] + 1 < nBest &&
			     memcmp( input.pBytes + i, symbol.szBytes, symbol.nBytes ) == 0 ) {
				nBest = workspace.pCosts[i + symbol.nBytes] + 1;
				nCode = static_cast<uint16_t>( 255 + iLink );
			}
		}
		workspace.pCosts[i] = nBest;
		workspace.pCodes[i] = nCode;
	}
	const uint32_t nCodes = workspace.pCosts[0];
	const uint64_t nBytes = ( static_cast<uint64_t>( nCodes ) * 12 + 7 ) / 8;
	if ( bAdaptive && nBytes >= input.nBytes ) {
		if ( input.nBytes > output.nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		if ( input.nBytes != 0 && output.pBytes == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( input.nBytes != 0 ) {
			memmove( output.pBytes, input.pBytes, input.nBytes );
		}
		output.nWritten = input.nBytes;
		output.nCodes = 0;
		output.bRaw = true;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( nBytes > output.nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( nBytes != 0 && output.pBytes == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( SIRStringOps::overlaps( input, { reinterpret_cast<const char *>( output.pBytes ), static_cast<uint32_t>( nBytes ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iInput = 0;
	uint32_t iOutput = 0;
	while ( iInput < input.nBytes ) {
		const uint16_t nFirst = workspace.pCodes[iInput];
		iInput += symbolLength( nFirst );
		output.pBytes[iOutput++] = static_cast<uint8_t>( nFirst );
		if ( iInput == input.nBytes ) {
			output.pBytes[iOutput++] = static_cast<uint8_t>( nFirst >> 8 );
			break;
		}
		const uint16_t nSecond = workspace.pCodes[iInput];
		iInput += symbolLength( nSecond );
		output.pBytes[iOutput++] = static_cast<uint8_t>( ( nFirst >> 8 ) | ( nSecond << 4 ) );
		output.pBytes[iOutput++] = static_cast<uint8_t>( nSecond >> 4 );
	}
	output.nWritten = iOutput;
	output.nCodes = nCodes;
	output.bRaw = false;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

uint16_t SIRStringCodec12::readCode( const char *pBytes, uint32_t iCode ) noexcept
{
	const size_t iByte = static_cast<size_t>( iCode / 2 ) * 3 + iCode % 2;
	const uint16_t nWord =
	    static_cast<uint16_t>( static_cast<uint8_t>( pBytes[iByte] ) | ( static_cast<uint16_t>( static_cast<uint8_t>( pBytes[iByte + 1] ) ) << 8 ) );
	return static_cast<uint16_t>( ( nWord >> ( ( iCode % 2 ) * 4 ) ) & 4095 );
}

uint32_t SIRStringCodec12::symbolLength( uint16_t nCode ) const noexcept
{
	return nCode < 256 ? 1 : m_Symbols[nCode - 256].nBytes;
}

SIRContainerResult SIRStringCodec12::tryDecode( sirStringView_t encoded, uint32_t nCodes, char *pOutput, uint32_t nCapacity ) const noexcept
{
	if ( !SIRStringOps::isValid( encoded ) || pOutput == nullptr || ( static_cast<uint64_t>( nCodes ) * 12 + 7 ) / 8 != encoded.nBytes ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nCodes % 2 != 0 && ( static_cast<uint8_t>( encoded.pBytes[encoded.nBytes - 1] ) & 0xf0 ) != 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint64_t nBytes = 0;
	for ( uint32_t i = 0; i < nCodes; ++i ) {
		const uint16_t nCode = readCode( encoded.pBytes, i );
		if ( nCode >= 256 + m_nCount ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		nBytes += symbolLength( nCode );
	}
	if ( nBytes >= nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( SIRStringOps::overlaps( encoded, { pOutput, static_cast<uint32_t>( nBytes + 1 ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iOutput = 0;
	for ( uint32_t i = 0; i < nCodes; ++i ) {
		const uint16_t nCode = readCode( encoded.pBytes, i );
		if ( nCode < 256 ) {
			pOutput[iOutput++] = static_cast<char>( nCode );
		} else {
			const sirStringSymbol12_t& symbol = m_Symbols[nCode - 256];
			memcpy( pOutput + iOutput, symbol.szBytes, symbol.nBytes );
			iOutput += symbol.nBytes;
		}
	}
	pOutput[iOutput] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRStringCodec12::tryDecodeAdaptive( const sirStringCodec12Output_t& input, char *pOutput, uint32_t nCapacity ) const noexcept
{
	if ( input.nWritten > input.nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const sirStringView_t view{ reinterpret_cast<const char *>( input.pBytes ), input.nWritten };
	if ( input.bRaw ) {
		if ( input.nCodes != 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		return SIRStringOps::copyTerminated( view, pOutput, nCapacity );
	}
	return tryDecode( view, input.nCodes, pOutput, nCapacity );
}
