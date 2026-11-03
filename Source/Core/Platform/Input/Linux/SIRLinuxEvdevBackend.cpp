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

#include <Core/Platform/Input/Linux/SIRLinuxEvdevBackend.h>
#include <Core/Platform/SIRPlatformTime.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

SIRLinuxEvdevBackend::SIRLinuxEvdevBackend( void ) noexcept : m_Slots{}, m_szDirectory{}, m_nWatchFd( -1 ), m_nWatch( -1 )
{
	for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
		m_Slots[i].nFd = -1;
		m_Slots[i].nRumbleEffect = -1;
	}
}

sirInputBackendId_t SIRLinuxEvdevBackend::id( void ) const noexcept
{
	return SIR_INPUT_LINUX_EVDEV;
}

int16_t SIRLinuxEvdevBackend::mapButton( uint16_t nNative ) noexcept
{
	switch ( nNative ) {
	case BTN_SOUTH:
		return SIR_INPUT_PAD_SOUTH;
	case BTN_EAST:
		return SIR_INPUT_PAD_EAST;
	case BTN_WEST:
		return SIR_INPUT_PAD_WEST;
	case BTN_NORTH:
		return SIR_INPUT_PAD_NORTH;
	case BTN_TL:
		return SIR_INPUT_PAD_LB;
	case BTN_TR:
		return SIR_INPUT_PAD_RB;
	case BTN_SELECT:
		return SIR_INPUT_PAD_BACK;
	case BTN_START:
		return SIR_INPUT_PAD_START;
	case BTN_MODE:
		return SIR_INPUT_PAD_GUIDE;
	case BTN_THUMBL:
		return SIR_INPUT_PAD_L3;
	case BTN_THUMBR:
		return SIR_INPUT_PAD_R3;
	case BTN_DPAD_UP:
		return SIR_INPUT_PAD_DPAD_UP;
	case BTN_DPAD_DOWN:
		return SIR_INPUT_PAD_DPAD_DOWN;
	case BTN_DPAD_LEFT:
		return SIR_INPUT_PAD_DPAD_LEFT;
	case BTN_DPAD_RIGHT:
		return SIR_INPUT_PAD_DPAD_RIGHT;
	case BTN_TL2:
		return SIR_INPUT_PAD_LT_BUTTON;
	case BTN_TR2:
		return SIR_INPUT_PAD_RT_BUTTON;
	default:
		return -1;
	}
}

int16_t SIRLinuxEvdevBackend::mapAxis( uint16_t nNative ) noexcept
{
	switch ( nNative ) {
	case ABS_X:
		return SIR_INPUT_AXIS_LX;
	case ABS_Y:
		return SIR_INPUT_AXIS_LY;
	case ABS_RX:
		return SIR_INPUT_AXIS_RX;
	case ABS_RY:
		return SIR_INPUT_AXIS_RY;
	case ABS_Z:
		return SIR_INPUT_AXIS_LT;
	case ABS_RZ:
		return SIR_INPUT_AXIS_RT;
	default:
		return -1;
	}
}

int32_t SIRLinuxEvdevBackend::normalize( int32_t nValue, int32_t nMinimum, int32_t nMaximum, bool bTrigger ) noexcept
{
	if ( nMaximum <= nMinimum ) {
		return 0;
	}
	const int64_t nClamped = nValue < nMinimum ? nMinimum : ( nValue > nMaximum ? nMaximum : nValue );
	const int64_t nScaled = ( nClamped - nMinimum ) * ( bTrigger ? 32767 : 65534 ) / ( nMaximum - ( int64_t )nMinimum );
	const int32_t nOut = ( int32_t )( bTrigger ? nScaled : nScaled - 32767 );
	return nOut;
}

bool SIRLinuxEvdevBackend::queueChange( sirInputEvdevSlot_t &slot, sirInputSignal_t eSignal, uint16_t nCode, int32_t nValue ) noexcept
{
	if ( slot.nPending >= SIR_INPUT_EVDEV_REPORT_CAPACITY ) {
		slot.bDesynchronized = true;
		slot.nPending = 0;
		return false;
	}
	slot.pending[slot.nPending++] = { eSignal, nCode, nValue };
	return true;
}

