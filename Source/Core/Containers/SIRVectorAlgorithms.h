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

#ifndef __SIR_VECTORALGORITHMS_H__
#define __SIR_VECTORALGORITHMS_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorAppendWriter.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <stdint.h>

class SIRVectorAlgorithms
{
public:
	// Scalar reference compaction. Order is stable, masks contain 0 or nonzero.
	// Candidate SIMD backends must be bit-for-bit equivalent to this result.
	template <typename T>
	static SIRContainerResult tryAppendFiltered( const SIRReadOnlyVectorView<T>& rInput, const uint8_t *pMask, SIRVector<T> *pDestination )
	{
		if ( pDestination == nullptr || ( rInput.getSize() != 0 && pMask == nullptr ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uintptr_t nInput = reinterpret_cast<uintptr_t>( rInput.data() );
		const uintptr_t nOutput = reinterpret_cast<uintptr_t>( pDestination->data() );
		const uint64_t nOutputBytes = static_cast<uint64_t>( pDestination->getSize() * sizeof( T ) );
		if ( nInput >= nOutput && static_cast<uint64_t>( nInput - nOutput ) < nOutputBytes ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t nSelected = 0;
		for ( uint32_t i = 0; i < rInput.getSize(); ++i ) {
			nSelected += pMask[i] != 0;
		}
		SIRVectorAppendWriter<T> writer( *pDestination );
		SIRContainerResult eResult = writer.tryBeginAppend( nSelected );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t i = 0; i < rInput.getSize(); ++i ) {
			if ( pMask[i] != 0 ) {
				eResult = writer.tryEmplaceNext( rInput[i] );
				if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
					return eResult;
				}
			}
		}
		return writer.commit();
	}

	// Deterministic shard merge. Scheduling and shard ownership belong to Core Jobs.
	template <typename T>
	static SIRContainerResult tryMergeShards( const SIRReadOnlyVectorView<T> *pShards, uint32_t nShards, SIRVector<T> *pDestination )
	{
		if ( pDestination == nullptr || ( nShards != 0 && pShards == nullptr ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nCombined = 0;
		for ( uint32_t i = 0; i < nShards; ++i ) {
			const uintptr_t nInput = reinterpret_cast<uintptr_t>( pShards[i].data() );
			const uintptr_t nOutput = reinterpret_cast<uintptr_t>( pDestination->data() );
			if ( nInput >= nOutput &&
			     static_cast<uint64_t>( nInput - nOutput ) < static_cast<uint64_t>( pDestination->getSize() * sizeof( T ) ) ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
			nCombined += pShards[i].getSize();
			if ( nCombined > UINT32_MAX ) {
				return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
			}
		}
		SIRVectorAppendWriter<T> writer( *pDestination );
		SIRContainerResult eResult = writer.tryBeginAppend( static_cast<uint32_t>( nCombined ) );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iShard = 0; iShard < nShards; ++iShard ) {
			for ( uint32_t i = 0; i < pShards[iShard].getSize(); ++i ) {
				eResult = writer.tryEmplaceNext( pShards[iShard][i] );
				if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
					return eResult;
				}
			}
		}
		return writer.commit();
	}
};

#endif // __SIR_VECTORALGORITHMS_H__
