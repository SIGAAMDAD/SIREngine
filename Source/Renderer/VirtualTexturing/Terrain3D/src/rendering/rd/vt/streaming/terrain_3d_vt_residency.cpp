// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "core/utility/terrain_3d_hash_bytes.h"
#include "core/utility/terrain_3d_checked_math.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_residency.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"

#include <limits.h>
#include <math.h>
#include <string.h>

void t3d_vt_default_residency_policy( t3dVTResidencyPolicy_t& rPolicy )
{
	memset( &rPolicy, 0, sizeof( rPolicy ) );
	rPolicy.nHotHoldFrames[T3D_VT_RESIDENCY_TERRAIN] = 24u;
	rPolicy.nWarmHoldFrames[T3D_VT_RESIDENCY_TERRAIN] = 96u;
	rPolicy.nHotHoldFrames[T3D_VT_RESIDENCY_CLUTTER] = 4u;
	rPolicy.nWarmHoldFrames[T3D_VT_RESIDENCY_CLUTTER] = 12u;
	rPolicy.nHotHoldFrames[T3D_VT_RESIDENCY_GROUND_COVER] = 8u;
	rPolicy.nWarmHoldFrames[T3D_VT_RESIDENCY_GROUND_COVER] = 24u;
	rPolicy.nHotHoldFrames[T3D_VT_RESIDENCY_OCCLUDER] = 16u;
	rPolicy.nWarmHoldFrames[T3D_VT_RESIDENCY_OCCLUDER] = 64u;
	rPolicy.nHotHoldFrames[T3D_VT_RESIDENCY_HLOD] = 16u;
	rPolicy.nWarmHoldFrames[T3D_VT_RESIDENCY_HLOD] = 64u;
	// Keep 25% of the cache unreserved for whichever visible producer needs it.
	// The protected floors only affect eviction; categories with no resident pages
	// consume no capacity and the remaining cache stays fully borrowable.
	rPolicy.nProtectedResidentPercent[T3D_VT_RESIDENCY_TERRAIN] = 40u;
	rPolicy.nProtectedResidentPercent[T3D_VT_RESIDENCY_CLUTTER] = 5u;
	rPolicy.nProtectedResidentPercent[T3D_VT_RESIDENCY_GROUND_COVER] = 10u;
	rPolicy.nProtectedResidentPercent[T3D_VT_RESIDENCY_OCCLUDER] = 5u;
	rPolicy.nProtectedResidentPercent[T3D_VT_RESIDENCY_HLOD] = 15u;
	rPolicy.nThrashWindowFrames = 120u;
	rPolicy.nRetireDelayFrames = 2u;
}

bool t3d_vt_validate_residency_policy( const t3dVTResidencyPolicy_t& pPolicy )
{
	if ( pPolicy.nThrashWindowFrames == 0u || pPolicy.nRetireDelayFrames == 0u ) {
		return false;
	}
	uint32_t nProtectedPercentTotal = 0u;
	for ( uint32_t i = 0u; i < T3D_VT_RESIDENCY_CATEGORY_COUNT; ++i ) {
		if ( pPolicy.nHotHoldFrames[i] == 0u || pPolicy.nWarmHoldFrames[i] < pPolicy.nHotHoldFrames[i] ||
		     pPolicy.nProtectedResidentPercent[i] > 100u ) {
			return false;
		}
		nProtectedPercentTotal += pPolicy.nProtectedResidentPercent[i];
	}
	// Always leave a shared pool so one category can burst without deadlocking
	// the clock victim search against fixed reservations.
	return nProtectedPercentTotal < 100u;
}

uint16_t t3d_vt_residency_category_reference_debug( const t3dVTDatabaseView_t& pDatabase, const uint32_t pVirtualPage )
{
	if ( pDatabase.pHeader == NULL || pDatabase.pPages == NULL || pDatabase.pAllocations == NULL ||
	     pVirtualPage >= pDatabase.pHeader->nPageCount ) {
		return T3D_VT_RESIDENCY_TERRAIN;
	}
	const t3dVTPageFileRecord_t& Page = pDatabase.pPages[pVirtualPage];
	if ( Page.nAllocationId >= pDatabase.pHeader->nAllocationCount ) {
		return T3D_VT_RESIDENCY_TERRAIN;
	}
	const uint16_t nCategory = pDatabase.pAllocations[Page.nAllocationId].nReserved0;
	return nCategory < T3D_VT_RESIDENCY_CATEGORY_COUNT ? nCategory : ( uint16_t )T3D_VT_RESIDENCY_TERRAIN;
}

uint16_t t3d_vt_residency_temperature_reference_debug( const t3dVTResidencyState_t& pState, const t3dVTPhysicalSlot_t& pSlot,
                                                       const uint32_t pFrameId )
{
	if ( pState.pDatabase == NULL || pSlot.m_nVirtualPage == T3D_VT_INVALID_PAGE ) {
		return T3D_VT_RESIDENCY_COLD;
	}
	const uint16_t nCategory = t3d_vt_residency_category_reference_debug( *pState.pDatabase, pSlot.m_nVirtualPage );
	const uint32_t nLastTouch =
	    pSlot.m_nLastRequestedFrame > pSlot.m_nLastVisibleFrame ? pSlot.m_nLastRequestedFrame : pSlot.m_nLastVisibleFrame;
	const uint32_t nAge = pFrameId - nLastTouch;
	if ( nAge <= pState.Policy.nHotHoldFrames[nCategory] ) {
		return T3D_VT_RESIDENCY_HOT;
	}
	if ( nAge <= pState.Policy.nWarmHoldFrames[nCategory] ) {
		return T3D_VT_RESIDENCY_WARM;
	}
	return T3D_VT_RESIDENCY_COLD;
}

bool t3d_vt_retirement_ready_reference_debug( const t3dVTResidencyState_t& pState, const t3dVTRetiredPageSet_t& pRetired,
                                              const uint32_t pFrameId )
{
	( void )pState;
	if ( pRetired.nPhysicalSlot == T3D_VT_INVALID_SLOT ) {
		return false;
	}
	return ( int32_t )( pFrameId - pRetired.nReleaseFrame ) >= 0;
}

static const t3dVTPageFileRecord_t *t3d_vt_legacy_find_page( const t3dVTDatabaseView_t& pDatabase, uint32_t pVirtualPage )
{
	if ( pVirtualPage >= pDatabase.pHeader->nPageCount ) {
		return NULL;
	}
	const t3dVTPageFileRecord_t& Page = pDatabase.pPages[pVirtualPage];
	return Page.nVirtualPage == pVirtualPage ? &Page : NULL;
}

uint64_t t3d_vt_compute_pack_bytes( const t3dVTPackSource_t& pSource )
{
	if ( pSource.pAllocations == NULL || pSource.nAllocationCount == 0u || pSource.pPages == NULL || pSource.nPageCount == 0u ||
	     pSource.pPayloads == NULL || pSource.nPayloadCount == 0u || pSource.pPayloadData == NULL || pSource.nPayloadDataBytes == 0u ) {
		return 0u;
	}

	uint64_t nOffset = sizeof( t3dVTFileHeader_t );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )pSource.nAllocationCount * sizeof( t3dVTAllocationRecord_t ), 16u );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )pSource.nPageCount * sizeof( t3dVTPageFileRecord_t ), 16u );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )pSource.nPayloadCount * sizeof( t3dVTPayloadRecord_t ), 16u );
	if ( nOffset > UINT64_MAX - pSource.nPayloadDataBytes ) {
		return 0u;
	}
	return nOffset + pSource.nPayloadDataBytes;
}

bool t3d_vt_write_pack_cold( const t3dVTPackSource_t& pSource, void *pDestination, uint64_t pDestinationBytes )
{
	const uint64_t nRequired = t3d_vt_compute_pack_bytes( pSource );
	if ( nRequired == 0u || pDestination == NULL || pDestinationBytes < nRequired || nRequired > SIZE_MAX ) {
		return false;
	}
	uint8_t *const pOut = ( uint8_t * )pDestination;
	memset( pOut, 0, ( size_t )nRequired );
	t3dVTFileHeader_t Header{};
	Header.nMagic = T3D_VT_LEGACY_FILE_MAGIC;
	Header.nVersion = T3D_VT_LEGACY_FILE_VERSION;
	Header.nHeaderBytes = sizeof( t3dVTFileHeader_t );
	Header.nAllocationCount = pSource.nAllocationCount;
	Header.nPageCount = pSource.nPageCount;
	Header.nPayloadCount = pSource.nPayloadCount;
	Header.nChannelCount = T3D_VT_CHANNEL_COUNT;
	Header.nInteriorTexels = T3D_VT_PAGE_INTERIOR;
	Header.nGutterTexels = T3D_VT_PAGE_GUTTER;
	Header.nPhysicalTexels = T3D_VT_PHYSICAL_TILE;
	uint64_t nOffset = sizeof( t3dVTFileHeader_t );
	Header.nAllocationOffset = nOffset;
	memcpy( pOut + nOffset, pSource.pAllocations, ( size_t )pSource.nAllocationCount * sizeof( t3dVTAllocationRecord_t ) );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )pSource.nAllocationCount * sizeof( t3dVTAllocationRecord_t ), 16u );
	Header.nPageOffset = nOffset;
	memcpy( pOut + nOffset, pSource.pPages, ( size_t )pSource.nPageCount * sizeof( t3dVTPageFileRecord_t ) );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )pSource.nPageCount * sizeof( t3dVTPageFileRecord_t ), 16u );
	Header.nPayloadDirectoryOffset = nOffset;
	memcpy( pOut + nOffset, pSource.pPayloads, ( size_t )pSource.nPayloadCount * sizeof( t3dVTPayloadRecord_t ) );
	nOffset = t3dAlignUpOrZero<uint64_t>( nOffset + ( uint64_t )pSource.nPayloadCount * sizeof( t3dVTPayloadRecord_t ), 16u );
	Header.nPayloadDataOffset = nOffset;
	Header.nPayloadDataBytes = pSource.nPayloadDataBytes;
	memcpy( pOut + nOffset, pSource.pPayloadData, ( size_t )pSource.nPayloadDataBytes );
	Header.nContentSignature = t3dHashPublicationBytes( pOut + sizeof( Header ), nRequired - sizeof( Header ) );
	memcpy( pOut, &Header, sizeof( Header ) );
	return true;
}

