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

#include <Scripting/Core/SIRScriptCachedProgramSource.h>
#include <string.h>

SIRScriptCachedProgramSource::SIRScriptCachedProgramSource( void ) noexcept
    : m_pCache( nullptr ), m_Allocator{}, m_OwnerThread( std::this_thread::get_id() ), m_nMaximumResidentBytes( 0 ), m_nResidentBytes( 0 ),
      m_nPeakResidentBytes( 0 ), m_szEntries{}, m_szLeases{}, m_bInAllocator( false )
{
}

SIRScriptCachedProgramSource::~SIRScriptCachedProgramSource( void ) noexcept
{
	( void )shutdown();
}

bool SIRScriptCachedProgramSource::isOwnerThread( void ) const noexcept
{
	return std::this_thread::get_id() == m_OwnerThread;
}

uint32_t SIRScriptCachedProgramSource::nextGeneration( uint32_t nGeneration ) noexcept
{
	++nGeneration;
	return nGeneration == 0 ? 1 : nGeneration;
}

bool SIRScriptCachedProgramSource::validDescriptor( const sirScriptCachedArtifact_t &descriptor ) noexcept
{
	const sirScriptCodeArtifact_t &a = descriptor.artifact;
	if ( a.nBackendId == 0 || a.nProgramId == 0 || a.nRevision == 0 || a.nFormatId == 0 || a.nSchemaFingerprint == 0 || a.nTargetId == 0 ||
	     a.nBuildFingerprint == 0 || descriptor.nBytes == 0 || descriptor.nBytes > UINT32_MAX || descriptor.nAlignment == 0 ||
	     descriptor.nAlignment > 4096 || ( descriptor.nAlignment & ( descriptor.nAlignment - 1 ) ) != 0 ) {
		return false;
	}
	uint8_t nDigest = 0;
	for ( uint32_t iByte = 0; iByte < 32; ++iByte ) {
		nDigest |= descriptor.key.szDigest[iByte];
	}
	return nDigest != 0;
}

bool SIRScriptCachedProgramSource::sameDescriptor( const sirScriptCachedArtifact_t &a, const sirScriptCachedArtifact_t &b ) noexcept
{
	return a.artifact.nBackendId == b.artifact.nBackendId && a.artifact.nProgramId == b.artifact.nProgramId &&
	       a.artifact.nRevision == b.artifact.nRevision && a.artifact.nFormatId == b.artifact.nFormatId &&
	       a.artifact.nSchemaFingerprint == b.artifact.nSchemaFingerprint && a.artifact.nTargetId == b.artifact.nTargetId &&
	       a.artifact.nBuildFingerprint == b.artifact.nBuildFingerprint && a.nBytes == b.nBytes && a.nAlignment == b.nAlignment &&
	       memcmp( a.key.szDigest, b.key.szDigest, 32 ) == 0 && memcmp( a.szPayloadDigest, b.szPayloadDigest, 32 ) == 0;
}

