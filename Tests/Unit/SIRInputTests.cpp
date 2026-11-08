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

#include <Core/Input/SIRInputService.h>
#include <Core/Input/SIRInputControllerProfiles.h>
#include <Core/Platform/SIRPlatformTime.h>
#if defined( __linux__ )
#include <Core/Platform/Linux/SIRLinuxEvdevBackend.h>
#include <linux/input.h>
#include <stdlib.h>
#include <unistd.h>
#endif
#if defined( SIR_INPUT_X11_AVAILABLE )
#include <Core/Platform/Linux/SIRLinuxX11Backend.h>
#endif
#include <stdio.h>

class SIRInputTestBackend final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override
	{
		return SIR_INPUT_TEST;
	}
	void sample( SIRInputService& service ) noexcept override
	{
		( void )service;
		++m_nSamples;
	}
	void resynchronize( SIRInputService& service ) noexcept override
	{
		( void )service;
		++m_nResyncs;
	}
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )hDevice;
		( void )nLeft;
		( void )nRight;
		return true;
	}
	void stop( SIRInputService& service ) noexcept override
	{
		( void )service;
		++m_nStops;
	}
	uint32_t m_nSamples = 0;
	uint32_t m_nStops = 0;
	uint32_t m_nResyncs = 0;
};

static bool testOwnershipAndGeneration( void )
{
	SIRInputService input;
	SIRInputTestBackend backend;
	if ( !input.attach( backend ) || input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	sirInputDeviceId_t first = {};
	const sirInputHardwareKey_t key = { 777, 15, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER };
	if ( !input.connect( key, &first ) || input.connect( key, &first ) || input.activeDeviceCount() != 1 || !input.pairUser( first, 3 ) ||
	     input.pairUser( first, 8 ) ) {
		return false;
	}
	sirPlatformDeviceIdentity_t native = {};
	if ( input.platformContext().nAbiVersion != SIR_PLATFORM_CONTEXT_ABI_VERSION || !input.describePlatformDevice( first, &native ) ||
	     native.nNativeId != 15 || native.nPhysicalId != 777 || native.nBackendId != ( SIR_PLATFORM_DEVICE_INPUT_BASE | SIR_INPUT_TEST ) ||
	     native.nGeneration != first.nGeneration || input.describePlatformDevice( first, nullptr ) ) {
		return false;
	}
	sirInputDeviceDescriptor_t d = {};
	if ( !input.describe( first, &d ) || d.nUserId != 3 || d.key.nPhysicalId != 777 || !input.findDevice( SIR_INPUT_TEST, 15, &d.id ) ) {
		return false;
	}
	sirInputDeviceId_t other = {};
	if ( input.connect( { 777, 99, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &other ) || !input.disconnect( SIR_INPUT_TEST, first ) ||
	     input.describe( first, &d ) || !input.connect( key, &other ) || other.nSlot != first.nSlot ||
	     other.nGeneration == first.nGeneration || input.describePlatformDevice( first, &native ) ||
	     !input.describePlatformDevice( other, &native ) || native.nGeneration != other.nGeneration ) {
		return false;
	}
	input.sample();
	if ( !input.publish() || backend.m_nSamples != 1 || input.frame()->nCount != 3 || input.frame()->nCutoffNs != 0 ||
	     input.frame()->nOldestReceiptAgeNs != 0 || input.frame()->nConnected[other.nSlot] != 1 ||
	     input.frame()->nUsers[other.nSlot] != SIR_INPUT_NO_USER ) {
		return false;
	}
	input.shutdown();
	return backend.m_nStops == 1 && input.activeDeviceCount() == 0;
}

static bool testFrameBoundaries( void )
{
	SIRInputService input;
	SIRInputTestBackend backend;
	if ( !input.attach( backend ) || !input.begin( 21 ) ) {
		return false;
	}
	sirInputDeviceId_t h = {};
	if ( !input.connect( { 0, 7, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &h ) ) {
		return false;
	}
	const sirInputControlChange_t changes[] = { { SIR_INPUT_BUTTON, 42, 1 }, { SIR_INPUT_BUTTON, 42, 0 }, { SIR_INPUT_BUTTON, 43, 1 } };
	input.setTimingMetricsEnabled( true );
	const uint64_t nStampedReceipt = SIRPlatformTime::getMonotonicNanoseconds();
	if ( !input.emitControls( SIR_INPUT_TEST, h, changes, 3, nStampedReceipt ) || input.begin( 22 ) || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = input.frame();
	const uint32_t nMask42 = 1U << ( 42 % 32 );
	if ( pFrame == nullptr || pFrame->nTick != 21 || pFrame->nCount != 4 || ( pFrame->state[h.nSlot].nPressed[42 / 32] & nMask42 ) == 0 ||
	     ( pFrame->state[h.nSlot].nReleased[42 / 32] & nMask42 ) == 0 || ( pFrame->state[h.nSlot].nHeld[42 / 32] & nMask42 ) != 0 ||
	     pFrame->events[1].nTimestampNs != nStampedReceipt || pFrame->events[1].nReceivedNs != nStampedReceipt ||
	     pFrame->events[1].eClock != SIR_INPUT_CLOCK_ENGINE_MONOTONIC || pFrame->events[1].nTimestampFlags != 0 ||
	     pFrame->nCutoffNs < pFrame->events[1].nReceivedNs || pFrame->nOldestReceiptAgeNs < pFrame->nNewestReceiptAgeNs ||
	     pFrame->events[2].nValue != 0 || input.begin( 21 ) ) {
		return false;
	}
	if ( !input.begin( 22 ) || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pNext = input.frame();
	return pNext != nullptr && pNext->nCount == 0 && pNext->nCutoffNs != 0 && pNext->nOldestReceiptAgeNs == 0 &&
	       pNext->nNewestReceiptAgeNs == 0 && pNext->state[h.nSlot].nPressed[42 / 32] == 0 &&
	       ( pNext->state[h.nSlot].nHeld[43 / 32] & ( 1U << ( 43 % 32 ) ) ) != 0;
}

static bool testAtomicOverflowAndRecovery( void )
{
	SIRInputService input;
	SIRInputTestBackend backend;
	if ( !input.attach( backend ) || !input.begin( 100 ) ) {
		return false;
	}
	sirInputDeviceId_t h = {};
	if ( !input.connect( { 0, 20, SIR_INPUT_TEST, SIR_INPUT_POINTER }, &h ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < SIR_INPUT_EVENT_CAPACITY - 2; ++i ) {
		if ( !input.emitPointer( SIR_INPUT_TEST, h, SIR_INPUT_MOTION, 0, 1, 0 ) ) {
			return false;
		}
	}
	const sirInputControlChange_t overflow[] = { { SIR_INPUT_BUTTON, 2, 1 }, { SIR_INPUT_BUTTON, 2, 0 } };
	if ( input.emitControls( SIR_INPUT_TEST, h, overflow, 2 ) || input.publish() || !input.recover() || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = input.frame();
	return pFrame != nullptr && backend.m_nResyncs == 1 && pFrame->nCount == 1 && pFrame->events[0].eSignal == SIR_INPUT_RESYNC &&
	       pFrame->nResyncCount == 1 && pFrame->state[h.nSlot].nMotionX == 0;
}

static bool testInvalidAndStale( void )
{
	SIRInputService input;
	SIRInputTestBackend backend;
	if ( !input.attach( backend ) || !input.begin( 100 ) ) {
		return false;
	}
	sirInputDeviceId_t h = {};
	if ( !input.connect( { 0, 111, SIR_INPUT_TEST, SIR_INPUT_CONTROLLER }, &h ) ) {
		return false;
	}
	const sirInputControlChange_t invalid[] = { { SIR_INPUT_BUTTON, 1, 1 }, { SIR_INPUT_AXIS, SIR_INPUT_AXIS_LT, -1 } };
	if ( input.emitControls( SIR_INPUT_TEST, h, invalid, 2 ) || input.emitControls( SIR_INPUT_LINUX_EVDEV, h, invalid, 2 ) ||
	     input.emitControls( SIR_INPUT_TEST, h, nullptr, 1 ) || !input.disconnect( SIR_INPUT_TEST, h ) ||
	     input.emitControls( SIR_INPUT_TEST, h, invalid, 2 ) || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *p = input.frame();
	return p != nullptr && p->nCount == 2 && p->events[0].eSignal == SIR_INPUT_CONNECTED &&
	       p->events[1].eSignal == SIR_INPUT_DISCONNECTED && !p->nConnected[h.nSlot];
}

static bool testControllerProfileRegistry( void )
{
	SIRInputControllerProfiles profiles;
	sirInputControllerProfile_t generic = {};
	generic.nVendor = 0x054c;
	generic.nProduct = 0x0ce6;
	generic.nButtonCount = 2;
	generic.buttons[0] = { 304, SIR_INPUT_PAD_EAST };
	generic.buttons[1] = { 305, SIR_INPUT_PAD_SOUTH };
	generic.nAxisCount = 2;
	generic.axes[0] = { 6, SIR_INPUT_AXIS_LX, false };
	generic.axes[1] = { 7, SIR_INPUT_AXIS_LY, true };
	generic.bReplaceButtons = true;
	generic.bReplaceAxes = true;
	if ( !profiles.add( generic ) || profiles.add( generic ) || profiles.count() != 1 ) {
		return false;
	}
	const sirInputControllerProfile_t *pGeneric = profiles.find( 3, 0x054c, 0x0ce6 );
	bool bInvert = false;
	if ( pGeneric == nullptr || SIRInputControllerProfiles::button( *pGeneric, 304 ) != SIR_INPUT_PAD_EAST ||
	     SIRInputControllerProfiles::button( *pGeneric, 0 ) != -1 ||
	     SIRInputControllerProfiles::axis( *pGeneric, 7, &bInvert ) != SIR_INPUT_AXIS_LY || !bInvert ||
	     profiles.find( 3, 0x045e, 0x0ce6 ) != nullptr ) {
		return false;
	}
	sirInputControllerProfile_t exact = generic;
	exact.nBus = 3;
	exact.buttons[0].nLogicalCode = SIR_INPUT_PAD_WEST;
	if ( !profiles.add( exact ) || profiles.find( 3, generic.nVendor, generic.nProduct ) == pGeneric ||
	     profiles.find( 5, generic.nVendor, generic.nProduct ) != pGeneric ) {
		return false;
	}
	sirInputControllerProfile_t invalid = generic;
	invalid.nBus = 5;
	invalid.buttons[1].nNativeCode = invalid.buttons[0].nNativeCode;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = generic;
	invalid.nBus = 5;
	invalid.buttons[1].nLogicalCode = invalid.buttons[0].nLogicalCode;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = generic;
	invalid.nBus = 5;
	invalid.axes[1].nNativeCode = invalid.axes[0].nNativeCode;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = generic;
	invalid.nBus = 5;
	invalid.axes[1].nLogicalCode = invalid.axes[0].nLogicalCode;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = generic;
	invalid.nBus = 5;
	invalid.axes[1].nLogicalCode = SIR_INPUT_AXIS_LT;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = generic;
	invalid.nBus = 5;
	invalid.nAxisCount = SIR_INPUT_AXIS_COUNT + 1;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	invalid = generic;
	invalid.nBus = 5;
	invalid.nButtonCount = SIR_INPUT_CONTROLLER_BUTTON_MAP_CAPACITY + 1;
	if ( profiles.add( invalid ) ) {
		return false;
	}
	if ( profiles.count() != 2 ) {
		return false;
	}
	// A missing mapping must leave the caller's output unchanged.
	bInvert = true;
	if ( SIRInputControllerProfiles::axis( *pGeneric, 700, &bInvert ) != -1 || !bInvert ) {
		return false;
	}
	invalid = generic;
	invalid.nBus = 5;
	invalid.nVendor = 0;
	if ( profiles.add( invalid ) ) {
		return false;
	}
#if defined( __linux__ )
	SIRLinuxEvdevBackend backend;
	if ( !backend.addProfile( generic ) || backend.addProfile( generic ) ) {
		return false;
	}
	SIRInputService input;
	if ( !input.attach( backend ) || !input.begin( 1 ) ) {
		return false;
	}
	char szDirectory[] = "/tmp/sir-input-profiles-XXXXXX";
	if ( mkdtemp( szDirectory ) == nullptr ) {
		return false;
	}
	const bool bDiscovered = backend.discover( input, szDirectory );
	const bool bRejectedLate = !backend.addProfile( exact );
	input.shutdown();
	const bool bRemoved = rmdir( szDirectory ) == 0;
	if ( !bDiscovered || !bRejectedLate || !bRemoved ) {
		return false;
	}
#endif
	return true;
}

static bool testDeviceResyncAccounted( void )
{
	SIRInputService input;
	SIRInputTestBackend backend;
	if ( !input.attach( backend ) || !input.begin( 8 ) ) {
		return false;
	}
	sirInputDeviceId_t keyboard = {};
	if ( !input.connect( { 0, 912, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &keyboard ) ) {
		return false;
	}
	const sirInputControlChange_t press = { SIR_INPUT_BUTTON, 30, 1 };
	if ( !input.emitControls( SIR_INPUT_TEST, keyboard, &press, 1 ) || !input.loseState( SIR_INPUT_TEST, keyboard ) || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *p = input.frame();
	if ( p == nullptr || p->nResyncCount != 1 || p->nCount != 3 || p->state[keyboard.nSlot].nHeld[0] != 0 ) {
		return false;
	}
	if ( !input.begin( 9 ) || !input.publish() ) {
		return false;
	}
	p = input.frame();
	return p != nullptr && p->nResyncCount == 0 && p->nCount == 0;
}

#if defined( __linux__ )
static bool testEvdevCodes( void )
{
	return SIRLinuxEvdevBackend::physicalKey( "usb-0000:00:14.0-1/input0", 0x045e, 0x0b12 ) ==
	           SIRLinuxEvdevBackend::physicalKey( "usb-0000:00:14.0-1/input1", 0x045e, 0x0b12 ) &&
	       SIRLinuxEvdevBackend::physicalKey( "usb-0000:00:14.0-1/input0", 0x045e, 0x0b12 ) !=
	           SIRLinuxEvdevBackend::physicalKey( "usb-0000:00:14.0-2/input0", 0x045e, 0x0b12 ) &&
	       SIRLinuxEvdevBackend::physicalKey( nullptr, 0, 0 ) == 0 && SIRLinuxEvdevBackend::mapButton( BTN_SOUTH ) == SIR_INPUT_PAD_SOUTH &&
	       SIRLinuxEvdevBackend::mapButton( KEY_A ) == -1 && SIRLinuxEvdevBackend::mapAxis( ABS_RX ) == SIR_INPUT_AXIS_RX &&
	       SIRLinuxEvdevBackend::mapAxis( ABS_HAT0X ) == -1 && SIRLinuxEvdevBackend::normalize( -32768, -32768, 32767, false ) == -32767 &&
	       SIRLinuxEvdevBackend::normalize( 32767, -32768, 32767, false ) == 32767 &&
	       SIRLinuxEvdevBackend::normalize( 255, 0, 255, true ) == 32767;
}
#endif

#if defined( SIR_INPUT_X11_AVAILABLE )
static bool testX11Adapter( void )
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	if ( !input.attach( backend ) || !input.begin( 1 ) || !backend.attachWindow( input, ( Window )1234 ) ) {
		return false;
	}
	XEvent event = {};
	event.xfocus.type = FocusIn;
	event.xfocus.window = ( Window )1234;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.xkey.type = KeyPress;
	event.xkey.window = ( Window )1234;
	event.xkey.keycode = 38;
	if ( !backend.processEvent( input, event ) ) {
		return false;
	}
	event.xfocus.type = FocusOut;
	event.xfocus.window = ( Window )1234;
	if ( !backend.processEvent( input, event ) || !input.publish() ) {
		return false;
	}
	const sirInputFrame_t *p = input.frame();
	return p != nullptr && p->nCount == 5 && p->events[2].eSignal == SIR_INPUT_BUTTON && p->events[3].eSignal == SIR_INPUT_RESYNC;
}
#endif

int main( void )
{
	const bool bOkay = testOwnershipAndGeneration() && testFrameBoundaries() && testAtomicOverflowAndRecovery() && testInvalidAndStale() &&
	                   testControllerProfileRegistry() && testDeviceResyncAccounted()
#if defined( __linux__ )
	                   && testEvdevCodes()
#endif
#if defined( SIR_INPUT_X11_AVAILABLE )
	                   && testX11Adapter()
#endif
	    ;
	if ( !bOkay ) {
		fputs( "SIRInputTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRInputTests PASS" );
	return 0;
}
