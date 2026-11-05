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

#include <Core/VFS/SIRVFSMountTable.h>
#include <Core/Platform/SIRScopedLock.h>

#include <string.h>

static bool sirVFSCheckPath( const sirVFSPath_t& path ) noexcept
{
	if ( path.nLength >= SIR_VFS_MAX_PATH_BYTES || path.szPath[path.nLength] != '\0' ) {
		return false;
	}
	sirVFSPath_t canonical = {};
	return SIRVFSPath::normalize( path.szPath, path.nLength, &canonical ) == SIR_VFS_PATH_OK &&
	       path.nNamespaceLength == canonical.nNamespaceLength && path.nLength == canonical.nLength &&
	       memcmp( path.szPath, canonical.szPath, path.nLength + 1 ) == 0;
}

static bool sirVFSCheckNamespace( const char *pszName, char *pszCanonical ) noexcept
{
	uint32_t nLength = 0;
	while ( nLength <= SIR_VFS_MAX_NAMESPACE_BYTES && pszName[nLength] != '\0' ) {
		++nLength;
	}
	if ( nLength < 2 || nLength > SIR_VFS_MAX_NAMESPACE_BYTES ) {
		return false;
	}
	char szInput[SIR_VFS_MAX_NAMESPACE_BYTES + 3] = {};
	memcpy( szInput, pszName, nLength );
	szInput[nLength] = ':';
	szInput[nLength + 1] = '/';
	sirVFSPath_t path = {};
	if ( SIRVFSPath::normalize( szInput, nLength + 2, &path ) != SIR_VFS_PATH_OK ) {
		return false;
	}
	memcpy( pszCanonical, path.szPath, nLength );
	pszCanonical[nLength] = '\0';
	return true;
}

static bool sirVFSNamespaceMatches( const sirVFSMountDescriptor_t& descriptor, const sirVFSPath_t& path ) noexcept
{
	return strlen( descriptor.szNamespace ) == path.nNamespaceLength &&
	       memcmp( descriptor.szNamespace, path.szPath, path.nNamespaceLength ) == 0;
}

SIRVFSMountTable::SIRVFSMountTable( void ) noexcept : SIRVFSMountTable( SIR_VFS_MAX_MOUNTS, SIR_VFS_MAX_LEASES )
{
}

SIRVFSMountTable::SIRVFSMountTable( uint32_t nMountCapacity, uint32_t nLeaseCapacity ) noexcept
    : m_szMounts{}, m_szLeases{}, m_nMountCapacity( nMountCapacity ), m_nLeaseCapacity( nLeaseCapacity ), m_nRevision( 1 ),
      m_nSequence( 0 ), m_nNextToken( 1 )
{
	if ( m_nMountCapacity > SIR_VFS_MAX_MOUNTS ) {
		m_nMountCapacity = SIR_VFS_MAX_MOUNTS;
	}
	if ( m_nLeaseCapacity > SIR_VFS_MAX_LEASES ) {
		m_nLeaseCapacity = SIR_VFS_MAX_LEASES;
	}
}

SIRVFSMountTable::~SIRVFSMountTable( void ) noexcept
{
}

uint64_t SIRVFSMountTable::getRevision( void ) const noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	return m_nRevision;
}

