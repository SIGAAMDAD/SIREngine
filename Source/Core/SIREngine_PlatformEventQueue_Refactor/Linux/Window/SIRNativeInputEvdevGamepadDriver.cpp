/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

/*
 * /dev/input/event* -> nonblocking evdev reads -> mapped device state
 *                                                   |
 *                                                   v
 *                                  engine-owned SPSC event queue
 *
 * X11, Wayland and libinput can each own this reader on their input thread.
 * Only the selected driver should construct and pump it.
 */
#include "SIRNativeInputEvdevGamepadDriver.h"
#include "Core/Input/SIRInputTypes.h"
#include <Core/Platform/Linux/Input/SIRLinuxEvdevBackend.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <time.h>

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr char s_szInputDirectory[] = "/dev/input";
static constexpr size_t s_nWordBits = sizeof( unsigned long ) * CHAR_BIT;

static bool hasBit( const unsigned long *pBits, size_t nCode ) noexcept
{
	return ( pBits[nCode / s_nWordBits] & ( 1UL << ( nCode % s_nWordBits ) ) ) != 0;
}

// Controller profiles iIndex these tables by SIRInputControllerControl and
// SIRInputAxisCode. Their button numbers differ from the window event ABI.
static constexpr uint16_t s_szDefaultKeys[] = { BTN_SOUTH,
                                                BTN_EAST,
                                                BTN_WEST,
                                                BTN_NORTH,
                                                BTN_TL,
                                                BTN_TR,
                                                BTN_SELECT,
                                                BTN_START,
                                                BTN_MODE,
                                                BTN_THUMBL,
                                                BTN_THUMBR,
                                                BTN_DPAD_UP,
                                                BTN_DPAD_DOWN,
                                                BTN_DPAD_LEFT,
                                                BTN_DPAD_RIGHT,
                                                BTN_TL2,
                                                BTN_TR2 };
static constexpr SIRGamepadButton s_szEventButtons[] = { SIRGamepadButton::South,
                                                         SIRGamepadButton::East,
                                                         SIRGamepadButton::West,
                                                         SIRGamepadButton::North,
                                                         SIRGamepadButton::LeftShoulder,
                                                         SIRGamepadButton::RightShoulder,
                                                         SIRGamepadButton::Back,
                                                         SIRGamepadButton::Start,
                                                         SIRGamepadButton::Guide,
                                                         SIRGamepadButton::LeftStick,
                                                         SIRGamepadButton::RightStick,
                                                         SIRGamepadButton::DPadUp,
                                                         SIRGamepadButton::DPadDown,
                                                         SIRGamepadButton::DPadLeft,
                                                         SIRGamepadButton::DPadRight,
                                                         SIRGamepadButton::Invalid,
                                                         SIRGamepadButton::Invalid };
static constexpr uint16_t s_szDefaultAxes[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ };
static constexpr size_t s_nLeftTrigger = static_cast<size_t>( SIRGamepadAxis::LeftTrigger );
static constexpr size_t s_nLeftTriggerKey = static_cast<size_t>( SIRInputControllerControl::SIR_INPUT_PAD_LT_BUTTON );

// A profile may replace a whole group or override individual logical controls.
// Remove generic owners of remapped native codes before applying overrides.
template <size_t Count>
static void initializeControls( uint16_t ( &szCodes )[Count], const uint16_t ( &szDefaults )[Count], bool bReplace ) noexcept
{
	for ( size_t iCode = 0; iCode < Count; ++iCode ) {
		szCodes[iCode] = bReplace ? UINT16_MAX : szDefaults[iCode];
	}
}

// An override owns its native code: remove any generic mapping first.
template <typename Binding, size_t Count>
static void overrideControls( uint16_t ( &szCodes )[Count], const Binding *pBindings, size_t nCount ) noexcept
{
	for ( size_t iCode = 0; iCode < Count; ++iCode ) {
		for ( size_t iBinding = 0; iBinding < nCount; ++iBinding ) {
			if ( pBindings[iBinding].nNativeCode == szCodes[iCode] ) {
				szCodes[iCode] = UINT16_MAX;
			}
		}
	}

	for ( size_t iBinding = 0; iBinding < nCount; ++iBinding ) {
		if ( pBindings[iBinding].nLogicalCode < Count ) {
			szCodes[pBindings[iBinding].nLogicalCode] = pBindings[iBinding].nNativeCode;
		}
	}
}

