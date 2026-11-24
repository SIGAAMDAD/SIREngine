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

#include <Core/Config/SIRConfigAtomicFile.h>
#include <string.h>

SIRConfigAtomicFile::SIRConfigAtomicFile( void ) noexcept
    : m_File(), m_pszTargetPath( nullptr ), m_pszTempPath( nullptr ), m_nMaxBytes( 0 ), m_nWrittenBytes( 0 ), m_bOpened( false ),
      m_bFailed( false )
{
}

SIRConfigAtomicFile::~SIRConfigAtomicFile( void ) noexcept
{
	cancel();
}

sirConfigSink_t SIRConfigAtomicFile::getSink( void ) noexcept
{
	return { this, sinkWrite };
}

SIRConfigResult SIRConfigAtomicFile::open( const char *pszTargetPath, const char *pszTempPath, uint64_t nMaxBytes ) noexcept
{
	if ( m_bOpened || pszTargetPath == nullptr || pszTempPath == nullptr || nMaxBytes == 0 || *pszTargetPath == 0 || *pszTempPath == 0 ||
	     strcmp( pszTargetPath, pszTempPath ) == 0 ) {
		return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
	}
	const uint32_t nFlags = static_cast<uint32_t>( SIRFileOpenFlag::SIR_FILE_OPEN_WRITE ) | static_cast<uint32_t>( SIRFileOpenFlag::SIR_FILE_OPEN_CREATE ) | static_cast<uint32_t>( SIRFileOpenFlag::SIR_FILE_OPEN_EXCLUSIVE );
	if ( m_File.open( pszTempPath, nFlags ) != SIRFileResult::SIR_FILE_OK ) {
		return SIRConfigResult::SIR_CONFIG_IO_ERROR;
	}
	m_pszTargetPath = pszTargetPath;
	m_pszTempPath = pszTempPath;
	m_nMaxBytes = nMaxBytes;
	m_nWrittenBytes = 0;
	m_bOpened = true;
	m_bFailed = false;
	return SIRConfigResult::SIR_CONFIG_OK;
}

SIRConfigResult SIRConfigAtomicFile::sinkWrite( void *pContext, const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	SIRConfigAtomicFile *pFile = static_cast<SIRConfigAtomicFile *>(pContext);
	if ( pFile == nullptr || !pFile->m_bOpened || pFile->m_bFailed || ( nBytes != 0 && pBytes == nullptr ) ) {
		return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
	}
	if ( nBytes > pFile->m_nMaxBytes - pFile->m_nWrittenBytes ) {
		pFile->m_bFailed = true;
		return SIRConfigResult::SIR_CONFIG_LIMIT_EXCEEDED;
	}
	if ( nBytes == 0 ) {
		return SIRConfigResult::SIR_CONFIG_OK;
	}
	uint64_t nTransferred = 0;
	if ( pFile->m_File.write( pBytes, nBytes, &nTransferred ) != SIRFileResult::SIR_FILE_OK || nTransferred != nBytes ) {
		pFile->m_bFailed = true;
		return SIRConfigResult::SIR_CONFIG_IO_ERROR;
	}
	pFile->m_nWrittenBytes += nBytes;
	return SIRConfigResult::SIR_CONFIG_OK;
}

SIRConfigResult SIRConfigAtomicFile::commit( void ) noexcept
{
	if ( !m_bOpened ) {
		return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
	}
	if ( m_bFailed ) {
		cancel();
		return SIRConfigResult::SIR_CONFIG_IO_ERROR;
	}
	const SIRFileResult eFlush = m_File.flush();
	const SIRFileResult eClose = m_File.close();
	if ( eFlush != SIRFileResult::SIR_FILE_OK || eClose != SIRFileResult::SIR_FILE_OK ||
	     SIRPlatformFile::renameReplace( m_pszTempPath, m_pszTargetPath ) != SIRFileResult::SIR_FILE_OK ) {
		cancel();
		return SIRConfigResult::SIR_CONFIG_IO_ERROR;
	}
	m_bOpened = false;
	m_pszTempPath = nullptr;
	m_pszTargetPath = nullptr;
	return SIRConfigResult::SIR_CONFIG_OK;
}

void SIRConfigAtomicFile::cancel( void ) noexcept
{
	if ( !m_bOpened ) {
		return;
	}
	m_File.close();
	SIRPlatformFile::remove( m_pszTempPath );
	m_bOpened = false;
	m_pszTargetPath = nullptr;
	m_pszTempPath = nullptr;
}

SIRConfigResult SIRConfigAtomicFile::read( const char *pszPath, uint8_t *pBytes, uint32_t nCapacity, uint32_t *pRead ) noexcept
{
	if ( pszPath == nullptr || pBytes == nullptr || pRead == nullptr || nCapacity == 0 ) {
		return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
	}
	*pRead = 0;
	SIRPlatformFile file;
	const SIRFileResult eOpen = file.open( pszPath, static_cast<uint32_t>( SIRFileOpenFlag::SIR_FILE_OPEN_READ ) );
	if ( eOpen == SIRFileResult::SIR_FILE_NOT_FOUND ) {
		return SIRConfigResult::SIR_CONFIG_NOT_FOUND;
	}
	if ( eOpen != SIRFileResult::SIR_FILE_OK ) {
		return SIRConfigResult::SIR_CONFIG_IO_ERROR;
	}
	uint64_t nSize = 0;
	if ( file.getSize( &nSize ) != SIRFileResult::SIR_FILE_OK || nSize > nCapacity ) {
		file.close();
		return SIRConfigResult::SIR_CONFIG_LIMIT_EXCEEDED;
	}
	uint64_t nRead = 0;
	const SIRFileResult eRead = file.read( pBytes, nSize, &nRead );
	const SIRFileResult eClose = file.close();
	if ( ( eRead != SIRFileResult::SIR_FILE_OK && eRead != SIRFileResult::SIR_FILE_END_OF_FILE ) || nRead != nSize || eClose != SIRFileResult::SIR_FILE_OK ) {
		return SIRConfigResult::SIR_CONFIG_IO_ERROR;
	}
	*pRead = static_cast<uint32_t>(nRead);
	return SIRConfigResult::SIR_CONFIG_OK;
}