bool t3d_vt_open_pack_cold( const void *pBytes, uint64_t pByteCount, t3dVTLegacyDecodeStorage_t& rStorage, t3dVTDatabaseView_t& rView )
{
	rView = t3dVTDatabaseView_t{};
	if ( pBytes == NULL || pByteCount < sizeof( t3dVTFileHeader_t ) ) {
		return false;
	}
	const uint8_t *const pData = ( const uint8_t * )pBytes;
	// Header bytes are serialized data, not a constructed header object.
	t3dVTFileHeader_t Header{};
	memcpy( &Header, pData, sizeof( Header ) );
	if ( Header.nMagic != T3D_VT_LEGACY_FILE_MAGIC || Header.nVersion != T3D_VT_LEGACY_FILE_VERSION ||
	     Header.nHeaderBytes != sizeof( Header ) || Header.nChannelCount != T3D_VT_CHANNEL_COUNT ||
	     Header.nInteriorTexels != T3D_VT_PAGE_INTERIOR || Header.nGutterTexels != T3D_VT_PAGE_GUTTER ||
	     Header.nPhysicalTexels != T3D_VT_PHYSICAL_TILE || Header.nAllocationCount == 0u || Header.nPageCount == 0u ||
	     Header.nPayloadCount == 0u || Header.nPayloadDataBytes == 0u ) {
		return false;
	}
	if ( !t3dByteRangeFits<uint64_t>( Header.nAllocationOffset, ( uint64_t )Header.nAllocationCount * sizeof( t3dVTAllocationRecord_t ),
	                                  pByteCount ) ||
	     !t3dByteRangeFits<uint64_t>( Header.nPageOffset, ( uint64_t )Header.nPageCount * sizeof( t3dVTPageFileRecord_t ), pByteCount ) ||
	     !t3dByteRangeFits<uint64_t>( Header.nPayloadDirectoryOffset, ( uint64_t )Header.nPayloadCount * sizeof( t3dVTPayloadRecord_t ),
	                                  pByteCount ) ||
	     !t3dByteRangeFits<uint64_t>( Header.nPayloadDataOffset, Header.nPayloadDataBytes, pByteCount ) ||
	     t3dHashPublicationBytes( pData + sizeof( Header ), pByteCount - sizeof( Header ) ) != Header.nContentSignature ) {
		return false;
	}
	// All allocation sizes are constrained by the validated byte ranges above.
	rStorage.Header = Header;
	rStorage.Allocations.resize( Header.nAllocationCount );
	rStorage.Pages.resize( Header.nPageCount );
	rStorage.Payloads.resize( Header.nPayloadCount );
	memcpy( rStorage.Allocations.data(), pData + Header.nAllocationOffset,
	        ( size_t )Header.nAllocationCount * sizeof( t3dVTAllocationRecord_t ) );
	memcpy( rStorage.Pages.data(), pData + Header.nPageOffset, ( size_t )Header.nPageCount * sizeof( t3dVTPageFileRecord_t ) );
	memcpy( rStorage.Payloads.data(), pData + Header.nPayloadDirectoryOffset,
	        ( size_t )Header.nPayloadCount * sizeof( t3dVTPayloadRecord_t ) );
	for ( uint32_t i = 0u; i < Header.nPageCount; ++i ) {
		const t3dVTPageFileRecord_t& Page = rStorage.Pages[i];
		if ( Page.nVirtualPage != i || Page.nChannel >= T3D_VT_CHANNEL_COUNT || Page.nPayloadId >= Header.nPayloadCount ||
		     ( Page.nParentPage != T3D_VT_INVALID_PAGE && Page.nParentPage >= Header.nPageCount ) ) {
			return false;
		}
	}
	for ( uint32_t i = 0u; i < Header.nPayloadCount; ++i ) {
		const t3dVTPayloadRecord_t& Payload = rStorage.Payloads[i];
		if ( !t3dByteRangeFits<uint64_t>( Payload.nOffset, Payload.nCompressedBytes, Header.nPayloadDataBytes ) ) {
			return false;
		}
	}
	rView.pHeader = &rStorage.Header;
	rView.pAllocations = rStorage.Allocations.data();
	rView.pPages = rStorage.Pages.data();
	rView.pPayloads = rStorage.Payloads.data();
	rView.pPayloadData = pData + Header.nPayloadDataOffset;
	rView.nFileBytes = pByteCount;
	return true;
}

static bool t3d_vt_build_runtime_tables_cold( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyBuffers_t& pBuffers,
                                              uint32_t pDatabaseGeneration, t3dVTResidencyState_t& rState )
{
	if ( pDatabase.pHeader->nAllocationCount == 0u || pDatabase.pAllocations == NULL || pBuffers.pAllocations == NULL ||
	     pBuffers.nAllocationCapacity < pDatabase.pHeader->nAllocationCount || pBuffers.pTailMetadata == NULL ) {
		return false;
	}

	if ( pDatabase.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK ) {
		if ( pDatabase.pRuntimeAllocations == NULL || pDatabase.pRuntimeTailMetadata == NULL || pDatabase.nRuntimeTailCount == 0u ||
		     pBuffers.nTailMetadataCapacity < pDatabase.nRuntimeTailCount ) {
			return false;
		}
		for ( uint32_t iAllocation = 0u; iAllocation < pDatabase.pHeader->nAllocationCount; ++iAllocation ) {
			const t3dVTAllocation_t& Source = pDatabase.pRuntimeAllocations[iAllocation];
			if ( Source.m_nPageCount == 0u ) {
				if ( Source.m_nMipCount != 0u || Source.m_nMip0WidthPages != 0u || Source.m_nMip0HeightPages != 0u ||
				     Source.m_nTailIndex != T3D_VT_INVALID_TAIL ) {
					return false;
				}
				pBuffers.pAllocations[iAllocation] = Source;
				continue;
			}
			if ( Source.m_nMipCount == 0u || Source.m_nMipCount > T3D_VT_MAX_MIPS || Source.m_nMip0WidthPages == 0u ||
			     Source.m_nMip0HeightPages == 0u || Source.m_nBaseVirtualPage >= pDatabase.pHeader->nPageCount ||
			     Source.m_nPageCount > pDatabase.pHeader->nPageCount - Source.m_nBaseVirtualPage ||
			     Source.m_nTailIndex >= pDatabase.nRuntimeTailCount ) {
				return false;
			}
			pBuffers.pAllocations[iAllocation] = Source;
		}
		memcpy( pBuffers.pTailMetadata, pDatabase.pRuntimeTailMetadata,
		        ( size_t )pDatabase.nRuntimeTailCount * sizeof( t3dVTTailMetadata_t ) );
		rState.nAllocationCount = pDatabase.pHeader->nAllocationCount;
		rState.nTailCount = pDatabase.nRuntimeTailCount;
		return true;
	}

	if ( pBuffers.nTailMetadataCapacity < pDatabase.pHeader->nAllocationCount ) {
		return false;
	}
	for ( uint32_t iAllocation = 0u; iAllocation < pDatabase.pHeader->nAllocationCount; ++iAllocation ) {
		const t3dVTAllocationRecord_t& Legacy = pDatabase.pAllocations[iAllocation];
		if ( Legacy.nPageCount == 0u || Legacy.nMipCount == 0u || Legacy.nMipCount > T3D_VT_MAX_MIPS || Legacy.nMip0WidthPages == 0u ||
		     Legacy.nMip0HeightPages == 0u || Legacy.nMip0WidthPages > 0xffffu || Legacy.nMip0HeightPages > 0xffffu ||
		     Legacy.nBaseVirtualPage >= pDatabase.pHeader->nPageCount ||
		     Legacy.nPageCount > pDatabase.pHeader->nPageCount - Legacy.nBaseVirtualPage ) {
			return false;
		}

		const t3dVTPageFileRecord_t& FirstPage = pDatabase.pPages[Legacy.nBaseVirtualPage];
		const uint32_t nGeneration =
		    FirstPage.nGeneration != 0u ? FirstPage.nGeneration : ( pDatabaseGeneration != 0u ? pDatabaseGeneration : 1u );
		t3dVTAllocation_t& Allocation = pBuffers.pAllocations[iAllocation];
		memset( &Allocation, 0, sizeof( Allocation ) );
		Allocation.m_nBaseVirtualPage = Legacy.nBaseVirtualPage;
		Allocation.m_nPageCount = Legacy.nPageCount;
		Allocation.m_nMip0WidthPages = ( uint16_t )Legacy.nMip0WidthPages;
		Allocation.m_nMip0HeightPages = ( uint16_t )Legacy.nMip0HeightPages;
		Allocation.m_nMipCount = Legacy.nMipCount;
		Allocation.m_nFlags = T3D_VT_ALLOCATION_MATERIAL_UV | T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE;
		Allocation.m_nTailIndex = iAllocation;
		Allocation.m_nGeneration = nGeneration;
		Allocation.m_nContentId = iAllocation;
		Allocation.m_nReserved =
		    Legacy.nReserved0 < T3D_VT_RESIDENCY_CATEGORY_COUNT ? Legacy.nReserved0 : ( uint32_t )( T3D_VT_RESIDENCY_TERRAIN );

		t3dVTTailMetadata_t& Tail = pBuffers.pTailMetadata[iAllocation];
		memset( &Tail, 0, sizeof( Tail ) );
		Tail.m_nAtlasX = ( iAllocation % T3D_VT_TAIL_TILES_PER_ROW ) * T3D_VT_TAIL_TILE;
		Tail.m_nAtlasY = ( iAllocation / T3D_VT_TAIL_TILES_PER_ROW ) * T3D_VT_TAIL_TILE;
		Tail.m_nWidthTexels = T3D_VT_TAIL_TILE;
		Tail.m_nHeightTexels = T3D_VT_TAIL_TILE;
		Tail.m_nFirstMip = Legacy.nMipCount - 1u;
		Tail.m_nMipCount = 1u;
		Tail.m_nGeneration = nGeneration;
		Tail.m_nChannelMask = Legacy.nChannel < T3D_VT_CHANNEL_COUNT ? 1u << Legacy.nChannel : T3D_VT_CHANNEL_MASK_ALL;
		Tail.m_nContentId = iAllocation;
	}

	rState.nAllocationCount = pDatabase.pHeader->nAllocationCount;
	rState.nTailCount = pDatabase.pHeader->nAllocationCount;
	return true;
}