bool SIRLinuxEvdevBackend::openDevice( SIRInputService &service, const char *pszPath ) noexcept
{
	if ( pszPath == nullptr || strlen( pszPath ) >= sizeof( m_Slots[0].szPath ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
		if ( m_Slots[i].bOpen && strcmp( pszPath, m_Slots[i].szPath ) == 0 ) {
			return false;
		}
	}
	uint32_t nFree = SIR_INPUT_EVDEV_CAPACITY;
	for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
		if ( !m_Slots[i].bOpen ) {
			nFree = i;
			break;
		}
	}
	if ( nFree == SIR_INPUT_EVDEV_CAPACITY ) {
		return false;
	}
	int nFd = open( pszPath, O_RDWR | O_NONBLOCK | O_CLOEXEC );
	const bool bWrite = nFd >= 0;
	if ( nFd < 0 ) {
		nFd = open( pszPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC );
	}
	if ( nFd < 0 ) {
		return false;
	}
	unsigned long nKeyBits[( KEY_MAX + sizeof( unsigned long ) * 8 ) / ( sizeof( unsigned long ) * 8 )] = {};
	const uint32_t nWord = BTN_GAMEPAD / ( sizeof( unsigned long ) * 8 );
	const uint32_t nMask = BTN_GAMEPAD % ( sizeof( unsigned long ) * 8 );
	struct stat info = {};
	if ( ioctl( nFd, EVIOCGBIT( EV_KEY, sizeof( nKeyBits ) ), nKeyBits ) < 0 || ( nKeyBits[nWord] & ( 1UL << nMask ) ) == 0 ||
	     fstat( nFd, &info ) != 0 || !S_ISCHR( info.st_mode ) ) {
		close( nFd );
		return false;
	}
	const uint64_t nNative = ( ( uint64_t )info.st_rdev << 32 ) ^ ( uint64_t )info.st_ino;
	if ( nNative == 0 ) {
		close( nFd );
		return false;
	}
	sirInputDeviceId_t idOut = {};
	if ( !service.connect( { 0, nNative, id(), SIR_INPUT_CONTROLLER }, &idOut ) ) {
		close( nFd );
		return false;
	}
	sirInputEvdevSlot_t &slot = m_Slots[nFree];
	slot = {};
	slot.nFd = nFd;
	slot.nRumbleEffect = -1;
	slot.bOpen = true;
	slot.id = idOut;
	slot.nNative = nNative;
	memcpy( slot.szPath, pszPath, strlen( pszPath ) + 1 );
	unsigned long nFF[( FF_MAX + sizeof( unsigned long ) * 8 ) / ( sizeof( unsigned long ) * 8 )] = {};
	slot.bHatDPad =
	    ( nKeyBits[BTN_DPAD_UP / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( BTN_DPAD_UP % ( sizeof( unsigned long ) * 8 ) ) ) ) == 0;
	slot.bCanRumble =
	    bWrite && ioctl( nFd, EVIOCGBIT( EV_FF, sizeof( nFF ) ), nFF ) >= 0 &&
	    ( nFF[FF_RUMBLE / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( FF_RUMBLE % ( sizeof( unsigned long ) * 8 ) ) ) ) != 0;
	const uint16_t nCodes[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ };
	for ( uint16_t i = 0; i < SIR_INPUT_AXIS_COUNT; ++i ) {
		input_absinfo axis = {};
		const uint16_t nCode = nCodes[i];
		const bool bValid = ioctl( nFd, EVIOCGABS( nCode ), &axis ) == 0 && axis.maximum > axis.minimum;
		slot.axes[i] = { axis.minimum, axis.maximum, nCode, i >= SIR_INPUT_AXIS_LT, bValid };
	}
	// Initial snapshot from actual evdev state, not assumed-neutral inputs.
	if ( !requery( service, slot ) ) {
		closeDevice( service, slot );
		return false;
	}
	return true;
}

