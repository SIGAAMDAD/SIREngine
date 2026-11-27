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

#include <UI/SIRImGuiWindowBridge.h>

#include <Core/Input/SIRInputService.h>
#include <imgui.h>

bool sirImGuiApplyWindowSnapshot( const sirWindowSnapshot_t *pSnapshot, float nDeltaSeconds ) noexcept
{
    if ( pSnapshot == nullptr || ImGui::GetCurrentContext() == nullptr || nDeltaSeconds <= 0.0f ||
         !pSnapshot->bReady ) return false;
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = nDeltaSeconds;
    io.DisplaySize = ImVec2( static_cast<float>( pSnapshot->nWidth ), static_cast<float>( pSnapshot->nHeight ) );
    io.AddFocusEvent( pSnapshot->bFocused );
    return true;
}

bool sirImGuiApplyWindowEvent( const sirWindowEvent_t *pEvent ) noexcept
{
    if ( pEvent == nullptr || ImGui::GetCurrentContext() == nullptr ) return false;
    ImGuiIO& io = ImGui::GetIO();
    switch ( pEvent->eKind ) {
    case sirWindowEventKind::SIR_WINDOW_EVENT_FOCUS_GAINED:
        io.AddFocusEvent( true );
        return true;
    case sirWindowEventKind::SIR_WINDOW_EVENT_FOCUS_LOST:
        io.AddFocusEvent( false );
        return true;
    case sirWindowEventKind::SIR_WINDOW_EVENT_RESIZED:
        io.DisplaySize = ImVec2( static_cast<float>( pEvent->nWidth ), static_cast<float>( pEvent->nHeight ) );
        return true;
    default: return false;
    }
}

static void sirImGuiInputText( void *, uint32_t nCodepoint )
{
	if ( ImGui::GetCurrentContext() != nullptr ) ImGui::GetIO().AddInputCharacter( nCodepoint );
}

static void sirImGuiInputFocus( void *, bool bFocused )
{
	if ( ImGui::GetCurrentContext() != nullptr ) ImGui::GetIO().AddFocusEvent( bFocused );
}

static void sirImGuiInputPointer( void *pContext, const sirInputEvent_t *pEvent )
{
	if ( pContext == nullptr || pEvent == nullptr || ImGui::GetCurrentContext() == nullptr ) return;
	sirImGuiInputState_t *pState = static_cast<sirImGuiInputState_t *>( pContext );
	ImGuiIO& io = ImGui::GetIO();
	switch ( pEvent->eSignal ) {
	case SIRInputSignal::SIR_INPUT_MOTION:
		pState->nMouseX += static_cast<float>( pEvent->nValue );
		pState->nMouseY += static_cast<float>( pEvent->nValueY );
		io.AddMousePosEvent( pState->nMouseX, pState->nMouseY );
		break;
	case SIRInputSignal::SIR_INPUT_BUTTON:
		if ( pEvent->nControl < 5 ) io.AddMouseButtonEvent( static_cast<int>( pEvent->nControl ), pEvent->nValue != 0 );
		break;
	case SIRInputSignal::SIR_INPUT_WHEEL:
		if ( pEvent->nControl == 0 ) io.AddMouseWheelEvent( 0.0f, static_cast<float>( pEvent->nValue ) / 120.0f );
		else if ( pEvent->nControl == 1 ) io.AddMouseWheelEvent( static_cast<float>( pEvent->nValue ) / 120.0f, 0.0f );
		break;
	default: break;
	}
}

bool sirImGuiConsumeInput( const SIRInputService& input, sirImGuiInputState_t *pState ) noexcept
{
	if ( pState == nullptr || ImGui::GetCurrentContext() == nullptr ) return false;
	const sirInputUIConsumer_t consumer{ pState, nullptr, 0, SIR_INPUT_NO_USER, nullptr,
	                                     sirImGuiInputText, nullptr, sirImGuiInputFocus, sirImGuiInputPointer };
	return pState->dispatch.dispatch( input, consumer );
}
