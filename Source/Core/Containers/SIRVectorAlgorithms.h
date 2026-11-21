/* SIREngine MPLv2 - range algorithms for borrowed and owning vectors. */
#ifndef __SIRENGINE_SIRVECTORALGORITHMS_H__
#define __SIRENGINE_SIRVECTORALGORITHMS_H__
#include <Core/SIRPCH.h>
#include <Core/Containers/SIRVectorAppendWriter.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <stdint.h>

class SIRVectorAlgorithms
{
public:
	// Scalar reference compaction. Order is stable, masks contain 0 or nonzero.
	// Candidate SIMD backends must be bit-for-bit equivalent to this result.
	template <typename T>
	static sirContainerResult_t tryAppendFiltered( const SIRReadOnlyVectorView<T>& rInput, const uint8_t *pMask,
	                                               SIRVector<T> *pDestination )
	{
		if ( pDestination == nullptr || ( rInput.getSize() != 0 && pMask == nullptr ) ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uintptr_t nInput = ( uintptr_t )rInput.data();
		const uintptr_t nOutput = ( uintptr_t )pDestination->data();
		const uint64_t nOutputBytes = ( uint64_t )pDestination->getSize() * sizeof( T );
		if ( nInput >= nOutput && ( uint64_t )( nInput - nOutput ) < nOutputBytes ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t nSelected = 0;
		for ( uint32_t i = 0; i < rInput.getSize(); ++i ) {
			nSelected += pMask[i] != 0;
		}
		SIRVectorAppendWriter<T> writer( *pDestination );
		sirContainerResult_t eResult = writer.tryBeginAppend( nSelected );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t i = 0; i < rInput.getSize(); ++i ) {
			if ( pMask[i] != 0 ) {
				eResult = writer.tryEmplaceNext( rInput[i] );
				if ( eResult != SIR_CONTAINER_OK ) {
					return eResult;
				}
			}
		}
		return writer.commit();
	}

	// Deterministic shard merge. Scheduling and shard ownership belong to Core Jobs.
	template <typename T>
	static sirContainerResult_t tryMergeShards( const SIRReadOnlyVectorView<T> *pShards, uint32_t nShards, SIRVector<T> *pDestination )
	{
		if ( pDestination == nullptr || ( nShards != 0 && pShards == nullptr ) ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nCombined = 0;
		for ( uint32_t i = 0; i < nShards; ++i ) {
			const uintptr_t nInput = ( uintptr_t )pShards[i].data();
			const uintptr_t nOutput = ( uintptr_t )pDestination->data();
			if ( nInput >= nOutput && ( uint64_t )( nInput - nOutput ) < ( uint64_t )pDestination->getSize() * sizeof( T ) ) {
				return SIR_CONTAINER_INVALID_ARGUMENT;
			}
			nCombined += pShards[i].getSize();
			if ( nCombined > UINT32_MAX ) {
				return SIR_CONTAINER_SIZE_OVERFLOW;
			}
		}
		SIRVectorAppendWriter<T> writer( *pDestination );
		sirContainerResult_t eResult = writer.tryBeginAppend( ( uint32_t )nCombined );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iShard = 0; iShard < nShards; ++iShard ) {
			for ( uint32_t i = 0; i < pShards[iShard].getSize(); ++i ) {
				eResult = writer.tryEmplaceNext( pShards[iShard][i] );
				if ( eResult != SIR_CONTAINER_OK ) {
					return eResult;
				}
			}
		}
		return writer.commit();
	}
};
#endif // __SIRENGINE_SIRVECTORALGORITHMS_H__
