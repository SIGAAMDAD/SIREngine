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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

SIRPlatformFileMapping::SIRPlatformFileMapping( void ) noexcept
    : m_pBase( nullptr ), m_pData( nullptr ), m_nMappedBytes( 0 ), m_nViewOffset( 0 ), m_nViewBytes( 0 ), m_nNativeMapping( 0 ),
      m_eMode( SIRFileMapMode::SIR_FILE_MAP_READ_ONLY )
{
}

SIRPlatformFileMapping::~SIRPlatformFileMapping( void ) noexcept
{
	unmap();
}

SIRFileResult SIRPlatformFileMapping::map( const SIRPlatformFile& rFile, uint64_t nOffset, uint64_t nBytes,
                                             SIRFileMapMode eMode ) noexcept
{
	if ( isMapped() || !rFile.isOpen() || eMode > SIRFileMapMode::SIR_FILE_MAP_COPY_ON_WRITE || ( rFile.m_nFlags & SIRFileOpenFlag::SIR_FILE_OPEN_READ ) == 0 ||
	     ( eMode == SIRFileMapMode::SIR_FILE_MAP_READ_WRITE && ( rFile.m_nFlags & SIRFileOpenFlag::SIR_FILE_OPEN_WRITE ) == 0 ) ) {
		return SIRFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	uint64_t nFileBytes = 0;
	SIRFileResult eResult = rFile.getSize( &nFileBytes );
	if ( eResult != SIRFileResult::SIR_FILE_OK ) {
		return eResult;
	}
	if ( nOffset >= nFileBytes || nOffset > static_cast<uint64_t>(INT64_MAX) ) {
		return SIRFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	if ( nBytes == 0 ) {
		nBytes = nFileBytes - nOffset;
	}
	if ( nBytes > nFileBytes - nOffset ) {
		return SIRFileResult::SIR_FILE_OUT_OF_RANGE;
	}

	SYSTEM_INFO info {};
	GetSystemInfo( &info );
	if ( info.dwAllocationGranularity == 0 ) {
		return SIRFileResult::SIR_FILE_IO_ERROR;
	}
	const uint64_t nAlignment = static_cast<uint64_t>(info.dwAllocationGranularity);
	const uint64_t nAlignedOffset = nOffset - nOffset % nAlignment;
	const uint64_t nDelta = nOffset - nAlignedOffset;
	if ( nBytes > SIZE_MAX - nDelta || nAlignedOffset > static_cast<uint64_t>(INT64_MAX) ) {
		return SIRFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	const uint64_t nMappedBytes = nDelta + nBytes;
	const DWORD nProtection =
	    eMode == SIRFileMapMode::SIR_FILE_MAP_READ_ONLY ? PAGE_READONLY : ( eMode == SIRFileMapMode::SIR_FILE_MAP_COPY_ON_WRITE ? PAGE_WRITECOPY : PAGE_READWRITE );
	const DWORD nAccess =
	    eMode == SIRFileMapMode::SIR_FILE_MAP_READ_ONLY ? FILE_MAP_READ : ( eMode == SIRFileMapMode::SIR_FILE_MAP_COPY_ON_WRITE ? FILE_MAP_COPY : FILE_MAP_WRITE );
	HANDLE hMapping = CreateFileMappingW( static_cast<HANDLE>(rFile.m_nNativeHandle), nullptr, nProtection, 0, 0, nullptr );
	if ( hMapping == nullptr ) {
		return GetLastError() == ERROR_ACCESS_DENIED ? SIRFileResult::SIR_FILE_ACCESS_DENIED : SIRFileResult::SIR_FILE_IO_ERROR;
	}
	void *pBase = MapViewOfFile( hMapping, nAccess, static_cast<DWORD>( nAlignedOffset >> 32 ), static_cast<DWORD>(nAlignedOffset), static_cast<SIZE_T>(nMappedBytes) );
	if ( pBase == nullptr ) {
		const DWORD nError = GetLastError();
		CloseHandle( hMapping );
		return nError == ERROR_ACCESS_DENIED ? SIRFileResult::SIR_FILE_ACCESS_DENIED : SIRFileResult::SIR_FILE_IO_ERROR;
	}
	m_pBase = pBase;
	m_pData = static_cast<uint8_t *>(pBase) + nDelta;
	m_nMappedBytes = nMappedBytes;
	m_nViewOffset = nOffset;
	m_nViewBytes = nBytes;
	m_nNativeMapping = static_cast<uintptr_t>(hMapping);
	m_eMode = eMode;
	return SIRFileResult::SIR_FILE_OK;
}

SIRFileResult SIRPlatformFileMapping::unmap( void ) noexcept
{
	if ( !isMapped() ) {
		return SIRFileResult::SIR_FILE_OK;
	}
	if ( !UnmapViewOfFile( m_pBase ) ) {
		return SIRFileResult::SIR_FILE_IO_ERROR;
	}
	const BOOL bClosed = CloseHandle( static_cast<HANDLE>(m_nNativeMapping) );
	m_pBase = nullptr;
	m_pData = nullptr;
	m_nMappedBytes = 0;
	m_nViewBytes = 0;
	m_nViewOffset = 0;
	m_nNativeMapping = 0;
	m_eMode = SIRFileMapMode::SIR_FILE_MAP_READ_ONLY;
	return bClosed ? SIRFileResult::SIR_FILE_OK : SIRFileResult::SIR_FILE_IO_ERROR;
}

SIRFileResult SIRPlatformFileMapping::flush( void ) const noexcept
{
	if ( !isMapped() ) {
		return SIRFileResult::SIR_FILE_CLOSED;
	}
	if ( m_eMode != SIRFileMapMode::SIR_FILE_MAP_READ_WRITE ) {
		return SIRFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	return FlushViewOfFile( m_pBase, static_cast<SIZE_T>(m_nMappedBytes) ) ? SIRFileResult::SIR_FILE_OK : SIRFileResult::SIR_FILE_IO_ERROR;
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
