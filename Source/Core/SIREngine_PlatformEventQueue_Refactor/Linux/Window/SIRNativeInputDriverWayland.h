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

#ifndef __SIR_NATIVE_INPUT_DRIVER_WAYLAND_H__
#define __SIR_NATIVE_INPUT_DRIVER_WAYLAND_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRNativeInputDriver.h>
#include <Core/Containers/SIRArray.h>
#include "SIRNativeInputEvdevGamepadDriver.h"

struct wl_display;
struct wl_registry;
struct wl_seat;
struct wl_surface;
struct wl_keyboard;
struct wl_pointer;
struct wl_touch;
struct wl_array;
struct wl_registry_listener;
struct wl_seat_listener;
struct wl_keyboard_listener;
struct wl_pointer_listener;
struct wl_touch_listener;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

class SIRNativeInputDriverWayland final : public SIRNativeInputDriver
{
public:
	explicit SIRNativeInputDriverWayland( SIRSPSCEventRing<SIREventData>& eventQueue,
		const SIRInputControllerProfiles *pProfiles = nullptr ) noexcept;
	~SIRNativeInputDriverWayland() noexcept;
	SIRNativeInputDriverWayland( const SIRNativeInputDriverWayland& ) = delete;
	SIRNativeInputDriverWayland& operator=( const SIRNativeInputDriverWayland& ) = delete;

	// Borrow the window driver's display/surface. The window driver owns dispatch.
	// Attach during setup, before the display event loop starts; detach before
	// either borrowed native object is destroyed.
	bool attachWindow( wl_display *pDisplay, wl_surface *pSurface, uint32_t nWindowId ) noexcept;
	void detachWindow( bool bConnectionLost = false ) noexcept;
	void setSurfaceSize( uint32_t nWidth, uint32_t nHeight ) noexcept;

	SIRNativeInputBackend getBackend( void ) const noexcept override;
	void pumpEvents( void ) noexcept override;
	uint32_t droppedEventCount( void ) const noexcept;
	bool isKeyPressed( SIRKeycode eKey ) const noexcept override;
	bool isMouseButtonPressed( SIRMouseButton eButton ) const noexcept override;

private:
	static void registryGlobal( void *pData, wl_registry *pRegistry, uint32_t nName, const char *szInterface, uint32_t nVersion ) noexcept;
	static void registryRemove( void *pData, wl_registry *pRegistry, uint32_t nName ) noexcept;
	static void seatCapabilities( void *pData, wl_seat *pSeat, uint32_t nCapabilities ) noexcept;
	static void seatName( void *pData, wl_seat *pSeat, const char *szName ) noexcept;
	static void keyboardKeymap( void *pData, wl_keyboard *pKeyboard, uint32_t nFormat, int32_t nFd, uint32_t nSize ) noexcept;
	static void keyboardEnter( void *pData, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface, wl_array *pKeys ) noexcept;
	static void keyboardLeave( void *pData, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface ) noexcept;
	static void keyboardKey( void *pData, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nTime, uint32_t nKey, uint32_t nState ) noexcept;
	static void keyboardModifiers( void *pData, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nDepressed, uint32_t nLatched, uint32_t nLocked, uint32_t nGroup ) noexcept;
	static void keyboardRepeatInfo( void *pData, wl_keyboard *pKeyboard, int32_t nRate, int32_t nDelay ) noexcept;
	static void pointerEnter( void *pData, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface, int32_t nX, int32_t nY ) noexcept;
	static void pointerLeave( void *pData, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface ) noexcept;
	static void pointerMotion( void *pData, wl_pointer *pPointer, uint32_t nTime, int32_t nX, int32_t nY ) noexcept;
	static void pointerButton( void *pData, wl_pointer *pPointer, uint32_t nSerial, uint32_t nTime, uint32_t nButton, uint32_t nState ) noexcept;
	static void pointerAxis( void *pData, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis, int32_t nValue ) noexcept;
	static void pointerFrame( void *pData, wl_pointer *pPointer ) noexcept;
	static void pointerAxisSource( void *pData, wl_pointer *pPointer, uint32_t nSource ) noexcept;
	static void pointerAxisStop( void *pData, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis ) noexcept;
	static void pointerAxisDiscrete( void *pData, wl_pointer *pPointer, uint32_t nAxis, int32_t nDiscrete ) noexcept;
	static void touchDown( void *pData, wl_touch *pTouch, uint32_t nSerial, uint32_t nTime, wl_surface *pSurface, int32_t nId, int32_t nX, int32_t nY ) noexcept;
	static void touchUp( void *pData, wl_touch *pTouch, uint32_t nSerial, uint32_t nTime, int32_t nId ) noexcept;
	static void touchMotion( void *pData, wl_touch *pTouch, uint32_t nTime, int32_t nId, int32_t nX, int32_t nY ) noexcept;
	static void touchFrame( void *pData, wl_touch *pTouch ) noexcept;
	static void touchCancel( void *pData, wl_touch *pTouch ) noexcept;

