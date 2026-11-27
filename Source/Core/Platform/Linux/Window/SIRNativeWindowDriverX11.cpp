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

#include "SIRNativeWindowDriverX11.h"

#include <Core/Platform/Shared/Threading/SIRScopedLock.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <cerrno>
#include <climits>
#include <cstring>
#include <poll.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <time.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
#include <X11/extensions/Xrandr.h>
#endif

static constexpr uint32_t MAX_OPEN_ATTEMPTS = 10;
static SIRMutex s_X11IOHandlerLock;
static uint32_t s_nX11HandlerUsers = 0;
static XIOErrorHandler s_pfnPreviousX11IOHandler = nullptr;
static thread_local sigjmp_buf *s_pX11RecoveryPoint = nullptr;
static thread_local Display *s_pX11RecoveryDisplay = nullptr;

static int ioErrorRecovery( Display *pDisplay )
{
	if ( pDisplay == s_pX11RecoveryDisplay && s_pX11RecoveryPoint != nullptr ) {
		siglongjmp( *s_pX11RecoveryPoint, 1 );
	}
	return s_pfnPreviousX11IOHandler != nullptr ? s_pfnPreviousX11IOHandler( pDisplay ) : 0;
}

static void clearRecovery( void ) noexcept
{
	s_pX11RecoveryPoint = nullptr;
	s_pX11RecoveryDisplay = nullptr;
}

static uint64_t getEventTimestamp( void ) noexcept
{
	timespec time{};
	if ( clock_gettime( CLOCK_MONOTONIC, &time ) != 0 ) {
		return 0;
	}
	return static_cast<uint64_t>( time.tv_sec ) * UINT64_C( 1000000000 ) + static_cast<uint64_t>( time.tv_nsec );
}

static SIRWindowResult setX11Title( Display *pDisplay, Window hWindow, const char *pszTitle ) noexcept
{
	if ( pDisplay == nullptr || hWindow == 0 || pszTitle == nullptr ) {
		return SIRWindowResult::InvalidArgument;
	}
	const size_t nLength = strnlen( pszTitle, MAX_NATIVE_WINDOW_NAME_LENGTH + 1U );
	if ( nLength > MAX_NATIVE_WINDOW_NAME_LENGTH ) {
		return SIRWindowResult::InvalidArgument;
	}
	if ( XStoreName( pDisplay, hWindow, pszTitle ) == 0 ) {
		return SIRWindowResult::NativeFailure;
	}
	const Atom hName = XInternAtom( pDisplay, "_NET_WM_NAME", False );
	const Atom hUtf8 = XInternAtom( pDisplay, "UTF8_STRING", False );
	if ( hName != None && hUtf8 != None ) {
		XChangeProperty( pDisplay, hWindow, hName, hUtf8, 8, PropModeReplace,
		                 reinterpret_cast<const unsigned char *>( pszTitle ), static_cast<int>( nLength ) );
	}
	return SIRWindowResult::Ok;
}

// X11 KeySyms describe the logical key; the native keycode is retained for
// physical-layout translation by the input system.
static SIRKeycode mapX11Keycode( KeySym nSymbol ) noexcept
{
	if ( nSymbol >= XK_A && nSymbol <= XK_Z ) {
		return static_cast<SIRKeycode>( static_cast<uint32_t>( nSymbol - XK_A ) + static_cast<uint32_t>( SIRKeycode::A ) );
	}
	if ( nSymbol >= XK_space && nSymbol <= XK_asciitilde ) {
		return static_cast<SIRKeycode>( nSymbol );
	}
	if ( nSymbol >= XK_F1 && nSymbol <= XK_F12 ) {
		return static_cast<SIRKeycode>( static_cast<uint32_t>( SIRKeycode::F1 ) + static_cast<uint32_t>( nSymbol - XK_F1 ) );
	}
	if ( nSymbol >= XK_F13 && nSymbol <= XK_F24 ) {
		return static_cast<SIRKeycode>( static_cast<uint32_t>( SIRKeycode::F13 ) + static_cast<uint32_t>( nSymbol - XK_F13 ) );
	}
	switch ( nSymbol ) {
	case XK_BackSpace: return SIRKeycode::Backspace;
	case XK_Tab: return SIRKeycode::Tab;
	case XK_Return: return SIRKeycode::Return;
	case XK_Escape: return SIRKeycode::Escape;
	case XK_Delete: return SIRKeycode::Delete;
	case XK_Left: return SIRKeycode::Left;
	case XK_Right: return SIRKeycode::Right;
	case XK_Up: return SIRKeycode::Up;
	case XK_Down: return SIRKeycode::Down;
	case XK_Home: return SIRKeycode::Home;
	case XK_End: return SIRKeycode::End;
	case XK_Prior: return SIRKeycode::PageUp;
	case XK_Next: return SIRKeycode::PageDown;
	case XK_Insert: return SIRKeycode::Insert;
	case XK_Caps_Lock: return SIRKeycode::CapsLock;
	case XK_Num_Lock: return SIRKeycode::NumLockClear;
	case XK_Scroll_Lock: return SIRKeycode::ScrollLock;
	case XK_Pause: return SIRKeycode::Pause;
	case XK_Print: return SIRKeycode::PrintScreen;
	case XK_KP_Enter: return SIRKeycode::KeypadEnter;
	case XK_Shift_L: return SIRKeycode::LeftShift;
	case XK_Shift_R: return SIRKeycode::RightShift;
	case XK_Control_L: return SIRKeycode::LeftCtrl;
	case XK_Control_R: return SIRKeycode::RightCtrl;
	case XK_Alt_L: return SIRKeycode::LeftAlt;
	case XK_Alt_R: return SIRKeycode::RightAlt;
	case XK_Super_L: return SIRKeycode::LeftGUI;
	case XK_Super_R: return SIRKeycode::RightGUI;
	default: return SIRKeycode::Unknown;
	}
}

