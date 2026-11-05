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

#include <Core/VFS/SIRVFSFileHandleTable.h>
#include <Core/Platform/SIRScopedLock.h>

#include <stdlib.h>
#include <string.h>

static bool sirVFSCanonicalFilePath( const sirVFSPath_t& path ) noexcept
{
	if ( path.nLength >= SIR_VFS_MAX_PATH_BYTES || path.szPath[path.nLength] != '\0' ) {
		return false;
	}
	sirVFSPath_t canonical = {};
	return SIRVFSPath::normalize( path.szPath, path.nLength, &canonical ) == SIR_VFS_PATH_OK && path.nLength == canonical.nLength &&
	       path.nNamespaceLength == canonical.nNamespaceLength && memcmp( path.szPath, canonical.szPath, path.nLength + 1 ) == 0;
}

SIRVFSFileHandleTable::SIRVFSFileHandleTable( SIRVFSMountTable& table, uint32_t nFileCapacity, uint32_t nBorrowCapacity ) noexcept
    : m_Table( table ), m_szFiles{}, m_szBorrows{}, m_nFileCapacity( nFileCapacity ), m_nBorrowCapacity( nBorrowCapacity ),
      m_nNextToken( 1 ), m_bStopped( false )
{
	if ( m_nFileCapacity > SIR_VFS_MAX_OPEN_FILES ) {
		m_nFileCapacity = SIR_VFS_MAX_OPEN_FILES;
	}
	if ( m_nBorrowCapacity > SIR_VFS_MAX_FILE_BORROWS ) {
		m_nBorrowCapacity = SIR_VFS_MAX_FILE_BORROWS;
	}
}

SIRVFSFileHandleTable::~SIRVFSFileHandleTable( void ) noexcept
{
	if ( shutdown() == SIR_VFS_BUSY ) {
		abort();
	}
}

uint32_t SIRVFSFileHandleTable::findFreeFile( void ) const noexcept
{
	for ( uint32_t i = 0; i < m_nFileCapacity; ++i ) {
		if ( !m_szFiles[i].bOpen && !m_szFiles[i].bClosing && m_szFiles[i].nGeneration != UINT32_MAX ) {
			return i;
		}
	}
	return m_nFileCapacity;
}

uint32_t SIRVFSFileHandleTable::findFreeBorrow( void ) const noexcept
{
	for ( uint32_t i = 0; i < m_nBorrowCapacity; ++i ) {
		if ( !m_szBorrows[i].bActive ) {
			return i;
		}
	}
	return m_nBorrowCapacity;
}

void SIRVFSFileHandleTable::retireFile( uint32_t nIndex ) noexcept
{
	sirVFSFileSlot_t& slot = m_szFiles[nIndex];
	const sirVFSMountLease_t lease = slot.mountLease;
	const uint32_t nGeneration = slot.nGeneration;
	slot = {};
	slot.nGeneration = nGeneration;
	( void )m_Table.release( lease );
}

sirVFSResult_t SIRVFSFileHandleTable::openResolved( const sirVFSPath_t& path, sirVFSMountLease_t sourceLease,
                                                    sirVFSFileHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr || !sirVFSCanonicalFilePath( path ) ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bStopped ) {
		return SIR_VFS_STOPPED;
	}
	const uint32_t nIndex = findFreeFile();
	if ( nIndex == m_nFileCapacity ) {
		return SIR_VFS_NO_CAPACITY;
	}
	sirVFSMountCandidate_t retained = {};
	const sirVFSResult_t eRetain = m_Table.retain( sourceLease, &retained );
	if ( eRetain != SIR_VFS_OK ) {
		return eRetain;
	}
	if ( ( retained.descriptor.nAllowedOps & SIR_VFS_ACCESS_READ ) == 0 ||
	     strlen( retained.descriptor.szNamespace ) != path.nNamespaceLength ||
	     memcmp( retained.descriptor.szNamespace, path.szPath, path.nNamespaceLength ) != 0 ) {
		( void )m_Table.release( retained.lease );
		return SIR_VFS_FORBIDDEN;
	}
	sirVFSFileSlot_t& slot = m_szFiles[nIndex];
	slot.nGeneration++;
	slot.descriptor = retained.descriptor;
	slot.mountLease = retained.lease;
	slot.path = path;
	slot.nBorrows = 0;
	slot.bOpen = true;
	slot.bClosing = false;
	*pHandle = { nIndex + 1, slot.nGeneration };
	return SIR_VFS_OK;
}