	static const wl_registry_listener s_RegistryListener;
	static const wl_seat_listener s_SeatListener;
	static const wl_keyboard_listener s_KeyboardListener;
	static const wl_pointer_listener s_PointerListener;
	static const wl_touch_listener s_TouchListener;

	struct Contact
	{
		int32_t nId = -1;
		SIRTouchFingerEvent eventData{};
	};

	void createSeatDevices( uint32_t nCapabilities ) noexcept;
	void destroySeatDevices( bool bConnectionLost ) noexcept;
	void destroySeat( bool bConnectionLost ) noexcept;
	void clearKeymap( void ) noexcept;
	void updateModifiers( void ) noexcept;
	void keyEvent( uint32_t nKey, bool bDown, bool bRepeat = false ) noexcept;
	void buttonEvent( SIRMouseButton eButton, bool bDown ) noexcept;
	void releaseKeyboard( void ) noexcept;
	void releasePointer( void ) noexcept;
	void cancelContacts( void ) noexcept;
	void emitContact( Contact& contact, SIREventType eType, float fX, float fY ) noexcept;
	void publish( SIREventData& eventData ) noexcept;
	void repeatKeys( void ) noexcept;
	Contact *findContact( int32_t nId ) noexcept;

	SIRArray<SIRKeyboardEvent, 768> m_szKeys{};
	SIRArray<Contact, 16> m_szContacts{};

	uint32_t m_nDroppedEvents = 0;
	SIRNativeInputEvdevGamepadDriver m_Gamepads;

	wl_display *m_pDisplay = nullptr;
	wl_surface *m_pSurface = nullptr;
	wl_registry *m_pRegistry = nullptr;
	wl_seat *m_pSeat = nullptr;
	wl_keyboard *m_pKeyboard = nullptr;
	wl_pointer *m_pPointer = nullptr;
	wl_touch *m_pTouch = nullptr;
	xkb_context *m_pXKBContext = nullptr;
	xkb_keymap *m_pXKBKeymap = nullptr;
	xkb_state *m_pXKBState = nullptr;

	uint32_t m_nWindowId = 0;
	uint32_t m_nSeatGlobal = UINT32_MAX;
	uint32_t m_nWidth = 0;
	uint32_t m_nHeight = 0;
	uint32_t m_nButtons = 0;
	uint32_t m_nRepeatKey = UINT32_MAX;
	uint64_t m_nNextRepeat = 0;
	uint64_t m_nRepeatInterval = 0;
	int32_t m_nRepeatRate = 0;
	int32_t m_nRepeatDelay = 0;
	uint16_t m_nModifiers = 0;

	float m_fX = 0.f;
	float m_fY = 0.f;
	bool m_bKeyboardFocus = false;
	bool m_bPointerFocus = false;
	bool m_bHavePointer = false;
};

#endif // __SIR_NATIVE_INPUT_DRIVER_WAYLAND_H__