static SIRMouseButton mapX11MouseButton( uint32_t nButton ) noexcept
{
	switch ( nButton ) {
	case Button1: return SIRMouseButton::Left;
	case Button2: return SIRMouseButton::Middle;
	case Button3: return SIRMouseButton::Right;
	case 8: return SIRMouseButton::X1;
	case 9: return SIRMouseButton::X2;
	default: return SIRMouseButton::Unknown;
	}
}

SIRNativeWindowDriverX11::SIRNativeWindowDriverX11( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept
	: SIRNativeWindowDriver( eventQueue )
{
}

SIRNativeWindowDriverX11::~SIRNativeWindowDriverX11() noexcept
{
	shutdown();
}

bool SIRNativeWindowDriverX11::isServerLost( void ) const noexcept
{
	return m_bServerLost;
}

SIRWindowResult SIRNativeWindowDriverX11::flushPendingEvents( void ) noexcept
{
	for ( uint32_t iEvent = 0; iEvent < m_nPendingCount; ++iEvent ) {
		const SIREventEnqueueStatus eResult = m_EventQueue.tryEnqueue( m_PendingEvents[iEvent] );
		if ( eResult == SIREventEnqueueStatus::SIR_EVENT_ENQUEUE_STOPPED ) {
			return SIRWindowResult::Stopped;
		}
		if ( eResult != SIREventEnqueueStatus::SIR_EVENT_ENQUEUE_OK ) {
			if ( iEvent != 0 ) {
				m_PendingEvents[0] = m_PendingEvents[1];
				--m_nPendingCount;
			}
			return SIRWindowResult::Busy;
		}
	}
	m_nPendingCount = 0;
	return SIRWindowResult::Ok;
}

SIRWindowResult SIRNativeWindowDriverX11::queueWindowEvent( SIREventType eType, uint32_t nWindowId,
                                                          int32_t nData1, int32_t nData2 ) noexcept
{
	SIREventData event{};
	event.type = eType;
	event.timestamp = getEventTimestamp();
	event.window.windowId = nWindowId;
	event.window.data1 = nData1;
	event.window.data2 = nData2;

	return queueEvent( event );
}

SIRWindowResult SIRNativeWindowDriverX11::queueEvent( const SIREventData& event ) noexcept
{
	if ( m_nPendingCount != 0 ) {
		if ( m_nPendingCount == 2 ) {
			return SIRWindowResult::Busy;
		}
		m_PendingEvents[m_nPendingCount++] = event;
		return SIRWindowResult::Busy;
	}

	const SIREventEnqueueStatus eResult = m_EventQueue.tryEnqueue( event );
	if ( eResult == SIREventEnqueueStatus::SIR_EVENT_ENQUEUE_STOPPED ) {
		return SIRWindowResult::Stopped;
	}
	if ( eResult != SIREventEnqueueStatus::SIR_EVENT_ENQUEUE_OK ) {
		m_PendingEvents[m_nPendingCount++] = event;
		return SIRWindowResult::Busy;
	}

	return SIRWindowResult::Ok;
}

SIRWindowResult SIRNativeWindowDriverX11::handleKeyboardEvent( const XKeyEvent& event, bool bDown ) noexcept
{
	SIREventData output{};
	output.type = bDown ? SIREventType::KeyDown : SIREventType::KeyUp;
	output.timestamp = getEventTimestamp();
	output.key.windowId = static_cast<uint32_t>( event.window );
	output.key.deviceId = 0; // Core X11 does not expose a per-device ID.

	XKeyEvent mutableEvent = event;
	output.key.keycode = mapX11Keycode( XLookupKeysym( &mutableEvent, 0 ) );
	output.key.scancode = SIRScancode::Unknown;
	output.key.nativeScancode = static_cast<uint16_t>( event.keycode );
	output.key.down = bDown;
	if ( ( event.state & ShiftMask ) != 0 ) {
		output.key.modifiers = output.key.modifiers | SIRKeyModifier::Shift;
	}
	if ( ( event.state & ControlMask ) != 0 ) {
		output.key.modifiers = output.key.modifiers | SIRKeyModifier::Ctrl;
	}
	if ( ( event.state & Mod1Mask ) != 0 ) {
		output.key.modifiers = output.key.modifiers | SIRKeyModifier::Alt;
	}
	if ( ( event.state & Mod4Mask ) != 0 ) {
		output.key.modifiers = output.key.modifiers | SIRKeyModifier::GUI;
	}
	if ( ( event.state & LockMask ) != 0 ) {
		output.key.modifiers = output.key.modifiers | SIRKeyModifier::CapsLock;
	}
	if ( event.keycode < 256 ) {
		output.key.repeat = bDown && m_bKeysDown[event.keycode];
		m_bKeysDown[event.keycode] = bDown;
	}

	return queueEvent( output );
}

SIRWindowResult SIRNativeWindowDriverX11::handleMouseEvent( const XEvent& event ) noexcept
{
	SIREventData output{};
	output.timestamp = getEventTimestamp();
	if ( event.type == MotionNotify ) {
		output.type = SIREventType::MouseMotion;
		output.motion.windowId = static_cast<uint32_t>( event.xmotion.window );
		output.motion.deviceId = 0;
		output.motion.buttonState = m_nMouseButtonState;
		output.motion.x = static_cast<float>( event.xmotion.x );
		output.motion.y = static_cast<float>( event.xmotion.y );
		if ( m_bMousePositionValid ) {
			output.motion.deltaX = output.motion.x - m_fMouseX;
			output.motion.deltaY = output.motion.y - m_fMouseY;
		}
		m_fMouseX = output.motion.x;
		m_fMouseY = output.motion.y;
		m_bMousePositionValid = true;
		return queueEvent( output );
	}

	const bool bDown = event.type == ButtonPress;
	const uint32_t nButton = event.xbutton.button;
	if ( nButton >= 4 && nButton <= 7 ) {
		if ( !bDown ) {
			return SIRWindowResult::Ok;
		}
		output.type = SIREventType::MouseWheel;
		output.wheel.windowId = static_cast<uint32_t>( event.xbutton.window );
		output.wheel.deviceId = 0;
		output.wheel.direction = SIRMouseWheelDirection::Normal;
		output.wheel.x = nButton == 6 ? -1.0F : ( nButton == 7 ? 1.0F : 0.0F );
		output.wheel.y = nButton == 4 ? 1.0F : ( nButton == 5 ? -1.0F : 0.0F );
		output.wheel.integerX = static_cast<int32_t>( output.wheel.x );
		output.wheel.integerY = static_cast<int32_t>( output.wheel.y );
		output.wheel.mouseX = static_cast<float>( event.xbutton.x );
		output.wheel.mouseY = static_cast<float>( event.xbutton.y );
		return queueEvent( output );
	}

	const SIRMouseButton eButton = mapX11MouseButton( nButton );
	if ( eButton == SIRMouseButton::Unknown ) {
		return SIRWindowResult::Ok;
	}

	output.type = bDown ? SIREventType::MouseButtonDown : SIREventType::MouseButtonUp;
	output.button.windowId = static_cast<uint32_t>( event.xbutton.window );
	output.button.deviceId = 0;
	output.button.button = eButton;
	output.button.down = bDown;
	output.button.clicks = 1;
	output.button.x = static_cast<float>( event.xbutton.x );
	output.button.y = static_cast<float>( event.xbutton.y );
	if ( bDown ) {
		m_nMouseButtonState |= SIRMouseButtonMask( eButton );
	} else {
		m_nMouseButtonState &= ~SIRMouseButtonMask( eButton );
	}

	return queueEvent( output );
}

void SIRNativeWindowDriverX11::markServerLost( void ) noexcept
{
	if ( m_bServerLost ) {
		return;
	}

	m_bServerLost = true;
	if ( m_hWindow != 0 ) {
		m_nLostNative = static_cast<uintptr_t>( m_hWindow );
		m_nLossPendingMask |= 1U;
		m_hWindow = 0;
		m_bWindowLost = true;
	}
	m_nDisplayChangePendingMask = 0;
}

bool SIRNativeWindowDriverX11::transportAlive( void ) noexcept
{
	if ( m_bServerLost || m_pDisplay == nullptr ) {
		return false;
	}

	const int nSocket = ConnectionNumber( m_pDisplay );
	if ( nSocket < 0 ) {
		markServerLost();
		return false;
	}

	pollfd descriptor{};
	descriptor.fd = nSocket;
	descriptor.events = POLLIN;

	const int nPoll = poll( &descriptor, 1, 0 );
	const bool bPollFailed = nPoll < 0 && errno != EINTR;
	const bool bDisconnected = nPoll > 0 && ( descriptor.revents & ( POLLHUP | POLLERR | POLLNVAL ) ) != 0;
	if ( bPollFailed || bDisconnected ) {
		markServerLost();
		return false;
	}
	if ( nPoll > 0 && ( descriptor.revents & POLLIN ) != 0 ) {
		char byte = 0;
		const ssize_t nPeek = recv( nSocket, &byte, 1, MSG_PEEK | MSG_DONTWAIT );
		const bool bUnexpectedError = errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR;
		if ( nPeek == 0 || ( nPeek < 0 && bUnexpectedError ) ) {
			markServerLost();
			return false;
		}
	}

	return true;
}

SIRWindowResult SIRNativeWindowDriverX11::initialize( void ) noexcept
{
	if ( m_pDisplay != nullptr ) {
		return SIRWindowResult::Busy;
	}
	Display *pDisplay = nullptr;
	for ( uint32_t iAttempt = 0; iAttempt < MAX_OPEN_ATTEMPTS; ++iAttempt ) {
		pDisplay = XOpenDisplay( nullptr );
		if ( pDisplay != nullptr || errno != ECONNRESET ) {
			break;
		}
		if ( iAttempt + 1 < MAX_OPEN_ATTEMPTS ) {
			static_cast<void>( poll( nullptr, 0, 5 ) );
		}
	}
	if ( pDisplay == nullptr ) {
		return SIRWindowResult::NotReady;
	}

	m_pDisplay = pDisplay;
	m_bServerLost = false;
	m_bWindowLost = false;
	{
		SIRScopedLock<SIRMutex> lock( s_X11IOHandlerLock );
		if ( s_nX11HandlerUsers == 0 ) {
			s_pfnPreviousX11IOHandler = XSetIOErrorHandler( ioErrorRecovery );
		}
		++s_nX11HandlerUsers;
		m_bHandlerRegistered = true;
	}

	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		clearRecovery();
		markServerLost();
		return SIRWindowResult::NativeFailure;
	}
	s_pX11RecoveryDisplay = pDisplay;
	s_pX11RecoveryPoint = &recovery;

	m_nDeleteWindow = XInternAtom( pDisplay, "WM_DELETE_WINDOW", False );
	m_nProtocols = XInternAtom( pDisplay, "WM_PROTOCOLS", False );
	if ( m_nDeleteWindow == None || m_nProtocols == None ) {
		clearRecovery();
		shutdown();
		return SIRWindowResult::NativeFailure;
	}
	Bool bDetectableRepeat = False;
	XkbSetDetectableAutoRepeat( pDisplay, True, &bDetectableRepeat );

	m_bDetectableRepeat = bDetectableRepeat == True;
	m_nRootWindow = DefaultRootWindow( pDisplay );

	XWindowAttributes attributes{};
	if ( !XGetWindowAttributes( pDisplay, m_nRootWindow, &attributes ) ) {
		clearRecovery();
		shutdown();
		return SIRWindowResult::NativeFailure;
	}

	m_nServerWidth = static_cast<uint32_t>( attributes.width );
	m_nServerHeight = static_cast<uint32_t>( attributes.height );
	XSelectInput( pDisplay, m_nRootWindow, StructureNotifyMask );
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
	int nRandrEvent = 0;
	int nRandrError = 0;
	if ( XRRQueryExtension( pDisplay, &nRandrEvent, &nRandrError ) ) {
		int nMajor = 1;
		int nMinor = 0;
		if ( XRRQueryVersion( pDisplay, &nMajor, &nMinor ) ) {
			const int nMask = RRScreenChangeNotifyMask |
                                 ( nMajor > 1 || ( nMajor == 1 && nMinor >= 2 )
                                       ? RROutputChangeNotifyMask | RRCrtcChangeNotifyMask : 0 );
			XRRSelectInput( pDisplay, m_nRootWindow, nMask );
			m_nRandrEventBase = nRandrEvent;
		}
	}
#endif
	clearRecovery();
	return SIRWindowResult::Ok;
}

