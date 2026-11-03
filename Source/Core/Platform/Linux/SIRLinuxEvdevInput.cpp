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

#include <Core/Platform/Linux/SIRLinuxEvdevInput.h>
#include <Core/Platform/SIRPlatformTime.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

SIRLinuxEvdevInput::SIRLinuxEvdevInput( SIRInputSystem &input ) noexcept : m_Input( input ), m_Devices{}, m_nDeviceCount( 0 )
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		m_Devices[i].nFd = -1;
	}
}

SIRLinuxEvdevInput::~SIRLinuxEvdevInput()
{
	shutdown();
}

bool SIRLinuxEvdevInput::openDevice( const char *pszPath ) noexcept
{
	if ( pszPath == nullptr || m_nDeviceCount >= SIR_INPUT_MAX_DEVICES ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		if ( m_Devices[i].nFd >= 0 ) {
			char szExisting[64];
			char szRequested[64];
			snprintf( szExisting, sizeof( szExisting ), "/proc/self/fd/%d", m_Devices[i].nFd );
			const ssize_t nExisting = readlink( szExisting, szRequested, sizeof( szRequested ) - 1 );
			if ( nExisting > 0 ) {
				szRequested[nExisting] = '\0';
				if ( strcmp( szRequested, pszPath ) == 0 ) {
					return false;
				}
			}
		}
	}
	const int nFd = open( pszPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC );
	if ( nFd < 0 ) {
		return false;
	}
	unsigned long nKeys[( KEY_MAX + ( sizeof( unsigned long ) * 8 ) ) / ( sizeof( unsigned long ) * 8 )] = {};
	if ( ioctl( nFd, EVIOCGBIT( EV_KEY, sizeof( nKeys ) ), nKeys ) < 0 ||
	     !( nKeys[BTN_GAMEPAD / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( BTN_GAMEPAD % ( sizeof( unsigned long ) * 8 ) ) ) ) ) {
		close( nFd );
		return false;
	}
	struct stat info = {};
	if ( fstat( nFd, &info ) != 0 || !S_ISCHR( info.st_mode ) ) {
		close( nFd );
		return false;
	}
	const uint64_t nId = ( ( uint64_t )info.st_rdev << 32 ) ^ ( uint64_t )info.st_ino;
	sirInputDeviceHandle_t hDevice = {};
	if ( !m_Input.addDevice( SIR_INPUT_DEVICE_GAMEPAD, nId, &hDevice ) ) {
		close( nFd );
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		if ( m_Devices[i].nFd == -1 ) {
			m_Devices[i] = { hDevice, nFd, nId, false };
			++m_nDeviceCount;
			if ( !resynchronize( m_Devices[i] ) ) {
				closeSlot( m_Devices[i] );
				return false;
			}
			return true;
		}
	}
	m_Input.removeDevice( hDevice );
	close( nFd );
	return false;
}

uint32_t SIRLinuxEvdevInput::discover( const char *pszDirectory ) noexcept
{
	if ( pszDirectory == nullptr ) {
		return 0;
	}
	DIR *pDirectory = opendir( pszDirectory );
	if ( pDirectory == nullptr ) {
		return 0;
	}
	uint32_t nOpened = 0;
	while ( dirent *pEntry = readdir( pDirectory ) ) {
		if ( strncmp( pEntry->d_name, "event", 5 ) != 0 ) {
			continue;
		}
		char szPath[512];
		const int nSize = snprintf( szPath, sizeof( szPath ), "%s/%s", pszDirectory, pEntry->d_name );
		if ( nSize > 0 && ( size_t )nSize < sizeof( szPath ) && openDevice( szPath ) ) {
			++nOpened;
		}
	}
	closedir( pDirectory );
	return nOpened;
}

bool SIRLinuxEvdevInput::resynchronize( sirEvdevSlot_t &slot ) noexcept
{
	if ( !m_Input.resynchronizeDevice( slot.hDevice ) ) {
		return false;
	}
	unsigned long nKeys[( KEY_MAX + ( sizeof( unsigned long ) * 8 ) ) / ( sizeof( unsigned long ) * 8 )] = {};
	if ( ioctl( slot.nFd, EVIOCGKEY( sizeof( nKeys ) ), nKeys ) < 0 ) {
		return false;
	}
	for ( uint16_t nCode = BTN_MISC; nCode <= KEY_MAX && nCode < SIR_INPUT_MAX_BUTTONS; ++nCode ) {
		const bool bHeld = ( nKeys[nCode / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( nCode % ( sizeof( unsigned long ) * 8 ) ) ) ) != 0;
		if ( bHeld && !m_Input.pushButton( slot.hDevice, nCode, true, 0 ) ) {
			return false;
		}
	}
	for ( uint16_t nAxis = 0; nAxis <= ABS_MAX && nAxis < SIR_INPUT_MAX_AXES; ++nAxis ) {
		input_absinfo axis = {};
		if ( ioctl( slot.nFd, EVIOCGABS( nAxis ), &axis ) == 0 && !m_Input.pushAxis( slot.hDevice, nAxis, axis.value, 0 ) ) {
			return false;
		}
	}
	return true;
}

uint32_t SIRLinuxEvdevInput::pump( void ) noexcept
{
	uint32_t nProcessed = 0;
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		sirEvdevSlot_t &slot = m_Devices[i];
		if ( slot.nFd < 0 ) {
			continue;
		}
		input_event events[32];
		for ( uint32_t nBatch = 0; nBatch < 8; ++nBatch ) {
			const ssize_t nRead = read( slot.nFd, events, sizeof( events ) );
			if ( nRead < 0 && ( errno == EAGAIN || errno == EINTR ) ) {
				break;
			}
			if ( nRead <= 0 || nRead % ( ssize_t )sizeof( input_event ) != 0 ) {
				closeSlot( slot );
				break;
			}
			const uint32_t nEvents = ( uint32_t )( nRead / ( ssize_t )sizeof( input_event ) );
			for ( uint32_t j = 0; j < nEvents; ++j ) {
				const input_event &event = events[j];
				if ( event.type == EV_SYN && event.code == SYN_DROPPED ) {
					slot.bDesynchronized = true;
					continue;
				}
				if ( slot.bDesynchronized ) {
					if ( event.type == EV_SYN && event.code == SYN_REPORT ) {
						slot.bDesynchronized = !resynchronize( slot );
					}
					continue;
				}
				const uint64_t nNow = SIRPlatformTime::getMonotonicNanoseconds();
				bool bSuccess = true;
				if ( event.type == EV_KEY && event.code < SIR_INPUT_MAX_BUTTONS && event.value != 2 ) {
					bSuccess = m_Input.pushButton( slot.hDevice, event.code, event.value != 0, nNow );
				} else if ( event.type == EV_ABS && event.code < SIR_INPUT_MAX_AXES ) {
					bSuccess = m_Input.pushAxis( slot.hDevice, event.code, event.value, nNow );
				}
				if ( !bSuccess ) {
					slot.bDesynchronized = true;
				}
				++nProcessed;
			}
		}
	}
	return nProcessed;
}

void SIRLinuxEvdevInput::closeSlot( sirEvdevSlot_t &slot ) noexcept
{
	if ( slot.nFd >= 0 ) {
		close( slot.nFd );
		m_Input.removeDevice( slot.hDevice );
		slot.nFd = -1;
		--m_nDeviceCount;
	}
}

uint32_t SIRLinuxEvdevInput::getDeviceCount( void ) const noexcept
{
	return m_nDeviceCount;
}

void SIRLinuxEvdevInput::shutdown( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		closeSlot( m_Devices[i] );
	}
}