static bool t3d_vt_has_required_page_storage( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyBuffers_t& pBuffers )
{
	return pDatabase.pHeader != NULL && pDatabase.pPages != NULL && pDatabase.pHeader->nPageCount != 0u && pBuffers.pPageTable != NULL &&
	       pBuffers.nPageTableCapacity >= pDatabase.pHeader->nPageCount;
}

static bool t3d_vt_has_required_physical_storage( const t3dVTResidencyBuffers_t& pBuffers, const uint32_t pRetirementDelayFrames )
{
	return pBuffers.pPhysicalPageSets != NULL && pBuffers.nPhysicalPageSetCapacity >= T3D_VT_MIN_PHYSICAL_PAGE_SETS &&
	       pBuffers.pFreeSlots != NULL && pBuffers.pRetiredPageSets != NULL && pBuffers.pLastEvictedFrame != NULL &&
	       pRetirementDelayFrames != 0u;
}

bool t3d_vt_initialize_residency_cold( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyBuffers_t& pBuffers,
                                       uint32_t pDatabaseGeneration, uint32_t pRetirementDelayFrames, t3dVTResidencyState_t& rState )
{
	memset( &rState, 0, sizeof( rState ) );
	if ( !t3d_vt_has_required_page_storage( pDatabase, pBuffers ) ) {
		return false;
	}
	if ( !t3d_vt_has_required_physical_storage( pBuffers, pRetirementDelayFrames ) ) {
		return false;
	}

	rState.pDatabase = &pDatabase;
	rState.Buffers = pBuffers;
	rState.nDatabaseGeneration = pDatabaseGeneration;
	t3d_vt_default_residency_policy( rState.Policy );
	rState.Policy.nRetireDelayFrames = pRetirementDelayFrames;
	rState.nRetirementDelayFrames = pRetirementDelayFrames;
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
		rState.nNeutralRootPage[iChannel] = T3D_VT_INVALID_PAGE;
	}
	if ( !t3d_vt_build_runtime_tables_cold( pDatabase, pBuffers, pDatabaseGeneration, rState ) ) {
		return false;
	}

	for ( uint32_t iPage = 0u; iPage < pDatabase.pHeader->nPageCount; ++iPage ) {
		const t3dVTPageFileRecord_t& Page = pDatabase.pPages[iPage];
		if ( Page.nVirtualPage != iPage || Page.nChannel >= T3D_VT_CHANNEL_COUNT || Page.nAllocationId >= rState.nAllocationCount ) {
			return false;
		}
		const t3dVTAllocation_t& Allocation = pBuffers.pAllocations[Page.nAllocationId];
		const uint32_t nAllocationEnd = Allocation.m_nBaseVirtualPage + Allocation.m_nPageCount;
		if ( iPage < Allocation.m_nBaseVirtualPage || iPage >= nAllocationEnd ) {
			return false;
		}
		if ( Page.nParentPage != T3D_VT_INVALID_PAGE &&
		     ( Page.nParentPage < Allocation.m_nBaseVirtualPage || Page.nParentPage >= nAllocationEnd || Page.nParentPage == iPage ) ) {
			return false;
		}

		t3dVTPageTableEntry_t& Entry = pBuffers.pPageTable[iPage];
		Entry.nPhysicalSlot = T3D_VT_INVALID_SLOT;
		Entry.nParentPage = Page.nParentPage;
		Entry.nGeneration = pDatabase.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK ? Page.nGeneration : Allocation.m_nGeneration;
		Entry.nResolvedMip = Page.nMip;
		Entry.nFlags = T3D_VT_PAGE_VALID;
		if ( Page.nParentPage == T3D_VT_INVALID_PAGE ) {
			Entry.nFlags |= T3D_VT_PAGE_ROOT | T3D_VT_PAGE_TAIL_FALLBACK;
		}
		if ( ( Page.nFlags & T3D_VT_PAGE_NEUTRAL ) != 0u ) {
			rState.nNeutralRootPage[Page.nChannel] = iPage;
			Entry.nFlags |= T3D_VT_PAGE_NEUTRAL;
		}
		pBuffers.pLastEvictedFrame[iPage] = T3D_VT_INVALID_PAGE;
	}

	for ( uint32_t iSlot = 0u; iSlot < pBuffers.nPhysicalPageSetCapacity; ++iSlot ) {
		t3dVTPhysicalSlot_t& Slot = pBuffers.pPhysicalPageSets[iSlot];
		memset( &Slot, 0, sizeof( Slot ) );
		Slot.m_nVirtualPage = T3D_VT_INVALID_PAGE;
		Slot.m_nPayloadId = T3D_VT_INVALID_PAGE;
		pBuffers.pFreeSlots[iSlot] = pBuffers.nPhysicalPageSetCapacity - iSlot - 1u;
		memset( &pBuffers.pRetiredPageSets[iSlot], 0, sizeof( pBuffers.pRetiredPageSets[iSlot] ) );
		pBuffers.pRetiredPageSets[iSlot].nPhysicalSlot = T3D_VT_INVALID_SLOT;
		pBuffers.pRetiredPageSets[iSlot].nVirtualPage = T3D_VT_INVALID_PAGE;
	}
	rState.Stats.nFreeSlotCount = pBuffers.nPhysicalPageSetCapacity;
	rState.bInitialized = true;
	return true;
}

static bool t3d_vt_remove_free_slot_warm( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot )
{
	for ( uint32_t i = 0u; i < rState.Stats.nFreeSlotCount; ++i ) {
		if ( rState.Buffers.pFreeSlots[i] != pPhysicalSlot ) {
			continue;
		}
		--rState.Stats.nFreeSlotCount;
		rState.Buffers.pFreeSlots[i] = rState.Buffers.pFreeSlots[rState.Stats.nFreeSlotCount];
		return true;
	}
	return false;
}

static bool t3d_vt_push_free_slot_hot( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot )
{
	if ( rState.Stats.nFreeSlotCount >= rState.Buffers.nPhysicalPageSetCapacity ) {
		return false;
	}
	rState.Buffers.pFreeSlots[rState.Stats.nFreeSlotCount++] = pPhysicalSlot;
	return true;
}

static uint32_t t3d_vt_pop_free_slot_hot( t3dVTResidencyState_t& rState )
{
	if ( rState.Stats.nFreeSlotCount == 0u ) {
		return T3D_VT_INVALID_SLOT;
	}
	return rState.Buffers.pFreeSlots[--rState.Stats.nFreeSlotCount];
}