SIRWindowResult SIRNativeWindowDriverX11::createWindow( const SIRNativeWindowConfiguration& config ) noexcept
{
	if ( m_bServerLost ) {
		return SIRWindowResult::Stopped;
	}
	if ( !transportAlive() ) {
		return SIRWindowResult::NativeFailure;
	}

	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		clearRecovery();
		markServerLost();
		return SIRWindowResult::NativeFailure;
	}

	s_pX11RecoveryDisplay = m_pDisplay;
	s_pX11RecoveryPoint = &recovery;

	const SIRWindowResult eResult = createWindowNative( config );
	clearRecovery();

	return eResult;
}

SIRWindowResult SIRNativeWindowDriverX11::createWindowNative( const SIRNativeWindowConfiguration& config ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return SIRWindowResult::Stopped;
	}
	if ( m_hWindow != 0 ) {
		return SIRWindowResult::Busy;
	}
	if ( config.nWidth == 0 || config.nHeight == 0 || config.nWidth > static_cast<uint32_t>( INT_MAX ) ) {
		return SIRWindowResult::InvalidArgument;
	}
	if ( config.nHeight > static_cast<uint32_t>( INT_MAX ) || config.nPositionX > static_cast<uint32_t>( INT_MAX ) ) {
		return SIRWindowResult::InvalidArgument;
	}
	if ( config.nPositionY > static_cast<uint32_t>( INT_MAX ) ) {
		return SIRWindowResult::InvalidArgument;
	}

	const int nScreen = DefaultScreen( m_pDisplay );
	const Window hWindow = XCreateSimpleWindow( m_pDisplay, RootWindow( m_pDisplay, nScreen ), static_cast<int>( config.nPositionX ),
	                                              static_cast<int>( config.nPositionY ), config.nWidth, config.nHeight, 0, 0, 0 );
	if ( hWindow == 0 ) {
		return SIRWindowResult::NativeFailure;
	}
	constexpr long X11_EVENT_MASK = StructureNotifyMask | FocusChangeMask | EnterWindowMask | LeaveWindowMask | ExposureMask |
                                KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	XSelectInput( m_pDisplay, hWindow, X11_EVENT_MASK );
	Atom hDelete = m_nDeleteWindow;
	if ( !XSetWMProtocols( m_pDisplay, hWindow, &hDelete, 1 ) ||
         setX11Title( m_pDisplay, hWindow, config.szName.cStr() ) != SIRWindowResult::Ok ) {
		static_cast<void>( XDestroyWindow( m_pDisplay, hWindow ) );
		return SIRWindowResult::NativeFailure;
	}
	XMapWindow( m_pDisplay, hWindow );
	XFlush( m_pDisplay );

	m_hWindow = hWindow;
	m_bWindowLost = false;
	m_nDisplayChangePendingMask = 0;
	m_nMouseButtonState = 0;
	m_bMousePositionValid = false;
	memset( m_bKeysDown, 0, sizeof( m_bKeysDown ) );
	m_nWindowX = static_cast<int32_t>( config.nPositionX );
	m_nWindowY = static_cast<int32_t>( config.nPositionY );
	m_nWindowWidth = config.nWidth;
	m_nWindowHeight = config.nHeight;
	if ( ++m_nWindowGeneration == 0 ) {
		++m_nWindowGeneration;
	}

	return SIRWindowResult::Ok;
}