bool SIRLinuxEvdevBackend::discover( SIRInputService &service, const char *pszDir ) noexcept
{
	if ( pszDir == nullptr || strlen( pszDir ) >= sizeof( m_szDirectory ) ) {
		return false;
	}
	if ( m_nWatchFd < 0 ) {
		m_nWatchFd = inotify_init1( IN_NONBLOCK | IN_CLOEXEC );
		if ( m_nWatchFd >= 0 ) {
			m_nWatch = inotify_add_watch( m_nWatchFd, pszDir, IN_CREATE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM );
			if ( m_nWatch < 0 ) {
				close( m_nWatchFd );
				m_nWatchFd = -1;
			}
		}
	}
	memcpy( m_szDirectory, pszDir, strlen( pszDir ) + 1 );
	DIR *pDir = opendir( pszDir );
	if ( pDir == nullptr ) {
		return false;
	}
	while ( dirent *pEntry = readdir( pDir ) ) {
		if ( strncmp( pEntry->d_name, "event", 5 ) != 0 ) {
			continue;
		}
		char szPath[512];
		const int nLen = snprintf( szPath, sizeof( szPath ), "%s/%s", pszDir, pEntry->d_name );
		if ( nLen > 0 && nLen < ( int )sizeof( szPath ) ) {
			openDevice( service, szPath );
		}
	}
	closedir( pDir );
	return true;
}

bool SIRLinuxEvdevBackend::requery( SIRInputService &service, sirInputEvdevSlot_t &slot ) noexcept
{
	if ( !service.loseState( id(), slot.id ) ) {
		return false;
	}
	unsigned long nKeys[( KEY_MAX + sizeof( unsigned long ) * 8 ) / ( sizeof( unsigned long ) * 8 )] = {};
	sirInputControlChange_t changes[SIR_INPUT_EVDEV_REPORT_CAPACITY] = {};
	uint32_t nCount = 0;
	if ( ioctl( slot.nFd, EVIOCGKEY( sizeof( nKeys ) ), nKeys ) < 0 ) {
		return false;
	}
	for ( uint16_t nCode = BTN_GAMEPAD; nCode < KEY_MAX && nCount < SIR_INPUT_EVDEV_REPORT_CAPACITY; ++nCode ) {
		const int16_t nLogical = mapButton( nCode );
		if ( nLogical >= 0 ) {
			const bool bPressed =
			    ( nKeys[nCode / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( nCode % ( sizeof( unsigned long ) * 8 ) ) ) ) != 0;
			if ( bPressed ) {
				changes[nCount++] = { SIR_INPUT_BUTTON, ( uint16_t )nLogical, 1 };
			}
		}
	}
	for ( uint16_t i = 0; i < SIR_INPUT_AXIS_COUNT; ++i ) {
		if ( slot.axes[i].bValid && nCount < SIR_INPUT_EVDEV_REPORT_CAPACITY ) {
			input_absinfo axis = {};
			if ( ioctl( slot.nFd, EVIOCGABS( slot.axes[i].nCode ), &axis ) == 0 ) {
				int32_t nValue = normalize( axis.value, slot.axes[i].nMinimum, slot.axes[i].nMaximum, slot.axes[i].bTrigger );
				if ( i == SIR_INPUT_AXIS_LY || i == SIR_INPUT_AXIS_RY ) {
					nValue = -nValue;
				}
				changes[nCount++] = { SIR_INPUT_AXIS, i, nValue };
			}
		}
	}
	if ( slot.bHatDPad && nCount + 4 <= SIR_INPUT_EVDEV_REPORT_CAPACITY ) {
		input_absinfo x = {}, y = {};
		if ( ioctl( slot.nFd, EVIOCGABS( ABS_HAT0X ), &x ) == 0 && ioctl( slot.nFd, EVIOCGABS( ABS_HAT0Y ), &y ) == 0 ) {
			slot.nHatX = ( int8_t )( x.value < 0 ? -1 : x.value > 0 ? 1 : 0 );
			slot.nHatY = ( int8_t )( y.value < 0 ? -1 : y.value > 0 ? 1 : 0 );
			changes[nCount++] = { SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_LEFT, slot.nHatX < 0 ? 1 : 0 };
			changes[nCount++] = { SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_RIGHT, slot.nHatX > 0 ? 1 : 0 };
			changes[nCount++] = { SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_UP, slot.nHatY < 0 ? 1 : 0 };
			changes[nCount++] = { SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_DOWN, slot.nHatY > 0 ? 1 : 0 };
		}
	}
	return service.emitControls( id(), slot.id, changes, nCount );
}