sirScriptResult_t SIRScriptCachedProgramSource::configure( SIRScriptArtifactCache *pCache, const sirScriptPayloadAllocatorAPI_t &allocator,
                                                           uint64_t nMaximumResidentBytes ) noexcept
{
	if ( !isOwnerThread() || m_bInAllocator ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_pCache != nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pCache == nullptr || allocator.nABIRevision != SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION || allocator.pAllocate == nullptr ||
	     allocator.pRelease == nullptr || allocator.nMaximumBytes == 0 || allocator.nMaximumAlignment == 0 ||
	     ( allocator.nMaximumAlignment & ( allocator.nMaximumAlignment - 1 ) ) != 0 || nMaximumResidentBytes == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	m_pCache = pCache;
	m_Allocator = allocator;
	m_nMaximumResidentBytes = nMaximumResidentBytes;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptCachedProgramSource::registerArtifact( const sirScriptCachedArtifact_t &descriptor,
                                                                  sirScriptCachedEntryHandle_t *pHandle ) noexcept
{
	if ( !isOwnerThread() || m_bInAllocator ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_pCache == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pHandle == nullptr || !validDescriptor( descriptor ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( descriptor.nBytes > m_Allocator.nMaximumBytes || descriptor.nAlignment > m_Allocator.nMaximumAlignment ||
	     descriptor.nBytes > m_nMaximumResidentBytes ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	int32_t iFree = -1;
	for ( uint32_t iEntry = 0; iEntry < MAX_ENTRIES; ++iEntry ) {
		const sirScriptCachedSourceEntry_t &entry = m_szEntries[iEntry];
		if ( !entry.bOccupied && iFree < 0 ) {
			iFree = ( int32_t )iEntry;
		}
		if ( entry.bOccupied && entry.descriptor.artifact.nBackendId == descriptor.artifact.nBackendId &&
		     entry.descriptor.artifact.nProgramId == descriptor.artifact.nProgramId &&
		     entry.descriptor.artifact.nRevision == descriptor.artifact.nRevision ) {
			return sameDescriptor( entry.descriptor, descriptor ) ? SIR_SCRIPT_DUPLICATE : SIR_SCRIPT_TYPE_MISMATCH;
		}
	}
	if ( iFree < 0 ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	sirScriptCachedSourceEntry_t &entry = m_szEntries[iFree];
	entry.descriptor = descriptor;
	entry.nGeneration = nextGeneration( entry.nGeneration );
	entry.nPins = 0;
	entry.allocation = {};
	entry.bOccupied = true;
	*pHandle = { ( uint32_t )iFree + 1, entry.nGeneration };
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptCachedProgramSource::unregisterArtifact( sirScriptCachedEntryHandle_t handle ) noexcept
{
	if ( !isOwnerThread() || m_bInAllocator ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( handle.nSlot == 0 || handle.nSlot > MAX_ENTRIES ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	sirScriptCachedSourceEntry_t &entry = m_szEntries[handle.nSlot - 1];
	if ( !entry.bOccupied || entry.nGeneration != handle.nGeneration ) {
		return SIR_SCRIPT_INVALID_HANDLE;
	}
	if ( entry.nPins != 0 ) {
		return SIR_SCRIPT_BUSY;
	}
	releaseEntry( handle.nSlot - 1 );
	entry.bOccupied = false;
	entry.nGeneration = nextGeneration( entry.nGeneration );
	entry.descriptor = {};
	return SIR_SCRIPT_OK;
}

void SIRScriptCachedProgramSource::releaseEntry( uint32_t iEntry ) noexcept
{
	sirScriptCachedSourceEntry_t &entry = m_szEntries[iEntry];
	if ( entry.nPins != 0 || entry.allocation.nToken == 0 ) {
		return;
	}
	const sirScriptPayloadAllocation_t allocation = entry.allocation;
	entry.allocation = {};
	m_nResidentBytes -= entry.descriptor.nBytes;
	m_bInAllocator = true;
	m_Allocator.pRelease( m_Allocator.pUser, allocation );
	m_bInAllocator = false;
}

sirScriptResult_t SIRScriptCachedProgramSource::acquire( uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
                                                         sirScriptProgramView_t *pView, uint64_t *pToken ) noexcept
{
	if ( !isOwnerThread() || m_bInAllocator ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( m_pCache == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pView == nullptr || pToken == nullptr || nBackendId == 0 || nProgramId == 0 || nRevision == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	int32_t iEntry = -1;
	for ( uint32_t i = 0; i < MAX_ENTRIES; ++i ) {
		const sirScriptCachedSourceEntry_t &entry = m_szEntries[i];
		if ( entry.bOccupied && entry.descriptor.artifact.nBackendId == nBackendId && entry.descriptor.artifact.nProgramId == nProgramId &&
		     entry.descriptor.artifact.nRevision == nRevision ) {
			iEntry = ( int32_t )i;
			break;
		}
	}
	if ( iEntry < 0 ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	int32_t iLease = -1;
	for ( uint32_t i = 0; i < MAX_LEASES; ++i ) {
		if ( !m_szLeases[i].bOccupied ) {
			iLease = ( int32_t )i;
			break;
		}
	}
	if ( iLease < 0 ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}

	sirScriptCachedSourceEntry_t &entry = m_szEntries[iEntry];
	if ( entry.allocation.nToken == 0 ) {
		if ( entry.descriptor.nBytes > m_nMaximumResidentBytes - m_nResidentBytes ) {
			return SIR_SCRIPT_CAPACITY_EXCEEDED;
		}
		sirScriptPayloadAllocation_t allocation = {};
		m_bInAllocator = true;
		const sirScriptResult_t eAllocated =
		    m_Allocator.pAllocate( m_Allocator.pUser, ( uint32_t )entry.descriptor.nBytes, entry.descriptor.nAlignment, &allocation );
		m_bInAllocator = false;
		if ( eAllocated != SIR_SCRIPT_OK ) {
			// Allocators must return no owned allocation on failure.
			return eAllocated;
		}
		if ( allocation.pData == nullptr || allocation.nToken == 0 ||
		     ( ( uintptr_t )allocation.pData & ( entry.descriptor.nAlignment - 1 ) ) != 0 ) {
			if ( allocation.pData != nullptr && allocation.nToken != 0 ) {
				m_bInAllocator = true;
				m_Allocator.pRelease( m_Allocator.pUser, allocation );
				m_bInAllocator = false;
			}
			return SIR_SCRIPT_BACKEND_FAILURE;
		}
		const sirScriptResult_t eFetch = m_pCache->fetch( entry.descriptor, allocation.pData, entry.descriptor.nBytes );
		if ( eFetch != SIR_SCRIPT_OK ) {
			m_bInAllocator = true;
			m_Allocator.pRelease( m_Allocator.pUser, allocation );
			m_bInAllocator = false;
			return eFetch;
		}
		entry.allocation = allocation;
		m_nResidentBytes += entry.descriptor.nBytes;
		if ( m_nResidentBytes > m_nPeakResidentBytes ) {
			m_nPeakResidentBytes = m_nResidentBytes;
		}
	}

	sirScriptCachedSourceLease_t &lease = m_szLeases[iLease];
	lease.nGeneration = nextGeneration( lease.nGeneration );
	lease.iEntry = ( uint32_t )iEntry;
	lease.bOccupied = true;
	++entry.nPins;
	*pView = { nProgramId,
	           nRevision,
	           entry.descriptor.artifact.nFormatId,
	           entry.allocation.pData,
	           entry.descriptor.nBytes,
	           entry.descriptor.nAlignment };
	*pToken = ( ( uint64_t )lease.nGeneration << 32 ) | ( uint32_t )( iLease + 1 );
	return SIR_SCRIPT_OK;
}

void SIRScriptCachedProgramSource::release( uint64_t nToken ) noexcept
{
	if ( !isOwnerThread() || m_bInAllocator ) {
		return;
	}
	const uint32_t nSlot = ( uint32_t )nToken;
	const uint32_t nGeneration = ( uint32_t )( nToken >> 32 );
	if ( nSlot == 0 || nSlot > MAX_LEASES || nGeneration == 0 ) {
		return;
	}
	sirScriptCachedSourceLease_t &lease = m_szLeases[nSlot - 1];
	if ( !lease.bOccupied || lease.nGeneration != nGeneration ) {
		return;
	}
	lease.bOccupied = false;
	sirScriptCachedSourceEntry_t &entry = m_szEntries[lease.iEntry];
	if ( entry.nPins == 0 ) {
		return;
	}
	--entry.nPins;
	if ( entry.nPins == 0 ) {
		releaseEntry( lease.iEntry );
	}
}

sirScriptResult_t SIRScriptCachedProgramSource::acquireThunk( void *pUser, uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
                                                              sirScriptProgramView_t *pView, uint64_t *pToken ) noexcept
{
	if ( pUser == nullptr ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	return ( ( SIRScriptCachedProgramSource * )pUser )->acquire( nBackendId, nProgramId, nRevision, pView, pToken );
}

void SIRScriptCachedProgramSource::releaseThunk( void *pUser, uint64_t nToken ) noexcept
{
	if ( pUser != nullptr ) {
		( ( SIRScriptCachedProgramSource * )pUser )->release( nToken );
	}
}

sirScriptProgramSourceAPI_t SIRScriptCachedProgramSource::makeProgramSourceAPI( void ) noexcept
{
	return { SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION, this, acquireThunk, releaseThunk };
}

sirScriptResult_t SIRScriptCachedProgramSource::shutdown( void ) noexcept
{
	if ( !isOwnerThread() || m_bInAllocator ) {
		return SIR_SCRIPT_BUSY;
	}
	for ( uint32_t iLease = 0; iLease < MAX_LEASES; ++iLease ) {
		if ( m_szLeases[iLease].bOccupied ) {
			return SIR_SCRIPT_BUSY;
		}
	}
	for ( uint32_t iEntry = 0; iEntry < MAX_ENTRIES; ++iEntry ) {
		sirScriptCachedSourceEntry_t &entry = m_szEntries[iEntry];
		if ( entry.allocation.nToken != 0 ) {
			releaseEntry( iEntry );
		}
		entry.bOccupied = false;
		entry.descriptor = {};
		entry.nGeneration = nextGeneration( entry.nGeneration );
	}
	m_pCache = nullptr;
	m_Allocator = {};
	m_nMaximumResidentBytes = 0;
	return SIR_SCRIPT_OK;
}

sirScriptCachedSourceStats_t SIRScriptCachedProgramSource::stats( void ) const noexcept
{
	sirScriptCachedSourceStats_t result = {};
	if ( !isOwnerThread() || m_bInAllocator ) {
		return result;
	}
	result.nResidentBytes = m_nResidentBytes;
	result.nPeakResidentBytes = m_nPeakResidentBytes;
	for ( uint32_t iEntry = 0; iEntry < MAX_ENTRIES; ++iEntry ) {
		const sirScriptCachedSourceEntry_t &entry = m_szEntries[iEntry];
		if ( entry.bOccupied ) {
			++result.nRegisteredArtifacts;
			if ( entry.allocation.nToken != 0 ) {
				++result.nResidentArtifacts;
			}
		}
		result.nActiveLeases += entry.nPins;
	}
	return result;
}
