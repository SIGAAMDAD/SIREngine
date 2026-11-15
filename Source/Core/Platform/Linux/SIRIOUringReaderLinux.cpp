/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/IO/SIRIOUringReader.h>

#include <errno.h>
#include <limits.h>
#include <linux/io_uring.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

static constexpr uint32_t SIR_IO_URING_DEPTH = 8;
static constexpr uint32_t SIR_IO_URING_READ_BYTES = 1U << 20;

SIRIOUringReader::SIRIOUringReader( void ) noexcept
    : m_nRing( -1 ), m_pSQMapping( nullptr ), m_pCQMapping( nullptr ), m_pEntriesMapping( nullptr ), m_nSQMapBytes( 0 ), m_nCQMapBytes( 0 ),
      m_nEntriesMapBytes( 0 ), m_bSingleMapping( false ), m_pSQHead( nullptr ), m_pSQTail( nullptr ), m_pSQMask( nullptr ),
      m_pSQArray( nullptr ), m_pCQHead( nullptr ), m_pCQTail( nullptr ), m_pCQMask( nullptr ), m_pSQEntries( nullptr ),
      m_pCQEntries( nullptr )
{
}

SIRIOUringReader::~SIRIOUringReader( void ) noexcept
{
	shutdown();
}

bool SIRIOUringReader::initialize( void ) noexcept
{
	if ( m_nRing >= 0 ) {
		return true;
	}

#if !defined( __NR_io_uring_setup ) || !defined( __NR_io_uring_enter )
	return false;
#else
	io_uring_params params = {};
	const int nRing = ( int )syscall( __NR_io_uring_setup, SIR_IO_URING_DEPTH, &params );
	if ( nRing < 0 ) {
		return false;
	}
	m_nRing = nRing;
	m_nSQMapBytes = params.sq_off.array + ( uint64_t )params.sq_entries * sizeof( uint32_t );
	m_nCQMapBytes = params.cq_off.cqes + ( uint64_t )params.cq_entries * sizeof( io_uring_cqe );
	m_nEntriesMapBytes = ( uint64_t )params.sq_entries * sizeof( io_uring_sqe );
	m_bSingleMapping = ( params.features & IORING_FEAT_SINGLE_MMAP ) != 0;
	if ( m_bSingleMapping && m_nCQMapBytes > m_nSQMapBytes ) {
		m_nSQMapBytes = m_nCQMapBytes;
	}
	m_pSQMapping = mmap( nullptr, ( size_t )m_nSQMapBytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, m_nRing, IORING_OFF_SQ_RING );
	if ( m_pSQMapping == MAP_FAILED ) {
		m_pSQMapping = nullptr;
		shutdown();
		return false;
	}

	if ( m_bSingleMapping ) {
		m_pCQMapping = m_pSQMapping;
	} else {
		m_pCQMapping =
		    mmap( nullptr, ( size_t )m_nCQMapBytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, m_nRing, IORING_OFF_CQ_RING );
		if ( m_pCQMapping == MAP_FAILED ) {
			m_pCQMapping = nullptr;
			shutdown();
			return false;
		}
	}

	m_pEntriesMapping =
	    mmap( nullptr, ( size_t )m_nEntriesMapBytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, m_nRing, IORING_OFF_SQES );
	if ( m_pEntriesMapping == MAP_FAILED ) {
		m_pEntriesMapping = nullptr;
		shutdown();
		return false;
	}

	uint8_t *pSQ = ( uint8_t * )m_pSQMapping;
	uint8_t *pCQ = ( uint8_t * )m_pCQMapping;
	m_pSQHead = ( uint32_t * )( pSQ + params.sq_off.head );
	m_pSQTail = ( uint32_t * )( pSQ + params.sq_off.tail );
	m_pSQMask = ( uint32_t * )( pSQ + params.sq_off.ring_mask );
	m_pSQArray = ( uint32_t * )( pSQ + params.sq_off.array );
	m_pCQHead = ( uint32_t * )( pCQ + params.cq_off.head );
	m_pCQTail = ( uint32_t * )( pCQ + params.cq_off.tail );
	m_pCQMask = ( uint32_t * )( pCQ + params.cq_off.ring_mask );
	m_pSQEntries = m_pEntriesMapping;
	m_pCQEntries = pCQ + params.cq_off.cqes;
	return true;
#endif
}

void SIRIOUringReader::shutdown( void ) noexcept
{
	if ( m_pEntriesMapping != nullptr ) {
		munmap( m_pEntriesMapping, ( size_t )m_nEntriesMapBytes );
	}
	if ( m_pCQMapping != nullptr && !m_bSingleMapping ) {
		munmap( m_pCQMapping, ( size_t )m_nCQMapBytes );
	}
	if ( m_pSQMapping != nullptr ) {
		munmap( m_pSQMapping, ( size_t )m_nSQMapBytes );
	}
	if ( m_nRing >= 0 ) {
		close( m_nRing );
	}
	m_nRing = -1;
	m_pSQMapping = nullptr;
	m_pCQMapping = nullptr;
	m_pEntriesMapping = nullptr;
	m_pSQEntries = nullptr;
	m_pCQEntries = nullptr;
	m_pSQHead = nullptr;
	m_pSQTail = nullptr;
	m_pCQHead = nullptr;
	m_pCQTail = nullptr;
}

