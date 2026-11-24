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

#include <Core/Platform/SIRPlatformFileMapping.h>

#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>

SIRPlatformFileMapping::SIRPlatformFileMapping( void ) noexcept
    : m_pBase( nullptr ), m_pData( nullptr ), m_nMappedBytes( 0 ), m_nViewOffset( 0 ), m_nViewBytes( 0 ), m_eMode( SIRFileMapMode::SIR_FILE_MAP_READ_ONLY )
{
}

SIRPlatformFileMapping::~SIRPlatformFileMapping( void ) noexcept
{
	unmap();
}

SIRFileResult SIRPlatformFileMapping::map( const SIRPlatformFile& rFile, uint64_t nOffset, uint64_t nBytes,
                                             SIRFileMapMode eMode ) noexcept
{
	if ( isMapped() || !rFile.isOpen() || eMode > SIRFileMapMode::SIR_FILE_MAP_COPY_ON_WRITE || ( rFile.m_nFlags & static_cast<uint32_t>( SIRFileOpenFlag::SIR_FILE_OPEN_READ ) ) == 0 ||
	     ( eMode == SIRFileMapMode::SIR_FILE_MAP_READ_WRITE && ( rFile.m_nFlags & static_cast<uint32_t>( SIRFileOpenFlag::SIR_FILE_OPEN_WRITE ) ) == 0 ) ) {
		return SIRFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	uint64_t nFileBytes = 0;
	SIRFileResult eResult = rFile.getSize( &nFileBytes );
	if ( eResult != SIRFileResult::SIR_FILE_OK ) {
		return eResult;
	}
	// getSize() reads a signed off_t, so a valid offset strictly smaller
	// than nFileBytes cannot exceed INT64_MAX.
	if ( nOffset >= nFileBytes ) {
		return SIRFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	if ( nBytes == 0 ) {
		nBytes = nFileBytes - nOffset;
	}
	if ( nBytes > nFileBytes - nOffset ) {
		return SIRFileResult::SIR_FILE_OUT_OF_RANGE;
	}

	const long nPage = sysconf( _SC_PAGESIZE );
	if ( nPage <= 0 ) {
		return SIRFileResult::SIR_FILE_IO_ERROR;
	}
	const uint64_t nAlignment = static_cast<uint64_t>(nPage);
	const uint64_t nAlignedOffset = nOffset - ( nOffset % nAlignment );
	const uint64_t nDelta = nOffset - nAlignedOffset;
	// nDelta + nBytes <= nFileBytes - nAlignedOffset <= INT64_MAX.
	// The size_t overflow guard remains necessary only for 32-bit Linux.
	if constexpr ( sizeof( size_t ) < sizeof( uint64_t ) ) {
		if ( nBytes > SIZE_MAX - nDelta ) {
			return SIRFileResult::SIR_FILE_OUT_OF_RANGE;
		}
	}
	const uint64_t nMappedBytes = nDelta + nBytes;
	const int nProtection = PROT_READ | ( eMode == SIRFileMapMode::SIR_FILE_MAP_READ_ONLY ? 0 : PROT_WRITE );
	const int nMode = eMode == SIRFileMapMode::SIR_FILE_MAP_READ_WRITE ? MAP_SHARED : MAP_PRIVATE;
	void *pBase = mmap( nullptr, static_cast<size_t>(nMappedBytes), nProtection, nMode, static_cast<int>(rFile.m_nNativeHandle), static_cast<off_t>(nAlignedOffset) );
	if ( pBase == MAP_FAILED ) {
		return errno == EACCES || errno == EPERM ? SIRFileResult::SIR_FILE_ACCESS_DENIED : SIRFileResult::SIR_FILE_IO_ERROR;
	}
	m_pBase = pBase;
	m_pData = static_cast<uint8_t *>(pBase) + nDelta;
	m_nMappedBytes = nMappedBytes;
	m_nViewOffset = nOffset;
	m_nViewBytes = nBytes;
	m_eMode = eMode;
	return SIRFileResult::SIR_FILE_OK;
}

SIRFileResult SIRPlatformFileMapping::unmap( void ) noexcept
{
	if ( !isMapped() ) {
		return SIRFileResult::SIR_FILE_OK;
	}
	const int nResult = munmap( m_pBase, static_cast<size_t>(m_nMappedBytes) );
	if ( nResult != 0 ) {
		return SIRFileResult::SIR_FILE_IO_ERROR;
	}
	m_pBase = nullptr;
	m_pData = nullptr;
	m_nMappedBytes = 0;
	m_nViewBytes = 0;
	m_nViewOffset = 0;
	m_eMode = SIRFileMapMode::SIR_FILE_MAP_READ_ONLY;
	return SIRFileResult::SIR_FILE_OK;
}

SIRFileResult SIRPlatformFileMapping::flush( void ) const noexcept
{
	if ( !isMapped() ) {
		return SIRFileResult::SIR_FILE_CLOSED;
	}
	if ( m_eMode != SIRFileMapMode::SIR_FILE_MAP_READ_WRITE ) {
		return SIRFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	return msync( m_pBase, static_cast<size_t>(m_nMappedBytes), MS_SYNC ) == 0 ? SIRFileResult::SIR_FILE_OK : SIRFileResult::SIR_FILE_IO_ERROR;
}

bool SIRPlatformFileMapping::isMapped( void ) const noexcept
{
	return m_pBase != nullptr;
}

const uint8_t *SIRPlatformFileMapping::getData( void ) const noexcept
{
	return m_pData;
}

uint8_t *SIRPlatformFileMapping::getWritableData( void ) noexcept
{
	return m_eMode == SIRFileMapMode::SIR_FILE_MAP_READ_ONLY ? nullptr : m_pData;
}

uint64_t SIRPlatformFileMapping::getSize( void ) const noexcept
{
	return m_nViewBytes;
}

uint64_t SIRPlatformFileMapping::getOffset( void ) const noexcept
{
	return m_nViewOffset;
}
