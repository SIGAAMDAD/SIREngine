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
#include <Core/Input/SIRInputUIDispatch.h>
#include <stdio.h>

typedef struct sirInputUIReentrantProbe_s
{
	SIRInputUIDispatch *pDispatcher;
	const SIRInputService *pInput;
	const sirInputUIConsumer_t *pConsumer;
	uint32_t nNestedAttempts;
	uint32_t nNestedAccepted;
	uint32_t nFocusCalls;
	uint32_t nTextCalls;
	uint32_t nPreeditCalls;
} sirInputUIReentrantProbe_t;

static void sirAttemptNestedUIDispatch( sirInputUIReentrantProbe_t *pProbe ) noexcept
{
	++pProbe->nNestedAttempts;
	if ( pProbe->pDispatcher->dispatch( *pProbe->pInput, *pProbe->pConsumer ) ) {
		++pProbe->nNestedAccepted;
	}
}

static void sirRecordReentrantUIFocus( void *pContext, bool bFocused ) noexcept
{
	( void )bFocused;
	sirInputUIReentrantProbe_t *pProbe = ( sirInputUIReentrantProbe_t * )pContext;
	++pProbe->nFocusCalls;
	sirAttemptNestedUIDispatch( pProbe );
}

static void sirRecordReentrantUIText( void *pContext, uint32_t nCodepoint ) noexcept
{
	( void )nCodepoint;
	sirInputUIReentrantProbe_t *pProbe = ( sirInputUIReentrantProbe_t * )pContext;
	++pProbe->nTextCalls;
	sirAttemptNestedUIDispatch( pProbe );
}

static void sirRecordReentrantUIPreedit( void *pContext, const uint32_t *pCodepoints, uint32_t nCount, uint32_t nCaret,
                                         uint32_t nSelection ) noexcept
{
	( void )pCodepoints;
	( void )nCount;
	( void )nCaret;
	( void )nSelection;
	sirInputUIReentrantProbe_t *pProbe = ( sirInputUIReentrantProbe_t * )pContext;
	++pProbe->nPreeditCalls;
	sirAttemptNestedUIDispatch( pProbe );
}

int main( void )
{
	SIRInputService input;
	SIRInputUIDispatch dispatcher;
	sirInputUIReentrantProbe_t probe = {};
	sirInputUIConsumer_t consumer = {};
	probe.pDispatcher = &dispatcher;
	probe.pInput = &input;
	probe.pConsumer = &consumer;
	consumer.pContext = &probe;
	consumer.pOnFocus = sirRecordReentrantUIFocus;
	consumer.pOnText = sirRecordReentrantUIText;
	consumer.pOnPreedit = sirRecordReentrantUIPreedit;
	input.text().setEnabled( true );
	input.text().setFocused( true );

	if ( !input.begin( 1 ) || !input.text().appendCodepoint( 'X' ) || !input.publish() || !dispatcher.dispatch( input, consumer ) ||
	     dispatcher.dispatch( input, consumer ) || probe.nNestedAttempts != 3 || probe.nNestedAccepted != 0 || probe.nFocusCalls != 1 ||
	     probe.nTextCalls != 1 || probe.nPreeditCalls != 1 ) {
		fputs( "SIRInputUIDispatchReentrancyTests recursive dispatch FAILED\n", stderr );
		return 1;
	}

	if ( !input.begin( 2 ) || !input.publish() || !dispatcher.dispatch( input, consumer ) || probe.nNestedAttempts != 3 ||
	     probe.nFocusCalls != 1 || probe.nTextCalls != 1 || probe.nPreeditCalls != 1 ) {
		fputs( "SIRInputUIDispatchReentrancyTests next tick FAILED\n", stderr );
		return 1;
	}

	dispatcher.reset();
	if ( !dispatcher.dispatch( input, consumer ) || dispatcher.dispatch( input, consumer ) || probe.nNestedAttempts != 5 ||
	     probe.nNestedAccepted != 0 || probe.nFocusCalls != 2 || probe.nTextCalls != 1 || probe.nPreeditCalls != 2 ) {
		fputs( "SIRInputUIDispatchReentrancyTests reset FAILED\n", stderr );
		return 1;
	}

	input.shutdown();
	puts( "SIRInputUIDispatchReentrancyTests PASS" );
	return 0;
}