void SIRLinuxEvdevBackend::closeDevice( SIRInputService &service, sirInputEvdevSlot_t &slot ) noexcept
{
	if ( !slot.bOpen ) {
		return;
	}
	if ( slot.nRumbleEffect >= 0 ) {
		input_event stop = {};
		stop.type = EV_FF;
		stop.code = slot.nRumbleEffect;
		write( slot.nFd, &stop, sizeof( stop ) );
		ioctl( slot.nFd, EVIOCRMFF, slot.nRumbleEffect );
	}
	service.disconnect( id(), slot.id );
	close( slot.nFd );
	slot = {};
	slot.nFd = -1;
	slot.nRumbleEffect = -1;
}

void SIRLinuxEvdevBackend::pollHotplug( SIRInputService &service ) noexcept
{
	if ( m_nWatchFd < 0 ) {
		return;
	}
	alignas( inotify_event ) char szEvents[4096];
	for ( uint32_t pass = 0; pass < 4; ++pass ) {
		const ssize_t nRead = read( m_nWatchFd, szEvents, sizeof( szEvents ) );
		if ( nRead <= 0 ) {
			break;
		}
		size_t nOffset = 0;
		while ( nOffset + sizeof( inotify_event ) <= ( size_t )nRead ) {
			const inotify_event *pEvent = ( const inotify_event * )( szEvents + nOffset );
			const size_t nStep = sizeof( inotify_event ) + pEvent->len;
			if ( nStep > ( size_t )nRead - nOffset ) {
				break;
			}
			if ( ( pEvent->mask & IN_Q_OVERFLOW ) != 0 ) {
				discover( service, m_szDirectory );
			}
			if ( pEvent->len > 5 && strncmp( pEvent->name, "event", 5 ) == 0 ) {
				char szPath[512];
				const int nSize = snprintf( szPath, sizeof( szPath ), "%s/%s", m_szDirectory, pEvent->name );
				if ( nSize > 0 && nSize < ( int )sizeof( szPath ) ) {
					if ( ( pEvent->mask & ( IN_CREATE | IN_MOVED_TO ) ) != 0 ) {
						openDevice( service, szPath );
					} else if ( ( pEvent->mask & ( IN_DELETE | IN_MOVED_FROM ) ) != 0 ) {
						for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
							if ( m_Slots[i].bOpen && strcmp( m_Slots[i].szPath, szPath ) == 0 ) {
								closeDevice( service, m_Slots[i] );
							}
						}
					}
				}
			}
			nOffset += nStep;
		}
	}
}

void SIRLinuxEvdevBackend::resynchronize( SIRInputService &service ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
		if ( m_Slots[i].bOpen && !requery( service, m_Slots[i] ) ) {
			m_Slots[i].bDesynchronized = true;
		}
	}
}

