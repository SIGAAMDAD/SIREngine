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

#include <Core/Platform/SIRFileLock.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

SIRFileLock::SIRFileLock( const char *pszLockPath ) noexcept : m_nHandle( -1 )
{
	if ( pszLockPath == nullptr || pszLockPath[0] == '\0' ) {
		return;
	}

	const int nDescriptor = open( pszLockPath, O_RDWR | O_CREAT | O_CLOEXEC, 0600 );
	if ( nDescriptor < 0 ) {
		return;
	}

	while ( flock( nDescriptor, LOCK_EX ) != 0 ) {
		if ( errno != EINTR ) {
			( void )close( nDescriptor );
			return;
		}
	}

	m_nHandle = nDescriptor;
}

SIRFileLock::~SIRFileLock( void ) noexcept
{
	if ( m_nHandle >= 0 ) {
		( void )flock( ( int )m_nHandle, LOCK_UN );
		( void )close( ( int )m_nHandle );
	}
}

bool SIRFileLock::isLocked( void ) const noexcept
{
	return m_nHandle >= 0;
}