SIRWindowResult SIRNativeWindowDriverX11::setTitle( const char *pszWindowTitle ) noexcept
{
	if ( m_bServerLost || m_hWindow == 0 ) {
		return SIRWindowResult::Stopped;
	}

	if ( !transportAlive() ) {
		return SIRWindowResult::NativeFailure;
	}

	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		clearRecovery();
		markServerLost();
		return SIRWindowResult::NativeFailure;
	}

	s_pX11RecoveryDisplay = m_pDisplay;
	s_pX11RecoveryPoint = &recovery;

	const SIRWindowResult eResult = setX11Title( m_pDisplay, m_hWindow, pszWindowTitle );
	if ( eResult == SIRWindowResult::Ok ) {
		XFlush( m_pDisplay );
	}
	clearRecovery();

	return eResult;
}

SIRWindowResult SIRNativeWindowDriverX11::setPosition( uint32_t nPositionX, uint32_t nPositionY ) noexcept
{
	if ( nPositionX > static_cast<uint32_t>( INT_MAX ) || nPositionY > static_cast<uint32_t>( INT_MAX ) ) {
		return SIRWindowResult::InvalidArgument;
	}
	if ( m_bServerLost || m_hWindow == 0 ) {
		return SIRWindowResult::Stopped;
	}

	if ( !transportAlive() ) {
		return SIRWindowResult::NativeFailure;
	}

	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		clearRecovery();
		markServerLost();
		return SIRWindowResult::NativeFailure;
	}

	s_pX11RecoveryDisplay = m_pDisplay;
	s_pX11RecoveryPoint = &recovery;

	XMoveWindow( m_pDisplay, m_hWindow, static_cast<int>( nPositionX ), static_cast<int>( nPositionY ) );
	XFlush( m_pDisplay );
	clearRecovery();

	return SIRWindowResult::Ok;
}

