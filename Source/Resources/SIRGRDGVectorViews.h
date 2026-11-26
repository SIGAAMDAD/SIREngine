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

#ifndef __SIR_GRDGVECTORVIEWS_H__
#define __SIR_GRDGVECTORVIEWS_H__

#include <Core/SIRPCH.h>
#include <Resources/SIRResourceGraph.h>
#include <Core/Containers/SIRStaticVector.h>
#include <atomic>
#include <Core/Jobs/SIRTaskGraph.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

// Cooked byte schema; RAW_NATIVE means platform ABI bytes at the documented
// format/schema revision, not an interchange format for arbitrary C++ objects.
static constexpr uint32_t SIR_GRDG_VECTOR_ENCODING_RAW_NATIVE = 0;
static constexpr uint16_t SIR_GRDG_VECTOR_ENDIAN_LITTLE = 1;
static constexpr uint16_t SIR_GRDG_VECTOR_ENDIAN_BIG = 2;

struct SIRGRDGVectorLayout
{
	uint32_t nElementBytes;
	uint32_t nElementAlignment;
	uint32_t nElementStrideBytes;
	uint32_t nFormatId;
	uint32_t nEncodingId;
	uint16_t nSchemaVersion;
	uint16_t nByteOrder;
	uint64_t nElementCount;
	uint64_t nOffsetBytes;
};

struct SIRGRDGVectorReadRequest
{
	SIRResourceChunkId idChunk;
	SIRGRDGVectorLayout layout;
	uint64_t nExpectedRevision;
};

struct SIRGRDGVectorStagingRequest
{
	SIRResourceKey keyResource;
	SIRGRDGVectorLayout layout;
	uint64_t nExpectedRevision;
	uint32_t nAllocationDomain;
};

struct SIRGRDGVectorLeaseRequest
{
	uint64_t nFirstElement;
	uint64_t nElementCount;
};

class SIRGRDGVectorLayoutChecks
{
public:
	static uint16_t hostByteOrder( void ) noexcept
	{
		const uint16_t nTest = 1;
		return *reinterpret_cast<const uint8_t *>(&nTest) ? SIR_GRDG_VECTOR_ENDIAN_LITTLE : SIR_GRDG_VECTOR_ENDIAN_BIG;
	}

	static bool totalBytes( const SIRGRDGVectorLayout& rLayout, uint64_t *pTotal ) noexcept
	{
		if ( pTotal == nullptr || rLayout.nElementStrideBytes < rLayout.nElementBytes || rLayout.nElementBytes == 0 ||
		     rLayout.nElementAlignment == 0 || ( rLayout.nElementAlignment & ( rLayout.nElementAlignment - 1 ) ) != 0 ) {
			return false;
		}
		if ( rLayout.nOffsetBytes % rLayout.nElementAlignment != 0 ||
		     rLayout.nElementCount > ( UINT64_MAX - rLayout.nOffsetBytes ) / rLayout.nElementStrideBytes ) {
			return false;
		}
		const uint64_t nSize = rLayout.nElementCount * rLayout.nElementStrideBytes;
		*pTotal = rLayout.nOffsetBytes + nSize;
		return true;
	}

	template <typename T>
	static bool isRawTypeCompatible( const SIRGRDGVectorLayout& rLayout ) noexcept
	{
		static_assert( std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value,
		               "GRDG native vector codec requires a trivial standard-layout value type" );
		return rLayout.nEncodingId == SIR_GRDG_VECTOR_ENCODING_RAW_NATIVE && rLayout.nElementBytes == sizeof( T ) &&
		       rLayout.nElementAlignment >= alignof( T ) && rLayout.nElementStrideBytes >= sizeof( T ) && rLayout.nSchemaVersion != 0 &&
		       rLayout.nByteOrder == hostByteOrder();
	}
};

template <typename T>
class SIRGRDGReadOnlyVectorView
{
private:
	SIRResourceView m_Resource;
	const uint8_t *m_pBytes = nullptr;
	SIRGRDGVectorLayout m_Layout {};
	uint64_t m_nRevision = 0;

public:
	SIRGRDGReadOnlyVectorView( void ) noexcept = default;
	uint64_t getSize( void ) const noexcept
	{
		return m_Layout.nElementCount;
	}
	SIRResourceHandle getResource( void ) const noexcept
	{
		return m_Resource.getHandle();
	}
	uint64_t getRevision( void ) const noexcept
	{
		return m_nRevision;
	}
	bool isValid( void ) const noexcept
	{
		return m_pBytes != nullptr && m_Resource.isValid() && m_Resource.getRevision() == m_nRevision;
	}

