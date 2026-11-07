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
#ifndef __SIRENGINE_SIRWAYLANDINPUTBRIDGE_H__
#define __SIRENGINE_SIRWAYLANDINPUTBRIDGE_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Input/SIRInputBackend.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/SIRInputCursorMode.h>
#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#include <wayland-client.h>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct zwp_relative_pointer_v1;
struct zwp_locked_pointer_v1;

// Borrows one compositor seat/display and registers one keyboard and pointer
// device, independent of how many windows receive seat focus.
class SIRWaylandInputBridge final : public SIRInputBackend
{
public:
	SIRWaylandInputBridge( void ) noexcept = default;
	~SIRWaylandInputBridge( void ) noexcept override;

	sirInputBackendId_t id( void ) const noexcept override;
	void sample( SIRInputService& service ) noexcept override;
	void resynchronize( SIRInputService& service ) noexcept override;
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override;
	void stop( SIRInputService& service ) noexcept override;

	bool bind( SIRInputService& input, SIRWindowDriverWayland& driver, const sirWindowNativeView_t& view ) noexcept;
	bool registerWindow( const sirWindowNativeView_t& view ) noexcept;
	void unregisterWindow( uintptr_t nNativeSurface ) noexcept;
	bool beginFrame( void ) noexcept;
	bool isBound( void ) const noexcept;
	void setTextEnabled( bool bEnabled ) noexcept;
	bool setCursorMode( sirInputCursorMode_t eMode ) noexcept;
	sirInputCursorMode_t cursorMode( void ) const noexcept;
	void unbind( void ) noexcept;

protected:
private:
	static void seatLost( void *pContext ) noexcept;
	static void pointerProtocolsLost( void *pContext ) noexcept;
	static void seatCapabilities( void *pContext, wl_seat *pSeat, uint32_t nCapabilities ) noexcept;
	static void seatName( void *pContext, wl_seat *pSeat, const char *pszName ) noexcept;
	static void keyboardKeymap( void *pContext, wl_keyboard *pKeyboard, uint32_t nFormat, int32_t nFd, uint32_t nSize ) noexcept;
	static void keyboardEnter( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface, wl_array *pKeys ) noexcept;
	static void keyboardLeave( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface ) noexcept;
	static void keyboardKey( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nTime, uint32_t nKey,
	                         uint32_t nState ) noexcept;
	static void keyboardModifiers( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nDepressed, uint32_t nLatched,
	                               uint32_t nLocked, uint32_t nGroup ) noexcept;
	static void keyboardRepeat( void *pContext, wl_keyboard *pKeyboard, int32_t nRate, int32_t nDelay ) noexcept;
	static void pointerEnter( void *pContext, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface, wl_fixed_t nX,
	                          wl_fixed_t nY ) noexcept;
	static void pointerLeave( void *pContext, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface ) noexcept;
	static void pointerMotion( void *pContext, wl_pointer *pPointer, uint32_t nTime, wl_fixed_t nX, wl_fixed_t nY ) noexcept;
	static void pointerButton( void *pContext, wl_pointer *pPointer, uint32_t nSerial, uint32_t nTime, uint32_t nButton,
	                           uint32_t nState ) noexcept;
	static void pointerAxis( void *pContext, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis, wl_fixed_t nValue ) noexcept;
	static void pointerFrame( void *pContext, wl_pointer *pPointer ) noexcept;
	static void pointerAxisSource( void *pContext, wl_pointer *pPointer, uint32_t nSource ) noexcept;
	static void pointerAxisStop( void *pContext, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis ) noexcept;
	static void pointerAxisDiscrete( void *pContext, wl_pointer *pPointer, uint32_t nAxis, int32_t nDiscrete ) noexcept;

	static void relativeMotion( void *pContext, zwp_relative_pointer_v1 *pRelative, uint32_t nTimeHi, uint32_t nTimeLo,
	                            wl_fixed_t nDx, wl_fixed_t nDy, wl_fixed_t nDxUnaccel, wl_fixed_t nDyUnaccel ) noexcept;
	static void pointerLocked( void *pContext, zwp_locked_pointer_v1 *pLock ) noexcept;
	static void pointerUnlocked( void *pContext, zwp_locked_pointer_v1 *pLock ) noexcept;
	bool beginPointerCapture( void ) noexcept;
	void endPointerCapture( void ) noexcept;
	bool validSurface( wl_surface *pSurface ) const noexcept;
	void ensureDevices( void ) noexcept;
	void clearKeyboard( void ) noexcept;
	void clearPointer( void ) noexcept;
	void clearKeymap( void ) noexcept;
	void releaseSeatObjects( void ) noexcept;

	SIRInputService *m_pInput = nullptr;
	SIRWindowDriverWayland *m_pDriver = nullptr;
	wl_display *m_pDisplay = nullptr;
	wl_seat *m_pSeat = nullptr;
	wl_keyboard *m_pKeyboard = nullptr;
	wl_pointer *m_pPointer = nullptr;
	zwp_relative_pointer_v1 *m_pRelativePointer = nullptr;
	zwp_locked_pointer_v1 *m_pLockedPointer = nullptr;
	wl_surface *m_Surfaces[SIR_WINDOW_MAX_WINDOWS] = {};
	wl_surface *m_pKeyboardFocus = nullptr;
	wl_surface *m_pPointerFocus = nullptr;
	xkb_context *m_pXkbContext = nullptr;
	xkb_keymap *m_pKeymap = nullptr;
	xkb_state *m_pKeyState = nullptr;
	sirInputDeviceId_t m_hKeyboard = {};
	sirInputDeviceId_t m_hPointer = {};
	uint32_t m_nWindowCount = 0;
	int32_t m_nLastX = 0;
	int32_t m_nLastY = 0;
	int32_t m_nRemainderX = 0;
	int32_t m_nRemainderY = 0;
	int32_t m_nWheelRemainder[2] = {};
	int32_t m_nRelativeRemainderX = 0;
	int32_t m_nRelativeRemainderY = 0;
	sirInputCursorMode_t m_eCursorMode = SIR_INPUT_CURSOR_FREE;
	bool m_bPointerLocked = false;
	bool m_bHavePointerPosition = false;
	bool m_bKeyboardDevice = false;
	bool m_bPointerDevice = false;
	bool m_bSeatLost = false;
};
#endif // __SIRENGINE_SIRWAYLANDINPUTBRIDGE_H__