bool t3d_vt_commit_neutral_backing_warm( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot )
{
	if ( !rState.bInitialized || rState.bNeutralPublished || pPhysicalSlot >= rState.Buffers.nPhysicalPageSetCapacity ) {
		return false;
	}
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		if ( rState.nNeutralRootPage[c] == T3D_VT_INVALID_PAGE || rState.nNeutralRootPage[c] >= rState.pDatabase->pHeader->nPageCount ) {
			return false;
		}
	}
	t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[pPhysicalSlot];
	if ( Slot.m_nVirtualPage != T3D_VT_INVALID_PAGE || !t3d_vt_remove_free_slot_warm( rState, pPhysicalSlot ) ) {
		return false;
	}
	const uint32_t nPrimaryRoot = rState.nNeutralRootPage[T3D_VT_CHANNEL_ALBEDO];
	Slot.m_nVirtualPage = nPrimaryRoot;
	Slot.m_nGeneration = rState.Buffers.pPageTable[nPrimaryRoot].nGeneration;
	Slot.m_nFlags = T3D_VT_SLOT_PINNED | T3D_VT_SLOT_REFERENCED | T3D_VT_SLOT_HOT;
	Slot.m_nClockAge = 0xffffu;
	Slot.m_nPinCount = 1u;
	Slot.m_nPayloadId = T3D_VT_INVALID_PAGE;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[rState.nNeutralRootPage[c]];
		Entry.nPhysicalSlot = pPhysicalSlot;
		Entry.nFlags |= T3D_VT_PAGE_RESIDENT | T3D_VT_PAGE_PINNED | T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_ROOT;
	}
	++rState.Stats.nResidentCount;
	++rState.nPageTableGeneration;
	return true;
}

bool t3d_vt_finalize_neutral_publication_warm( t3dVTResidencyState_t& rState )
{
	if ( !rState.bInitialized || rState.Stats.nFreeSlotCount == 0u || rState.Stats.nResidentCount == 0u ) {
		return false;
	}
	uint32_t nSlot = T3D_VT_INVALID_SLOT;
	for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
		const uint32_t nRoot = rState.nNeutralRootPage[c];
		if ( nRoot == T3D_VT_INVALID_PAGE || nRoot >= rState.pDatabase->pHeader->nPageCount ) {
			return false;
		}
		const t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[nRoot];
		if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || Entry.nPhysicalSlot == T3D_VT_INVALID_SLOT ) {
			return false;
		}
		if ( nSlot == T3D_VT_INVALID_SLOT ) {
			nSlot = Entry.nPhysicalSlot;
		} else if ( Entry.nPhysicalSlot != nSlot ) {
			return false;
		}
	}
	rState.bNeutralPublished = true;
	return true;
}

bool t3d_vt_finalize_package_publication_warm( t3dVTResidencyState_t& rState )
{
	if ( !rState.bInitialized || rState.pDatabase == NULL || rState.pDatabase->nRuntimeKind != T3D_VT_RUNTIME_DATABASE_PACK ||
	     rState.pDatabase->pRuntimePayloads == NULL || rState.nAllocationCount == 0u || rState.nTailCount == 0u ) {
		return false;
	}
	rState.bNeutralPublished = true;
	return true;
}

uint32_t t3d_vt_resolve_page_hot( t3dVTResidencyState_t& rState, uint32_t pVirtualPage, uint16_t& rResolvedMip )
{
	rResolvedMip = 0u;
	if ( !rState.bNeutralPublished || pVirtualPage >= rState.pDatabase->pHeader->nPageCount ) {
		return T3D_VT_INVALID_SLOT;
	}
	uint32_t nPage = pVirtualPage;
	for ( uint32_t i = 0u; i < T3D_VT_ANCESTOR_PROBE_CAP; ++i ) {
		t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[nPage];
		if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) != 0u && Entry.nPhysicalSlot != T3D_VT_INVALID_SLOT ) {
			rResolvedMip = Entry.nResolvedMip;
			if ( nPage != pVirtualPage ) {
				++rState.Stats.nFallbackCount;
			}
			return Entry.nPhysicalSlot;
		}
		if ( Entry.nParentPage == T3D_VT_INVALID_PAGE || Entry.nParentPage >= rState.pDatabase->pHeader->nPageCount ) {
			break;
		}
		nPage = Entry.nParentPage;
	}
	const t3dVTPageFileRecord_t *pPage = t3d_vt_legacy_find_page( *rState.pDatabase, pVirtualPage );
	if ( pPage == NULL ) {
		return T3D_VT_INVALID_SLOT;
	}
	const uint32_t nRoot = rState.nNeutralRootPage[pPage->nChannel];
	if ( nRoot == T3D_VT_INVALID_PAGE || nRoot >= rState.pDatabase->pHeader->nPageCount ) {
		return T3D_VT_INVALID_SLOT;
	}
	++rState.Stats.nFallbackCount;
	rResolvedMip = rState.Buffers.pPageTable[nRoot].nResolvedMip;
	return rState.Buffers.pPageTable[nRoot].nPhysicalSlot;
}

bool t3d_vt_allocation_lookup_hot( const t3dVTResidencyState_t& pState, uint32_t pAllocationId, const t3dVTAllocation_t *& rAllocation )
{
	rAllocation = NULL;
	if ( !pState.bInitialized || pState.Buffers.pAllocations == NULL || pAllocationId >= pState.nAllocationCount ) {
		return false;
	}
	const t3dVTAllocation_t& Allocation = pState.Buffers.pAllocations[pAllocationId];
	if ( Allocation.m_nPageCount == 0u || Allocation.m_nMipCount == 0u || Allocation.m_nMipCount > T3D_VT_MAX_MIPS ||
	     Allocation.m_nBaseVirtualPage >= pState.Buffers.nPageTableCapacity ||
	     Allocation.m_nPageCount > pState.Buffers.nPageTableCapacity - Allocation.m_nBaseVirtualPage ) {
		return false;
	}
	rAllocation = &Allocation;
	return true;
}

bool t3d_vt_compute_page( const t3dVTAllocation_t& pAllocation, uint32_t pMip, uint32_t pPageX, uint32_t pPageY, uint32_t& rVirtualPage )
{
	return t3d_vt_allocation_page_id( pAllocation, pMip, pPageX, pPageY, rVirtualPage );
}

bool t3d_vt_resolve_allocation_page_hot( t3dVTResidencyState_t& rState, const uint32_t pAllocationId, const uint32_t pRequestedPage,
                                         const uint16_t pRequestedMip, t3dVTResolveResult_t& rResult )
{
	if ( !t3d_vt_resolve_allocation_page_reference_debug( rState, pAllocationId, pRequestedPage, pRequestedMip, rResult ) ) {
		return false;
	}
	if ( ( rResult.m_nFlags & T3D_VT_RESOLVE_ANCESTOR ) != 0u ) {
		++rState.Stats.nAncestorFallbackCount;
		++rState.Stats.nFallbackCount;
	}
	if ( ( rResult.m_nFlags & T3D_VT_RESOLVE_TAIL ) != 0u ) {
		++rState.Stats.nTailFallbackCount;
		++rState.Stats.nFallbackCount;
	}
	return true;
}