	SIRResourceResult tryReadAt( uint64_t iElement, T *pValue ) const noexcept
	{
		if ( pValue == nullptr || !isValid() ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		if ( iElement >= m_Layout.nElementCount ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		// Byte copy creates a real T value in caller-owned live storage.
		memcpy( pValue, m_pBytes + static_cast<size_t>( iElement * m_Layout.nElementStrideBytes ), sizeof( T ) );
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}

	SIRResourceResult getByteRange( SIRResourceByteRange rRange, SIRResourceConstBytes *pBytes ) const noexcept
	{
		if ( !isValid() || pBytes == nullptr ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		const uint64_t nAvailable = m_Layout.nElementCount * m_Layout.nElementStrideBytes;
		if ( rRange.m_nOffset > nAvailable || rRange.m_nSize > nAvailable - rRange.m_nOffset ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		*pBytes = { m_pBytes + static_cast<size_t>(rRange.m_nOffset), rRange.m_nSize };
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}

	friend class SIRGRDGVectorViewFactory;
};

class SIRGRDGVectorViewFactory
{
public:
	template <typename T>
	static SIRResourceResult tryResolveRead( const SIRResourceReadScope& rScope, const SIRResourceView& rResource,
	                                         const SIRGRDGVectorReadRequest& rRequest, SIRGRDGReadOnlyVectorView<T> *pView ) noexcept
	{
		if ( pView == nullptr || !rResource.belongsToScope( rScope ) ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		if ( !SIRGRDGVectorLayoutChecks::isRawTypeCompatible<T>( rRequest.layout ) ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		SIRResourceInfo info {};
		SIRResourceResult eResult = rResource.getInfo( &info );
		if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		if ( info.m_nFormatId != rRequest.layout.nFormatId || info.m_nSchemaVersion != rRequest.layout.nSchemaVersion ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( rRequest.nExpectedRevision != 0 && info.m_nRevision != rRequest.nExpectedRevision ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_STALE_REVISION;
		}
		SIRResourceConstBytes bytes {};
		eResult = rResource.getChunk( rRequest.idChunk, &bytes );
		if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		uint64_t nRequired = 0;
		if ( !SIRGRDGVectorLayoutChecks::totalBytes( rRequest.layout, &nRequired ) || nRequired > bytes.m_nSize ||
		     rRequest.layout.nOffsetBytes > SIZE_MAX || nRequired > SIZE_MAX ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		SIRGRDGReadOnlyVectorView<T> resolved;
		resolved.m_Resource = rResource;
		resolved.m_pBytes = bytes.m_pData + static_cast<size_t>(rRequest.layout.nOffsetBytes);
		resolved.m_Layout = rRequest.layout;
		resolved.m_nRevision = info.m_nRevision;
		*pView = resolved;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
};

// Per-lease state is allocated inside the owning staging transaction and never
// relocated; workers only publish completion using the engine atomic primitive.
struct SIRGRDGVectorLeaseState
{
	uint64_t nFirst;
	uint64_t nCount;
	uint64_t nGeneration;
	uint8_t *pStaging;
	std::atomic<uint8_t> nState;

	SIRGRDGVectorLeaseState( uint64_t nFirstElement, uint64_t nElementCount, uint64_t nGen, uint8_t *pBytes ) noexcept
	    : nFirst( nFirstElement ), nCount( nElementCount ), nGeneration( nGen ), pStaging( pBytes ), nState( 1 )
	{
	}
};

template <typename T>
class SIRGRDGWriteOnlyVectorView
{
private:
	SIRGRDGVectorLeaseState *m_pLease = nullptr;
	SIRGRDGVectorLayout m_Layout {};
	uint64_t m_nGeneration = 0;

public:
	SIRGRDGWriteOnlyVectorView( void ) noexcept = default;
	uint64_t getSize( void ) const noexcept
	{
		return m_pLease == nullptr ? 0 : m_pLease->nCount;
	}
	SIRResourceResult tryWriteAt( uint64_t iElement, const T& rValue ) noexcept
	{
		static_assert( std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value,
		               "GRDG native writer requires a trivial standard-layout value type" );
		if ( m_pLease == nullptr || m_pLease->nGeneration != m_nGeneration || m_pLease->nState.load( std::memory_order_acquire ) != 1 ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		if ( iElement >= m_pLease->nCount ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		const uint64_t nIndex = m_pLease->nFirst + iElement;
		memcpy( m_pLease->pStaging + static_cast<size_t>( m_Layout.nOffsetBytes + nIndex * m_Layout.nElementStrideBytes ), &rValue, sizeof( T ) );
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
	SIRResourceResult tryWriteRange( uint64_t iElement, const T *pValues, uint64_t nCount ) noexcept
	{
		if ( pValues == nullptr || m_pLease == nullptr || m_pLease->nGeneration != m_nGeneration ||
		     m_pLease->nState.load( std::memory_order_acquire ) != 1 ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		if ( iElement > m_pLease->nCount || nCount > m_pLease->nCount - iElement ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		if ( m_Layout.nElementStrideBytes == sizeof( T ) ) {
			uint8_t *pTarget =
			    m_pLease->pStaging + static_cast<size_t>(  m_Layout.nOffsetBytes + ( m_pLease->nFirst + iElement ) * m_Layout.nElementStrideBytes  );
			memmove( pTarget, pValues, static_cast<size_t>(nCount * sizeof(  T  )) );
			return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
		}
		for ( uint64_t i = 0; i < nCount; ++i ) {
			const SIRResourceResult eResult = tryWriteAt( iElement + i, pValues[i] );
			if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
				return eResult;
			}
		}
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
	SIRResourceResult completeRange( void ) noexcept
	{
		if ( m_pLease == nullptr || m_pLease->nGeneration != m_nGeneration ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		uint8_t nExpected = 1;
		return m_pLease->nState.compare_exchange_strong( nExpected, 2, std::memory_order_release, std::memory_order_relaxed )
		           ? SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS
				   : SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	friend class SIRGRDGVectorStagingTransaction;
};

class SIRGRDGVectorStagingTransaction
{
private:
	static constexpr uint32_t MAX_LEASES = 128;
	SIRResourceGraphWriter *m_pWriter = nullptr;
	SIRResourceStagingHandle m_Staging {};
	SIRResourceMutableBytes m_Bytes {};
	SIRGRDGVectorStagingRequest m_Request {};
	SIRStaticVector<SIRGRDGVectorLeaseState, MAX_LEASES> m_Leases;
	uint64_t m_nGeneration = 0;
	bool m_bActive = false;
	bool m_bSealed = false;

	// A sealed resource must cover every element, not merely the leases which
	// happened to be requested. Otherwise GRDG could publish uninitialized bytes.
	bool allLeasesComplete( void ) const noexcept
	{
		if ( m_Leases.getSize() == 0 ) {
			return false;
		}
		uint64_t nCursor = 0;
		while ( nCursor < m_Request.layout.nElementCount ) {
			bool bFound = false;
			for ( uint32_t i = 0; i < m_Leases.getSize(); ++i ) {
				const SIRGRDGVectorLeaseState& rLease = m_Leases[i];
				if ( rLease.nFirst == nCursor && rLease.nState.load( std::memory_order_acquire ) == 2 ) {
					nCursor += rLease.nCount;
					bFound = true;
					break;
				}
			}
			if ( !bFound ) {
				return false;
			}
		}
		return nCursor == m_Request.layout.nElementCount;
	}

	void invalidate( void ) noexcept
	{
		for ( uint32_t i = 0; i < m_Leases.getSize(); ++i ) {
			m_Leases[i].nState.store( 3, std::memory_order_release );
		}
		m_bActive = false;
		m_bSealed = false;
		m_Bytes = {};
		m_Staging = {};
	}

public:
	SIRGRDGVectorStagingTransaction( void ) noexcept = default;
	~SIRGRDGVectorStagingTransaction( void ) noexcept
	{
		if ( m_bActive ) {
			const SIRResourceResult eResult = tryCancel();
			if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
				// The transaction must outlive all worker capabilities.
				abort();
			}
		}
	}
	SIRGRDGVectorStagingTransaction( const SIRGRDGVectorStagingTransaction& ) = delete;
	SIRGRDGVectorStagingTransaction& operator=( const SIRGRDGVectorStagingTransaction& ) = delete;

	SIRResourceResult tryBegin( SIRResourceGraphWriter& rWriter, const SIRGRDGVectorStagingRequest& rRequest ) noexcept
	{
		if ( m_bActive || rRequest.keyResource.m_nTypeId == 0 || rRequest.layout.nElementCount == 0 ||
		     rRequest.layout.nSchemaVersion == 0 ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		uint64_t nRequired = 0;
		if ( !SIRGRDGVectorLayoutChecks::totalBytes( rRequest.layout, &nRequired ) || nRequired == 0 || nRequired > SIZE_MAX ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		SIRResourceMutableBytes bytes {};
		SIRResourceStagingHandle staging {};
		const SIRResourceResult eResult =
		    rWriter.allocateStaging( nRequired, rRequest.layout.nElementAlignment, rRequest.nAllocationDomain, &staging, &bytes );
		if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		// Strided layouts and header prefixes contain bytes not written by any
		// element lease. Initialize them deterministically before publication.
		if ( rRequest.layout.nElementStrideBytes != rRequest.layout.nElementBytes || rRequest.layout.nOffsetBytes != 0 ) {
			memset( bytes.m_pData, 0, static_cast<size_t>(nRequired) );
		}
		m_Leases.clear();
		m_pWriter = &rWriter;
		m_Staging = staging;
		m_Bytes = bytes;
		m_Request = rRequest;
		++m_nGeneration;
		m_bActive = true;
		m_bSealed = false;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}

	template <typename T>
	SIRResourceResult tryLeaseRange( const SIRGRDGVectorLeaseRequest& rRange, SIRGRDGWriteOnlyVectorView<T> *pView ) noexcept
	{
		if ( pView == nullptr || !m_bActive || m_bSealed || !SIRGRDGVectorLayoutChecks::isRawTypeCompatible<T>( m_Request.layout ) ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		if ( rRange.nElementCount == 0 || rRange.nFirstElement > m_Request.layout.nElementCount ||
		     rRange.nElementCount > m_Request.layout.nElementCount - rRange.nFirstElement ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		for ( uint32_t i = 0; i < m_Leases.getSize(); ++i ) {
			const SIRGRDGVectorLeaseState& rLease = m_Leases[i];
			if ( rRange.nFirstElement < rLease.nFirst + rLease.nCount && rLease.nFirst < rRange.nFirstElement + rRange.nElementCount ) {
				return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
			}
		}
		SIRGRDGVectorLeaseState *pLease = nullptr;
		const SIRContainerResult eResult =
		    m_Leases.tryEmplaceBack( rRange.nFirstElement, rRange.nElementCount, m_nGeneration, m_Bytes.m_pData );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
		}
		pLease = m_Leases.tryGet( m_Leases.getSize() - 1 );
		pView->m_pLease = pLease;
		pView->m_Layout = m_Request.layout;
		pView->m_nGeneration = m_nGeneration;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}

	SIRResourceResult trySealCompleted( void ) noexcept
	{
		if ( !m_bActive ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		if ( !allLeasesComplete() ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
		m_bSealed = true;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}

	SIRResourceResult trySeal( const SIRTaskGraph& rTaskGraph ) noexcept
	{
		sirTaskGraphExecutionSummary_t summary {};
		if ( !rTaskGraph.queryExecution( &summary ) ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_FAILED;
		}
		if ( summary.eState == SIRTaskGraphExecutionState::SIR_TASK_GRAPH_EXEC_PENDING ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
		if ( summary.eState != SIRTaskGraphExecutionState::SIR_TASK_GRAPH_EXEC_SUCCEEDED ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_FAILED;
		}
		return trySealCompleted();
	}

	SIRResourceResult tryPublish( const SIRResourcePublication& rPublication, const SIRResourceDependencyPlanEntry *pPlan,
	                              uint32_t nPlanCount, SIRResourceHandle *pPublished ) noexcept
	{
		if ( !m_bActive || !m_bSealed || m_pWriter == nullptr || pPublished == nullptr || !allLeasesComplete() ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
		if ( rPublication.m_nChunkCount != 1 || rPublication.m_pChunks == nullptr || rPublication.m_pChunks[0].m_bReuseExisting ||
		     rPublication.m_pChunks[0].m_Staging.m_nSlot != m_Staging.m_nSlot ||
		     rPublication.m_pChunks[0].m_Staging.m_nGeneration != m_Staging.m_nGeneration ||
		     rPublication.m_nFormatId != m_Request.layout.nFormatId || rPublication.m_nSchemaVersion != m_Request.layout.nSchemaVersion ||
		     rPublication.m_Key.m_nAssetId != m_Request.keyResource.m_nAssetId ||
		     rPublication.m_Key.m_nTypeId != m_Request.keyResource.m_nTypeId ||
		     rPublication.m_Key.m_nVariantId != m_Request.keyResource.m_nVariantId ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		if ( nPlanCount != 0 ) {
			const SIRResourceResult ePlan = m_pWriter->validateTaskPlan( pPlan, nPlanCount );
			if ( ePlan != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
				return ePlan;
			}
		}
		const SIRResourceResult ePublished = m_pWriter->publishExpected( rPublication, m_Request.nExpectedRevision, pPublished );
		if ( ePublished == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			invalidate();
		}
		return ePublished;
	}

	SIRResourceResult tryCancel( void ) noexcept
	{
		if ( !m_bActive || m_pWriter == nullptr ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		// Outstanding writes must finish before storage reclamation.
		if ( !allLeasesComplete() && m_Leases.getSize() != 0 ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
		const SIRResourceResult eResult = m_pWriter->cancelStaging( m_Staging );
		if ( eResult == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			invalidate();
		}
		return eResult;
	}
	SIRResourceStagingHandle getStagingHandle( void ) const noexcept
	{
		return m_Staging;
	}
};

// Opaque provider-native backing: no CPU pointer or mutable device borrow.
class SIRGRDGDeviceReadOnlyVectorView
{
private:
	sirSharedDeviceView_t m_View {};
	SIRResourceDerivedRegistry *m_pRegistry = nullptr;

public:
	SIRGRDGDeviceReadOnlyVectorView( void ) noexcept = default;
	~SIRGRDGDeviceReadOnlyVectorView( void ) noexcept
	{
		// Device borrows require explicit completion-aware release.
		assert( m_pRegistry == nullptr );
	}
	SIRGRDGDeviceReadOnlyVectorView( const SIRGRDGDeviceReadOnlyVectorView& ) = delete;
	SIRGRDGDeviceReadOnlyVectorView& operator=( const SIRGRDGDeviceReadOnlyVectorView& ) = delete;
	SIRResourceResult tryAcquire( SIRResourceDerivedRegistry& rRegistry, const SIRResourceReadScope& rScope, const SIRResourceView& rSource,
	                              const sirSharedDeviceViewRequest_t& rRequest ) noexcept
	{
		if ( m_pRegistry != nullptr ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
		sirSharedDeviceView_t acquired {};
		const SIRResourceResult eResult = rRegistry.acquireSharedDeviceView( rScope, rSource, rRequest, &acquired );
		if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		if ( !acquired.bReadOnly ) {
			// A mutable borrow cannot be owned by this adapter.
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		m_View = acquired;
		m_pRegistry = &rRegistry;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
	SIRResourceResult tryRelease( const sirProviderCompletionSet_t& rCompleted ) noexcept
	{
		if ( m_pRegistry == nullptr ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		const SIRResourceResult eResult = m_pRegistry->releaseSharedDeviceView( m_View, rCompleted );
		if ( eResult == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			m_View = {};
			m_pRegistry = nullptr;
		}
		return eResult;
	}
	uint64_t getNativeToken( void ) const noexcept
	{
		return m_View.nNativeResourceToken;
	}
	uint64_t getOffset( void ) const noexcept
	{
		return m_View.nOffset;
	}
	uint64_t getBytes( void ) const noexcept
	{
		return m_View.nBytes;
	}
	uint64_t getSourceRevision( void ) const noexcept
	{
		return m_View.nSourceRevision;
	}
};

#endif // __SIR_GRDGVECTORVIEWS_H__