static uint64_t nativeId( const struct stat& info ) noexcept
{
	return ( static_cast<uint64_t>( info.st_rdev ) << 32 ) ^ static_cast<uint64_t>( info.st_ino );
}

static uint64_t physicalId( int nFd, const input_id& identity ) noexcept
{
	char szPath[256]{};
	if ( ioctl( nFd, EVIOCGPHYS( sizeof( szPath ) ), szPath ) <= 0 || !szPath[0] ) {
		return 0;
	}
	szPath[sizeof( szPath ) - 1] = 0;
	return SIRLinuxEvdevBackend::physicalKey( szPath, identity.vendor, identity.product );
}

SIRNativeInputEvdevGamepadDriver::SIRNativeInputEvdevGamepadDriver( SIRSPSCEventRing<SIREventData>& eventQueue, const SIRInputControllerProfiles *pProfiles ) noexcept
    : m_EventQueue( eventQueue ),
      m_pProfiles( pProfiles )
{
	for ( uint32_t iDevice = 0; iDevice < SIR_INPUT_DEVICE_CAPACITY; ++iDevice ) {
		m_szDeviceGenerations[iDevice] = 1;
	}
}

SIRNativeInputEvdevGamepadDriver::~SIRNativeInputEvdevGamepadDriver() noexcept
{
	m_bPumping = true;
	for ( uint32_t iDevice = 0; iDevice < SIR_INPUT_DEVICE_CAPACITY; iDevice++ ) {
		Device& device = m_Devices[iDevice];
		closeDevice( device );
	}
	if ( m_nWatchFd >= 0 ) {
		close( m_nWatchFd );
	}
}

void SIRNativeInputEvdevGamepadDriver::publishDevice( SIREventType eType, const Device& device, uint64_t nTimestamp ) noexcept
{
	SIREventData eventData{};
	eventData.type = eType;
	eventData.timestamp = nTimestamp;
	eventData.gdevice.deviceId = device.nEventId;

	publish( eventData );
}

void SIRNativeInputEvdevGamepadDriver::closeDevice( Device& device ) noexcept
{
	if ( device.nFd >= 0 ) {
		close( device.nFd );
		device.nFd = -1;
		publishDevice( SIREventType::GamepadRemoved, device, SIRPlatformTime::getMonotonicNanoseconds() );
		++m_szDeviceGenerations[device.nSlot];
		if ( m_szDeviceGenerations[device.nSlot] == 0 ) {
			m_szDeviceGenerations[device.nSlot] = 1;
		}
	}
}

void SIRNativeInputEvdevGamepadDriver::setButton( Device& device, SIRGamepadButton eButton, bool bDown, uint64_t nTimestamp ) noexcept
{
	const uint8_t nCode = static_cast<uint8_t>( eButton );
	if ( eButton == SIRGamepadButton::Invalid || nCode >= 32 ) {
		return;
	}

	const uint32_t nButtonMask = 1U << nCode;
	if ( ( ( device.nButtons & nButtonMask ) != 0 ) == bDown ) {
		return;
	}
	device.nButtons ^= nButtonMask;
	device.bChanged = true;

	SIREventData eventData{};
	eventData.type = bDown ? SIREventType::GamepadButtonDown : SIREventType::GamepadButtonUp;
	eventData.timestamp = nTimestamp;
	eventData.gbutton = { device.nEventId, nCode, bDown };

	publish( eventData );
}

void SIRNativeInputEvdevGamepadDriver::setAxis( Device& device, size_t iIndex, int16_t nValue, uint64_t nTimestamp ) noexcept
{
	if ( device.szValues[iIndex] != nValue ) {
		device.szValues[iIndex] = nValue;
		device.bChanged = true;

		SIREventData eventData{};
		eventData.type = SIREventType::GamepadAxisMotion;
		eventData.timestamp = nTimestamp;
		eventData.gaxis = { device.nEventId, static_cast<uint8_t>( iIndex ), nValue };

		publish( eventData );
	}
}

