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

#include <Core/Platform/Linux/SIRLinuxEvdevBackend.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int s_nWatchedFd = -1;
static uint32_t s_nReadCalls = 0;
static bool s_bFakeGamepadIoctl = false;
static int s_nFakeGamepadFd = -1;
static uint32_t s_nFakeIoctlCalls = 0;
static uint32_t s_nInjectedReadInterrupts = 0;
static uint32_t s_nInjectedRequeryFailures = 0;
static uint32_t s_nInjectedAxisFailureCode = UINT32_MAX;

extern "C" int __real_ioctl( int nFd, unsigned long nRequest, ... );
extern "C" int __wrap_ioctl( int nFd, unsigned long nRequest, ... )
{
	va_list args;
	va_start( args, nRequest );
	if ( nRequest == EVIOCRMFF ) {
		const int nEffectId = va_arg( args, int );
		va_end( args );
		if ( s_bFakeGamepadIoctl && nFd == s_nFakeGamepadFd ) {
			return nEffectId == 7 ? 0 : -1;
		}
		return __real_ioctl( nFd, nRequest, nEffectId );
	}
	void *pArgument = va_arg( args, void * );
	va_end( args );
	if ( !s_bFakeGamepadIoctl || _IOC_TYPE( nRequest ) != 'E' ) {
		return __real_ioctl( nFd, nRequest, pArgument );
	}
	const uint32_t nCode = _IOC_NR( nRequest );
	if ( s_nFakeGamepadFd < 0 && nCode == 0x21 ) {
		s_nFakeGamepadFd = nFd;
	}
	if ( nFd != s_nFakeGamepadFd ) {
		return __real_ioctl( nFd, nRequest, pArgument );
	}
	++s_nFakeIoctlCalls;
	if ( nCode == 0x21 || nCode == 0x35 ) {
		memset( pArgument, 0, _IOC_SIZE( nRequest ) );
		unsigned long *pBits = ( unsigned long * )pArgument;
		const uint32_t nBit = nCode == 0x21 ? BTN_GAMEPAD : FF_RUMBLE;
		pBits[nBit / ( sizeof( unsigned long ) * 8 )] |= 1UL << ( nBit % ( sizeof( unsigned long ) * 8 ) );
		return 0;
	}
	if ( nCode == 0x02 ) {
		input_id *pIdentity = ( input_id * )pArgument;
		*pIdentity = { BUS_USB, 0x1234, 0x5678, 1 };
		return 0;
	}
	if ( nCode == 0x07 ) {
		const char pszPhysical[] = "usb-test/input0";
		const size_t nSize = _IOC_SIZE( nRequest );
		if ( nSize < sizeof( pszPhysical ) ) {
			return -1;
		}
		memcpy( pArgument, pszPhysical, sizeof( pszPhysical ) );
		return sizeof( pszPhysical );
	}
	if ( nCode == 0xa0 ) {
		return 0;
	}
	if ( nCode == 0x18 ) {
		if ( s_nInjectedRequeryFailures != 0 ) {
			--s_nInjectedRequeryFailures;
			errno = EIO;
			return -1;
		}
		memset( pArgument, 0, _IOC_SIZE( nRequest ) );
		unsigned long *pKeys = ( unsigned long * )pArgument;
		pKeys[BTN_SOUTH / ( sizeof( unsigned long ) * 8 )] |= 1UL << ( BTN_SOUTH % ( sizeof( unsigned long ) * 8 ) );
		return 0;
	}
	if ( nCode >= 0x40 && nCode < 0x40 + ABS_MAX ) {
		if ( s_nInjectedAxisFailureCode == nCode - 0x40 ) {
			s_nInjectedAxisFailureCode = UINT32_MAX;
			errno = EIO;
			return -1;
		}
		input_absinfo *pAxis = ( input_absinfo * )pArgument;
		*pAxis = {};
		const uint32_t nAxis = nCode - 0x40;
		pAxis->minimum = nAxis == ABS_Z || nAxis == ABS_RZ ? 0 : -32768;
		pAxis->maximum = nAxis == ABS_Z || nAxis == ABS_RZ ? 255 : 32767;
		pAxis->value = nAxis == ABS_HAT0X ? -1 : ( nAxis == ABS_HAT0Y ? 0 : pAxis->maximum / 2 );
		return 0;
	}
	if ( nCode == 0x80 ) {
		ff_effect *pEffect = ( ff_effect * )pArgument;
		pEffect->id = 7;
		return 0;
	}
	return __real_ioctl( nFd, nRequest, pArgument );
}