SIRWindowResult SIRNativeWindowDriverX11::setSize( uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( nWidth == 0 || nHeight == 0 || nWidth > static_cast<uint32_t>( INT_MAX ) ) {
		return SIRWindowResult::InvalidArgument;
	}
	if ( nHeight > static_cast<uint32_t>( INT_MAX ) ) {
		return SIRWindowResult::InvalidArgument;
	}
	if ( m_bServerLost || m_hWindow == 0 ) {
		return SIRWindowResult::Stopped;
	}

	if ( !transportAlive() ) {
		return SIRWindowResult::NativeFailure;
	}

	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		clearRecovery();
		markServerLost();
		return SIRWindowResult::NativeFailure;
	}

	s_pX11RecoveryDisplay = m_pDisplay;
	s_pX11RecoveryPoint = &recovery;

	XResizeWindow( m_pDisplay, m_hWindow, nWidth, nHeight );
	XFlush( m_pDisplay );
	clearRecovery();

	return SIRWindowResult::Ok;
}

SIRWindowResult SIRNativeWindowDriverX11::destroyWindow( void ) noexcept
{
	const SIRWindowResult eQueueStatus = flushPendingEvents();
	if ( eQueueStatus != SIRWindowResult::Ok ) {
		return eQueueStatus;
	}
	if ( m_bServerLost ) {
		if ( !m_bWindowLost ) {
			return SIRWindowResult::InvalidHandle;
		}
		m_hWindow = 0;
		m_bWindowLost = false;
		m_nLostNative = 0;
		m_nLossPendingMask = 0;
		m_nDisplayChangePendingMask = 0;
		return SIRWindowResult::Ok;
	}
	if ( m_hWindow == 0 && m_bWindowLost ) {
		m_bWindowLost = false;
		m_nLostNative = 0;
		m_nLossPendingMask = 0;
		m_nDisplayChangePendingMask = 0;
		return SIRWindowResult::Ok;
	}
	if ( m_hWindow == 0 ) {
		return SIRWindowResult::InvalidHandle;
	}
	if ( !transportAlive() ) {
		return SIRWindowResult::NativeFailure;
	}
	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		clearRecovery();
		markServerLost();
		return SIRWindowResult::NativeFailure;
	}
	s_pX11RecoveryDisplay = m_pDisplay;
	s_pX11RecoveryPoint = &recovery;

	const uint32_t nWindowId = static_cast<uint32_t>( m_hWindow );
	XDestroyWindow( m_pDisplay, m_hWindow );
	XFlush( m_pDisplay );

	m_hWindow = 0;
	m_bWindowLost = false;
	m_nLostNative = 0;
	m_nLossPendingMask = 0;
	m_nDisplayChangePendingMask = 0;
	m_nMouseButtonState = 0;
	m_bMousePositionValid = false;
	memset( m_bKeysDown, 0, sizeof( m_bKeysDown ) );

	clearRecovery();

	// The native XID is already invalid. The event is preserved on backpressure.
	static_cast<void>( queueWindowEvent( SIREventType::WindowDestroyed, nWindowId ) );
	return SIRWindowResult::Ok;
}