void SIRNativeInputEvdevGamepadDriver::applyKey( Device& device, size_t iIndex, bool bDown, uint64_t nTimestamp ) noexcept
{
	if ( iIndex < s_nLeftTriggerKey ) {
		setButton( device, s_szEventButtons[iIndex], bDown, nTimestamp );
	} else {
		// The event ABI represents triggers as axes. A digital trigger supplies
		// their endpoints on controllers without the corresponding analog axis.
		const size_t axis = s_nLeftTrigger + iIndex - s_nLeftTriggerKey;
		if ( device.szAxes[axis].nCode == UINT16_MAX ) {
			setAxis( device, axis, bDown ? INT16_MAX : 0, nTimestamp );
		}
	}
}

void SIRNativeInputEvdevGamepadDriver::applyAxis( Device& device, size_t iIndex, int32_t nValue, uint64_t nTimestamp ) noexcept
{
	const Axis& axis = device.szAxes[iIndex];
	const bool bTrigger = iIndex >= s_nLeftTrigger;

	// Use the same range and orientation as the Linux input service.
	const int32_t nNormalized = SIRLinuxEvdevBackend::normalize( nValue, axis.nMinimum, axis.nMaximum, bTrigger );
	int32_t nMapped = axis.bInvert ? ( bTrigger ? INT16_MAX - nNormalized : -nNormalized ) : nNormalized;

	// Negating -32768 produces +32768, which is outside the signed event ABI.
	// Saturate the endpoint rather than wrapping it back to the wrong side.
	if ( nMapped > INT16_MAX ) {
		nMapped = INT16_MAX;
	} else if ( nMapped < INT16_MIN ) {
		nMapped = INT16_MIN;
	}

	setAxis( device, iIndex, static_cast<int16_t>( nMapped ), nTimestamp );
}

void SIRNativeInputEvdevGamepadDriver::applyHat( Device& device, size_t iIndex, int32_t nValue, uint64_t nTimestamp ) noexcept
{
	const SIRGamepadButton eNegative = iIndex ? SIRGamepadButton::DPadUp : SIRGamepadButton::DPadLeft;
	const SIRGamepadButton ePositive = iIndex ? SIRGamepadButton::DPadDown : SIRGamepadButton::DPadRight;

	// Release the previous side before pressing its opposite.
	if ( nValue <= 0 ) {
		setButton( device, ePositive, false, nTimestamp );
	}

	setButton( device, eNegative, nValue < 0, nTimestamp );
	if ( nValue > 0 ) {
		setButton( device, ePositive, true, nTimestamp );
	}
}

bool SIRNativeInputEvdevGamepadDriver::snapshot( Device& device, bool bInitial ) noexcept
{
	unsigned long szHeld[( KEY_MAX / s_nWordBits ) + 1]{};
	input_absinfo szAxisInfo[AXIS_COUNT]{};
	input_absinfo szHatInfo[2]{};

	if ( ioctl( device.nFd, EVIOCGKEY( sizeof( szHeld ) ), szHeld ) < 0 ) {
		return false;
	}

	for ( size_t iIndex = 0; iIndex < AXIS_COUNT; ++iIndex ) {
		if ( device.szAxes[iIndex].nCode != UINT16_MAX && ioctl( device.nFd, EVIOCGABS( device.szAxes[iIndex].nCode ), &szAxisInfo[iIndex] ) < 0 ) {
			return false;
		}
	}
	for ( size_t iIndex = 0; iIndex < 2; ++iIndex ) {
		if ( device.bHat[iIndex] && ioctl( device.nFd, EVIOCGABS( ABS_HAT0X + iIndex ), &szHatInfo[iIndex] ) < 0 ) {
			return false;
		}
	}

	// Publish only after the complete snapshot succeeded. Comparing with the
	// published state emits missed releases as well as missed presses.
	const uint64_t nTimestamp = SIRPlatformTime::getMonotonicNanoseconds();
	if ( bInitial ) {
		publishDevice( SIREventType::GamepadAdded, device, nTimestamp );
	}
	for ( size_t iIndex = 0; iIndex < BUTTON_COUNT; ++iIndex ) {
		if ( device.szKeys[iIndex] != UINT16_MAX ) {
			applyKey( device, iIndex, hasBit( szHeld, device.szKeys[iIndex] ), nTimestamp );
		}
	}
	for ( size_t iIndex = 0; iIndex < AXIS_COUNT; ++iIndex ) {
		if ( device.szAxes[iIndex].nCode != UINT16_MAX ) {
			applyAxis( device, iIndex, szAxisInfo[iIndex].value, nTimestamp );
		}
	}
	for ( size_t iIndex = 0; iIndex < 2; ++iIndex ) {
		if ( device.bHat[iIndex] ) {
			applyHat( device, iIndex, szHatInfo[iIndex].value, nTimestamp );
		}
	}

	publishDevice( SIREventType::GamepadUpdateComplete, device, nTimestamp );
	device.bChanged = false;
	return true;
}

