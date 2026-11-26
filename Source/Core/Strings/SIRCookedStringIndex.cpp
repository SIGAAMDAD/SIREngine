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

#include <Core/Strings/SIRCookedStringIndex.h>
#include <Core/Strings/SIRStringOps.h>

uint64_t SIRCookedStringIndex::mix( uint64_t nValue ) noexcept
{
	// SplitMix64 finalizer; stable FNV content identity remains unchanged.
	nValue = ( nValue ^ ( nValue >> 30 ) ) * UINT64_C( 0xbf58476d1ce4e5b9 );
	nValue = ( nValue ^ ( nValue >> 27 ) ) * UINT64_C( 0x94d049bb133111eb );
	return nValue ^ ( nValue >> 31 );
}

uint32_t SIRCookedStringIndex::bucket( sirStringView_t key, uint32_t nBuckets, uint64_t nSeed ) noexcept
{
	return static_cast<uint32_t>( mix( SIRStringID::hashBytes( key.pBytes, key.nBytes ) ^ nSeed ) % nBuckets );
}

uint32_t SIRCookedStringIndex::slot( sirStringView_t key, uint32_t nKeys, uint32_t nDisplacement, uint64_t nSeed ) noexcept
{
	const uint64_t nHash = SIRStringID::hashBytes( key.pBytes, key.nBytes, UINT64_C( 14695981039346656037 ) ^ nSeed );
	return static_cast<uint32_t>( mix( nHash + UINT64_C( 0x9e3779b97f4a7c15 ) * nDisplacement ) % nKeys );
}

