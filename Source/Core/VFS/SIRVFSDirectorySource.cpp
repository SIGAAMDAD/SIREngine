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

#include <Core/VFS/SIRVFSDirectorySource.h>
#include <Core/Platform/Shared/Threading/SIRScopedLock.h>

#include <string.h>

SIRVFSDirectorySource::SIRVFSDirectorySource( void ) noexcept
    : m_pMountTable( nullptr ), m_MountLease{}, m_Descriptor{}, m_bReady( false ), m_bRetired( false )
{
}

SIRVFSDirectorySource::~SIRVFSDirectorySource( void ) noexcept
{
	static_cast<void>(close());
}

bool SIRVFSDirectorySource::isOpen( void ) const noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	return m_bReady;
}

SIRVFSResult SIRVFSDirectorySource::fileResult( sirFileResult eResult ) noexcept
{
	switch ( eResult ) {
	case sirFileResult::SIR_FILE_OK:
		return SIRVFSResult::SIR_VFS_OK;
	case sirFileResult::SIR_FILE_NOT_FOUND:
		return SIRVFSResult::SIR_VFS_NOT_FOUND;
	case sirFileResult::SIR_FILE_ACCESS_DENIED:
		return SIRVFSResult::SIR_VFS_FORBIDDEN;
	case sirFileResult::SIR_FILE_INVALID_ARGUMENT:
		return SIRVFSResult::SIR_VFS_INVALID_ARGUMENT;
	case sirFileResult::SIR_FILE_CLOSED:
		return SIRVFSResult::SIR_VFS_STOPPED;
	case sirFileResult::SIR_FILE_NO_SPACE:
		return SIRVFSResult::SIR_VFS_NO_CAPACITY;
	default:
		return SIRVFSResult::SIR_VFS_IO_ERROR;
	}
}

SIRVFSResult SIRVFSDirectorySource::bind( SIRVFSMountTable& table, sirVFSMountLease_t lease, const char *pszRootPath ) noexcept
{
	if ( pszRootPath == nullptr || pszRootPath[0] == '\0' ) {
		return SIRVFSResult::SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bRetired || m_bReady ) {
		return SIRVFSResult::SIR_VFS_STOPPED;
	}
	// Authenticate the mount slot and generation against the live lease pool.
	// This source owns a separate mount pin until terminal close.
	sirVFSMountCandidate_t retained {};
	const SIRVFSResult eRetain = table.retain( lease, &retained );
	if ( eRetain != SIRVFSResult::SIR_VFS_OK ) {
		return eRetain;
	}
	if ( retained.descriptor.eKind != SIRVFSBackendKind::SIR_VFS_BACKEND_DIRECTORY || ( retained.descriptor.nAllowedOps & static_cast<uint32_t>( SIRVFSAccess::SIR_VFS_ACCESS_READ ) ) == 0 ) {
		static_cast<void>(table.release( retained.lease ));
		return SIRVFSResult::SIR_VFS_FORBIDDEN;
	}
	const SIRVFSResult eOpen = fileResult( m_Root.openRoot( pszRootPath ) );
	if ( eOpen != SIRVFSResult::SIR_VFS_OK ) {
		static_cast<void>(table.release( retained.lease ));
		return eOpen;
	}
	m_pMountTable = &table;
	m_MountLease = retained.lease;
	m_Descriptor = retained.descriptor;
	m_bReady = true;
	return SIRVFSResult::SIR_VFS_OK;
}

SIRVFSResult SIRVFSDirectorySource::openRead( SIRVFSFileHandleTable& files, sirVFSFileBorrow_t ticket, SIRPlatformFile *pFile ) noexcept
{
	if ( pFile == nullptr || pFile->isOpen() ) {
		return SIRVFSResult::SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bReady ) {
		return SIRVFSResult::SIR_VFS_STOPPED;
	}
	sirVFSFileView_t retained {};
	const SIRVFSResult eRetain = files.retainBorrow( ticket, &retained );
	if ( eRetain != SIRVFSResult::SIR_VFS_OK ) {
		return eRetain;
	}
	const sirVFSMountDescriptor_t& source = retained.descriptor;
	const uint32_t nNamespaceLength = retained.path.nNamespaceLength;
	const bool bMatch = files.usesMountTable( *m_pMountTable ) && retained.sourceMount.nSlot == m_MountLease.nSlot &&
	                    retained.sourceMount.nGeneration == m_MountLease.nGeneration && source.eKind == SIRVFSBackendKind::SIR_VFS_BACKEND_DIRECTORY &&
	                    ( source.nAllowedOps & static_cast<uint32_t>( SIRVFSAccess::SIR_VFS_ACCESS_READ ) ) != 0 && source.nSourceId == m_Descriptor.nSourceId &&
	                    source.nSourceRevision == m_Descriptor.nSourceRevision && source.nOwnerScope == m_Descriptor.nOwnerScope &&
	                    source.bTrusted == m_Descriptor.bTrusted && strcmp( source.szNamespace, m_Descriptor.szNamespace ) == 0 &&
	                    nNamespaceLength == strlen( m_Descriptor.szNamespace ) &&
	                    memcmp( retained.path.szPath, m_Descriptor.szNamespace, nNamespaceLength ) == 0 &&
	                    retained.path.nLength > nNamespaceLength + 2 && retained.path.szPath[nNamespaceLength] == ':' &&
	                    retained.path.szPath[nNamespaceLength + 1] == '/';
	SIRVFSResult eResult = SIRVFSResult::SIR_VFS_FORBIDDEN;
	if ( bMatch ) {
		const char *pszRelativePath = retained.path.szPath + nNamespaceLength + 2;
		eResult = fileResult( m_Root.openRead( pszRelativePath, pFile ) );
	}
	const SIRVFSResult eRelease = files.releaseBorrow( retained.borrow );
	if ( eRelease != SIRVFSResult::SIR_VFS_OK ) {
		static_cast<void>(pFile->close());
		return eRelease;
	}
	return eResult;
}

SIRVFSResult SIRVFSDirectorySource::close( void ) noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bRetired ) {
		return SIRVFSResult::SIR_VFS_STOPPED;
	}
	m_bRetired = true;
	m_bReady = false;
	const SIRVFSResult eClose = fileResult( m_Root.close() );
	if ( m_pMountTable != nullptr ) {
		const SIRVFSResult eRelease = m_pMountTable->release( m_MountLease );
		m_pMountTable = nullptr;
		m_MountLease = {};
		if ( eRelease != SIRVFSResult::SIR_VFS_OK ) {
			return eRelease;
		}
	}
	return eClose;
}