void SIRNativeInputEvdevGamepadDriver::configureDeviceControls( Device& device, const input_id& identity, bool bIdentified, const unsigned long *pKeyBits ) noexcept
{
	// The same controller profile applies whether X11, Wayland or libinput owns
	// this shared evdev reader. Configure the native mapping once per device.
	const auto *pProfile = bIdentified && m_pProfiles ? m_pProfiles->find( identity.bustype, identity.vendor, identity.product ) : nullptr;
	unsigned long szAbs[ABS_MAX / s_nWordBits + 1]{};
	initializeControls( device.szKeys, s_szDefaultKeys, pProfile && pProfile->bReplaceButtons );

	if ( pProfile ) {
		overrideControls( device.szKeys, pProfile->buttons, pProfile->nButtonCount );
	}
	for ( uint16_t& nKey : device.szKeys ) {
		if ( nKey > KEY_MAX || !hasBit( pKeyBits, nKey ) ) {
			nKey = UINT16_MAX;
		}
	}

	uint16_t szAxisCodes[AXIS_COUNT];
	initializeControls( szAxisCodes, s_szDefaultAxes, pProfile && pProfile->bReplaceAxes );
	if ( pProfile ) {
		overrideControls( szAxisCodes, pProfile->axes, pProfile->nAxisCount );
	}

	ioctl( device.nFd, EVIOCGBIT( EV_ABS, sizeof( szAbs ) ), szAbs );
	for ( size_t iIndex = 0; iIndex < AXIS_COUNT; ++iIndex ) {
		const uint16_t nCode = szAxisCodes[iIndex];
		input_absinfo range{};

		Axis& axis = device.szAxes[iIndex];
		axis.nCode = UINT16_MAX;

		const bool bValidAxis = nCode <= ABS_MAX && hasBit( szAbs, nCode );
		const bool bReadableAxis = bValidAxis && ioctl( device.nFd, EVIOCGABS( nCode ), &range ) == 0;
		if ( bReadableAxis && range.maximum > range.minimum ) {
			bool bInvert = iIndex == static_cast<size_t>( SIRGamepadAxis::LeftY ) || iIndex == static_cast<size_t>( SIRGamepadAxis::RightY );
			if ( pProfile ) {
				bool bProfileInvert = false;
				if ( SIRInputControllerProfiles::axis( *pProfile, nCode, &bProfileInvert ) >= 0 ) {
					bInvert = bProfileInvert;
				}
			}
			axis = { nCode, range.minimum, range.maximum, bInvert };
		}
	}

	// Use a hat only when a profile or discrete D-pad button hasn't already
	// claimed that direction and the hat isn't bound as an analog axis.
	for ( size_t iIndex = 0; iIndex < 2; ++iIndex ) {
		const uint16_t nCode = ABS_HAT0X + iIndex;
		const size_t iPair = static_cast<size_t>( iIndex ? SIRInputControllerControl::SIR_INPUT_PAD_DPAD_UP : SIRInputControllerControl::SIR_INPUT_PAD_DPAD_LEFT );
		const bool bUseDefaultDPad = !( pProfile && pProfile->bReplaceButtons );
		const bool bNoMappedButtons = device.szKeys[iPair] == UINT16_MAX && device.szKeys[iPair + 1] == UINT16_MAX;

		device.bHat[iIndex] = bUseDefaultDPad && bNoMappedButtons && hasBit( szAbs, nCode );
		for ( const Axis& axis : device.szAxes ) {
			if ( axis.nCode == nCode ) {
				device.bHat[iIndex] = false;
			}
		}
	}
}