sirVFSResult_t SIRVFSMountTable::mount( const sirVFSMountDescriptor_t& descriptor, sirVFSMountHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr || descriptor.nSourceId == 0 || descriptor.nAllowedOps == 0 ||
	     ( descriptor.nAllowedOps & ~( uint32_t )( SIR_VFS_ACCESS_READ | SIR_VFS_ACCESS_WRITE | SIR_VFS_ACCESS_ENUMERATE ) ) != 0 ||
	     descriptor.eKind > SIR_VFS_BACKEND_GENERATED ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	char szNamespace[SIR_VFS_MAX_NAMESPACE_BYTES + 1] = {};
	if ( !sirVFSCheckNamespace( descriptor.szNamespace, szNamespace ) ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	const bool bWritable = strcmp( szNamespace, "user" ) == 0 || strcmp( szNamespace, "cache" ) == 0;
	if ( ( descriptor.nAllowedOps & SIR_VFS_ACCESS_WRITE ) != 0 && ( !bWritable || descriptor.eKind == SIR_VFS_BACKEND_SIRPACK ) ) {
		return SIR_VFS_FORBIDDEN;
	}
	if ( !descriptor.bTrusted && strcmp( szNamespace, "mods" ) != 0 ) {
		return SIR_VFS_FORBIDDEN;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_nSequence == UINT64_MAX || m_nRevision == UINT64_MAX ) {
		return SIR_VFS_NO_CAPACITY;
	}
	for ( uint32_t i = 0; i < m_nMountCapacity; ++i ) {
		sirVFSMountSlot_t& slot = m_szMounts[i];
		if ( slot.bActive || slot.bRetired || slot.nGeneration == UINT32_MAX ) {
			continue;
		}
		slot.descriptor = descriptor;
		memcpy( slot.descriptor.szNamespace, szNamespace, sizeof( szNamespace ) );
		slot.nGeneration++;
		slot.nSequence = ++m_nSequence;
		slot.nPins = 0;
		slot.bActive = true;
		++m_nRevision;
		*pHandle = { i + 1, slot.nGeneration };
		return SIR_VFS_OK;
	}
	return SIR_VFS_NO_CAPACITY;
}

void SIRVFSMountTable::retireSlot( uint32_t nIndex ) noexcept
{
	sirVFSMountSlot_t& slot = m_szMounts[nIndex];
	if ( slot.nPins == 0 ) {
		slot.bRetired = false;
		slot.descriptor = {};
	}
}

sirVFSResult_t SIRVFSMountTable::unmount( sirVFSMountHandle_t handle ) noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > m_nMountCapacity ) {
		return SIR_VFS_STALE_HANDLE;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	sirVFSMountSlot_t& slot = m_szMounts[handle.nSlot - 1];
	if ( !slot.bActive || slot.nGeneration != handle.nGeneration ) {
		return SIR_VFS_STALE_HANDLE;
	}
	if ( m_nRevision == UINT64_MAX ) {
		return SIR_VFS_NO_CAPACITY;
	}
	slot.bActive = false;
	slot.bRetired = true;
	++m_nRevision;
	retireSlot( handle.nSlot - 1 );
	return SIR_VFS_OK;
}

uint32_t SIRVFSMountTable::findFreeLease( void ) const noexcept
{
	for ( uint32_t i = 0; i < m_nLeaseCapacity; ++i ) {
		if ( !m_szLeases[i].bActive ) {
			return i;
		}
	}
	return m_nLeaseCapacity;
}