SIRWindowResult SIRNativeWindowDriverX11::emitServerLoss( void ) noexcept
{
	const SIRWindowResult eQueueStatus = flushPendingEvents();
	if ( eQueueStatus != SIRWindowResult::Ok ) {
		return eQueueStatus;
	}
	if ( ( m_nLossPendingMask & 1U ) != 0 ) {
		m_nLossPendingMask &= ~1U;
		if ( queueWindowEvent( SIREventType::WindowDestroyed, static_cast<uint32_t>( m_nLostNative ) ) != SIRWindowResult::Ok ) {
			return SIRWindowResult::Busy;
		}
	}
	return SIRWindowResult::NativeFailure;
}

SIRWindowResult SIRNativeWindowDriverX11::pumpEvents( void ) noexcept
{
	if ( m_pDisplay == nullptr && !m_bServerLost ) {
		return SIRWindowResult::Stopped;
	}

	const SIRWindowResult eQueueStatus = flushPendingEvents();
	if ( eQueueStatus != SIRWindowResult::Ok ) {
		return eQueueStatus;
	}

	if ( m_bServerLost || !transportAlive() ) {
		return emitServerLoss();
	}

	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		clearRecovery();
		markServerLost();
		return emitServerLoss();
	}
	s_pX11RecoveryDisplay = m_pDisplay;
	s_pX11RecoveryPoint = &recovery;

	const SIRWindowResult eResult = pumpEventsNative();
	clearRecovery();

	return eResult;
}