void SIRNativeInputEvdevGamepadDriver::openDevice( const char *szPath ) noexcept
{
	for ( uint32_t iDevice = 0; iDevice < SIR_INPUT_DEVICE_CAPACITY; iDevice++ ) {
		const Device& device = m_Devices[iDevice];
		if ( device.nFd >= 0 && strcmp( device.szPath, szPath ) == 0 ) {
			return;
		}
	}

	// evdev exposes keyboards, mice and touch devices in the same directory.
	// Require the gamepad face-button capability used by the Linux backend.
	const int nFd = open( szPath, O_RDONLY | O_NONBLOCK | O_CLOEXEC );
	if ( nFd < 0 ) {
		return;
	}
	unsigned long szKeys[KEY_MAX / s_nWordBits + 1]{};

	struct stat info
	{
	};

	input_id identity{};
	const bool bValidCharacterDevice = fstat( nFd, &info ) == 0 && S_ISCHR( info.st_mode );
	const bool bReadableKeys = bValidCharacterDevice && ioctl( nFd, EVIOCGBIT( EV_KEY, sizeof( szKeys ) ), szKeys ) >= 0;
	if ( !bReadableKeys || !hasBit( szKeys, BTN_GAMEPAD ) ) {
		close( nFd );
		return;
	}

	const bool bIdentified = ioctl( nFd, EVIOCGID, &identity ) == 0;
	const uint64_t nNativeId = nativeId( info );
	const uint64_t nPhysicalId = bIdentified ? physicalId( nFd, identity ) : 0;
	uint32_t nSlot = SIR_INPUT_DEVICE_CAPACITY;

	for ( uint32_t iDevice = 0; iDevice < SIR_INPUT_DEVICE_CAPACITY; ++iDevice ) {
		const Device& existing = m_Devices[iDevice];
		if ( existing.nFd < 0 ) {
			if ( nSlot == SIR_INPUT_DEVICE_CAPACITY ) {
				nSlot = iDevice;
			}
			continue;
		}
		if ( existing.nNativeId == nNativeId || ( nPhysicalId != 0 && existing.nPhysicalId == nPhysicalId ) ) {
			close( nFd );
			return;
		}
	}

	if ( nNativeId == 0 || nSlot == SIR_INPUT_DEVICE_CAPACITY ) {
		close( nFd );
		return;
	}

	Device& device = m_Devices[nSlot];
	device = Device{};
	device.nFd = nFd;
	device.nSlot = nSlot;
	// The window event payload has one 32-bit ID. Include the slot generation
	// so reconnecting in a vacated slot yields a fresh public instance ID.
	device.nEventId = ( m_szDeviceGenerations[nSlot] - 1 ) * SIR_INPUT_DEVICE_CAPACITY + nSlot + 1;
	device.nNativeId = nNativeId;
	device.nPhysicalId = nPhysicalId;
	snprintf( device.szPath, sizeof( device.szPath ), "%s", szPath );

	const int nClock = CLOCK_MONOTONIC;
	device.bMonotonic = ioctl( nFd, EVIOCSCLOCKID, &nClock ) == 0;

	configureDeviceControls( device, identity, bIdentified, szKeys );
	if ( !snapshot( device, true ) ) {
		// No registration event was published for this device.
		close( nFd );
		device.nFd = -1;
		++m_szDeviceGenerations[nSlot];
		if ( m_szDeviceGenerations[nSlot] == 0 ) {
			m_szDeviceGenerations[nSlot] = 1;
		}
	}
}