bool t3d_vt_resolve_allocation_page_reference_debug( const t3dVTResidencyState_t& rState, const uint32_t pAllocationId,
                                                     const uint32_t pRequestedPage, const uint16_t pRequestedMip,
                                                     t3dVTResolveResult_t& rResult )
{
	memset( &rResult, 0, sizeof( rResult ) );
	rResult.m_nAllocation = pAllocationId;
	rResult.m_nRequestedPage = pRequestedPage;
	rResult.m_nResolvedPage = T3D_VT_INVALID_PAGE;
	rResult.m_nPhysicalSlot = T3D_VT_INVALID_SLOT;
	rResult.m_nTailIndex = T3D_VT_INVALID_TAIL;
	rResult.m_nRequestedMip = pRequestedMip;
	const t3dVTAllocation_t *pAllocation = NULL;
	if ( !t3d_vt_allocation_lookup_hot( rState, pAllocationId, pAllocation ) || pRequestedMip >= pAllocation->m_nMipCount ||
	     pRequestedPage < pAllocation->m_nBaseVirtualPage ||
	     pRequestedPage >= pAllocation->m_nBaseVirtualPage + pAllocation->m_nPageCount ) {
		return false;
	}
	const t3dVTTailMetadata_t *pTail = NULL;
	if ( pAllocation->m_nTailIndex != T3D_VT_INVALID_TAIL && pAllocation->m_nTailIndex < rState.nTailCount &&
	     rState.Buffers.pTailMetadata != NULL ) {
		pTail = &rState.Buffers.pTailMetadata[pAllocation->m_nTailIndex];
		if ( pTail->m_nGeneration != pAllocation->m_nGeneration || pTail->m_nWidthTexels == 0u || pTail->m_nHeightTexels == 0u ||
		     pTail->m_nMipCount == 0u || pTail->m_nFirstMip >= pAllocation->m_nMipCount ||
		     pTail->m_nMipCount > pAllocation->m_nMipCount - pTail->m_nFirstMip ) {
			pTail = NULL;
		}
	}
	if ( pTail != NULL && pRequestedMip >= pTail->m_nFirstMip ) {
		rResult.m_nTailIndex = pAllocation->m_nTailIndex;
		rResult.m_nResolvedMip = pTail->m_nFirstMip;
		rResult.m_nFallbackDepth = 0u;
		rResult.m_nFlags = T3D_VT_RESOLVE_TAIL;
		return true;
	}
	uint32_t nPage = pRequestedPage;
	uint16_t nFallbackDepth = 0u;
	const uint32_t nRemainingMipCount = pAllocation->m_nMipCount - pRequestedMip;
	const uint32_t nProbeCap = nRemainingMipCount < T3D_VT_ANCESTOR_PROBE_CAP ? nRemainingMipCount : T3D_VT_ANCESTOR_PROBE_CAP;
	for ( uint32_t iProbe = 0u; iProbe < nProbeCap; ++iProbe ) {
		if ( nPage < pAllocation->m_nBaseVirtualPage || nPage >= pAllocation->m_nBaseVirtualPage + pAllocation->m_nPageCount ) {
			return false;
		}
		const t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[nPage];
		if ( ( Entry.nFlags & T3D_VT_PAGE_VALID ) == 0u || Entry.nGeneration != pAllocation->m_nGeneration ) {
			return false;
		}
		if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) != 0u && Entry.nPhysicalSlot < rState.Buffers.nPhysicalPageSetCapacity ) {
			const t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[Entry.nPhysicalSlot];
			if ( Slot.m_nVirtualPage == nPage && Slot.m_nGeneration == Entry.nGeneration &&
			     ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) == 0u ) {
				rResult.m_nResolvedPage = nPage;
				rResult.m_nPhysicalSlot = Entry.nPhysicalSlot;
				rResult.m_nResolvedMip = Entry.nResolvedMip;
				rResult.m_nFallbackDepth = nFallbackDepth;
				rResult.m_nFlags =
				    ( uint16_t )( T3D_VT_RESOLVE_PHYSICAL | ( nFallbackDepth ? ( uint16_t )( T3D_VT_RESOLVE_ANCESTOR ) : 0u ) );
				return true;
			}
		}
		if ( ( Entry.nFlags & T3D_VT_PAGE_TAIL_FALLBACK ) != 0u || Entry.nParentPage == T3D_VT_INVALID_PAGE ) {
			break;
		}
		nPage = Entry.nParentPage;
		if ( nFallbackDepth != 0xffffu )
			++nFallbackDepth;
	}
	if ( pTail == NULL ) {
		return false;
	}
	rResult.m_nTailIndex = pAllocation->m_nTailIndex;
	rResult.m_nResolvedMip = pTail->m_nFirstMip;
	rResult.m_nFallbackDepth = nFallbackDepth;
	rResult.m_nFlags = T3D_VT_RESOLVE_TAIL;
	return true;
}

bool t3d_vt_resolve_allocation_hot( t3dVTResidencyState_t& rState, uint32_t pAllocationId, uint16_t pRequestedMip, uint32_t pPageX,
                                    uint32_t pPageY, t3dVTResolveResult_t& rResult )
{
	const t3dVTAllocation_t *pAllocation = NULL;
	uint32_t nRequestedPage = T3D_VT_INVALID_PAGE;
	if ( !t3d_vt_allocation_lookup_hot( rState, pAllocationId, pAllocation ) ||
	     !t3d_vt_compute_page( *pAllocation, pRequestedMip, pPageX, pPageY, nRequestedPage ) ) {
		memset( &rResult, 0, sizeof( rResult ) );
		rResult.m_nAllocation = pAllocationId;
		rResult.m_nRequestedPage = T3D_VT_INVALID_PAGE;
		rResult.m_nResolvedPage = T3D_VT_INVALID_PAGE;
		rResult.m_nPhysicalSlot = T3D_VT_INVALID_SLOT;
		rResult.m_nTailIndex = T3D_VT_INVALID_TAIL;
		rResult.m_nRequestedMip = pRequestedMip;
		return false;
	}
	return t3d_vt_resolve_allocation_page_hot( rState, pAllocationId, nRequestedPage, pRequestedMip, rResult );
}

bool t3d_vt_touch_page_hot( t3dVTResidencyState_t& rState, uint32_t pVirtualPage, uint32_t pFrameId )
{
	if ( !rState.bInitialized || pVirtualPage >= rState.pDatabase->pHeader->nPageCount ) {
		return false;
	}
	t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[pVirtualPage];
	if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || Entry.nPhysicalSlot >= rState.Buffers.nPhysicalPageSetCapacity ) {
		return false;
	}
	t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[Entry.nPhysicalSlot];
	if ( Slot.m_nVirtualPage != pVirtualPage || ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) != 0u ) {
		return false;
	}
	Slot.m_nLastVisibleFrame = pFrameId;
	Slot.m_nLastRequestedFrame = pFrameId;
	Slot.m_nFlags |= T3D_VT_SLOT_REFERENCED | T3D_VT_SLOT_HOT;
	Slot.m_nClockAge = 0u;
	++rState.Stats.nTouchCount;
	return true;
}

static uint32_t t3d_vt_choose_victim_hot( t3dVTResidencyState_t& rState, uint32_t pFrameId )
{
	const uint32_t nCapacity = rState.Buffers.nPhysicalPageSetCapacity;
	uint32_t residentByCategory[T3D_VT_RESIDENCY_CATEGORY_COUNT];
	memset( residentByCategory, 0, sizeof( residentByCategory ) );
	for ( uint32_t i = 0u; i < nCapacity; ++i ) {
		const t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[i];
		if ( Slot.m_nVirtualPage == T3D_VT_INVALID_PAGE || ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) != 0u ) {
			continue;
		}
		const uint16_t nCategory = t3d_vt_residency_category_reference_debug( *rState.pDatabase, Slot.m_nVirtualPage );
		++residentByCategory[nCategory];
	}
	for ( uint32_t i = 0u; i < nCapacity; ++i ) {
		const uint32_t nSlot = rState.nClockHand++ % nCapacity;
		t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[nSlot];
		++rState.Stats.nClockScanCount;
		if ( Slot.m_nVirtualPage == T3D_VT_INVALID_PAGE ) {
			continue;
		}
		if ( ( Slot.m_nFlags & ( T3D_VT_SLOT_PINNED | T3D_VT_SLOT_PENDING | T3D_VT_SLOT_RETIRED ) ) != 0u ) {
			++rState.Stats.nPinnedProtectionCount;
			continue;
		}
		t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[Slot.m_nVirtualPage];
		if ( ( Entry.nFlags & ( T3D_VT_PAGE_PINNED | T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_ROOT | T3D_VT_PAGE_TAIL_FALLBACK ) ) != 0u ) {
			++rState.Stats.nPinnedProtectionCount;
			continue;
		}
		Slot.m_nFlags &= ( uint16_t )~T3D_VT_SLOT_REFERENCED;
		const uint16_t nTemperature = t3d_vt_residency_temperature_reference_debug( rState, Slot, pFrameId );
		if ( nTemperature == T3D_VT_RESIDENCY_HOT ) {
			Slot.m_nFlags |= T3D_VT_SLOT_HOT;
			++rState.Stats.nHotScanCount;
			continue;
		}
		if ( nTemperature == T3D_VT_RESIDENCY_WARM ) {
			Slot.m_nFlags &= ( uint16_t )~T3D_VT_SLOT_HOT;
			++rState.Stats.nWarmScanCount;
			continue;
		}
		Slot.m_nFlags &= ( uint16_t )~T3D_VT_SLOT_HOT;
		++rState.Stats.nColdScanCount;
		const uint16_t nCategory = t3d_vt_residency_category_reference_debug( *rState.pDatabase, Slot.m_nVirtualPage );
		const uint32_t nProtectedFloor = ( nCapacity * rState.Policy.nProtectedResidentPercent[nCategory] ) / 100u;
		if ( residentByCategory[nCategory] <= nProtectedFloor ) {
			continue;
		}
		return nSlot;
	}
	return T3D_VT_INVALID_SLOT;
}

