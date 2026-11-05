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

#include <Core/VFS/SIRVFSDirectorySource.h>
#include <Core/Platform/SIRScopedLock.h>

#include <string.h>

SIRVFSDirectorySource::SIRVFSDirectorySource( void ) noexcept : m_Descriptor{}, m_bReady( false ), m_bRetired( false )
{
}

SIRVFSDirectorySource::~SIRVFSDirectorySource( void ) noexcept
{
	( void )close();
}

bool SIRVFSDirectorySource::isOpen( void ) const noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	return m_bReady;
}

sirVFSResult_t SIRVFSDirectorySource::fileResult( sirFileResult_t eResult ) noexcept
{
	switch ( eResult ) {
	case SIR_FILE_OK:
		return SIR_VFS_OK;
	case SIR_FILE_NOT_FOUND:
		return SIR_VFS_NOT_FOUND;
	case SIR_FILE_ACCESS_DENIED:
		return SIR_VFS_FORBIDDEN;
	case SIR_FILE_INVALID_ARGUMENT:
		return SIR_VFS_INVALID_ARGUMENT;
	case SIR_FILE_CLOSED:
		return SIR_VFS_STOPPED;
	case SIR_FILE_NO_SPACE:
		return SIR_VFS_NO_CAPACITY;
	default:
		return SIR_VFS_IO_ERROR;
	}
}

sirVFSResult_t SIRVFSDirectorySource::bind( const char *pszRootPath, const sirVFSMountDescriptor_t& descriptor ) noexcept
{
	if ( pszRootPath == nullptr || descriptor.eKind != SIR_VFS_BACKEND_DIRECTORY || descriptor.nSourceId == 0 ||
	     ( descriptor.nAllowedOps & SIR_VFS_ACCESS_READ ) == 0 ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	uint32_t nNamespaceLength = 0;
	while ( nNamespaceLength <= SIR_VFS_MAX_NAMESPACE_BYTES && descriptor.szNamespace[nNamespaceLength] != '\0' ) {
		++nNamespaceLength;
	}
	if ( nNamespaceLength < 2 || nNamespaceLength > SIR_VFS_MAX_NAMESPACE_BYTES ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	char szNamespaceRoot[SIR_VFS_MAX_NAMESPACE_BYTES + 3] = {};
	memcpy( szNamespaceRoot, descriptor.szNamespace, nNamespaceLength );
	szNamespaceRoot[nNamespaceLength] = ':';
	szNamespaceRoot[nNamespaceLength + 1] = '/';
	sirVFSPath_t canonical = {};
	if ( SIRVFSPath::normalize( szNamespaceRoot, nNamespaceLength + 2, &canonical ) != SIR_VFS_PATH_OK ||
	     memcmp( szNamespaceRoot, canonical.szPath, nNamespaceLength + 2 ) != 0 ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bRetired || m_bReady ) {
		return SIR_VFS_STOPPED;
	}
	const sirVFSResult_t eOpen = fileResult( m_Root.openRoot( pszRootPath ) );
	if ( eOpen != SIR_VFS_OK ) {
		return eOpen;
	}
	m_Descriptor = descriptor;
	m_bReady = true;
	return SIR_VFS_OK;
}

sirVFSResult_t SIRVFSDirectorySource::openRead( SIRVFSFileHandleTable& files, sirVFSFileBorrow_t ticket, SIRPlatformFile *pFile ) noexcept
{
	if ( pFile == nullptr || pFile->isOpen() ) {
		return SIR_VFS_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bReady ) {
		return SIR_VFS_STOPPED;
	}
	sirVFSFileView_t retained = {};
	const sirVFSResult_t eRetain = files.retainBorrow( ticket, &retained );
	if ( eRetain != SIR_VFS_OK ) {
		return eRetain;
	}
	const sirVFSMountDescriptor_t& source = retained.descriptor;
	const uint32_t nNamespaceLength = retained.path.nNamespaceLength;
	const bool bMatch =
	    source.eKind == SIR_VFS_BACKEND_DIRECTORY && ( source.nAllowedOps & SIR_VFS_ACCESS_READ ) != 0 &&
	    source.nSourceId == m_Descriptor.nSourceId && source.nSourceRevision == m_Descriptor.nSourceRevision &&
	    source.nOwnerScope == m_Descriptor.nOwnerScope && source.bTrusted == m_Descriptor.bTrusted &&
	    strcmp( source.szNamespace, m_Descriptor.szNamespace ) == 0 && nNamespaceLength == strlen( m_Descriptor.szNamespace ) &&
	    memcmp( retained.path.szPath, m_Descriptor.szNamespace, nNamespaceLength ) == 0 && retained.path.nLength > nNamespaceLength + 2 &&
	    retained.path.szPath[nNamespaceLength] == ':' && retained.path.szPath[nNamespaceLength + 1] == '/';
	sirVFSResult_t eResult = SIR_VFS_FORBIDDEN;
	if ( bMatch ) {
		const char *pszRelativePath = retained.path.szPath + nNamespaceLength + 2;
		eResult = fileResult( m_Root.openRead( pszRelativePath, pFile ) );
	}
	const sirVFSResult_t eRelease = files.releaseBorrow( retained.borrow );
	if ( eRelease != SIR_VFS_OK ) {
		( void )pFile->close();
		return eRelease;
	}
	return eResult;
}

sirVFSResult_t SIRVFSDirectorySource::close( void ) noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( m_bRetired ) {
		return SIR_VFS_STOPPED;
	}
	m_bRetired = true;
	m_bReady = false;
	return fileResult( m_Root.close() );
}
