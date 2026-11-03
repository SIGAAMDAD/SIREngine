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
#include <sys/inotify.h>
#include <unistd.h>

SIRLinuxEvdevInput::SIRLinuxEvdevInput( SIRInputSystem &input ) noexcept
    : m_Input( input ), m_Devices{}, m_nDeviceCount( 0 ), m_nNotifyFd( -1 ), m_nNotifyWatch( -1 ), m_szWatchDirectory{}
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		m_Devices[i].nFd = -1;
	}
}

bool SIRLinuxEvdevInput::registerProfile( const sirInputControllerProfile_t &profile ) noexcept
{
	// A connected device holds a stable pointer to its selected mapping.
	return m_nDeviceCount == 0 && m_Profiles.addProfile( profile );
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
	if ( strlen( pszPath ) >= sizeof( m_Devices[0].szPath ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		if ( m_Devices[i].nFd >= 0 && strcmp( m_Devices[i].szPath, pszPath ) == 0 ) {
			return false;
		}
	}
	int nFd = open( pszPath, O_RDWR | O_NONBLOCK | O_CLOEXEC );
	const bool bWritable = nFd >= 0;
	if ( nFd < 0 ) {
		nFd = open( pszPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC );
	}
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
			sirEvdevSlot_t &slot = m_Devices[i];
			slot.hDevice = hDevice;
			slot.nFd = nFd;
			slot.nDeviceId = nId;
			input_id identity = {};
			if ( ioctl( nFd, EVIOCGID, &identity ) == 0 ) {
				slot.pProfile = m_Profiles.findProfile( identity.bustype, identity.vendor, identity.product );
			} else {
				slot.pProfile = nullptr;
			}
			slot.bDesynchronized = false;
			slot.nFrameCount = 0;
			slot.nEffectId = -1;
			unsigned long nEffects[( FF_MAX + ( sizeof( unsigned long ) * 8 ) ) / ( sizeof( unsigned long ) * 8 )] = {};
			slot.bCanRumble =
			    bWritable && ioctl( nFd, EVIOCGBIT( EV_FF, sizeof( nEffects ) ), nEffects ) >= 0 &&
			    ( nEffects[FF_RUMBLE / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( FF_RUMBLE % ( sizeof( unsigned long ) * 8 ) ) ) ) != 0;
			memcpy( slot.szPath, pszPath, strlen( pszPath ) + 1 );
			const uint16_t nDPadCodes[] = { BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT };
			slot.bHatDPad = true;
			for ( uint32_t j = 0; j < sizeof( nDPadCodes ) / sizeof( nDPadCodes[0] ); ++j ) {
				const uint16_t nCode = nDPadCodes[j];
				if ( ( nKeys[nCode / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( nCode % ( sizeof( unsigned long ) * 8 ) ) ) ) != 0 ) {
					slot.bHatDPad = false;
				}
			}
			const uint16_t nAxisCodes[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ };
			for ( uint32_t j = 0; j < SIR_GAMEPAD_AXIS_COUNT; ++j ) {
				uint16_t nCode = nAxisCodes[j];
				bool bInvert = j == SIR_GAMEPAD_AXIS_LEFT_Y || j == SIR_GAMEPAD_AXIS_RIGHT_Y;
				if ( slot.pProfile != nullptr ) {
					for ( uint32_t iAxis = 0; iAxis < slot.pProfile->nAxisCount; ++iAxis ) {
						const sirInputControllerAxisBinding_t &binding = slot.pProfile->axes[iAxis];
						if ( binding.nLogicalCode == j ) {
							nCode = binding.nNativeCode;
							bInvert = binding.bInvert;
							break;
						}
					}
				}
				input_absinfo axis = {};
				if ( ioctl( nFd, EVIOCGABS( nCode ), &axis ) == 0 && axis.maximum > axis.minimum ) {
					slot.axes[j] = { axis.minimum, axis.maximum, nCode, bInvert, true };
				} else {
					slot.axes[j] = { 0, 0, nCode, bInvert, false };
				}
			}
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
	// Arm hotplug notifications before enumeration to close the scan/watch gap.
	if ( !isWatching() ) {
		watchDirectory( pszDirectory );
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

bool SIRLinuxEvdevInput::watchDirectory( const char *pszDirectory ) noexcept
{
	if ( pszDirectory == nullptr || ( m_nNotifyFd >= 0 && m_nNotifyWatch >= 0 ) ||
	     strlen( pszDirectory ) >= sizeof( m_szWatchDirectory ) ) {
		return false;
	}
	if ( m_nNotifyFd >= 0 ) {
		close( m_nNotifyFd );
		m_nNotifyFd = -1;
	}
	const int nFd = inotify_init1( IN_NONBLOCK | IN_CLOEXEC );
	if ( nFd < 0 ) {
		return false;
	}
	const int nWatch = inotify_add_watch( nFd, pszDirectory, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM );
	if ( nWatch < 0 ) {
		close( nFd );
		return false;
	}
	m_nNotifyFd = nFd;
	m_nNotifyWatch = nWatch;
	memcpy( m_szWatchDirectory, pszDirectory, strlen( pszDirectory ) + 1 );
	return true;
}

bool SIRLinuxEvdevInput::isWatching( void ) const noexcept
{
	return m_nNotifyFd >= 0 && m_nNotifyWatch >= 0;
}

void SIRLinuxEvdevInput::pumpHotplug( void ) noexcept
{
	if ( m_nNotifyFd < 0 ) {
		return;
	}
	alignas( inotify_event ) char szEvents[4096];
	for ( uint32_t iBatch = 0; iBatch < 4; ++iBatch ) {
		const ssize_t nBytes = read( m_nNotifyFd, szEvents, sizeof( szEvents ) );
		if ( nBytes < 0 && errno == EINTR ) {
			continue;
		}
		if ( nBytes <= 0 ) {
			break;
		}
		uint32_t nOffset = 0;
		while ( nOffset + sizeof( inotify_event ) <= ( uint32_t )nBytes ) {
			const inotify_event *pEvent = ( const inotify_event * )( szEvents + nOffset );
			const uint32_t nLength = ( uint32_t )sizeof( inotify_event ) + pEvent->len;
			if ( nLength > ( uint32_t )nBytes - nOffset ) {
				break;
			}
			if ( ( pEvent->mask & IN_Q_OVERFLOW ) != 0 ) {
				discover( m_szWatchDirectory );
			}
			if ( ( pEvent->mask & IN_IGNORED ) != 0 && pEvent->wd == m_nNotifyWatch ) {
				m_nNotifyWatch = -1;
			}
			if ( pEvent->len >= 6 && strncmp( pEvent->name, "event", 5 ) == 0 ) {
				char szPath[512];
				const int nWritten = snprintf( szPath, sizeof( szPath ), "%s/%s", m_szWatchDirectory, pEvent->name );
				if ( nWritten > 0 && ( size_t )nWritten < sizeof( szPath ) ) {
					if ( ( pEvent->mask & ( IN_CREATE | IN_MOVED_TO ) ) != 0 ) {
						openDevice( szPath );
					} else if ( ( pEvent->mask & ( IN_DELETE | IN_MOVED_FROM ) ) != 0 ) {
						for ( uint32_t iSlot = 0; iSlot < SIR_INPUT_MAX_DEVICES; ++iSlot ) {
							if ( m_Devices[iSlot].nFd >= 0 && strcmp( m_Devices[iSlot].szPath, szPath ) == 0 ) {
								closeSlot( m_Devices[iSlot] );
							}
						}
					}
				}
			}
			nOffset += nLength;
		}
	}
}

int16_t SIRLinuxEvdevInput::mapDeviceButton( const sirEvdevSlot_t &slot, uint16_t nCode ) const noexcept
{
	if ( slot.pProfile != nullptr ) {
		const int16_t nMapped = SIRInputControllerProfiles::findButton( *slot.pProfile, nCode );
		if ( nMapped >= 0 || slot.pProfile->bReplaceButtons ) {
			return nMapped;
		}
	}
	return mapButton( nCode );
}

int16_t SIRLinuxEvdevInput::mapDeviceAxis( const sirEvdevSlot_t &slot, uint16_t nCode ) const noexcept
{
	return slot.pProfile != nullptr ? SIRInputControllerProfiles::findAxis( *slot.pProfile, nCode ) : mapAxis( nCode );
}

int16_t SIRLinuxEvdevInput::mapButton( uint16_t nCode ) noexcept
{
	switch ( nCode ) {
	case BTN_SOUTH:
		return SIR_GAMEPAD_BUTTON_SOUTH;
	case BTN_EAST:
		return SIR_GAMEPAD_BUTTON_EAST;
	case BTN_WEST:
		return SIR_GAMEPAD_BUTTON_WEST;
	case BTN_NORTH:
		return SIR_GAMEPAD_BUTTON_NORTH;
	case BTN_TL:
		return SIR_GAMEPAD_BUTTON_LEFT_SHOULDER;
	case BTN_TR:
		return SIR_GAMEPAD_BUTTON_RIGHT_SHOULDER;
	case BTN_TL2:
		return SIR_GAMEPAD_BUTTON_LEFT_TRIGGER_DIGITAL;
	case BTN_TR2:
		return SIR_GAMEPAD_BUTTON_RIGHT_TRIGGER_DIGITAL;
	case BTN_SELECT:
		return SIR_GAMEPAD_BUTTON_BACK;
	case BTN_START:
		return SIR_GAMEPAD_BUTTON_START;
	case BTN_MODE:
		return SIR_GAMEPAD_BUTTON_GUIDE;
	case BTN_THUMBL:
		return SIR_GAMEPAD_BUTTON_LEFT_THUMB;
	case BTN_THUMBR:
		return SIR_GAMEPAD_BUTTON_RIGHT_THUMB;
	case BTN_DPAD_UP:
		return SIR_GAMEPAD_BUTTON_DPAD_UP;
	case BTN_DPAD_DOWN:
		return SIR_GAMEPAD_BUTTON_DPAD_DOWN;
	case BTN_DPAD_LEFT:
		return SIR_GAMEPAD_BUTTON_DPAD_LEFT;
	case BTN_DPAD_RIGHT:
		return SIR_GAMEPAD_BUTTON_DPAD_RIGHT;
	default:
		return -1;
	}
}

int16_t SIRLinuxEvdevInput::mapAxis( uint16_t nCode ) noexcept
{
	switch ( nCode ) {
	case ABS_X:
		return SIR_GAMEPAD_AXIS_LEFT_X;
	case ABS_Y:
		return SIR_GAMEPAD_AXIS_LEFT_Y;
	case ABS_RX:
		return SIR_GAMEPAD_AXIS_RIGHT_X;
	case ABS_RY:
		return SIR_GAMEPAD_AXIS_RIGHT_Y;
	case ABS_Z:
		return SIR_GAMEPAD_AXIS_LEFT_TRIGGER;
	case ABS_RZ:
		return SIR_GAMEPAD_AXIS_RIGHT_TRIGGER;
	default:
		return -1;
	}
}

int32_t SIRLinuxEvdevInput::normalizeAxis( int32_t nValue, int32_t nMinimum, int32_t nMaximum, bool bTrigger ) noexcept
{
	if ( nMaximum <= nMinimum ) {
		return 0;
	}
	if ( nValue < nMinimum ) {
		nValue = nMinimum;
	} else if ( nValue > nMaximum ) {
		nValue = nMaximum;
	}
	const int64_t nScaled = ( ( int64_t )nValue - nMinimum ) * ( bTrigger ? 32767 : 65534 ) / ( ( int64_t )nMaximum - nMinimum );
	return ( int32_t )( bTrigger ? nScaled : nScaled - 32767 );
}

bool SIRLinuxEvdevInput::appendFrameEvent( sirEvdevSlot_t &slot, sirInputEventKind_t eKind, uint16_t nCode, int32_t nValue ) noexcept
{
	if ( slot.nFrameCount >= SIR_EVDEV_MAX_FRAME_EVENTS ) {
		return false;
	}
	slot.frame[slot.nFrameCount++] = { nCode, nValue, eKind };
	return true;
}

bool SIRLinuxEvdevInput::stageHat( sirEvdevSlot_t &slot, uint16_t nCode, int32_t nValue ) noexcept
{
	if ( !slot.bHatDPad ) {
		return true;
	}
	if ( nCode == ABS_HAT0X ) {
		return appendFrameEvent( slot, SIR_INPUT_EVENT_BUTTON, SIR_GAMEPAD_BUTTON_DPAD_LEFT, nValue < 0 ? 1 : 0 ) &&
		       appendFrameEvent( slot, SIR_INPUT_EVENT_BUTTON, SIR_GAMEPAD_BUTTON_DPAD_RIGHT, nValue > 0 ? 1 : 0 );
	}
	if ( nCode == ABS_HAT0Y ) {
		return appendFrameEvent( slot, SIR_INPUT_EVENT_BUTTON, SIR_GAMEPAD_BUTTON_DPAD_UP, nValue < 0 ? 1 : 0 ) &&
		       appendFrameEvent( slot, SIR_INPUT_EVENT_BUTTON, SIR_GAMEPAD_BUTTON_DPAD_DOWN, nValue > 0 ? 1 : 0 );
	}
	return true;
}

bool SIRLinuxEvdevInput::resynchronize( sirEvdevSlot_t &slot ) noexcept
{
	slot.nFrameCount = 0;
	if ( !m_Input.resynchronizeDevice( slot.hDevice ) ) {
		return false;
	}
	unsigned long nKeys[( KEY_MAX + ( sizeof( unsigned long ) * 8 ) ) / ( sizeof( unsigned long ) * 8 )] = {};
	if ( ioctl( slot.nFd, EVIOCGKEY( sizeof( nKeys ) ), nKeys ) < 0 ) {
		return false;
	}
	for ( uint16_t nCode = BTN_MISC; nCode <= KEY_MAX && nCode < SIR_INPUT_MAX_BUTTONS; ++nCode ) {
		const bool bHeld = ( nKeys[nCode / ( sizeof( unsigned long ) * 8 )] & ( 1UL << ( nCode % ( sizeof( unsigned long ) * 8 ) ) ) ) != 0;
		const int16_t nMapped = mapDeviceButton( slot, nCode );
		const uint16_t nLogical = nMapped >= 0 ? ( uint16_t )nMapped : nCode;
		if ( bHeld && !m_Input.pushButton( slot.hDevice, nLogical, true, 0 ) ) {
			return false;
		}
	}
	for ( uint16_t iAxis = 0; iAxis < SIR_GAMEPAD_AXIS_COUNT; ++iAxis ) {
		if ( !slot.axes[iAxis].bValid ) {
			continue;
		}
		input_absinfo axis = {};
		if ( ioctl( slot.nFd, EVIOCGABS( slot.axes[iAxis].nNativeCode ), &axis ) != 0 ) {
			return false;
		}
		const bool bTrigger = iAxis >= SIR_GAMEPAD_AXIS_LEFT_TRIGGER;
		int32_t nMapped = normalizeAxis( axis.value, slot.axes[iAxis].nMin, slot.axes[iAxis].nMax, bTrigger );
		if ( slot.axes[iAxis].bInvert ) {
			nMapped = -nMapped;
		}
		if ( !m_Input.pushAxis( slot.hDevice, iAxis, nMapped, 0 ) ) {
			return false;
		}
	}
	if ( slot.bHatDPad ) {
		input_absinfo hat = {};
		if ( ioctl( slot.nFd, EVIOCGABS( ABS_HAT0X ), &hat ) == 0 &&
		     !( m_Input.pushButton( slot.hDevice, SIR_GAMEPAD_BUTTON_DPAD_LEFT, hat.value < 0, 0 ) &&
		        m_Input.pushButton( slot.hDevice, SIR_GAMEPAD_BUTTON_DPAD_RIGHT, hat.value > 0, 0 ) ) ) {
			return false;
		}
		if ( ioctl( slot.nFd, EVIOCGABS( ABS_HAT0Y ), &hat ) == 0 &&
		     !( m_Input.pushButton( slot.hDevice, SIR_GAMEPAD_BUTTON_DPAD_UP, hat.value < 0, 0 ) &&
		        m_Input.pushButton( slot.hDevice, SIR_GAMEPAD_BUTTON_DPAD_DOWN, hat.value > 0, 0 ) ) ) {
			return false;
		}
	}
	return true;
}

uint32_t SIRLinuxEvdevInput::pump( void ) noexcept
{
	pumpHotplug();
	uint32_t nProcessed = 0;
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		sirEvdevSlot_t &slot = m_Devices[i];
		if ( slot.nFd < 0 ) {
			continue;
		}
		input_event events[32];
		for ( uint32_t nBatch = 0; nBatch < 8; ++nBatch ) {
			const ssize_t nRead = read( slot.nFd, events, sizeof( events ) );
			if ( nRead < 0 && errno == EINTR ) {
				continue;
			}
			if ( nRead < 0 && errno == EAGAIN ) {
				break;
			}
			if ( nRead <= 0 || nRead % ( ssize_t )sizeof( input_event ) != 0 ) {
				closeSlot( slot );
				break;
			}
			const uint32_t nEvents = ( uint32_t )( nRead / ( ssize_t )sizeof( input_event ) );
			for ( uint32_t j = 0; j < nEvents; ++j ) {
				const input_event &event = events[j];
				++nProcessed;
				if ( event.type == EV_SYN && event.code == SYN_DROPPED ) {
					slot.nFrameCount = 0;
					slot.bDesynchronized = true;
					continue;
				}
				if ( slot.bDesynchronized ) {
					if ( event.type == EV_SYN && event.code == SYN_REPORT ) {
						slot.bDesynchronized = !resynchronize( slot );
					}
					continue;
				}
				if ( event.type == EV_SYN && event.code == SYN_REPORT ) {
					const uint64_t nNow = SIRPlatformTime::getMonotonicNanoseconds();
					if ( !m_Input.submitGamepadFrame( slot.hDevice, slot.frame, slot.nFrameCount, nNow ) ) {
						slot.bDesynchronized = true;
					}
					slot.nFrameCount = 0;
					continue;
				}
				bool bSuccess = true;
				if ( event.type == EV_KEY && event.code < SIR_INPUT_MAX_BUTTONS && ( event.value == 0 || event.value == 1 ) ) {
					const int16_t nMapped = mapDeviceButton( slot, event.code );
					const uint16_t nLogical = nMapped >= 0 ? ( uint16_t )nMapped : event.code;
					bSuccess = appendFrameEvent( slot, SIR_INPUT_EVENT_BUTTON, nLogical, event.value );
				} else if ( event.type == EV_ABS ) {
					const int16_t nAxis = mapDeviceAxis( slot, event.code );
					if ( nAxis >= 0 && slot.axes[nAxis].bValid ) {
						const bool bTrigger = nAxis >= SIR_GAMEPAD_AXIS_LEFT_TRIGGER;
						int32_t nMapped = normalizeAxis( event.value, slot.axes[nAxis].nMin, slot.axes[nAxis].nMax, bTrigger );
						if ( slot.axes[nAxis].bInvert ) {
							nMapped = -nMapped;
						}
						bSuccess = appendFrameEvent( slot, SIR_INPUT_EVENT_AXIS, ( uint16_t )nAxis, nMapped );
					} else if ( event.code == ABS_HAT0X || event.code == ABS_HAT0Y ) {
						bSuccess = stageHat( slot, event.code, event.value );
					} else if ( nAxis < 0 && ( uint32_t )event.code + 16 < SIR_INPUT_MAX_AXES ) {
						// Preserve unknown axes in the extension range.
						bSuccess = appendFrameEvent( slot, SIR_INPUT_EVENT_AXIS, ( uint16_t )( 16 + event.code ), event.value );
					}
				}
				if ( !bSuccess ) {
					slot.nFrameCount = 0;
					slot.bDesynchronized = true;
				}
			}
		}
	}
	return nProcessed;
}

bool SIRLinuxEvdevInput::setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		sirEvdevSlot_t &slot = m_Devices[i];
		if ( slot.nFd < 0 || !slot.bCanRumble || slot.hDevice.nSlot != hDevice.nSlot || slot.hDevice.nGeneration != hDevice.nGeneration ) {
			continue;
		}
		if ( nStrong == 0 && nWeak == 0 ) {
			if ( slot.nEffectId < 0 ) {
				return true;
			}
			input_event stop = {};
			stop.type = EV_FF;
			stop.code = ( uint16_t )slot.nEffectId;
			stop.value = 0;
			return write( slot.nFd, &stop, sizeof( stop ) ) == ( ssize_t )sizeof( stop );
		}
		if ( nDurationMs == 0 ) {
			return false;
		}
		ff_effect effect = {};
		effect.type = FF_RUMBLE;
		effect.id = ( int16_t )slot.nEffectId;
		effect.u.rumble.strong_magnitude = nStrong;
		effect.u.rumble.weak_magnitude = nWeak;
		effect.replay.length = nDurationMs;
		if ( ioctl( slot.nFd, EVIOCSFF, &effect ) < 0 ) {
			return false;
		}
		slot.nEffectId = effect.id;
		input_event start = {};
		start.type = EV_FF;
		start.code = ( uint16_t )effect.id;
		start.value = 1;
		return write( slot.nFd, &start, sizeof( start ) ) == ( ssize_t )sizeof( start );
	}
	return false;
}

