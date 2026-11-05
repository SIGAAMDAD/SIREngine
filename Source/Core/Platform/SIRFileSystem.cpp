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

#include <Core/Platform/SIRFileSystem.h>

#include <string.h>

const char *SIRFileSystem::lastSeparator( const char *pszPath ) noexcept
{
	const char *pszLast = nullptr;
	for ( const char *pszCurrent = pszPath; *pszCurrent != '\0'; ++pszCurrent ) {
		if ( *pszCurrent == '/'
#if defined( _WIN32 )
		     || *pszCurrent == '\\'
#endif
		) {
			pszLast = pszCurrent;
		}
	}
	return pszLast;
}

static size_t sirFileSystemBoundedLength( const char *pszText ) noexcept
{
	size_t nBytes = 0;
	while ( nBytes < SIR_FILE_SYSTEM_MAX_PATH_BYTES && pszText[nBytes] != '\0' ) {
		++nBytes;
	}
	return nBytes;
}

bool SIRFileSystem::validPaths( const char *pszDestination, const char *pszTemporary ) noexcept
{
	if ( pszDestination == nullptr || pszTemporary == nullptr ) {
		return false;
	}
	const size_t nDestination = sirFileSystemBoundedLength( pszDestination );
	const size_t nTemporary = sirFileSystemBoundedLength( pszTemporary );
	if ( nDestination == 0 || nTemporary == 0 || nDestination == SIR_FILE_SYSTEM_MAX_PATH_BYTES ||
	     nTemporary == SIR_FILE_SYSTEM_MAX_PATH_BYTES || strcmp( pszDestination, pszTemporary ) == 0 ) {
		return false;
	}
	const char *pszDestinationEnd = lastSeparator( pszDestination );
	const char *pszTemporaryEnd = lastSeparator( pszTemporary );
	if ( ( pszDestinationEnd == nullptr ) != ( pszTemporaryEnd == nullptr ) ) {
		return false;
	}
	if ( pszDestinationEnd == nullptr ) {
		return true;
	}
	const size_t nDestinationParent = ( size_t )( pszDestinationEnd - pszDestination );
	const size_t nTemporaryParent = ( size_t )( pszTemporaryEnd - pszTemporary );
	return nDestinationParent == nTemporaryParent && memcmp( pszDestination, pszTemporary, nDestinationParent ) == 0 &&
	       pszDestinationEnd[1] != '\0' && pszTemporaryEnd[1] != '\0';
}

sirFileSystemResult_t SIRFileSystem::readFile( const char *pszPath, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept
{
	if ( pOutBytes == nullptr ) {
		return SIR_FILE_SYSTEM_INVALID_ARGUMENT;
	}
	*pOutBytes = 0;
	if ( ( pszOutput == nullptr ) != ( nCapacity == 0 ) ) {
		return SIR_FILE_SYSTEM_INVALID_ARGUMENT;
	}
	if ( pszPath == nullptr ) {
		return SIR_FILE_SYSTEM_INVALID_PATH;
	}
	const size_t nPathBytes = sirFileSystemBoundedLength( pszPath );
	if ( nPathBytes == 0 || nPathBytes == SIR_FILE_SYSTEM_MAX_PATH_BYTES ) {
		return SIR_FILE_SYSTEM_INVALID_PATH;
	}
	return readFilePlatform( pszPath, pszOutput, nCapacity, pOutBytes );
}

sirFileSystemResult_t SIRFileSystem::writeAtomically( const sirFileSystemAtomicWriteRequest_t &request ) noexcept
{
	if ( request.pBytes == nullptr || request.nBytes == 0 || ( request.pExpectedBytes == nullptr && request.nExpectedBytes != 0 ) ) {
		return SIR_FILE_SYSTEM_INVALID_ARGUMENT;
	}
	if ( !validPaths( request.pszDestination, request.pszTemporary ) ) {
		return SIR_FILE_SYSTEM_INVALID_PATH;
	}
	return writeAtomicallyPlatform( request );
}