void SIRNativeInputEvdevGamepadDriver::discover( void ) noexcept
{
	// Reconcile active nodes before opening new ones. inotify may coalesce
	// changes, overflow, or report replacement of an eventN between pumps.
	for ( uint32_t iDevice = 0; iDevice < SIR_INPUT_DEVICE_CAPACITY; iDevice++ ) {
		Device& device = m_Devices[iDevice];

		struct stat info
		{
		};

		if ( device.nFd >= 0 && ( stat( device.szPath, &info ) != 0 || nativeId( info ) != device.nNativeId ) ) {
			closeDevice( device );
		}
	}

	DIR *pDirectory = opendir( s_szInputDirectory );
	if ( !pDirectory ) {
		return;
	}
	while ( const dirent *pEntry = readdir( pDirectory ) ) {
		const bool bHasEventPrefix = strncmp( pEntry->d_name, "event", 5 ) == 0;
		const bool bHasNumericSuffix = bHasEventPrefix && pEntry->d_name[5] != 0;

		if ( !bHasNumericSuffix ) {
			continue;
		}
		if ( pEntry->d_name[5 + strspn( pEntry->d_name + 5, "0123456789" )] != 0 ) {
			continue;
		}

		char szPath[sizeof( Device::szPath )];
		const int nLength = snprintf( szPath, sizeof( szPath ), "%s/%s", s_szInputDirectory, pEntry->d_name );
		if ( nLength > 0 && static_cast<size_t>( nLength ) < sizeof( szPath ) ) {
			openDevice( szPath );
		}
	}
	closedir( pDirectory );
}

void SIRNativeInputEvdevGamepadDriver::pollHotplug( void ) noexcept
{
	bool bScan = false;

	if ( m_nWatchFd < 0 ) {
		const uint64_t nNow = SIRPlatformTime::getMonotonicNanoseconds();

		if ( nNow < m_nNextScan ) {
			return;
		}

		// Some sessions start before /dev/input exists. Retry discovery and
		// the watch once per second until inotify can take over.
		m_nNextScan = nNow + UINT64_C( 1000000000 );
		m_nWatchFd = inotify_init1( IN_CLOEXEC | IN_NONBLOCK );
		if ( m_nWatchFd >= 0 &&
		     inotify_add_watch( m_nWatchFd, s_szInputDirectory, IN_CREATE | IN_ATTRIB | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF ) < 0 ) {
			close( m_nWatchFd );
			m_nWatchFd = -1;
		}

		bScan = true;
	}

	if ( m_nWatchFd >= 0 ) {
		alignas( inotify_event ) char szBuffer[4096];
		for ( unsigned iBatch = 0; iBatch < 4; ++iBatch ) {
			const ssize_t nBytes = read( m_nWatchFd, szBuffer, sizeof( szBuffer ) );

			if ( nBytes < 0 && errno == EINTR ) {
				continue;
			}
			if ( nBytes < 0 && errno == EAGAIN ) {
				break;
			}

			bool bLost = nBytes <= 0;
			for ( ssize_t iOffset = 0; iOffset < nBytes; ) {
				const inotify_event *pChange = reinterpret_cast<const inotify_event *>( szBuffer + iOffset );
				bLost |= ( pChange->mask & ( IN_IGNORED | IN_UNMOUNT | IN_DELETE_SELF | IN_MOVE_SELF ) ) != 0;
				iOffset += static_cast<ssize_t>( sizeof( *pChange ) + pChange->len );
			}
			bScan = true;
			if ( bLost ) {
				close( m_nWatchFd );
				m_nWatchFd = -1;
				m_nNextScan = 0;
				break;
			}
		}
	}

	if ( bScan ) {
		discover();
	}
}

bool SIRNativeInputEvdevGamepadDriver::process( Device& device, const input_event& eventData ) noexcept
{
	static_assert( REPORT_CAPACITY == SIR_INPUT_EVDEV_REPORT_CAPACITY, "Match Linux evdev report capacity" );
	if ( eventData.type == EV_SYN && eventData.code == SYN_DROPPED ) {
		// The kernel discarded records for this descriptor. Ignore increments
		// through the next report and re-read every advertised mapped control.
		device.bDropped = true;
		device.nPendingCount = 0;
		return true;
	}

	const bool bReport = eventData.type == EV_SYN && eventData.code == SYN_REPORT;
	if ( device.bDropped ) {
		if ( bReport ) {
			device.bDropped = false;
			return snapshot( device, false );
		}
		return true;
	}

	const uint64_t nEventSeconds = static_cast<uint64_t>( eventData.input_event_sec );
	const uint64_t nEventMicroseconds = static_cast<uint64_t>( eventData.input_event_usec );
	const uint64_t nTimestamp = device.bMonotonic ? nEventSeconds * UINT64_C( 1000000000 ) + nEventMicroseconds * UINT64_C( 1000 ) : SIRPlatformTime::getMonotonicNanoseconds();

	// read() may split a packet. Keep native-order changes until SYN_REPORT
	// so an overrun never publishes part of an abandoned packet.
	if ( bReport ) {
		for ( size_t iIndex = 0; iIndex < device.nPendingCount; ++iIndex ) {
			applyChange( device, device.szPending[iIndex] );
		}
		device.nPendingCount = 0;
		if ( device.bChanged ) {
			publishDevice( SIREventType::GamepadUpdateComplete, device, nTimestamp );
			device.bChanged = false;
		}
	} else {
		const bool bButtonTransition = eventData.type == EV_KEY && ( eventData.value == 0 || eventData.value == 1 );
		if ( !bButtonTransition && eventData.type != EV_ABS ) {
			return true;
		}
		if ( device.nPendingCount == REPORT_CAPACITY ) {
			// The existing Linux input policy recovers an oversized report from
			// kernel state instead of publishing a truncated packet.
			device.nPendingCount = 0;
			device.bDropped = true;
		} else {
			device.szPending[device.nPendingCount++] = { nTimestamp, eventData.value, eventData.code, eventData.type };
		}
	}

	return true;
}