bool t3d_vt_reserve_upload_hot( t3dVTResidencyState_t& rState, uint32_t pVirtualPage, uint16_t pWantedMip, uint32_t pFrameId,
                                t3dVTUploadReservation_t& rReservation )
{
	memset( &rReservation, 0, sizeof( rReservation ) );
	rReservation.nVirtualPage = T3D_VT_INVALID_PAGE;
	rReservation.nPhysicalSlot = T3D_VT_INVALID_SLOT;
	rReservation.nVictimPage = T3D_VT_INVALID_PAGE;
	if ( !rState.bInitialized || pVirtualPage >= rState.pDatabase->pHeader->nPageCount || rState.Stats.nFreeSlotCount == 0u ) {
		++rState.Stats.nReservationFailureCount;
		return false;
	}
	t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[pVirtualPage];
	if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) != 0u ) {
		return false;
	}
	if ( rState.Buffers.pLastEvictedFrame[pVirtualPage] != T3D_VT_INVALID_PAGE ) {
		const uint32_t nSinceEviction = pFrameId - rState.Buffers.pLastEvictedFrame[pVirtualPage];
		if ( nSinceEviction <= rState.Policy.nThrashWindowFrames ) {
			++rState.Stats.nThrashCount;
		}
		rState.Buffers.pLastEvictedFrame[pVirtualPage] = T3D_VT_INVALID_PAGE;
	}

	uint32_t nVictimPage = T3D_VT_INVALID_PAGE;
	if ( rState.Stats.nFreeSlotCount <= 1u ) {
		const uint32_t nVictimSlot = t3d_vt_choose_victim_hot( rState, pFrameId );
		if ( nVictimSlot == T3D_VT_INVALID_SLOT ) {
			++rState.Stats.nReservationFailureCount;
			return false;
		}
		nVictimPage = rState.Buffers.pPhysicalPageSets[nVictimSlot].m_nVirtualPage;
	}

	const uint32_t nFree = t3d_vt_pop_free_slot_hot( rState );
	if ( nFree == T3D_VT_INVALID_SLOT ) {
		++rState.Stats.nReservationFailureCount;
		return false;
	}
	t3dVTPhysicalSlot_t& FreeSlot = rState.Buffers.pPhysicalPageSets[nFree];
	if ( FreeSlot.m_nVirtualPage != T3D_VT_INVALID_PAGE || FreeSlot.m_nFlags != 0u ) {
		t3d_vt_push_free_slot_hot( rState, nFree );
		++rState.Stats.nReservationFailureCount;
		return false;
	}
	FreeSlot.m_nFlags = T3D_VT_SLOT_PENDING;
	FreeSlot.m_nLastRequestedFrame = pFrameId;
	++rState.Stats.nPendingSlotCount;

	rReservation.nVirtualPage = pVirtualPage;
	rReservation.nPhysicalSlot = nFree;
	rReservation.nVictimPage = nVictimPage;
	rReservation.nExpectedGeneration = Entry.nGeneration;
	rReservation.nDatabaseGeneration = rState.nDatabaseGeneration;
	rReservation.nWantedMip = pWantedMip;
	++rState.Stats.nUploadReservationCount;
	return true;
}

bool t3d_vt_can_commit_upload_hot( const t3dVTResidencyState_t& pState, const t3dVTUploadReservation_t& pReservation )
{
	if ( !pState.bInitialized || pState.pDatabase == NULL || pReservation.nVirtualPage >= pState.pDatabase->pHeader->nPageCount ||
	     pReservation.nPhysicalSlot >= pState.Buffers.nPhysicalPageSetCapacity ||
	     pReservation.nDatabaseGeneration != pState.nDatabaseGeneration ) {
		return false;
	}
	const t3dVTPageTableEntry_t& NewEntry = pState.Buffers.pPageTable[pReservation.nVirtualPage];
	const t3dVTPhysicalSlot_t& NewSlot = pState.Buffers.pPhysicalPageSets[pReservation.nPhysicalSlot];
	if ( ( NewEntry.nFlags & T3D_VT_PAGE_RESIDENT ) != 0u || NewEntry.nPhysicalSlot != T3D_VT_INVALID_SLOT ||
	     NewEntry.nGeneration != pReservation.nExpectedGeneration || NewSlot.m_nVirtualPage != T3D_VT_INVALID_PAGE ||
	     ( NewSlot.m_nFlags & T3D_VT_SLOT_PENDING ) == 0u ) {
		return false;
	}
	if ( pReservation.nVictimPage != T3D_VT_INVALID_PAGE ) {
		if ( pReservation.nVictimPage >= pState.pDatabase->pHeader->nPageCount ||
		     pState.Stats.nRetiredSlotCount >= pState.Buffers.nPhysicalPageSetCapacity ) {
			return false;
		}
		const t3dVTPageTableEntry_t& VictimEntry = pState.Buffers.pPageTable[pReservation.nVictimPage];
		if ( ( VictimEntry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || VictimEntry.nPhysicalSlot >= pState.Buffers.nPhysicalPageSetCapacity ) {
			return false;
		}
		const t3dVTPhysicalSlot_t& VictimSlot = pState.Buffers.pPhysicalPageSets[VictimEntry.nPhysicalSlot];
		if ( VictimSlot.m_nVirtualPage != pReservation.nVictimPage ||
		     ( VictimSlot.m_nFlags & ( T3D_VT_SLOT_PENDING | T3D_VT_SLOT_RETIRED | T3D_VT_SLOT_PINNED ) ) != 0u ) {
			return false;
		}
	}
	return true;
}

bool t3d_vt_commit_upload_hot( t3dVTResidencyState_t& rState, const t3dVTUploadReservation_t& pReservation, uint32_t pUploadedChannelMask,
                               uint32_t pFrameId )
{
	if ( !t3d_vt_can_commit_upload_hot( rState, pReservation ) ) {
		return false;
	}
	const t3dVTPageFileRecord_t& Page = rState.pDatabase->pPages[pReservation.nVirtualPage];
	const uint32_t nExpectedMask =
	    rState.pDatabase->nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK ? ( 1u << Page.nChannel ) : T3D_VT_PAGE_SET_CHANNEL_MASK;
	if ( pUploadedChannelMask != nExpectedMask ) {
		return false;
	}
	t3dVTPageTableEntry_t& NewEntry = rState.Buffers.pPageTable[pReservation.nVirtualPage];
	t3dVTPhysicalSlot_t& NewSlot = rState.Buffers.pPhysicalPageSets[pReservation.nPhysicalSlot];

	NewSlot.m_nVirtualPage = pReservation.nVirtualPage;
	NewSlot.m_nGeneration = NewEntry.nGeneration;
	NewSlot.m_nLastVisibleFrame = pFrameId;
	NewSlot.m_nLastRequestedFrame = pFrameId;
	NewSlot.m_nFlags = T3D_VT_SLOT_REFERENCED | T3D_VT_SLOT_HOT;
	if ( ( NewEntry.nFlags & ( T3D_VT_PAGE_PINNED | T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_ROOT | T3D_VT_PAGE_TAIL_FALLBACK ) ) != 0u ) {
		NewSlot.m_nFlags |= T3D_VT_SLOT_PINNED;
		NewSlot.m_nPinCount = 1u;
	} else {
		NewSlot.m_nPinCount = 0u;
	}
	NewSlot.m_nClockAge = 0u;
	NewSlot.m_nRetireFrame = 0u;
	NewSlot.m_nPayloadId = Page.nPayloadId;
	NewSlot.m_nReserved = 0u;
	NewEntry.nPhysicalSlot = pReservation.nPhysicalSlot;
	NewEntry.nResolvedMip = pReservation.nWantedMip;
	NewEntry.nFlags |= T3D_VT_PAGE_RESIDENT;
	--rState.Stats.nPendingSlotCount;
	++rState.Stats.nResidentCount;
	++rState.Stats.nUploadCommitCount;
	++rState.nPageTableGeneration;

	if ( pReservation.nVictimPage != T3D_VT_INVALID_PAGE ) {
		t3dVTPageTableEntry_t& VictimEntry = rState.Buffers.pPageTable[pReservation.nVictimPage];
		if ( !t3d_vt_begin_eviction_hot( rState, VictimEntry.nPhysicalSlot, pReservation.nVictimPage, pFrameId ) ) {
			return false;
		}
	}
	return true;
}

bool t3d_vt_begin_eviction_hot( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot, uint32_t pVictimPage, uint32_t pFrameId )
{
	if ( !rState.bInitialized || pPhysicalSlot >= rState.Buffers.nPhysicalPageSetCapacity ||
	     pVictimPage >= rState.pDatabase->pHeader->nPageCount ||
	     rState.Stats.nRetiredSlotCount >= rState.Buffers.nPhysicalPageSetCapacity ) {
		return false;
	}
	t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[pPhysicalSlot];
	t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[pVictimPage];
	if ( Slot.m_nVirtualPage != pVictimPage || ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || Entry.nPhysicalSlot != pPhysicalSlot ||
	     ( Slot.m_nFlags & ( T3D_VT_SLOT_PINNED | T3D_VT_SLOT_PENDING | T3D_VT_SLOT_RETIRED ) ) != 0u ||
	     rState.Stats.nResidentCount == 0u ) {
		return false;
	}

	Slot.m_nFlags &= ( uint16_t )~( T3D_VT_SLOT_REFERENCED | T3D_VT_SLOT_HOT );
	Slot.m_nFlags |= T3D_VT_SLOT_RETIRED;
	const uint32_t nReleaseFrame = pFrameId + rState.Policy.nRetireDelayFrames;
	Slot.m_nRetireFrame = ( uint16_t )( nReleaseFrame & 0xffffu );
	t3dVTRetiredPageSet_t& Retired = rState.Buffers.pRetiredPageSets[rState.nRetireTail];
	Retired.nPhysicalSlot = pPhysicalSlot;
	Retired.nVirtualPage = pVictimPage;
	Retired.nGeneration = Slot.m_nGeneration;
	Retired.nReleaseFrame = nReleaseFrame;
	rState.nRetireTail = ( rState.nRetireTail + 1u ) % rState.Buffers.nPhysicalPageSetCapacity;
	++rState.Stats.nRetiredSlotCount;
	--rState.Stats.nResidentCount;
	return true;
}

bool t3d_vt_finalize_eviction_hot( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot, uint32_t pVictimPage, uint32_t pFrameId )
{
	if ( !rState.bInitialized || pPhysicalSlot >= rState.Buffers.nPhysicalPageSetCapacity ||
	     pVictimPage >= rState.pDatabase->pHeader->nPageCount || rState.Stats.nRetiredSlotCount == 0u ) {
		return false;
	}
	t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[pPhysicalSlot];
	t3dVTPageTableEntry_t& Entry = rState.Buffers.pPageTable[pVictimPage];
	if ( Slot.m_nVirtualPage != pVictimPage || ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) == 0u ||
	     ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || Entry.nPhysicalSlot != pPhysicalSlot ) {
		return false;
	}
	Entry.nPhysicalSlot = T3D_VT_INVALID_SLOT;
	Entry.nFlags &= ( uint16_t )~T3D_VT_PAGE_RESIDENT;
	rState.Buffers.pLastEvictedFrame[pVictimPage] = pFrameId;
	memset( &Slot, 0, sizeof( Slot ) );
	Slot.m_nVirtualPage = T3D_VT_INVALID_PAGE;
	Slot.m_nPayloadId = T3D_VT_INVALID_PAGE;
	--rState.Stats.nRetiredSlotCount;
	if ( !t3d_vt_push_free_slot_hot( rState, pPhysicalSlot ) ) {
		return false;
	}
	++rState.Stats.nEvictionCount;
	++rState.Stats.nRetireFinalizeCount;
	++rState.nPageTableGeneration;
	return true;
}