SIRWindowResult SIRNativeWindowDriverX11::processWindowEvent( const XEvent& event ) noexcept
{
	const uint32_t nWindowId = static_cast<uint32_t>( event.xany.window );
	SIRWindowResult eResult = SIRWindowResult::Ok;
	switch ( event.type ) {
	case ConfigureNotify: {
		const int32_t nX = event.xconfigure.x;
		const int32_t nY = event.xconfigure.y;
		if ( nX != m_nWindowX || nY != m_nWindowY ) {
			m_nWindowX = nX;
			m_nWindowY = nY;
			eResult = queueWindowEvent( SIREventType::WindowMoved, nWindowId, nX, nY );
		}
		if ( event.xconfigure.width > 0 && event.xconfigure.height > 0 ) {
			const uint32_t nWidth = static_cast<uint32_t>( event.xconfigure.width );
			const uint32_t nHeight = static_cast<uint32_t>( event.xconfigure.height );
			if ( nWidth != m_nWindowWidth || nHeight != m_nWindowHeight ) {
				m_nWindowWidth = nWidth;
				m_nWindowHeight = nHeight;
				eResult = queueWindowEvent( SIREventType::WindowResized, nWindowId, static_cast<int32_t>( nWidth ), static_cast<int32_t>( nHeight ) );
			}
		}
		break;
	}
	case MapNotify:
		eResult = queueWindowEvent( SIREventType::WindowShown, nWindowId );
		break;
	case UnmapNotify:
		eResult = queueWindowEvent( SIREventType::WindowHidden, nWindowId );
		break;
	case Expose:
		if ( event.xexpose.count == 0 ) {
			eResult = queueWindowEvent( SIREventType::WindowExposed, nWindowId );
		}
		break;
	case EnterNotify:
		eResult = queueWindowEvent( SIREventType::WindowMouseEnter, nWindowId );
		break;
	case LeaveNotify:
		eResult = queueWindowEvent( SIREventType::WindowMouseLeave, nWindowId );
		break;
	case KeyPress:
		eResult = handleKeyboardEvent( event.xkey, true );
		break;
	case KeyRelease:
		if ( !m_bDetectableRepeat && XPending( m_pDisplay ) > 0 ) {
			XEvent next{};
			XPeekEvent( m_pDisplay, &next );
			if ( next.type == KeyPress && next.xkey.keycode == event.xkey.keycode && next.xkey.time == event.xkey.time ) {
				break; // Synthetic release preceding an auto-repeat press.
			}
		}
		eResult = handleKeyboardEvent( event.xkey, false );
		break;
	case MotionNotify:
	case ButtonPress:
	case ButtonRelease:
		eResult = handleMouseEvent( event );
		break;
	case FocusIn:
		eResult = queueWindowEvent( SIREventType::WindowFocusGained, nWindowId );
		break;
	case FocusOut:
		eResult = queueWindowEvent( SIREventType::WindowFocusLost, nWindowId );
		m_nMouseButtonState = 0;
		m_bMousePositionValid = false;
		memset( m_bKeysDown, 0, sizeof( m_bKeysDown ) );
		break;
	case DestroyNotify:
		eResult = queueWindowEvent( SIREventType::WindowDestroyed, nWindowId );
		m_nLostNative = static_cast<uintptr_t>( m_hWindow );
		m_hWindow = 0;
		m_bWindowLost = true;
		break;
	case ClientMessage:
		if ( event.xclient.format == 32 && event.xclient.message_type == m_nProtocols &&
                 static_cast<Atom>( event.xclient.data.l[0] ) == m_nDeleteWindow ) {
			eResult = queueWindowEvent( SIREventType::WindowCloseRequested, nWindowId );
		}
		break;
	default:
		break;
	}
	return eResult;
}