sirVFSResult_t SIRVFSFileHandleTable::borrow( sirVFSFileHandle_t handle, sirVFSFileView_t *pView ) noexcept
{
	if ( pView == nullptr || handle.nSlot == 0 || handle.nSlot > m_nFileCapacity ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bStopped ) {
		return SIR_VFS_STOPPED;
	}
	sirVFSFileSlot_t& slot = m_szFiles[handle.nSlot - 1];
	if ( !slot.bOpen || slot.nGeneration != handle.nGeneration ) {
		return SIR_VFS_STALE_HANDLE;
	}
	if ( m_nNextToken == UINT64_MAX ) {
		return SIR_VFS_NO_CAPACITY;
	}
	const uint32_t nBorrow = findFreeBorrow();
	if ( nBorrow == m_nBorrowCapacity ) {
		return SIR_VFS_NO_CAPACITY;
	}
	sirVFSFileBorrowSlot_t& ticket = m_szBorrows[nBorrow];
	ticket.nToken = m_nNextToken++;
	ticket.nFileSlot = handle.nSlot - 1;
	ticket.bActive = true;
	++slot.nBorrows;
	*pView = { slot.descriptor, slot.path, { handle.nSlot, handle.nGeneration, ticket.nToken } };
	return SIR_VFS_OK;
}

sirVFSResult_t SIRVFSFileHandleTable::retainBorrow( sirVFSFileBorrow_t borrow, sirVFSFileView_t *pView ) noexcept
{
	if ( pView == nullptr ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	if ( borrow.nSlot == 0 || borrow.nSlot > m_nFileCapacity || borrow.nToken == 0 ) {
		return SIR_VFS_STALE_HANDLE;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bStopped ) {
		return SIR_VFS_STOPPED;
	}
	sirVFSFileSlot_t& slot = m_szFiles[borrow.nSlot - 1];
	if ( slot.nGeneration != borrow.nGeneration || slot.nBorrows == 0 || ( !slot.bOpen && !slot.bClosing ) ) {
		return SIR_VFS_STALE_HANDLE;
	}
	bool bFound = false;
	for ( uint32_t i = 0; i < m_nBorrowCapacity; ++i ) {
		const sirVFSFileBorrowSlot_t& record = m_szBorrows[i];
		if ( record.bActive && record.nFileSlot == borrow.nSlot - 1 && record.nToken == borrow.nToken ) {
			bFound = true;
			break;
		}
	}
	if ( !bFound ) {
		return SIR_VFS_STALE_HANDLE;
	}
	if ( m_nNextToken == UINT64_MAX ) {
		return SIR_VFS_NO_CAPACITY;
	}
	const uint32_t nBorrow = findFreeBorrow();
	if ( nBorrow == m_nBorrowCapacity ) {
		return SIR_VFS_NO_CAPACITY;
	}
	sirVFSFileBorrowSlot_t& retained = m_szBorrows[nBorrow];
	retained.nToken = m_nNextToken++;
	retained.nFileSlot = borrow.nSlot - 1;
	retained.bActive = true;
	++slot.nBorrows;
	*pView = { slot.descriptor, slot.path, { borrow.nSlot, borrow.nGeneration, retained.nToken } };
	return SIR_VFS_OK;
}

sirVFSResult_t SIRVFSFileHandleTable::close( sirVFSFileHandle_t handle ) noexcept
{
	if ( handle.nSlot == 0 || handle.nSlot > m_nFileCapacity ) {
		return SIR_VFS_STALE_HANDLE;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	sirVFSFileSlot_t& slot = m_szFiles[handle.nSlot - 1];
	if ( !slot.bOpen || slot.nGeneration != handle.nGeneration ) {
		return SIR_VFS_STALE_HANDLE;
	}
	slot.bOpen = false;
	slot.bClosing = true;
	if ( slot.nBorrows == 0 ) {
		retireFile( handle.nSlot - 1 );
	}
	return SIR_VFS_OK;
}

sirVFSResult_t SIRVFSFileHandleTable::releaseBorrow( sirVFSFileBorrow_t borrow ) noexcept
{
	if ( borrow.nSlot == 0 || borrow.nSlot > m_nFileCapacity || borrow.nToken == 0 ) {
		return SIR_VFS_STALE_HANDLE;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	sirVFSFileSlot_t& slot = m_szFiles[borrow.nSlot - 1];
	if ( slot.nGeneration != borrow.nGeneration || slot.nBorrows == 0 ) {
		return SIR_VFS_STALE_HANDLE;
	}
	for ( uint32_t i = 0; i < m_nBorrowCapacity; ++i ) {
		sirVFSFileBorrowSlot_t& ticket = m_szBorrows[i];
		if ( ticket.bActive && ticket.nToken == borrow.nToken && ticket.nFileSlot == borrow.nSlot - 1 ) {
			ticket.bActive = false;
			--slot.nBorrows;
			if ( slot.bClosing && slot.nBorrows == 0 ) {
				retireFile( borrow.nSlot - 1 );
			}
			return SIR_VFS_OK;
		}
	}
	return SIR_VFS_STALE_HANDLE;
}

sirVFSResult_t SIRVFSFileHandleTable::shutdown( void ) noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bStopped ) {
		return SIR_VFS_STOPPED;
	}
	for ( uint32_t i = 0; i < m_nBorrowCapacity; ++i ) {
		if ( m_szBorrows[i].bActive ) {
			return SIR_VFS_BUSY;
		}
	}
	for ( uint32_t i = 0; i < m_nFileCapacity; ++i ) {
		if ( m_szFiles[i].bOpen || m_szFiles[i].bClosing ) {
			retireFile( i );
		}
	}
	m_bStopped = true;
	return SIR_VFS_OK;
}