SIRContainerResult SIRCookedStringIndex::tryBuild( const sirStringView_t *pKeys,
                                                   const sirCookedStringIndexStorage_t& storage,
                                                   const sirCookedStringIndexScratch_t& scratch,
                                                   uint64_t nSeed ) noexcept
{
	if ( storage.nKeys == 0 || storage.nKeys >= DIRECT || storage.nBuckets == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( pKeys == nullptr || storage.pDisplacements == nullptr || storage.pOrdinals == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( scratch.pHeads == nullptr || scratch.pCounts == nullptr || scratch.pNext == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( storage.nKeys > UINT32_MAX / sizeof( sirStringView_t ) || storage.nBuckets > UINT32_MAX / sizeof( uint32_t ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	for ( uint32_t iKey = 0; iKey < storage.nKeys; ++iKey ) {
		if ( !SIRStringOps::isValid( pKeys[iKey] ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	memset( storage.pDisplacements, 0, static_cast<size_t>( storage.nBuckets ) * sizeof( uint32_t ) );
	memset( storage.pOrdinals, 0, static_cast<size_t>( storage.nKeys ) * sizeof( uint32_t ) );
	memset( scratch.pHeads, 0, static_cast<size_t>( storage.nBuckets ) * sizeof( uint32_t ) );
	memset( scratch.pCounts, 0, static_cast<size_t>( storage.nBuckets ) * sizeof( uint32_t ) );
	for ( uint32_t iKey = 0; iKey < storage.nKeys; ++iKey ) {
		const uint32_t iBucket = bucket( pKeys[iKey], storage.nBuckets, nSeed );
		scratch.pNext[iKey] = scratch.pHeads[iBucket];
		scratch.pHeads[iBucket] = iKey + 1;
		++scratch.pCounts[iBucket];
	}
	if ( !tryPlace( pKeys, storage, scratch, nSeed ) ) {
		memset( storage.pDisplacements, 0, static_cast<size_t>( storage.nBuckets ) * sizeof( uint32_t ) );
		memset( storage.pOrdinals, 0, static_cast<size_t>( storage.nKeys ) * sizeof( uint32_t ) );
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

bool SIRCookedStringIndex::tryPlace( const sirStringView_t *pKeys,
                                     const sirCookedStringIndexStorage_t& storage,
                                     const sirCookedStringIndexScratch_t& scratch,
                                     uint64_t nSeed ) noexcept
{
	uint32_t nLargest = 0;
	for ( uint32_t iBucket = 0; iBucket < storage.nBuckets; ++iBucket ) {
		if ( scratch.pCounts[iBucket] > nLargest ) {
			nLargest = scratch.pCounts[iBucket];
		}
	}
	// Larger buckets first. A bounded displacement search permits offline retries.
	for ( uint32_t nSize = nLargest; nSize > 1; --nSize ) {
		for ( uint32_t iBucket = 0; iBucket < storage.nBuckets; ++iBucket ) {
			if ( scratch.pCounts[iBucket] != nSize ) {
				continue;
			}
			bool bPlaced = false;
			for ( uint32_t nDisplacement = 1; nDisplacement <= MAX_DISPLACEMENT; ++nDisplacement ) {
				uint32_t iLink = scratch.pHeads[iBucket];
				while ( iLink != 0 ) {
					const uint32_t iSlot = slot( pKeys[iLink - 1], storage.nKeys, nDisplacement, nSeed );
					if ( storage.pOrdinals[iSlot] != 0 ) {
						break;
					}
					storage.pOrdinals[iSlot] = iLink;
					iLink = scratch.pNext[iLink - 1];
				}
				if ( iLink == 0 ) {
					storage.pDisplacements[iBucket] = nDisplacement;
					bPlaced = true;
					break;
				}
				for ( uint32_t iUndo = scratch.pHeads[iBucket]; iUndo != iLink; iUndo = scratch.pNext[iUndo - 1] ) {
					storage.pOrdinals[slot( pKeys[iUndo - 1], storage.nKeys, nDisplacement, nSeed )] = 0;
				}
			}
			if ( !bPlaced ) {
				return false;
			}
		}
	}
	uint32_t iFree = 0;
	for ( uint32_t iBucket = 0; iBucket < storage.nBuckets; ++iBucket ) {
		if ( scratch.pCounts[iBucket] != 1 ) {
			continue;
		}
		while ( iFree < storage.nKeys && storage.pOrdinals[iFree] != 0 ) {
			++iFree;
		}
		if ( iFree == storage.nKeys ) {
			return false;
		}
		storage.pOrdinals[iFree] = scratch.pHeads[iBucket];
		storage.pDisplacements[iBucket] = DIRECT | iFree;
	}
	return true;
}

uint32_t SIRCookedStringIndex::find( sirStringView_t key, const sirStringView_t *pKeys, const sirCookedStringIndexStorage_t& storage, uint64_t nSeed ) noexcept
{
	if ( !SIRStringOps::isValid( key ) || pKeys == nullptr || storage.nKeys == 0 ) {
		return INVALID_ORDINAL;
	}
	if ( storage.nBuckets == 0 || storage.pOrdinals == nullptr || storage.pDisplacements == nullptr ) {
		return INVALID_ORDINAL;
	}
	const uint32_t nDisplacement = storage.pDisplacements[bucket( key, storage.nBuckets, nSeed )];
	if ( nDisplacement == 0 ) {
		return INVALID_ORDINAL;
	}
	const uint32_t iSlot = ( nDisplacement & DIRECT ) != 0 ? nDisplacement & ~DIRECT : slot( key, storage.nKeys, nDisplacement, nSeed );
	if ( iSlot >= storage.nKeys ) {
		return INVALID_ORDINAL;
	}
	const uint32_t iLink = storage.pOrdinals[iSlot];
	if ( iLink == 0 || iLink > storage.nKeys ) {
		return INVALID_ORDINAL;
	}
	const sirStringView_t candidate = pKeys[iLink - 1];
	if ( candidate.nBytes != key.nBytes || !SIRStringOps::isValid( candidate ) ) {
		return INVALID_ORDINAL;
	}
	if ( key.nBytes != 0 && memcmp( key.pBytes, candidate.pBytes, key.nBytes ) != 0 ) {
		return INVALID_ORDINAL;
	}
	return iLink - 1;
}

uint32_t SIRCookedStringIndex::read32( const char *pBytes ) noexcept
{
	uint32_t nValue = 0;
	for ( uint32_t i = 0; i < 4; ++i ) {
		nValue |= static_cast<uint32_t>( static_cast<uint8_t>( pBytes[i] ) ) << ( i * 8 );
	}
	return nValue;
}

void SIRCookedStringIndex::write32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pBytes[i] = static_cast<uint8_t>( nValue >> ( i * 8 ) );
	}
}

uint64_t SIRCookedStringIndex::getWireBytes( uint32_t nKeys, uint32_t nBuckets ) noexcept
{
	return HEADER_BYTES + ( static_cast<uint64_t>( nKeys ) + nBuckets ) * 4;
}

SIRContainerResult
SIRCookedStringIndex::trySerialize( const sirCookedStringIndexStorage_t& storage, uint64_t nSeed, uint8_t *pBytes, uint32_t nCapacity ) noexcept
{
	if ( storage.nKeys == 0 || storage.nKeys >= DIRECT || storage.nBuckets == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( storage.pDisplacements == nullptr || storage.pOrdinals == nullptr || pBytes == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( getWireBytes( storage.nKeys, storage.nBuckets ) > nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	for ( uint32_t i = 0; i < storage.nKeys; ++i ) {
		if ( storage.pOrdinals[i] == 0 || storage.pOrdinals[i] > storage.nKeys ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t i = 0; i < storage.nBuckets; ++i ) {
		const uint32_t nDisplacement = storage.pDisplacements[i];
		if ( ( nDisplacement & DIRECT ) != 0 ? ( nDisplacement & ~DIRECT ) >= storage.nKeys : nDisplacement > MAX_DISPLACEMENT ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	memcpy( pBytes, "SMP1", 4 );
	write32( pBytes + 4, storage.nKeys );
	write32( pBytes + 8, storage.nBuckets );
	write32( pBytes + 12, static_cast<uint32_t>( nSeed ) );
	write32( pBytes + 16, static_cast<uint32_t>( nSeed >> 32 ) );
	uint8_t *pArrays = pBytes + HEADER_BYTES;
	for ( uint32_t i = 0; i < storage.nBuckets; ++i ) {
		write32( pArrays + i * 4, storage.pDisplacements[i] );
	}
	pArrays += storage.nBuckets * 4;
	for ( uint32_t i = 0; i < storage.nKeys; ++i ) {
		write32( pArrays + i * 4, storage.pOrdinals[i] );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

bool SIRCookedStringIndex::isWireValid( sirStringView_t wire, uint32_t nKeys ) noexcept
{
	if ( !SIRStringOps::isValid( wire ) || wire.nBytes < HEADER_BYTES || nKeys == 0 ) {
		return false;
	}
	if ( memcmp( wire.pBytes, "SMP1", 4 ) != 0 || read32( wire.pBytes + 4 ) != nKeys || nKeys >= DIRECT ) {
		return false;
	}
	const uint32_t nBuckets = read32( wire.pBytes + 8 );
	return nBuckets != 0 && getWireBytes( nKeys, nBuckets ) == wire.nBytes;
}

uint32_t SIRCookedStringIndex::findCandidate( sirStringView_t key, sirStringView_t wire ) noexcept
{
	if ( !SIRStringOps::isValid( key ) || !SIRStringOps::isValid( wire ) || wire.nBytes < HEADER_BYTES ) {
		return INVALID_ORDINAL;
	}
	const uint32_t nKeys = read32( wire.pBytes + 4 );
	if ( !isWireValid( wire, nKeys ) ) {
		return INVALID_ORDINAL;
	}
	const uint32_t nBuckets = read32( wire.pBytes + 8 );
	const uint64_t nSeed = static_cast<uint64_t>( read32( wire.pBytes + 12 ) ) | ( static_cast<uint64_t>( read32( wire.pBytes + 16 ) ) << 32 );
	const char *pArrays = wire.pBytes + HEADER_BYTES;
	const uint32_t nDisplacement = read32( pArrays + bucket( key, nBuckets, nSeed ) * 4 );
	if ( nDisplacement == 0 ) {
		return INVALID_ORDINAL;
	}
	const uint32_t iSlot = ( nDisplacement & DIRECT ) != 0 ? nDisplacement & ~DIRECT : slot( key, nKeys, nDisplacement, nSeed );
	if ( iSlot >= nKeys ) {
		return INVALID_ORDINAL;
	}
	const uint32_t iLink = read32( pArrays + nBuckets * 4 + iSlot * 4 );
	return iLink == 0 || iLink > nKeys ? INVALID_ORDINAL : iLink - 1;
}