void SIRLinuxEvdevInput::closeSlot( sirEvdevSlot_t &slot ) noexcept
{
	if ( slot.nFd >= 0 ) {
		if ( slot.nEffectId >= 0 ) {
			input_event stop = {};
			stop.type = EV_FF;
			stop.code = ( uint16_t )slot.nEffectId;
			stop.value = 0;
			write( slot.nFd, &stop, sizeof( stop ) );
			ioctl( slot.nFd, EVIOCRMFF, slot.nEffectId );
			slot.nEffectId = -1;
		}
		close( slot.nFd );
		m_Input.removeDevice( slot.hDevice );
		slot.nFrameCount = 0;
		slot.bDesynchronized = false;
		slot.pProfile = nullptr;
		slot.nFd = -1;
		slot.szPath[0] = '\0';
		--m_nDeviceCount;
	}
}

uint32_t SIRLinuxEvdevInput::getDeviceCount( void ) const noexcept
{
	return m_nDeviceCount;
}

void SIRLinuxEvdevInput::shutdown( void ) noexcept
{
	if ( m_nNotifyFd >= 0 ) {
		if ( m_nNotifyWatch >= 0 ) {
			inotify_rm_watch( m_nNotifyFd, m_nNotifyWatch );
		}
		close( m_nNotifyFd );
		m_nNotifyFd = -1;
		m_nNotifyWatch = -1;
		m_szWatchDirectory[0] = '\0';
	}
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		closeSlot( m_Devices[i] );
	}
}

bool SIRLinuxEvdevInput::ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept
{
	for ( uint32_t i = 0; i < SIR_INPUT_MAX_DEVICES; ++i ) {
		const sirEvdevSlot_t &slot = m_Devices[i];
		if ( slot.nFd >= 0 && slot.hDevice.nSlot == hDevice.nSlot && slot.hDevice.nGeneration == hDevice.nGeneration ) {
			return true;
		}
	}
	return false;
}