sirVFSResult_t SIRVFSMountTable::acquireCandidates( const sirVFSPath_t& path, uint32_t nOperation,
                                                    sirVFSCandidateList_t *pCandidates ) noexcept
{
	if ( pCandidates == nullptr || !sirVFSCheckPath( path ) ||
	     ( nOperation != SIR_VFS_ACCESS_READ && nOperation != SIR_VFS_ACCESS_ENUMERATE ) ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	uint32_t szOrder[SIR_VFS_MAX_MOUNTS] = {};
	uint32_t nMatches = 0;
	for ( uint32_t i = 0; i < m_nMountCapacity; ++i ) {
		const sirVFSMountSlot_t& slot = m_szMounts[i];
		if ( !slot.bActive || ( slot.descriptor.nAllowedOps & nOperation ) == 0 || !sirVFSNamespaceMatches( slot.descriptor, path ) ) {
			continue;
		}
		uint32_t j = nMatches;
		while ( j != 0 ) {
			const sirVFSMountSlot_t& prior = m_szMounts[szOrder[j - 1]];
			if ( prior.descriptor.iPriority > slot.descriptor.iPriority ||
			     ( prior.descriptor.iPriority == slot.descriptor.iPriority && prior.nSequence < slot.nSequence ) ) {
				break;
			}
			szOrder[j] = szOrder[j - 1];
			--j;
		}
		szOrder[j] = i;
		++nMatches;
	}
	if ( nMatches == 0 ) {
		return SIR_VFS_NOT_FOUND;
	}
	uint32_t nFree = 0;
	for ( uint32_t i = 0; i < m_nLeaseCapacity; ++i ) {
		nFree += !m_szLeases[i].bActive;
	}
	if ( nMatches > nFree || m_nNextToken > UINT64_MAX - nMatches ) {
		return SIR_VFS_NO_CAPACITY;
	}
	sirVFSCandidateList_t next = {};
	next.nRevision = m_nRevision;
	next.nCount = nMatches;
	for ( uint32_t i = 0; i < nMatches; ++i ) {
		const uint32_t nIndex = szOrder[i];
		sirVFSMountSlot_t& slot = m_szMounts[nIndex];
		const uint32_t nLease = findFreeLease();
		sirVFSLeaseSlot_t& lease = m_szLeases[nLease];
		lease.nToken = m_nNextToken++;
		lease.nMountSlot = nIndex;
		lease.bActive = true;
		++slot.nPins;
		next.szCandidates[i].descriptor = slot.descriptor;
		next.szCandidates[i].lease = { nIndex + 1, slot.nGeneration, lease.nToken };
	}
	*pCandidates = next;
	return SIR_VFS_OK;
}

sirVFSResult_t SIRVFSMountTable::acquireWriteTarget( const sirVFSPath_t& path, sirVFSMountHandle_t handle,
                                                     sirVFSMountCandidate_t *pCandidate ) noexcept
{
	if ( pCandidate == nullptr || !sirVFSCheckPath( path ) || handle.nSlot == 0 || handle.nSlot > m_nMountCapacity ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	sirVFSMountSlot_t& slot = m_szMounts[handle.nSlot - 1];
	if ( !slot.bActive || slot.nGeneration != handle.nGeneration ) {
		return SIR_VFS_STALE_HANDLE;
	}
	if ( !sirVFSNamespaceMatches( slot.descriptor, path ) || ( slot.descriptor.nAllowedOps & SIR_VFS_ACCESS_WRITE ) == 0 ) {
		return SIR_VFS_FORBIDDEN;
	}
	if ( m_nNextToken == UINT64_MAX ) {
		return SIR_VFS_NO_CAPACITY;
	}
	const uint32_t nLease = findFreeLease();
	if ( nLease == m_nLeaseCapacity ) {
		return SIR_VFS_NO_CAPACITY;
	}
	sirVFSLeaseSlot_t& lease = m_szLeases[nLease];
	lease.nToken = m_nNextToken++;
	lease.nMountSlot = handle.nSlot - 1;
	lease.bActive = true;
	++slot.nPins;
	*pCandidate = { slot.descriptor, { handle.nSlot, handle.nGeneration, lease.nToken } };
	return SIR_VFS_OK;
}

sirVFSResult_t SIRVFSMountTable::release( sirVFSMountLease_t lease ) noexcept
{
	if ( lease.nSlot == 0 || lease.nSlot > m_nMountCapacity || lease.nToken == 0 ) {
		return SIR_VFS_STALE_HANDLE;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	sirVFSMountSlot_t& slot = m_szMounts[lease.nSlot - 1];
	if ( slot.nGeneration != lease.nGeneration || slot.nPins == 0 ) {
		return SIR_VFS_STALE_HANDLE;
	}
	for ( uint32_t i = 0; i < m_nLeaseCapacity; ++i ) {
		sirVFSLeaseSlot_t& record = m_szLeases[i];
		if ( record.bActive && record.nToken == lease.nToken && record.nMountSlot == lease.nSlot - 1 ) {
			record.bActive = false;
			--slot.nPins;
			if ( slot.bRetired ) {
				retireSlot( lease.nSlot - 1 );
			}
			return SIR_VFS_OK;
		}
	}
	return SIR_VFS_STALE_HANDLE;
}
