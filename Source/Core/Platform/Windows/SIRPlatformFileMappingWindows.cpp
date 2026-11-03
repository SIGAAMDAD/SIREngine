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
      m_eMode( SIR_FILE_MAP_READ_ONLY )
{
}

SIRPlatformFileMapping::~SIRPlatformFileMapping( void ) noexcept
{
	unmap();
}

sirFileResult_t SIRPlatformFileMapping::map( const SIRPlatformFile &rFile, uint64_t nOffset, uint64_t nBytes,
                                             sirFileMapMode_t eMode ) noexcept
{
	if ( isMapped() || !rFile.isOpen() || eMode > SIR_FILE_MAP_COPY_ON_WRITE || ( rFile.m_nFlags & SIR_FILE_OPEN_READ ) == 0 ||
	     ( eMode == SIR_FILE_MAP_READ_WRITE && ( rFile.m_nFlags & SIR_FILE_OPEN_WRITE ) == 0 ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	uint64_t nFileBytes = 0;
	sirFileResult_t eResult = rFile.getSize( &nFileBytes );
	if ( eResult != SIR_FILE_OK ) {
		return eResult;
	}
	if ( nOffset >= nFileBytes || nOffset > ( uint64_t )INT64_MAX ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	if ( nBytes == 0 ) {
		nBytes = nFileBytes - nOffset;
	}
	if ( nBytes > nFileBytes - nOffset ) {
		return SIR_FILE_OUT_OF_RANGE;
	}

	SYSTEM_INFO info = {};
	GetSystemInfo( &info );
	if ( info.dwAllocationGranularity == 0 ) {
		return SIR_FILE_IO_ERROR;
	}
	const uint64_t nAlignment = ( uint64_t )info.dwAllocationGranularity;
	const uint64_t nAlignedOffset = nOffset - nOffset % nAlignment;
	const uint64_t nDelta = nOffset - nAlignedOffset;
	if ( nBytes > SIZE_MAX - nDelta || nAlignedOffset > ( uint64_t )INT64_MAX ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	const uint64_t nMappedBytes = nDelta + nBytes;
	const DWORD nProtection =
	    eMode == SIR_FILE_MAP_READ_ONLY ? PAGE_READONLY : ( eMode == SIR_FILE_MAP_COPY_ON_WRITE ? PAGE_WRITECOPY : PAGE_READWRITE );
	const DWORD nAccess =
	    eMode == SIR_FILE_MAP_READ_ONLY ? FILE_MAP_READ : ( eMode == SIR_FILE_MAP_COPY_ON_WRITE ? FILE_MAP_COPY : FILE_MAP_WRITE );
	HANDLE hMapping = CreateFileMappingW( ( HANDLE )rFile.m_nNativeHandle, nullptr, nProtection, 0, 0, nullptr );
	if ( hMapping == nullptr ) {
		return GetLastError() == ERROR_ACCESS_DENIED ? SIR_FILE_ACCESS_DENIED : SIR_FILE_IO_ERROR;
	}
	void *pBase = MapViewOfFile( hMapping, nAccess, ( DWORD )( nAlignedOffset >> 32 ), ( DWORD )nAlignedOffset, ( SIZE_T )nMappedBytes );
	if ( pBase == nullptr ) {
		const DWORD nError = GetLastError();
		CloseHandle( hMapping );
		return nError == ERROR_ACCESS_DENIED ? SIR_FILE_ACCESS_DENIED : SIR_FILE_IO_ERROR;
	}
	m_pBase = pBase;
	m_pData = ( uint8_t * )pBase + nDelta;
	m_nMappedBytes = nMappedBytes;
	m_nViewOffset = nOffset;
	m_nViewBytes = nBytes;
	m_nNativeMapping = ( uintptr_t )hMapping;
	m_eMode = eMode;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFileMapping::unmap( void ) noexcept
{
	if ( !isMapped() ) {
		return SIR_FILE_OK;
	}
	if ( !UnmapViewOfFile( m_pBase ) ) {
		return SIR_FILE_IO_ERROR;
	}
	const BOOL bClosed = CloseHandle( ( HANDLE )m_nNativeMapping );
	m_pBase = nullptr;
	m_pData = nullptr;
	m_nMappedBytes = 0;
	m_nViewBytes = 0;
	m_nViewOffset = 0;
	m_nNativeMapping = 0;
	m_eMode = SIR_FILE_MAP_READ_ONLY;
	return bClosed ? SIR_FILE_OK : SIR_FILE_IO_ERROR;
}

sirFileResult_t SIRPlatformFileMapping::flush( void ) const noexcept
{
	if ( !isMapped() ) {
		return SIR_FILE_CLOSED;
	}
	if ( m_eMode != SIR_FILE_MAP_READ_WRITE ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	return FlushViewOfFile( m_pBase, ( SIZE_T )m_nMappedBytes ) ? SIR_FILE_OK : SIR_FILE_IO_ERROR;
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
	return m_eMode == SIR_FILE_MAP_READ_ONLY ? nullptr : m_pData;
}

uint64_t SIRPlatformFileMapping::getSize( void ) const noexcept
{
	return m_nViewBytes;
}

uint64_t SIRPlatformFileMapping::getOffset( void ) const noexcept
{
	return m_nViewOffset;
}