void SIRNativeInputEvdevGamepadDriver::applyChange( Device& device, const Change& eventData ) noexcept
{
	const uint64_t nTimestamp = eventData.nTime;

	if ( eventData.nType == EV_KEY ) {
		for ( size_t iIndex = 0; iIndex < BUTTON_COUNT; ++iIndex ) {
			if ( device.szKeys[iIndex] == eventData.nCode ) {
				applyKey( device, iIndex, eventData.nValue != 0, nTimestamp );
				break;
			}
		}
	} else if ( eventData.nType == EV_ABS ) {
		if ( ( eventData.nCode == ABS_HAT0X || eventData.nCode == ABS_HAT0Y ) && device.bHat[eventData.nCode - ABS_HAT0X] ) {
			applyHat( device, eventData.nCode - ABS_HAT0X, eventData.nValue, nTimestamp );
		} else {
			for ( size_t iIndex = 0; iIndex < AXIS_COUNT; ++iIndex ) {
				if ( device.szAxes[iIndex].nCode == eventData.nCode ) {
					applyAxis( device, iIndex, eventData.nValue, nTimestamp );
					break;
				}
			}
		}
	}
}

void SIRNativeInputEvdevGamepadDriver::pumpEvents( void ) noexcept
{
	if ( m_bPumping ) {
		return;
	}
	m_bPumping = true;

	pollHotplug();
	for ( uint32_t iDevice = 0; iDevice < SIR_INPUT_DEVICE_CAPACITY; iDevice++ ) {
		Device& device = m_Devices[iDevice];

		// Fixed-size stack reads amortize syscalls. Bound work per controller so
		// one busy device cannot starve another or monopolize the engine frame.
		for ( unsigned iBatch = 0; device.nFd >= 0 && iBatch < 8; ++iBatch ) {
			input_event szNativeEvents[64];
			const ssize_t nBytes = read( device.nFd, szNativeEvents, sizeof( szNativeEvents ) );

			if ( nBytes < 0 && errno == EINTR ) {
				continue;
			}
			if ( nBytes < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) {
				break;
			}

			// Unplugged evdev descriptors return ENODEV even if their path
			// has already been reused. Retire this particular registration.
			if ( nBytes <= 0 || nBytes % sizeof( input_event ) ) {
				closeDevice( device );
				break;
			}
			for ( size_t iIndex = 0; iIndex < static_cast<size_t>( nBytes ) / sizeof( input_event ); ++iIndex ) {
				if ( !process( device, szNativeEvents[iIndex] ) ) {
					closeDevice( device );
					break;
				}
			}
		}
	}

	m_bPumping = false;
}

void SIRNativeInputEvdevGamepadDriver::publish( const SIREventData& eventData ) noexcept
{
	if ( m_EventQueue.tryEnqueue( eventData ) != SIREventEnqueueStatus::SIR_EVENT_ENQUEUE_OK && m_nDroppedEvents != UINT32_MAX ) {
		++m_nDroppedEvents;
	}
}

uint32_t SIRNativeInputEvdevGamepadDriver::droppedEventCount( void ) const noexcept
{
	return m_nDroppedEvents;
}