void SIRLinuxEvdevBackend::sample( SIRInputService &service ) noexcept
{
	pollHotplug( service );
	for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
		sirInputEvdevSlot_t &slot = m_Slots[i];
		if ( !slot.bOpen ) {
			continue;
		}
		input_event event = {};
		uint32_t nIterations = 0;
		while ( nIterations++ < 512 ) {
			const ssize_t nRead = read( slot.nFd, &event, sizeof( event ) );
			if ( nRead < 0 && errno == EINTR ) {
				continue;
			}
			if ( nRead < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) {
				break;
			}
			if ( nRead != ( ssize_t )sizeof( event ) ) {
				closeDevice( service, slot );
				break;
			}
			if ( event.type == EV_SYN && event.code == SYN_DROPPED ) {
				slot.bDesynchronized = true;
				slot.nPending = 0;
				continue;
			}
			if ( event.type == EV_SYN && event.code == SYN_REPORT ) {
				if ( slot.bDesynchronized ) {
					slot.bDesynchronized = false;
					slot.nPending = 0;
					requery( service, slot );
				} else if ( slot.nPending != 0 ) {
					service.emitControls( id(), slot.id, slot.pending, slot.nPending );
					slot.nPending = 0;
				}
				continue;
			}
			if ( slot.bDesynchronized ) {
				continue;
			}
			if ( event.type == EV_KEY ) {
				const int16_t nMapped = mapButton( event.code );
				if ( nMapped >= 0 && event.value <= 1 &&
				     ( !slot.bHatDPad || nMapped < SIR_INPUT_PAD_DPAD_UP || nMapped > SIR_INPUT_PAD_DPAD_RIGHT ) ) {
					queueChange( slot, SIR_INPUT_BUTTON, ( uint16_t )nMapped, event.value );
				}
			} else if ( event.type == EV_ABS ) {
				if ( slot.bHatDPad && event.code == ABS_HAT0X ) {
					const int8_t nValue = ( int8_t )( event.value < 0 ? -1 : event.value > 0 ? 1 : 0 );
					if ( nValue != slot.nHatX ) {
						queueChange( slot, SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_LEFT, nValue < 0 ? 1 : 0 );
						queueChange( slot, SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_RIGHT, nValue > 0 ? 1 : 0 );
						slot.nHatX = nValue;
					}
					continue;
				}
				if ( slot.bHatDPad && event.code == ABS_HAT0Y ) {
					const int8_t nValue = ( int8_t )( event.value < 0 ? -1 : event.value > 0 ? 1 : 0 );
					if ( nValue != slot.nHatY ) {
						queueChange( slot, SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_UP, nValue < 0 ? 1 : 0 );
						queueChange( slot, SIR_INPUT_BUTTON, SIR_INPUT_PAD_DPAD_DOWN, nValue > 0 ? 1 : 0 );
						slot.nHatY = nValue;
					}
					continue;
				}
				const int16_t nMapped = mapAxis( event.code );
				if ( nMapped >= 0 && slot.axes[nMapped].bValid ) {
					const sirInputEvdevAxis_t &axis = slot.axes[nMapped];
					int32_t nNormalized = normalize( event.value, axis.nMinimum, axis.nMaximum, axis.bTrigger );
					if ( nMapped == SIR_INPUT_AXIS_LY || nMapped == SIR_INPUT_AXIS_RY ) {
						nNormalized = -nNormalized;
					}
					queueChange( slot, SIR_INPUT_AXIS, ( uint16_t )nMapped, nNormalized );
				}
			}
		}
	}
}

bool SIRLinuxEvdevBackend::vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
		sirInputEvdevSlot_t &slot = m_Slots[i];
		if ( !slot.bOpen || !slot.bCanRumble || slot.id.nSlot != id.nSlot || slot.id.nGeneration != id.nGeneration ) {
			continue;
		}
		ff_effect effect = {};
		effect.type = FF_RUMBLE;
		effect.id = slot.nRumbleEffect;
		effect.u.rumble.strong_magnitude = nLeft;
		effect.u.rumble.weak_magnitude = nRight;
		effect.replay.length = 1000;
		if ( ioctl( slot.nFd, EVIOCSFF, &effect ) != 0 ) {
			return false;
		}
		slot.nRumbleEffect = effect.id;
		input_event play = {};
		play.type = EV_FF;
		play.code = effect.id;
		play.value = ( nLeft || nRight ) ? 1 : 0;
		return write( slot.nFd, &play, sizeof( play ) ) == sizeof( play );
	}
	return false;
}

void SIRLinuxEvdevBackend::stop( SIRInputService &service ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_EVDEV_CAPACITY; ++i ) {
		closeDevice( service, m_Slots[i] );
	}
	if ( m_nWatchFd >= 0 ) {
		close( m_nWatchFd );
	}
	m_nWatchFd = m_nWatch = -1;
	m_szDirectory[0] = '\0';
}
