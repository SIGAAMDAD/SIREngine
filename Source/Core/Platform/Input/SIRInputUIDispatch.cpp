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

#include <Core/Platform/Input/SIRInputUIDispatch.h>
#include <Core/Platform/Input/SIRInputService.h>

SIRInputUIDispatch::SIRInputUIDispatch( void ) noexcept
    : m_nLastTick( 0 ), m_nPreeditRevision( 0 ), m_bFocusKnown( false ), m_bFocused( false )
{
}

bool SIRInputUIDispatch::dispatch( const SIRInputService &input, const sirInputUIConsumer_t &consumer ) noexcept
{
	const sirInputFrame_t *pFrame = input.frame();
	const sirInputTextSnapshot_t &text = input.text().snapshot();
	if ( pFrame == nullptr || pFrame->nTick == 0 || pFrame->nTick <= m_nLastTick ||
	     pFrame->nTick != text.nTick || consumer.nActionCount > SIR_INPUT_ACTION_CAPACITY ||
	     ( consumer.nActionCount != 0 && consumer.pActionIds == nullptr ) || consumer.nUser >= SIR_INPUT_USER_CAPACITY ) {
		return false;
	}

	if ( consumer.pOnFocus != nullptr && ( !m_bFocusKnown || m_bFocused != text.bFocused ) ) {
		consumer.pOnFocus( consumer.pContext, text.bFocused );
	}
	m_bFocused = text.bFocused;
	m_bFocusKnown = true;

	if ( consumer.pOnAction != nullptr ) {
		for ( uint32_t i = 0; i < consumer.nActionCount; ++i ) {
			sirInputActionState_t state = {};
			if ( input.queryAction( consumer.pActionIds[i], consumer.nUser, &state ) ) {
				consumer.pOnAction( consumer.pContext, &state );
			}
		}
	}

	if ( consumer.pOnPointer != nullptr ) {
		for ( uint32_t i = 0; i < pFrame->nCount; ++i ) {
			const sirInputEvent_t &event = pFrame->events[i];
			if ( event.eClass == SIR_INPUT_POINTER ) {
				consumer.pOnPointer( consumer.pContext, &event );
			}
		}
	}

	if ( consumer.pOnText != nullptr ) {
		for ( uint32_t i = 0; i < text.nCount; ++i ) {
			consumer.pOnText( consumer.pContext, text.szCodepoints[i] );
		}
	}
	if ( consumer.pOnPreedit != nullptr && ( m_nLastTick == 0 || m_nPreeditRevision != text.nPreeditRevision ) ) {
		consumer.pOnPreedit( consumer.pContext, text.szPreedit, text.nPreeditCount, text.nCaret, text.nSelection );
	}

	m_nPreeditRevision = text.nPreeditRevision;
	m_nLastTick = pFrame->nTick;
	return true;
}

void SIRInputUIDispatch::reset( void ) noexcept
{
	m_nLastTick = 0;
	m_nPreeditRevision = 0;
	m_bFocusKnown = false;
	m_bFocused = false;
}
