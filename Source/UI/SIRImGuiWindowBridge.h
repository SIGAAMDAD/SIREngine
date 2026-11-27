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

#ifndef __SIR_IMGUI_WINDOW_BRIDGE_H__
#define __SIR_IMGUI_WINDOW_BRIDGE_H__

#include <Core/Platform/Shared/Window/SIRWindowTypes.h>
#include <Core/Input/SIRInputUIDispatch.h>

struct sirImGuiInputState_t
{
	SIRInputUIDispatch dispatch;
	float nMouseX{};
	float nMouseY{};
};

// Call on the ImGui context owner thread, after window events are pumped and
// before ImGui::NewFrame(). These functions do not own a native window.
bool sirImGuiApplyWindowSnapshot( const sirWindowSnapshot_t *pSnapshot, float nDeltaSeconds ) noexcept;
bool sirImGuiApplyWindowEvent( const sirWindowEvent_t *pEvent ) noexcept;
bool sirImGuiConsumeInput( const SIRInputService& input, sirImGuiInputState_t *pState ) noexcept;

#endif // __SIR_IMGUI_WINDOW_BRIDGE_H__
