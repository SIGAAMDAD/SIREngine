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

#include <Core/Platform/Window/Windows/SIRWGLBootstrapWindow.h>

bool sirCreateWGLBootstrapWindow( sirWGLBootstrapWindow_t *pWindow ) noexcept
{
	if ( pWindow == nullptr || pWindow->hWindow != nullptr || pWindow->hDC != nullptr ) {
		return false;
	}
	const HINSTANCE hInstance = GetModuleHandleA( nullptr );
	WNDCLASSA windowClass = {};
	windowClass.lpfnWndProc = DefWindowProcA;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = "SIREngineGL46Context";
	if ( RegisterClassA( &windowClass ) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS ) {
		return false;
	}
	pWindow->hWindow = CreateWindowExA( 0, windowClass.lpszClassName, "", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, hInstance, nullptr );
	if ( pWindow->hWindow == nullptr ) {
		return false;
	}
	pWindow->hDC = GetDC( pWindow->hWindow );
	if ( pWindow->hDC == nullptr ) {
		return false;
	}
	return true;
}

void sirDestroyWGLBootstrapWindow( sirWGLBootstrapWindow_t *pWindow ) noexcept
{
	if ( pWindow == nullptr ) {
		return;
	}
	if ( pWindow->hDC != nullptr && pWindow->hWindow != nullptr ) {
		ReleaseDC( pWindow->hWindow, pWindow->hDC );
	}
	if ( pWindow->hWindow != nullptr ) {
		DestroyWindow( pWindow->hWindow );
	}
	*pWindow = {};
}