SIRWindowResult SIRNativeWindowDriverX11::pumpEventsNative( void ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return SIRWindowResult::Stopped;
	}

	while ( XPending( m_pDisplay ) > 0 ) {
		XEvent event{};
		XNextEvent( m_pDisplay, &event );
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
		if ( m_nRandrEventBase >= 0 &&
             ( event.type == m_nRandrEventBase + RRScreenChangeNotify || event.type == m_nRandrEventBase + RRNotify ) ) {
			if ( event.type == m_nRandrEventBase + RRScreenChangeNotify ) {
				XRRUpdateConfiguration( &event );
			}
			if ( m_hWindow != 0 ) {
				m_nDisplayChangePendingMask |= 1U;
			}
			continue;
		}
#endif
		if ( event.type == ConfigureNotify && event.xconfigure.window == m_nRootWindow ) {
			if ( event.xconfigure.width > 0 && event.xconfigure.height > 0 ) {
				const uint32_t nWidth = static_cast<uint32_t>( event.xconfigure.width );
				const uint32_t nHeight = static_cast<uint32_t>( event.xconfigure.height );
				if ( nWidth != m_nServerWidth || nHeight != m_nServerHeight ) {
					m_nServerWidth = nWidth;
					m_nServerHeight = nHeight;
					m_nDisplayChangePendingMask |= 1U;
				}
			}
			continue;
		}
		if ( m_hWindow == 0 || event.xany.window != m_hWindow ) {
			continue;
		}
		const SIRWindowResult eResult = processWindowEvent( event );
		if ( eResult != SIRWindowResult::Ok ) {
			return eResult;
		}
	}

	if ( m_hWindow != 0 && ( m_nDisplayChangePendingMask & 1U ) != 0 ) {
		m_nDisplayChangePendingMask &= ~1U;
		XWindowAttributes attributes{};
		if ( XGetWindowAttributes( m_pDisplay, m_hWindow, &attributes ) && attributes.width > 0 && attributes.height > 0 ) {
			return queueWindowEvent( SIREventType::WindowDisplayChanged, static_cast<uint32_t>( m_hWindow ), attributes.width, attributes.height );
		}
	}

	return SIRWindowResult::Ok;
}

void SIRNativeWindowDriverX11::shutdown( void ) noexcept
{
	if ( m_pDisplay != nullptr && !m_bServerLost && transportAlive() ) {
		sigjmp_buf recovery;
		if ( sigsetjmp( recovery, 1 ) == 0 ) {
			s_pX11RecoveryDisplay = m_pDisplay;
			s_pX11RecoveryPoint = &recovery;
			shutdownNative();
		} else {
			markServerLost();
		}
		clearRecovery();
	}

	m_pDisplay = nullptr;
	m_hWindow = 0;
	m_nLostNative = 0;
	m_nLossPendingMask = 0;
	m_nDisplayChangePendingMask = 0;
	m_nPendingCount = 0;
	m_nDeleteWindow = 0;
	m_nProtocols = 0;
	m_nRootWindow = 0;
	m_nServerWidth = 0;
	m_nServerHeight = 0;
	m_nWindowWidth = 0;
	m_nWindowHeight = 0;
	m_nWindowX = 0;
	m_nWindowY = 0;
	m_nMouseButtonState = 0;
	m_bMousePositionValid = false;
	m_bDetectableRepeat = false;
	memset( m_bKeysDown, 0, sizeof( m_bKeysDown ) );
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
	m_nRandrEventBase = -1;
#endif
	m_bWindowLost = false;
	m_bServerLost = false;

	if ( m_bHandlerRegistered ) {
		const SIRScopedLock<SIRMutex> lock( s_X11IOHandlerLock );
		if ( s_nX11HandlerUsers != 0 ) {
			--s_nX11HandlerUsers;
			if ( s_nX11HandlerUsers == 0 ) {
				XSetIOErrorHandler( s_pfnPreviousX11IOHandler );
				s_pfnPreviousX11IOHandler = nullptr;
			}
		}
		m_bHandlerRegistered = false;
	}
}

void SIRNativeWindowDriverX11::shutdownNative( void ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return;
	}
	if ( m_hWindow != 0 ) {
		XDestroyWindow( m_pDisplay, m_hWindow );
		m_hWindow = 0;
	}
	XCloseDisplay( m_pDisplay );
	m_pDisplay = nullptr;
}
