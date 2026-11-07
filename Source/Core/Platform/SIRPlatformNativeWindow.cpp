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
#include <Core/Platform/SIRPlatformNativeWindow.h>

bool sirPlatformDecodeWindowsWindow( const sirWindowNativeView_t& view, sirWindowsNativeWindow_t *pWindow ) noexcept
{
	if ( pWindow == nullptr ) {
		return false;
	}
	*pWindow = {};
	if ( view.ePlatform != SIR_WINDOW_PLATFORM_WIN32 || view.nNativeWindow == 0 || view.nDisplayOrInstance == 0 ||
	     view.nWindowGeneration == 0 ) {
		return false;
	}
	*pWindow = { view.nNativeWindow, view.nDisplayOrInstance, view.nWindowGeneration };
	return true;
}

bool sirPlatformDecodeLinuxWindow( const sirWindowNativeView_t& view, sirLinuxNativeWindow_t *pWindow ) noexcept
{
	if ( pWindow == nullptr ) {
		return false;
	}
	*pWindow = {};
	sirLinuxDisplayKind_t eDisplay = SIR_LINUX_DISPLAY_NONE;
	if ( view.ePlatform == SIR_WINDOW_PLATFORM_X11 ) {
		eDisplay = SIR_LINUX_DISPLAY_X11;
	} else if ( view.ePlatform == SIR_WINDOW_PLATFORM_WAYLAND ) {
		eDisplay = SIR_LINUX_DISPLAY_WAYLAND;
	} else {
		return false;
	}
	if ( view.nDisplayOrInstance == 0 || view.nNativeWindow == 0 || view.nWindowGeneration == 0 ) {
		return false;
	}
	*pWindow = { view.nDisplayOrInstance, view.nNativeWindow, view.nWindowGeneration, eDisplay, 0 };
	return true;
}
