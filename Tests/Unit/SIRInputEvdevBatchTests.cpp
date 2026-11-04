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
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int s_nWatchedFd = -1;
static uint32_t s_nReadCalls = 0;

extern "C" ssize_t __real_read( int nFd, void *pBuf, size_t nBytes );
extern "C" ssize_t __wrap_read( int nFd, void *pBuf, size_t nBytes )
{
	if ( nFd == s_nWatchedFd ) {
		++s_nReadCalls;
	}
	return __real_read( nFd, pBuf, nBytes );
}

class SIRInputEvdevBatchTestPeer
{
public:
	static bool install( SIRLinuxEvdevBackend &backend, int nReadFd, sirInputDeviceId_t device, bool bMonotonic = false ) noexcept
	{
		sirInputEvdevSlot_t &slot = backend.m_Slots[0];
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

int main( void )
{
	if ( !testBatchedReports() || !testPartialReportAcrossReads() ) {
		fprintf( stderr, "SIRInputEvdevBatchTests FAIL\n" );
		return 1;
	}
	printf( "SIRInputEvdevBatchTests PASS\n" );
	return 0;
}
