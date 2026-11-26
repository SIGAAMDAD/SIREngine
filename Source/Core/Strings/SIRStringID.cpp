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

#include <Core/Strings/SIRStringID.h>
#include <Core/Strings/SIRStringOps.h>
#include <Core/Strings/SIRStringSIMD.h>

SIRContainerResult SIRStringID::tryAssign( sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	m_nHash = hashBytes( view.pBytes, view.nBytes );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRStringID::tryAppend( sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	m_nHash = hashBytes( view.pBytes, view.nBytes, m_nHash );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRStringID::tryHashBatch( const sirStringView_t *pInputs, uint32_t nCount, uint64_t *pHashes ) noexcept
{
	if ( nCount == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( pInputs == nullptr || pHashes == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nCount > UINT32_MAX / sizeof( sirStringView_t ) || nCount > UINT32_MAX / sizeof( uint64_t ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const sirStringView_t output{ reinterpret_cast<const char *>( pHashes ), static_cast<uint32_t>( nCount * sizeof( uint64_t ) ) };
	if ( SIRStringOps::overlaps( output, { reinterpret_cast<const char *>( pInputs ), static_cast<uint32_t>( nCount * sizeof( sirStringView_t ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( !SIRStringOps::isValid( pInputs[i] ) || SIRStringOps::overlaps( output, pInputs[i] ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	uint32_t iInput = 0;
	for ( ; nCount - iInput >= 4; iInput += 4 ) {
		uint64_t nHashes[4]{ 14695981039346656037ULL, 14695981039346656037ULL, 14695981039346656037ULL, 14695981039346656037ULL };
		uint32_t nMinimum = pInputs[iInput].nBytes;
		for ( uint32_t iLane = 1; iLane < 4; ++iLane ) {
			if ( pInputs[iInput + iLane].nBytes < nMinimum ) {
				nMinimum = pInputs[iInput + iLane].nBytes;
			}
		}
		SIRStringSIMD::hashFour( pInputs + iInput, nMinimum, nHashes );
		for ( uint32_t iLane = 0; iLane < 4; ++iLane ) {
			const sirStringView_t input = pInputs[iInput + iLane];
			pHashes[iInput + iLane] = input.nBytes == nMinimum ? nHashes[iLane] : hashBytes( input.pBytes + nMinimum, input.nBytes - nMinimum, nHashes[iLane] );
		}
	}
	for ( ; iInput < nCount; ++iInput ) {
		pHashes[iInput] = hashBytes( pInputs[iInput].pBytes, pInputs[iInput].nBytes );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}