bool t3d_vt_service_retired_slots_hot( t3dVTResidencyState_t& rState, uint32_t pFrameId )
{
	if ( !rState.bInitialized ) {
		return false;
	}
	while ( rState.Stats.nRetiredSlotCount != 0u ) {
		t3dVTRetiredPageSet_t& Retired = rState.Buffers.pRetiredPageSets[rState.nRetireHead];
		if ( !t3d_vt_retirement_ready_reference_debug( rState, Retired, pFrameId ) ) {
			++rState.Stats.nRetireDeferredCount;
			break;
		}
		const uint32_t nPhysicalSlot = Retired.nPhysicalSlot;
		const uint32_t nVirtualPage = Retired.nVirtualPage;
		if ( nPhysicalSlot >= rState.Buffers.nPhysicalPageSetCapacity || nVirtualPage >= rState.pDatabase->pHeader->nPageCount ) {
			return false;
		}
		const t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[nPhysicalSlot];
		if ( Slot.m_nGeneration != Retired.nGeneration || !t3d_vt_finalize_eviction_hot( rState, nPhysicalSlot, nVirtualPage, pFrameId ) ) {
			return false;
		}
		Retired.nPhysicalSlot = T3D_VT_INVALID_SLOT;
		Retired.nVirtualPage = T3D_VT_INVALID_PAGE;
		Retired.nGeneration = 0u;
		Retired.nReleaseFrame = 0u;
		rState.nRetireHead = ( rState.nRetireHead + 1u ) % rState.Buffers.nPhysicalPageSetCapacity;
	}
	return true;
}

void t3d_vt_abort_upload_hot( t3dVTResidencyState_t& rState, const t3dVTUploadReservation_t& pReservation )
{
	if ( !rState.bInitialized || pReservation.nPhysicalSlot >= rState.Buffers.nPhysicalPageSetCapacity ) {
		return;
	}
	t3dVTPhysicalSlot_t& Slot = rState.Buffers.pPhysicalPageSets[pReservation.nPhysicalSlot];
	if ( Slot.m_nVirtualPage == T3D_VT_INVALID_PAGE && ( Slot.m_nFlags & T3D_VT_SLOT_PENDING ) != 0u ) {
		memset( &Slot, 0, sizeof( Slot ) );
		Slot.m_nVirtualPage = T3D_VT_INVALID_PAGE;
		Slot.m_nPayloadId = T3D_VT_INVALID_PAGE;
		if ( rState.Stats.nPendingSlotCount != 0u ) {
			--rState.Stats.nPendingSlotCount;
		}
		t3d_vt_push_free_slot_hot( rState, pReservation.nPhysicalSlot );
	}
}

bool t3d_vt_validate_residency_reference_debug( const t3dVTResidencyState_t& pState )
{
	if ( !pState.bInitialized || pState.pDatabase == NULL || pState.Buffers.pAllocations == NULL || pState.Buffers.pTailMetadata == NULL ||
	     pState.Buffers.pLastEvictedFrame == NULL || !t3d_vt_validate_residency_policy( pState.Policy ) || pState.nAllocationCount == 0u ||
	     pState.nAllocationCount > pState.Buffers.nAllocationCapacity || pState.nTailCount == 0u ||
	     pState.nTailCount > pState.Buffers.nTailMetadataCapacity ||
	     pState.Stats.nFreeSlotCount > pState.Buffers.nPhysicalPageSetCapacity ||
	     pState.Stats.nRetiredSlotCount > pState.Buffers.nPhysicalPageSetCapacity ) {
		return false;
	}
	for ( uint32_t iAllocation = 0u; iAllocation < pState.nAllocationCount; ++iAllocation ) {
		const t3dVTAllocation_t& Allocation = pState.Buffers.pAllocations[iAllocation];
		if ( pState.pDatabase->nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK && Allocation.m_nPageCount == 0u ) {
			continue;
		}
		if ( Allocation.m_nPageCount == 0u || Allocation.m_nMipCount == 0u || Allocation.m_nMipCount > T3D_VT_MAX_MIPS ||
		     Allocation.m_nBaseVirtualPage >= pState.pDatabase->pHeader->nPageCount ||
		     Allocation.m_nPageCount > pState.pDatabase->pHeader->nPageCount - Allocation.m_nBaseVirtualPage ||
		     Allocation.m_nTailIndex >= pState.nTailCount || Allocation.m_nReserved >= T3D_VT_RESIDENCY_CATEGORY_COUNT ) {
			return false;
		}
		const t3dVTTailMetadata_t& Tail = pState.Buffers.pTailMetadata[Allocation.m_nTailIndex];
		const uint32_t nTailAtlasWidth =
		    pState.pDatabase->nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK ? T3D_VT_PACK_TAIL_ATLAS_WIDTH : T3D_VT_TAIL_ATLAS_WIDTH;
		if ( Tail.m_nGeneration != Allocation.m_nGeneration || Tail.m_nWidthTexels == 0u || Tail.m_nHeightTexels == 0u ||
		     Tail.m_nMipCount == 0u || Tail.m_nFirstMip >= Allocation.m_nMipCount ||
		     Tail.m_nMipCount > Allocation.m_nMipCount - Tail.m_nFirstMip || Tail.m_nAtlasX >= nTailAtlasWidth ||
		     Tail.m_nWidthTexels > nTailAtlasWidth - Tail.m_nAtlasX ) {
			return false;
		}
	}
	for ( uint32_t iPage = 0u; iPage < pState.pDatabase->pHeader->nPageCount; ++iPage ) {
		const t3dVTPageFileRecord_t& Page = pState.pDatabase->pPages[iPage];
		if ( Page.nAllocationId >= pState.nAllocationCount ) {
			return false;
		}
		const t3dVTAllocation_t& Allocation = pState.Buffers.pAllocations[Page.nAllocationId];
		const t3dVTPageTableEntry_t& Entry = pState.Buffers.pPageTable[iPage];
		if ( Entry.nGeneration != Allocation.m_nGeneration || ( Entry.nFlags & T3D_VT_PAGE_VALID ) == 0u ||
		     ( Entry.nParentPage != T3D_VT_INVALID_PAGE &&
		       ( Entry.nParentPage < Allocation.m_nBaseVirtualPage ||
		         Entry.nParentPage >= Allocation.m_nBaseVirtualPage + Allocation.m_nPageCount ) ) ) {
			return false;
		}
	}
	uint32_t nFree = 0u;
	uint32_t nResident = 0u;
	uint32_t nPending = 0u;
	uint32_t nRetired = 0u;
	for ( uint32_t s = 0u; s < pState.Buffers.nPhysicalPageSetCapacity; ++s ) {
		const t3dVTPhysicalSlot_t& Slot = pState.Buffers.pPhysicalPageSets[s];
		if ( Slot.m_nVirtualPage == T3D_VT_INVALID_PAGE ) {
			if ( ( Slot.m_nFlags & T3D_VT_SLOT_PENDING ) != 0u ) {
				++nPending;
			} else if ( Slot.m_nFlags == 0u ) {
				++nFree;
			} else {
				return false;
			}
			continue;
		}
		if ( Slot.m_nVirtualPage >= pState.pDatabase->pHeader->nPageCount ) {
			return false;
		}
		const t3dVTPageTableEntry_t& Entry = pState.Buffers.pPageTable[Slot.m_nVirtualPage];
		if ( ( Slot.m_nFlags & T3D_VT_SLOT_RETIRED ) != 0u ) {
			if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || Entry.nPhysicalSlot != s ) {
				return false;
			}
			++nRetired;
			continue;
		}
		if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) == 0u || Entry.nPhysicalSlot != s ) {
			return false;
		}
		++nResident;
	}
	if ( nFree != pState.Stats.nFreeSlotCount || nResident != pState.Stats.nResidentCount || nPending != pState.Stats.nPendingSlotCount ||
	     nRetired != pState.Stats.nRetiredSlotCount ||
	     nFree + nResident + nPending + nRetired != pState.Buffers.nPhysicalPageSetCapacity ) {
		return false;
	}
	for ( uint32_t i = 0u; i < pState.Stats.nFreeSlotCount; ++i ) {
		const uint32_t nSlot = pState.Buffers.pFreeSlots[i];
		if ( nSlot >= pState.Buffers.nPhysicalPageSetCapacity ||
		     pState.Buffers.pPhysicalPageSets[nSlot].m_nVirtualPage != T3D_VT_INVALID_PAGE ||
		     pState.Buffers.pPhysicalPageSets[nSlot].m_nFlags != 0u ) {
			return false;
		}
		for ( uint32_t j = i + 1u; j < pState.Stats.nFreeSlotCount; ++j ) {
			if ( pState.Buffers.pFreeSlots[j] == nSlot ) {
				return false;
			}
		}
	}
	if ( pState.bNeutralPublished ) {
		uint32_t nNeutralSlot = T3D_VT_INVALID_SLOT;
		for ( uint32_t c = 0u; c < T3D_VT_CHANNEL_COUNT; ++c ) {
			if ( pState.nNeutralRootPage[c] == T3D_VT_INVALID_PAGE ||
			     pState.nNeutralRootPage[c] >= pState.pDatabase->pHeader->nPageCount ) {
				return false;
			}
			const t3dVTPageTableEntry_t& Entry = pState.Buffers.pPageTable[pState.nNeutralRootPage[c]];
			if ( ( Entry.nFlags & ( T3D_VT_PAGE_RESIDENT | T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_PINNED ) ) !=
			     ( T3D_VT_PAGE_RESIDENT | T3D_VT_PAGE_NEUTRAL | T3D_VT_PAGE_PINNED ) ) {
				return false;
			}
			if ( nNeutralSlot == T3D_VT_INVALID_SLOT ) {
				nNeutralSlot = Entry.nPhysicalSlot;
			} else if ( Entry.nPhysicalSlot != nNeutralSlot ) {
				return false;
			}
		}
	}
	return true;
}

