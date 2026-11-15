/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRNATIVEWINDOWDRIVERS_H__
#define __SIRENGINE_SIRNATIVEWINDOWDRIVERS_H__

#include <Core/Platform/Window/SIRWindowSystem.h>

#if defined( _WIN32 )
#include <Windows.h>

class SIRWin32WindowDriver final : public SIRWindowDriver
{
public:
	SIRWin32WindowDriver( void ) noexcept;
	bool initialize( SIRWindowSystem& system ) noexcept override;
	void shutdown( void ) noexcept override;
	bool create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept override;
	void destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept override;
	bool pump( void ) noexcept override;
	bool bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept override;
	static intptr_t dispatchNative( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam ) noexcept;
	intptr_t dispatchMessage( uint32_t iSlot, HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam ) noexcept;

private:
	typedef struct sirWin32WindowSlot_s
	{
		SIRWin32WindowDriver *pOwner;
		HWND hNative;
		sirWindowHandle_t hWindow;
	} sirWin32WindowSlot_t;

	SIRWindowSystem *m_pSystem;
	HINSTANCE m_hInstance;
	sirWin32WindowSlot_t m_szSlots[SIR_WINDOW_MAX_WINDOWS];
	SIRInputService *m_pInput;
	SIRInputBackend *m_pDesktop;
	sirWindowHandle_t m_hInputWindow;
	bool m_bOwnClass;
	bool m_bFocusLost;
};
#elif defined( SIR_INPUT_X11_AVAILABLE )
#include <X11/Xlib.h>

class SIRX11WindowDriver final : public SIRWindowDriver
{
public:
	SIRX11WindowDriver( void ) noexcept;
	bool initialize( SIRWindowSystem& system ) noexcept override;
	void shutdown( void ) noexcept override;
	bool create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept override;
	void destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept override;
	bool pump( void ) noexcept override;
	bool bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept override;

private:
	typedef struct sirX11WindowSlot_s
	{
		Window hNative;
		XIC hInputContext;
		sirWindowHandle_t hWindow;
	} sirX11WindowSlot_t;

	SIRWindowSystem *m_pSystem;
	Display *m_pDisplay;
	XIM m_hInputMethod;
	Atom m_hDelete;
	Atom m_hProtocols;
	sirX11WindowSlot_t m_szSlots[SIR_WINDOW_MAX_WINDOWS];
	SIRInputService *m_pInput;
	SIRInputBackend *m_pDesktop;
	sirWindowHandle_t m_hInputWindow;
	bool m_bInputAttached;

	void processEvent( XEvent& event ) noexcept;
	void processInput( sirX11WindowSlot_t& slot, XEvent& event ) noexcept;
};
#endif

#endif // __SIRENGINE_SIRNATIVEWINDOWDRIVERS_H__