bool SIRIOUringReader::isAvailable( void ) const noexcept
{
	return m_nRing >= 0;
}

bool SIRIOUringReader::submitRead( int nFile, uint64_t nOffset, uint8_t *pOutput, uint32_t nBytes, int32_t *pResult ) noexcept
{
#if !defined( __NR_io_uring_enter )
	return false;
#else
	// Exactly one request is outstanding in this worker-owned ring. The
	// submission and completion sequence never exposes a borrowed destination
	// to a second operation before its kernel CQE has been consumed.
	const uint32_t nTail = __atomic_load_n( m_pSQTail, __ATOMIC_RELAXED );
	const uint32_t nHead = __atomic_load_n( m_pSQHead, __ATOMIC_ACQUIRE );
	if ( nTail - nHead >= SIR_IO_URING_DEPTH ) {
		return false;
	}
	const uint32_t iEntry = nTail & *m_pSQMask;
	io_uring_sqe *pEntry = ( io_uring_sqe * )m_pSQEntries + iEntry;
	memset( pEntry, 0, sizeof( *pEntry ) );
	pEntry->opcode = IORING_OP_READ;
	pEntry->fd = nFile;
	pEntry->off = nOffset;
	pEntry->addr = ( uint64_t )( uintptr_t )pOutput;
	pEntry->len = nBytes;
	pEntry->user_data = 1;
	m_pSQArray[iEntry] = iEntry;
	__atomic_store_n( m_pSQTail, nTail + 1, __ATOMIC_RELEASE );

	// EINTR is not a terminal outcome. Once submitted, the buffer remains
	// pinned until an actual completion is observed; the caller must not
	// substitute a fallback read while the kernel may still access it.
	bool bSubmitted = false;
	while ( !bSubmitted ) {
		const int nSubmitted = ( int )syscall( __NR_io_uring_enter, m_nRing, 1U, 0U, 0U, nullptr, 0U );
		if ( nSubmitted == 1 ) {
			bSubmitted = true;
		} else if ( nSubmitted < 0 && errno != EINTR ) {
			// A permanent submission failure invalidates this ring. No read
			// was accepted by the kernel on this error path.
			return false;
		}
	}

	for ( ;; ) {
		const uint32_t nCQHead = __atomic_load_n( m_pCQHead, __ATOMIC_RELAXED );
		const uint32_t nCQTail = __atomic_load_n( m_pCQTail, __ATOMIC_ACQUIRE );
		if ( nCQHead != nCQTail ) {
			const io_uring_cqe *pCompletion = ( io_uring_cqe * )m_pCQEntries + ( nCQHead & *m_pCQMask );
			*pResult = pCompletion->res;
			__atomic_store_n( m_pCQHead, nCQHead + 1, __ATOMIC_RELEASE );
			return true;
		}
		const int nWait = ( int )syscall( __NR_io_uring_enter, m_nRing, 0U, 1U, IORING_ENTER_GETEVENTS, nullptr, 0U );
		if ( nWait < 0 && errno != EINTR ) {
			// A post-submit error must not release an in-flight buffer.
			// Retry the completion wait; ownership remains with the worker.
			continue;
		}
	}
#endif
}

sirFileResult_t SIRIOUringReader::readAt( const SIRPlatformFile& file, uint64_t nOffset, void *pOutput, uint64_t nBytes,
                                          uint64_t *pTransferred ) noexcept
{
	if ( m_nRing < 0 ) {
		return file.readAt( nOffset, pOutput, nBytes, pTransferred );
	}
	if ( pTransferred == nullptr || pOutput == nullptr || nBytes == 0 || !file.isOpen() || nOffset > INT64_MAX ||
	     nBytes > INT64_MAX - nOffset ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	uint8_t *pBytes = ( uint8_t * )pOutput;
	while ( *pTransferred < nBytes ) {
		const uint64_t nRemaining = nBytes - *pTransferred;
		const uint32_t nStep = nRemaining > SIR_IO_URING_READ_BYTES ? SIR_IO_URING_READ_BYTES : ( uint32_t )nRemaining;
		int32_t nResult = 0;
		if ( !submitRead( ( int )file.m_nNativeHandle, nOffset + *pTransferred, pBytes + *pTransferred, nStep, &nResult ) ) {
			return SIR_FILE_IO_ERROR;
		}
		if ( nResult < 0 ) {
			if ( nResult == -EINTR || nResult == -EAGAIN ) {
				continue;
			}
			return SIR_FILE_IO_ERROR;
		}
		if ( nResult == 0 ) {
			return SIR_FILE_END_OF_FILE;
		}
		*pTransferred += ( uint32_t )nResult;
	}
	return SIR_FILE_OK;
}