bool t3d_vt_allocation_page_id( const t3dVTAllocation_t& pAllocation, uint32_t pMip, uint32_t pPageX, uint32_t pPageY,
                                uint32_t& rVirtualPage )
{
	if ( pMip >= pAllocation.m_nMipCount || pMip >= T3D_VT_MAX_MIPS ) {
		return false;
	}
	uint32_t nWidth = pAllocation.m_nMip0WidthPages;
	uint32_t nHeight = pAllocation.m_nMip0HeightPages;
	uint64_t nOffset = 0u;
	for ( uint32_t i = 0u; i < pMip; ++i ) {
		nOffset += ( uint64_t )nWidth * nHeight;
		nWidth = nWidth > 1u ? ( nWidth + 1u ) >> 1u : 1u;
		nHeight = nHeight > 1u ? ( nHeight + 1u ) >> 1u : 1u;
	}
	if ( pPageX >= nWidth || pPageY >= nHeight ) {
		return false;
	}
	const uint64_t nPage = ( uint64_t )pAllocation.m_nBaseVirtualPage + nOffset + ( uint64_t )pPageY * nWidth + pPageX;
	if ( nPage >= ( uint64_t )pAllocation.m_nBaseVirtualPage + pAllocation.m_nPageCount || nPage > UINT32_MAX ) {
		return false;
	}
	rVirtualPage = ( uint32_t )nPage;
	return true;
}

bool t3d_vt_predict_page_rect_reference_debug( const t3dVTAllocation_t& pAllocation, const float pUvMin[2], const float pUvMax[2],
                                               uint32_t pWantedMip, uint32_t pMaximumPages, t3dVTPredictedPageRect_t& rRect )
{
	memset( &rRect, 0, sizeof( rRect ) );
	if ( pUvMin == NULL || pUvMax == NULL || pAllocation.m_nPageCount == 0u || pAllocation.m_nMipCount == 0u ||
	     pAllocation.m_nMip0WidthPages == 0u || pAllocation.m_nMip0HeightPages == 0u || pMaximumPages == 0u ) {
		return false;
	}
	if ( !isfinite( pUvMin[0] ) || !isfinite( pUvMin[1] ) || !isfinite( pUvMax[0] ) || !isfinite( pUvMax[1] ) ) {
		return false;
	}

	uint32_t nMip = pWantedMip < pAllocation.m_nMipCount ? pWantedMip : ( uint32_t )pAllocation.m_nMipCount - 1u;
	for ( ;; ) {
		uint32_t nWidth = pAllocation.m_nMip0WidthPages;
		uint32_t nHeight = pAllocation.m_nMip0HeightPages;
		for ( uint32_t m = 0u; m < nMip; ++m ) {
			nWidth = nWidth > 1u ? ( nWidth + 1u ) >> 1u : 1u;
			nHeight = nHeight > 1u ? ( nHeight + 1u ) >> 1u : 1u;
		}

		const float fMinU = pUvMin[0] < pUvMax[0] ? pUvMin[0] : pUvMax[0];
		const float fMaxU = pUvMin[0] > pUvMax[0] ? pUvMin[0] : pUvMax[0];
		const float fMinV = pUvMin[1] < pUvMax[1] ? pUvMin[1] : pUvMax[1];
		const float fMaxV = pUvMin[1] > pUvMax[1] ? pUvMin[1] : pUvMax[1];
		const bool bFullU = fMinU < 0.0f || fMaxU > 1.0f || fMaxU - fMinU >= 1.0f;
		const bool bFullV = fMinV < 0.0f || fMaxV > 1.0f || fMaxV - fMinV >= 1.0f;
		const float fClampedMinU = fMinU < 0.0f ? 0.0f : ( fMinU > 1.0f ? 1.0f : fMinU );
		const float fClampedMaxU = fMaxU < 0.0f ? 0.0f : ( fMaxU > 1.0f ? 1.0f : fMaxU );
		const float fClampedMinV = fMinV < 0.0f ? 0.0f : ( fMinV > 1.0f ? 1.0f : fMinV );
		const float fClampedMaxV = fMaxV < 0.0f ? 0.0f : ( fMaxV > 1.0f ? 1.0f : fMaxV );
		uint32_t nMinX = bFullU ? 0u : ( uint32_t )floorf( fClampedMinU * ( float )nWidth );
		uint32_t nMaxX = bFullU ? nWidth - 1u : ( uint32_t )floorf( fClampedMaxU * ( float )nWidth );
		uint32_t nMinY = bFullV ? 0u : ( uint32_t )floorf( fClampedMinV * ( float )nHeight );
		uint32_t nMaxY = bFullV ? nHeight - 1u : ( uint32_t )floorf( fClampedMaxV * ( float )nHeight );
		if ( nMinX >= nWidth ) {
			nMinX = nWidth - 1u;
		}
		if ( nMaxX >= nWidth ) {
			nMaxX = nWidth - 1u;
		}
		if ( nMinY >= nHeight ) {
			nMinY = nHeight - 1u;
		}
		if ( nMaxY >= nHeight ) {
			nMaxY = nHeight - 1u;
		}
		if ( nMinX > nMaxX ) {
			const uint32_t t = nMinX;
			nMinX = nMaxX;
			nMaxX = t;
		}
		if ( nMinY > nMaxY ) {
			const uint32_t t = nMinY;
			nMinY = nMaxY;
			nMaxY = t;
		}
		const uint64_t nPages = ( uint64_t )( nMaxX - nMinX + 1u ) * ( uint64_t )( nMaxY - nMinY + 1u );
		if ( nPages <= pMaximumPages || nMip + 1u >= pAllocation.m_nMipCount ) {
			rRect.nMip = nMip;
			rRect.nMinPageX = nMinX;
			rRect.nMinPageY = nMinY;
			rRect.nMaxPageX = nMaxX;
			rRect.nMaxPageY = nMaxY;
			rRect.nPageCount = ( uint32_t )nPages;
			return nPages <= 0xffffffffull;
		}
		++nMip;
	}
}