extern "C" ssize_t __real_read( int nFd, void *pBuf, size_t nBytes );
extern "C" ssize_t __wrap_read( int nFd, void *pBuf, size_t nBytes )
{
	if ( nFd == s_nWatchedFd ) {
		++s_nReadCalls;
		if ( s_nInjectedReadInterrupts != 0 ) {
			--s_nInjectedReadInterrupts;
			errno = EINTR;
			return -1;
		}
	}
	return __real_read( nFd, pBuf, nBytes );
}

class SIRInputEvdevBatchTestPeer
{
public:
	static bool install( SIRLinuxEvdevBackend& backend, int nReadFd, sirInputDeviceId_t device, bool bMonotonic = false ) noexcept
	{
		sirInputEvdevSlot_t& slot = backend.m_Slots[0];
		if ( slot.bOpen ) {
			return false;
		}
		slot = {};
		slot.bOpen = true;
		slot.nFd = nReadFd;
		slot.id = device;
		slot.nRumbleEffect = -1;
		slot.bClockMonotonic = bMonotonic;
		return true;
	}

	static int16_t resolve( SIRLinuxEvdevBackend& backend, uint16_t nCode, bool bProfile, bool bReplace ) noexcept
	{
		sirInputEvdevSlot_t slot = {};
		slot.bHasProfile = bProfile;
		slot.profile.nButtonCount = 1;
		slot.profile.buttons[0] = { BTN_EAST, SIR_INPUT_PAD_SOUTH };
		slot.profile.bReplaceButtons = bReplace;
		return backend.resolveButton( slot, nCode );
	}

	static void pollHotplug( SIRLinuxEvdevBackend& backend, SIRInputService& service ) noexcept
	{
		backend.pollHotplug( service );
	}

	static sirInputDeviceId_t firstDevice( const SIRLinuxEvdevBackend& backend ) noexcept
	{
		return backend.m_Slots[0].id;
	}

	static bool firstOpen( const SIRLinuxEvdevBackend& backend ) noexcept
	{
		return backend.m_Slots[0].bOpen;
	}

	static int8_t firstHatX( const SIRLinuxEvdevBackend& backend ) noexcept
	{
		return backend.m_Slots[0].nHatX;
	}

	static bool queueBoundary( SIRLinuxEvdevBackend& backend ) noexcept
	{
		sirInputEvdevSlot_t slot = {};
		if ( !backend.queueChange( slot, SIR_INPUT_BUTTON, SIR_INPUT_PAD_SOUTH, 1 ) || slot.nPending != 1 ) {
			return false;
		}
		slot.nPending = SIR_INPUT_EVDEV_REPORT_CAPACITY;
		return !backend.queueChange( slot, SIR_INPUT_BUTTON, SIR_INPUT_PAD_SOUTH, 1 ) && slot.bDesynchronized && slot.nPending == 0;
	}

	static bool open( SIRLinuxEvdevBackend& backend, SIRInputService& service, const char *pszPath ) noexcept
	{
		return backend.openDevice( service, pszPath );
	}

	static bool desynchronized( const SIRLinuxEvdevBackend& backend ) noexcept
	{
		return backend.m_Slots[0].bDesynchronized;
	}

	static void configureAxes( SIRLinuxEvdevBackend& backend ) noexcept
	{
		sirInputEvdevSlot_t& slot = backend.m_Slots[0];
		slot.bHatDPad = true;
		slot.axes[SIR_INPUT_AXIS_LX] = { -100, 100, ABS_X, false, true, false };
		slot.axes[SIR_INPUT_AXIS_LY] = { -100, 100, ABS_Y, false, true, true };
	}

	static void setRumble( SIRLinuxEvdevBackend& backend, bool bCanRumble ) noexcept
	{
		backend.m_Slots[0].bCanRumble = bCanRumble;
	}
};

static bool createPipe( int *pReadFd, int *pWriteFd ) noexcept
{
	int szFds[2] = {};
	if ( pipe( szFds ) != 0 ) {
		return false;
	}
	const int nFlags = fcntl( szFds[0], F_GETFL, 0 );
	if ( nFlags < 0 || fcntl( szFds[0], F_SETFL, nFlags | O_NONBLOCK ) != 0 ) {
		close( szFds[0] );
		close( szFds[1] );
		return false;
	}
	*pReadFd = szFds[0];
	*pWriteFd = szFds[1];
	return true;
}

