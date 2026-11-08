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

#include <Core/SIRApplication.h>
#include <Core/Input/SIRInputBackend.h>
#include <Core/Input/SIRInputService.h>
#include <stdio.h>

class SIRApplicationMockInput final : public SIRInputBackend
{
public:
	sirInputBackendId_t id( void ) const noexcept override
	{
		return SIR_INPUT_TEST;
	}
	void sample( SIRInputService& input ) noexcept override
	{
		++m_nSamples;
		if ( m_bConnectOnSample ) {
			m_bConnectOnSample = false;
			( void )input.connect( { 0, 99, id(), SIR_INPUT_KEYBOARD }, &m_hDevice );
		}
	}
	void resynchronize( SIRInputService& input ) noexcept override
	{
		( void )input;
		++m_nResyncs;
	}
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override
	{
		( void )hDevice;
		( void )nLeft;
		( void )nRight;
		return false;
	}
	void stop( SIRInputService& input ) noexcept override
	{
		( void )input;
		++m_nStops;
	}
	sirInputDeviceId_t m_hDevice = {};
	uint32_t m_nSamples = 0;
	uint32_t m_nStops = 0;
	uint32_t m_nResyncs = 0;
	bool m_bConnectOnSample = false;
};

typedef struct sirInputAppTestContext_s
{
	SIRApplicationMockInput *pBackend;
	SIRInputService *pInput;
	uint32_t nPumps;
	bool bReject;
} sirInputAppTestContext_t;

static bool sirApplicationInputPump( void *pContext )
{
	sirInputAppTestContext_t *pTest = ( sirInputAppTestContext_t * )pContext;
	SIRInputService& input = *pTest->pInput;
	++pTest->nPumps;
	if ( pTest->nPumps == 1 ) {
		return input.connect( { 0, 99, SIR_INPUT_TEST, SIR_INPUT_KEYBOARD }, &pTest->pBackend->m_hDevice );
	}

	const sirInputControlChange_t change = { SIR_INPUT_BUTTON, 38, pTest->nPumps == 2 ? 1 : 0 };
	if ( !input.emitControls( SIR_INPUT_TEST, pTest->pBackend->m_hDevice, &change, 1 ) ) {
		return false;
	}
	return !pTest->bReject;
}

static bool sirTestApplicationInputLifecycle( void )
{
	SIRInputService input;
	SIRApplicationMockInput backend;
	SIRApplication application;
	sirInputAppTestContext_t context = { &backend, &input, 0, false };
	if ( !input.attach( backend ) || !application.attachInput( input ) || application.attachInput( input ) ||
	     !application.attachWindowPump( sirApplicationInputPump, &context ) ||
	     application.attachWindowPump( sirApplicationInputPump, &context ) || application.getInputFrame() != nullptr ) {
		return false;
	}

	if ( !application.initialize() || application.attachInput( input ) || !application.tick() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = application.getInputFrame();
	if ( pFrame == nullptr || pFrame->nTick != 1 || pFrame->nCount != 1 || backend.m_nSamples != 1 ||
	     !pFrame->nConnected[backend.m_hDevice.nSlot] ) {
		return false;
	}

	if ( !application.tick() ) {
		return false;
	}
	pFrame = application.getInputFrame();
	const uint32_t nMask = 1U << ( 38 % 32 );
	if ( pFrame == nullptr || pFrame->nTick != 2 || pFrame->nCount != 1 ||
	     ( pFrame->state[backend.m_hDevice.nSlot].nHeld[38 / 32] & nMask ) == 0 ||
	     ( pFrame->state[backend.m_hDevice.nSlot].nPressed[38 / 32] & nMask ) == 0 ) {
		return false;
	}

	context.bReject = true;
	if ( application.tick() || application.getTickCount() != 3 || context.nPumps != 3 ) {
		return false;
	}
	pFrame = application.getInputFrame();
	if ( pFrame == nullptr || pFrame->nTick != 3 || ( pFrame->state[backend.m_hDevice.nSlot].nReleased[38 / 32] & nMask ) == 0 ||
	     ( pFrame->state[backend.m_hDevice.nSlot].nHeld[38 / 32] & nMask ) != 0 ) {
		return false;
	}

	application.shutdown();
	application.shutdown();
	return backend.m_nStops == 1 && backend.m_nSamples == 3 && application.getInputFrame() == nullptr && input.activeDeviceCount() == 0 &&
	       !application.tick();
}

static bool sirTestHeadlessApplicationStillWorks( void )
{
	SIRApplication application;
	if ( !application.initialize() || !application.tick() || application.getTickCount() != 1 ) {
		return false;
	}
	application.shutdown();
	return application.getState() == SIR_APP_STOPPED;
}

typedef struct sirNoInputWindowPumpContext_s
{
	uint32_t nPumps;
	bool bReject;
} sirNoInputWindowPumpContext_t;

static bool sirApplicationNoInputWindowPump( void *pContext )
{
	sirNoInputWindowPumpContext_t *pTest = ( sirNoInputWindowPumpContext_t * )pContext;
	++pTest->nPumps;
	return !pTest->bReject;
}

static bool sirTestWindowPumpWithoutInput( void ) noexcept
{
	SIRApplication application;
	sirNoInputWindowPumpContext_t context = { 0, false };
	if ( application.attachWindowPump( nullptr, &context ) || !application.attachWindowPump( sirApplicationNoInputWindowPump, &context ) ||
	     application.attachWindowPump( sirApplicationNoInputWindowPump, &context ) ) {
		return false;
	}
	if ( !application.initialize() || application.attachWindowPump( sirApplicationNoInputWindowPump, &context ) || !application.tick() ||
	     context.nPumps != 1 || application.getTickCount() != 1 ) {
		return false;
	}
	context.bReject = true;
	if ( application.tick() || context.nPumps != 2 || application.getTickCount() != 2 ) {
		return false;
	}
	( void )application.shutdown();
	return application.getState() == SIR_APP_STOPPED && !application.tick();
}

int main( void )
{
	if ( !sirTestHeadlessApplicationStillWorks() || !sirTestApplicationInputLifecycle() || !sirTestWindowPumpWithoutInput() ) {
		fputs( "SIRApplicationInputTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRApplicationInputTests PASS" );
	return 0;
}