static bool testBatchedReports( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	if ( !service.connect( { 0, 987, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &id ) || !service.publish() || !service.begin( 2 ) ) {
		return false;
	}
	int nReadFd = -1;
	int nWriteFd = -1;
	if ( !createPipe( &nReadFd, &nWriteFd ) ) {
		return false;
	}
	if ( !SIRInputEvdevBatchTestPeer::install( backend, nReadFd, id ) ) {
		close( nReadFd );
		close( nWriteFd );
		return false;
	}
	constexpr uint32_t RECORDS = 140;
	input_event szRecords[RECORDS] = {};
	for ( uint32_t i = 0; i < RECORDS; ++i ) {
		if ( ( i & 1 ) == 0 ) {
			szRecords[i].type = EV_KEY;
			szRecords[i].code = BTN_SOUTH;
			szRecords[i].value = ( ( i / 2 ) & 1 ) == 0 ? 1 : 0;
		} else {
			szRecords[i].type = EV_SYN;
			szRecords[i].code = SYN_REPORT;
		}
	}
	const ssize_t nWritten = write( nWriteFd, szRecords, sizeof( szRecords ) );
	if ( nWritten != ( ssize_t )sizeof( szRecords ) ) {
		close( nWriteFd );
		service.shutdown();
		return false;
	}
	s_nWatchedFd = nReadFd;
	s_nReadCalls = 0;
	backend.sample( service );
	const uint32_t nReads = s_nReadCalls;
	s_nWatchedFd = -1;
	const bool bPublished = service.publish();
	const sirInputFrame_t *pFrame = service.frame();
	const uint32_t nBit = UINT32_C( 1 ) << SIR_INPUT_PAD_SOUTH;
	const bool bValid = bPublished && pFrame != nullptr && pFrame->nCount == RECORDS / 2 && nReads >= 3 && nReads <= 4 &&
	                    ( pFrame->state[id.nSlot].nHeld[0] & nBit ) == 0 && ( pFrame->state[id.nSlot].nPressed[0] & nBit ) != 0 &&
	                    ( pFrame->state[id.nSlot].nReleased[0] & nBit ) != 0;
	close( nWriteFd );
	service.shutdown();
	if ( !bValid ) {
		fprintf( stderr, "evdev batch failure: events=%u reads=%u published=%d\n", pFrame != nullptr ? pFrame->nCount : 0, nReads,
		         bPublished ? 1 : 0 );
	}
	return bValid;
}

static bool testPartialReportAcrossReads( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	if ( !service.connect( { 0, 985, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &id ) || !service.publish() || !service.begin( 2 ) ) {
		return false;
	}
	int nReadFd = -1;
	int nWriteFd = -1;
	if ( !createPipe( &nReadFd, &nWriteFd ) ) {
		return false;
	}
	if ( !SIRInputEvdevBatchTestPeer::install( backend, nReadFd, id, true ) ) {
		close( nReadFd );
		close( nWriteFd );
		return false;
	}
	input_event press = {};
	press.type = EV_KEY;
	press.code = BTN_SOUTH;
	press.value = 1;
	input_event report = {};
	report.type = EV_SYN;
	report.code = SYN_REPORT;
	report.time.tv_sec = 250;
	report.time.tv_usec = 321;
	if ( write( nWriteFd, &press, sizeof( press ) ) != sizeof( press ) ) {
		service.shutdown();
		close( nWriteFd );
		return false;
	}
	backend.sample( service );
	if ( !service.publish() || service.frame()->nCount != 0 || !service.begin( 3 ) ) {
		service.shutdown();
		close( nWriteFd );
		return false;
	}
	if ( write( nWriteFd, &report, sizeof( report ) ) != sizeof( report ) ) {
		service.shutdown();
		close( nWriteFd );
		return false;
	}
	backend.sample( service );
	bool bValid = service.publish() && service.frame()->nCount == 1 && service.frame()->events[0].nTimestampNs == 250000321000ULL &&
	              service.frame()->events[0].eClock == SIR_INPUT_CLOCK_EVDEV_MONOTONIC &&
	              ( service.frame()->events[0].nTimestampFlags & SIR_INPUT_TIMESTAMP_NATIVE ) != 0 &&
	              service.frame()->events[0].nReceivedNs != 0 && ( service.frame()->state[id.nSlot].nHeld[0] & UINT32_C( 1 ) ) != 0;
	if ( bValid ) {
		input_event release = {};
		release.type = EV_KEY;
		release.code = BTN_SOUTH;
		release.value = 0;
		input_event badTime = {};
		badTime.type = EV_SYN;
		badTime.code = SYN_REPORT;
		badTime.time.tv_sec = 300;
		badTime.time.tv_usec = 1000000;
		bValid = service.begin( 4 ) && write( nWriteFd, &release, sizeof( release ) ) == ( ssize_t )sizeof( release ) &&
		         write( nWriteFd, &badTime, sizeof( badTime ) ) == ( ssize_t )sizeof( badTime );
		if ( bValid ) {
			backend.sample( service );
			bValid = service.publish() && service.frame()->nCount == 1 &&
			         service.frame()->events[0].eClock == SIR_INPUT_CLOCK_ENGINE_MONOTONIC &&
			         service.frame()->events[0].nTimestampFlags == 0 &&
			         service.frame()->events[0].nTimestampNs == service.frame()->events[0].nReceivedNs &&
			         ( service.frame()->state[id.nSlot].nHeld[0] & UINT32_C( 1 ) ) == 0;
		}
	}
	service.shutdown();
	close( nWriteFd );
	return bValid;
}

static bool testMappingAndQueueBoundaries( void ) noexcept
{
	SIRLinuxEvdevBackend backend;
	if ( SIRLinuxEvdevBackend::physicalKey( nullptr, 1, 1 ) != 0 || SIRLinuxEvdevBackend::physicalKey( "", 1, 1 ) != 0 ||
	     SIRLinuxEvdevBackend::physicalKey( "usb-7/input0", 123, 456 ) != SIRLinuxEvdevBackend::physicalKey( "usb-7/input1", 123, 456 ) ||
	     SIRLinuxEvdevBackend::physicalKey( "usb-7/input0", 123, 456 ) == SIRLinuxEvdevBackend::physicalKey( "usb-7/input0", 124, 456 ) ||
	     SIRLinuxEvdevBackend::normalize( 4, 4, 4, false ) != 0 || SIRLinuxEvdevBackend::normalize( -1000, -50, 50, false ) != -32767 ||
	     SIRLinuxEvdevBackend::normalize( 1000, -50, 50, false ) != 32767 || SIRLinuxEvdevBackend::normalize( -1000, -50, 50, true ) != 0 ||
	     SIRLinuxEvdevBackend::normalize( 1000, -50, 50, true ) != 32767 ) {
		return false;
	}
	if ( SIRInputEvdevBatchTestPeer::resolve( backend, BTN_SOUTH, false, false ) != SIR_INPUT_PAD_SOUTH ||
	     SIRInputEvdevBatchTestPeer::resolve( backend, BTN_EAST, true, false ) != SIR_INPUT_PAD_SOUTH ||
	     SIRInputEvdevBatchTestPeer::resolve( backend, BTN_TR, true, false ) != SIR_INPUT_PAD_RB ||
	     SIRInputEvdevBatchTestPeer::resolve( backend, BTN_SOUTH, true, false ) != -1 ||
	     SIRInputEvdevBatchTestPeer::resolve( backend, BTN_SOUTH, true, true ) != -1 ) {
		return false;
	}
	return SIRInputEvdevBatchTestPeer::queueBoundary( backend );
}

static bool testDiscoveryAndOpenFailures( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	char szTooLong[600] = {};
	for ( uint32_t i = 0; i < 599; ++i ) {
		szTooLong[i] = 'x';
	}
	if ( backend.discover( service, nullptr ) || backend.discover( service, szTooLong ) ||
	     SIRInputEvdevBatchTestPeer::open( backend, service, nullptr ) || SIRInputEvdevBatchTestPeer::open( backend, service, szTooLong ) ||
	     SIRInputEvdevBatchTestPeer::open( backend, service, "/tmp/sir-r0-absent-device-node" ) ||
	     backend.loadCookedProfilesFile( nullptr ) || backend.loadCookedProfilesFile( "" ) ||
	     backend.loadCookedProfilesFile( "/tmp/sir-r0-absent-profile" ) ) {
		return false;
	}
	char szDirectory[] = "/tmp/sir-evdev-r0-XXXXXX";
	if ( mkdtemp( szDirectory ) == nullptr ) {
		return false;
	}
	char szEventPath[640] = {};
	const int nLength = snprintf( szEventPath, sizeof( szEventPath ), "%s/event0", szDirectory );
	bool bValid = nLength > 0 && nLength < ( int )sizeof( szEventPath );
	FILE *pFile = bValid ? fopen( szEventPath, "wb" ) : nullptr;
	if ( pFile == nullptr ) {
		bValid = false;
	} else {
		fclose( pFile );
	}
	if ( bValid ) {
		bValid = backend.discover( service, szDirectory ) && !SIRInputEvdevBatchTestPeer::open( backend, service, szEventPath ) &&
		         service.activeDeviceCount() == 0;
		sirInputControllerProfile_t profile = {};
		profile.nVendor = 1;
		profile.nProduct = 2;
		bValid = bValid && !backend.addProfile( profile ) && !backend.importCookedProfiles( nullptr, 4 );
	}
	unlink( szEventPath );
	if ( bValid ) {
		backend.sample( service );
		bValid = service.publish() && service.activeDeviceCount() == 0;
	}
	service.shutdown();
	rmdir( szDirectory );
	return bValid;
}

static bool testDesyncAndReadFailureLifecycle( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	if ( !service.connect( { 0, 952, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &id ) || !service.publish() || !service.begin( 2 ) ) {
		return false;
	}
	int nReadFd = -1;
	int nWriteFd = -1;
	if ( !createPipe( &nReadFd, &nWriteFd ) ) {
		return false;
	}
	if ( !SIRInputEvdevBatchTestPeer::install( backend, nReadFd, id ) ) {
		close( nReadFd );
		close( nWriteFd );
		return false;
	}
	input_event szEvents[3] = {};
	szEvents[0].type = EV_SYN;
	szEvents[0].code = SYN_DROPPED;
	szEvents[1].type = EV_KEY;
	szEvents[1].code = BTN_EAST;
	szEvents[1].value = 1;
	szEvents[2].type = EV_SYN;
	szEvents[2].code = SYN_REPORT;
	bool bValid = write( nWriteFd, szEvents, sizeof( szEvents ) ) == ( ssize_t )sizeof( szEvents );
	if ( bValid ) {
		backend.sample( service );
		bValid = service.publish() && service.frame() != nullptr && service.frame()->nCount == 0 &&
		         SIRInputEvdevBatchTestPeer::desynchronized( backend );
	}
	if ( bValid ) {
		bValid = service.begin( 3 );
		backend.resynchronize( service );
		bValid = bValid && SIRInputEvdevBatchTestPeer::desynchronized( backend );
	}
	close( nWriteFd );
	if ( bValid ) {
		backend.sample( service );
		bValid = service.activeDeviceCount() == 0 && !service.describe( id, nullptr ) && service.publish();
	}
	service.shutdown();
	return bValid;
}

static bool testAxisHatAndRumbleErrors( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	if ( !service.connect( { 0, 951, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &id ) || !service.publish() || !service.begin( 2 ) ) {
		return false;
	}
	int nReadFd = -1;
	int nWriteFd = -1;
	if ( !createPipe( &nReadFd, &nWriteFd ) ) {
		return false;
	}
	if ( !SIRInputEvdevBatchTestPeer::install( backend, nReadFd, id ) ) {
		close( nReadFd );
		close( nWriteFd );
		return false;
	}
	SIRInputEvdevBatchTestPeer::configureAxes( backend );
	input_event szEvents[7] = {};
	szEvents[0].type = EV_ABS;
	szEvents[0].code = ABS_X;
	szEvents[0].value = 50;
	szEvents[1].type = EV_ABS;
	szEvents[1].code = ABS_Y;
	szEvents[1].value = -50;
	szEvents[2].type = EV_ABS;
	szEvents[2].code = ABS_HAT0X;
	szEvents[2].value = -1;
	szEvents[3].type = EV_ABS;
	szEvents[3].code = ABS_HAT0Y;
	szEvents[3].value = 1;
	szEvents[4].type = EV_KEY;
	szEvents[4].code = BTN_EAST;
	szEvents[4].value = 2;
	szEvents[5].type = EV_ABS;
	szEvents[5].code = ABS_HAT1X;
	szEvents[5].value = 1;
	szEvents[6].type = EV_SYN;
	szEvents[6].code = SYN_REPORT;
	bool bValid = write( nWriteFd, szEvents, sizeof( szEvents ) ) == ( ssize_t )sizeof( szEvents );
	if ( bValid ) {
		backend.sample( service );
		bValid = service.publish() && service.frame()->nCount == 4 && service.frame()->state[id.nSlot].nAxes[SIR_INPUT_AXIS_LX] == 16383 &&
		         service.frame()->state[id.nSlot].nAxes[SIR_INPUT_AXIS_LY] == 16384;
	}
	if ( bValid ) {
		bValid =
		    !backend.vibrate( { id.nSlot, id.nGeneration + 1 }, 100, 100 ) && backend.hapticCapabilities( id ) == SIR_INPUT_HAPTIC_CAP_NONE;
		SIRInputEvdevBatchTestPeer::setRumble( backend, true );
		bValid = bValid && backend.hapticCapabilities( id ) == SIR_INPUT_HAPTIC_CAP_RUMBLE && !backend.vibrate( id, 50, 25 );
	}
	close( nWriteFd );
	service.shutdown();
	return bValid;
}

static bool testEmulatedControllerOpenAndRumble( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	s_nFakeGamepadFd = -1;
	s_nFakeIoctlCalls = 0;
	s_bFakeGamepadIoctl = true;
	const bool bOpened = SIRInputEvdevBatchTestPeer::open( backend, service, "/dev/null" );
	const sirInputDeviceId_t id = SIRInputEvdevBatchTestPeer::firstDevice( backend );
	bool bValid = bOpened && SIRInputEvdevBatchTestPeer::firstOpen( backend ) && service.activeDeviceCount() == 1 &&
	              s_nFakeIoctlCalls >= 10 && backend.hapticCapabilities( id ) == SIR_INPUT_HAPTIC_CAP_RUMBLE;
	if ( bValid ) {
		bValid = backend.vibrate( id, 12000, 9000 ) && backend.vibrate( id, 0, 0 ) &&
		         !SIRInputEvdevBatchTestPeer::open( backend, service, "/dev/null" );
	}
	if ( bValid ) {
		bValid = service.publish() && service.frame() != nullptr && service.frame()->nCount >= 2;
	}
	service.shutdown();
	s_bFakeGamepadIoctl = false;
	s_nFakeGamepadFd = -1;
	return bValid && service.activeDeviceCount() == 0;
}

static bool testEmulatedHotplugLifecycle( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	char szDirectory[] = "/tmp/sir-evdev-hotplug-XXXXXX";
	if ( mkdtemp( szDirectory ) == nullptr ) {
		return false;
	}
	bool bValid = backend.discover( service, szDirectory );
	char szEventPath[512] = {};
	const int nLen = snprintf( szEventPath, sizeof( szEventPath ), "%s/event5", szDirectory );
	if ( bValid && nLen > 0 && nLen < ( int )sizeof( szEventPath ) ) {
		s_nFakeGamepadFd = -1;
		s_nFakeIoctlCalls = 0;
		s_bFakeGamepadIoctl = true;
		bValid = symlink( "/dev/null", szEventPath ) == 0;
		if ( bValid ) {
			SIRInputEvdevBatchTestPeer::pollHotplug( backend, service );
			bValid = SIRInputEvdevBatchTestPeer::firstOpen( backend ) && service.activeDeviceCount() == 1 && s_nFakeIoctlCalls >= 10;
		}
		if ( bValid ) {
			bValid = unlink( szEventPath ) == 0;
			SIRInputEvdevBatchTestPeer::pollHotplug( backend, service );
			bValid = bValid && !SIRInputEvdevBatchTestPeer::firstOpen( backend ) && service.activeDeviceCount() == 0;
		}
	}
	unlink( szEventPath );
	service.shutdown();
	s_bFakeGamepadIoctl = false;
	s_nFakeGamepadFd = -1;
	rmdir( szDirectory );
	return bValid;
}

static bool testInterruptBudgetAndRetry( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	if ( !service.connect( { 0, 982, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &id ) || !service.publish() || !service.begin( 2 ) ) {
		return false;
	}
	int nReadFd = -1;
	int nWriteFd = -1;
	if ( !createPipe( &nReadFd, &nWriteFd ) ) {
		return false;
	}
	if ( !SIRInputEvdevBatchTestPeer::install( backend, nReadFd, id ) ) {
		close( nReadFd );
		close( nWriteFd );
		return false;
	}
	input_event szEvents[2] = {};
	szEvents[0].type = EV_KEY;
	szEvents[0].code = BTN_SOUTH;
	szEvents[0].value = 1;
	szEvents[1].type = EV_SYN;
	szEvents[1].code = SYN_REPORT;
	bool bValid = write( nWriteFd, szEvents, sizeof( szEvents ) ) == ( ssize_t )sizeof( szEvents );
	if ( bValid ) {
		s_nWatchedFd = nReadFd;
		s_nReadCalls = 0;
		s_nInjectedReadInterrupts = 16;
		backend.sample( service );
		bValid = s_nReadCalls == 4 && s_nInjectedReadInterrupts == 12;
		s_nInjectedReadInterrupts = 0;
		backend.sample( service );
		s_nWatchedFd = -1;
		bValid = bValid && service.publish() && service.frame() != nullptr && service.frame()->nCount == 1 &&
		         ( service.frame()->state[id.nSlot].nHeld[0] & UINT32_C( 1 ) ) != 0;
	}
	s_nInjectedReadInterrupts = 0;
	s_nWatchedFd = -1;
	close( nWriteFd );
	service.shutdown();
	return bValid;
}

static bool testFailedDesyncRequeryRecovery( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	if ( !service.connect( { 0, 983, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &id ) || !service.publish() || !service.begin( 2 ) ) {
		return false;
	}
	int nReadFd = -1;
	int nWriteFd = -1;
	if ( !createPipe( &nReadFd, &nWriteFd ) ) {
		return false;
	}
	if ( !SIRInputEvdevBatchTestPeer::install( backend, nReadFd, id ) ) {
		close( nReadFd );
		close( nWriteFd );
		return false;
	}
	s_nFakeGamepadFd = nReadFd;
	s_bFakeGamepadIoctl = true;
	s_nInjectedRequeryFailures = 1;
	input_event szDropped[3] = {};
	szDropped[0].type = EV_SYN;
	szDropped[0].code = SYN_DROPPED;
	szDropped[1].type = EV_KEY;
	szDropped[1].code = BTN_EAST;
	szDropped[1].value = 1;
	szDropped[2].type = EV_SYN;
	szDropped[2].code = SYN_REPORT;
	bool bValid = write( nWriteFd, szDropped, sizeof( szDropped ) ) == ( ssize_t )sizeof( szDropped );
	if ( bValid ) {
		backend.sample( service );
		bValid = SIRInputEvdevBatchTestPeer::desynchronized( backend ) && s_nInjectedRequeryFailures == 0 && service.publish() &&
		         service.frame() != nullptr && service.frame()->nCount == 0 && service.begin( 3 );
	}
	if ( bValid ) {
		input_event szIgnored[2] = {};
		szIgnored[0].type = EV_KEY;
		szIgnored[0].code = BTN_WEST;
		szIgnored[0].value = 1;
		szIgnored[1].type = EV_SYN;
		szIgnored[1].code = SYN_REPORT;
		bValid = write( nWriteFd, szIgnored, sizeof( szIgnored ) ) == ( ssize_t )sizeof( szIgnored );
		if ( bValid ) {
			backend.sample( service );
			const sirInputFrame_t *pFrame = nullptr;
			bValid = !SIRInputEvdevBatchTestPeer::desynchronized( backend ) && service.publish() &&
			         ( pFrame = service.frame() ) != nullptr && pFrame->nCount == 2 && pFrame->events[0].eSignal == SIR_INPUT_RESYNC &&
			         pFrame->events[1].eSignal == SIR_INPUT_BUTTON && pFrame->events[1].nControl == SIR_INPUT_PAD_SOUTH &&
			         pFrame->events[1].nValue == 1 && ( pFrame->state[id.nSlot].nHeld[0] & UINT32_C( 1 ) ) != 0;
		}
	}
	if ( bValid ) {
		input_event szRelease[2] = {};
		szRelease[0].type = EV_KEY;
		szRelease[0].code = BTN_SOUTH;
		szRelease[0].value = 0;
		szRelease[1].type = EV_SYN;
		szRelease[1].code = SYN_REPORT;
		bValid = service.begin( 4 ) && write( nWriteFd, szRelease, sizeof( szRelease ) ) == ( ssize_t )sizeof( szRelease );
		if ( bValid ) {
			backend.sample( service );
			bValid =
			    service.publish() && service.frame()->nCount == 1 && ( service.frame()->state[id.nSlot].nHeld[0] & UINT32_C( 1 ) ) == 0;
		}
	}
	s_bFakeGamepadIoctl = false;
	s_nFakeGamepadFd = -1;
	s_nInjectedRequeryFailures = 0;
	close( nWriteFd );
	service.shutdown();
	return bValid;
}

static bool testAtomicAnalogResynchronization( void ) noexcept
{
	SIRInputService service;
	SIRLinuxEvdevBackend backend;
	if ( !service.attach( backend ) || !service.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t id = {};
	const sirInputControlChange_t held = { SIR_INPUT_BUTTON, SIR_INPUT_PAD_EAST, 1 };
	if ( !service.connect( { 0, 984, SIR_INPUT_LINUX_EVDEV, SIR_INPUT_CONTROLLER }, &id ) ||
	     !service.emitControls( SIR_INPUT_LINUX_EVDEV, id, &held, 1 ) || !service.publish() || !service.begin( 2 ) ) {
		return false;
	}

	int nReadFd = -1;
	int nWriteFd = -1;
	if ( !createPipe( &nReadFd, &nWriteFd ) ) {
		return false;
	}
	if ( !SIRInputEvdevBatchTestPeer::install( backend, nReadFd, id ) ) {
		close( nReadFd );
		close( nWriteFd );
		return false;
	}
	SIRInputEvdevBatchTestPeer::configureAxes( backend );
	s_nFakeGamepadFd = nReadFd;
	s_bFakeGamepadIoctl = true;
	s_nInjectedAxisFailureCode = ABS_X;

	input_event szDropped[2] = {};
	szDropped[0].type = EV_SYN;
	szDropped[0].code = SYN_DROPPED;
	szDropped[1].type = EV_SYN;
	szDropped[1].code = SYN_REPORT;
	bool bValid = write( nWriteFd, szDropped, sizeof( szDropped ) ) == ( ssize_t )sizeof( szDropped );
	if ( bValid ) {
		backend.sample( service );
		bValid = s_nInjectedAxisFailureCode == UINT32_MAX && SIRInputEvdevBatchTestPeer::desynchronized( backend ) && service.publish() &&
		         service.frame()->nCount == 0 &&
		         ( service.frame()->state[id.nSlot].nHeld[0] & ( UINT32_C( 1 ) << SIR_INPUT_PAD_EAST ) ) != 0 && service.begin( 3 );
	}
	if ( bValid ) {
		s_nInjectedAxisFailureCode = ABS_HAT0Y;
		input_event report = {};
		report.type = EV_SYN;
		report.code = SYN_REPORT;
		bValid = write( nWriteFd, &report, sizeof( report ) ) == sizeof( report );
		if ( bValid ) {
			backend.sample( service );
			bValid = s_nInjectedAxisFailureCode == UINT32_MAX && SIRInputEvdevBatchTestPeer::desynchronized( backend ) &&
			         SIRInputEvdevBatchTestPeer::firstHatX( backend ) == 0 && service.publish() && service.frame()->nCount == 0 &&
			         ( service.frame()->state[id.nSlot].nHeld[0] & ( UINT32_C( 1 ) << SIR_INPUT_PAD_EAST ) ) != 0 && service.begin( 4 );
		}
	}
	if ( bValid ) {
		input_event report = {};
		report.type = EV_SYN;
		report.code = SYN_REPORT;
		bValid = write( nWriteFd, &report, sizeof( report ) ) == sizeof( report );
		if ( bValid ) {
			backend.sample( service );
			bValid = !SIRInputEvdevBatchTestPeer::desynchronized( backend ) && SIRInputEvdevBatchTestPeer::firstHatX( backend ) == -1 &&
			         service.publish();
			if ( bValid ) {
				const sirInputFrame_t *pFrame = service.frame();
				bValid = pFrame != nullptr && pFrame->nCount == 5 && pFrame->events[0].eSignal == SIR_INPUT_RESYNC &&
				         ( pFrame->state[id.nSlot].nHeld[0] & ( UINT32_C( 1 ) << SIR_INPUT_PAD_EAST ) ) == 0 &&
				         ( pFrame->state[id.nSlot].nHeld[0] & ( UINT32_C( 1 ) << SIR_INPUT_PAD_SOUTH ) ) != 0 &&
				         ( pFrame->state[id.nSlot].nHeld[0] & ( UINT32_C( 1 ) << SIR_INPUT_PAD_DPAD_LEFT ) ) != 0 &&
				         pFrame->state[id.nSlot].nAxes[SIR_INPUT_AXIS_LX] == 32767 &&
				         pFrame->state[id.nSlot].nAxes[SIR_INPUT_AXIS_LY] == -32767;
			}
		}
	}
	s_nInjectedAxisFailureCode = UINT32_MAX;
	s_nFakeGamepadFd = -1;
	s_bFakeGamepadIoctl = false;
	close( nWriteFd );
	service.shutdown();
	return bValid;
}

int main( void )
{
	if ( !testBatchedReports() || !testPartialReportAcrossReads() || !testMappingAndQueueBoundaries() || !testDiscoveryAndOpenFailures() ||
	     !testDesyncAndReadFailureLifecycle() || !testAxisHatAndRumbleErrors() || !testEmulatedControllerOpenAndRumble() ||
	     !testEmulatedHotplugLifecycle() || !testInterruptBudgetAndRetry() || !testFailedDesyncRequeryRecovery() ||
	     !testAtomicAnalogResynchronization() ) {
		fprintf( stderr, "SIRInputEvdevBatchTests FAIL\n" );
		return 1;
	}
	printf( "SIRInputEvdevBatchTests PASS\n" );
	return 0;
}
