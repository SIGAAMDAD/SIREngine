/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRWINDOWSYSTEM_H__
#define __SIRENGINE_SIRWINDOWSYSTEM_H__

#include <SIRPCH.hpp>
#include <Core/Platform/SIRSync.h>
#include <stdint.h>

#if defined( _WIN32 )
#include <Windows.h>
#elif defined( __linux__ )
#include <pthread.h>
#endif

constexpr uint32_t SIR_WINDOW_MAX_WINDOWS = 8;
constexpr uint32_t SIR_WINDOW_EVENT_CAPACITY = 128;

typedef enum sirWindowResult_e : uint8_t
{
	SIR_WINDOW_OK = 0,
	SIR_WINDOW_INVALID = 1,
	SIR_WINDOW_BUSY = 2,
	SIR_WINDOW_NO_CAPACITY = 3,
	SIR_WINDOW_PLATFORM_ERROR = 4,
	SIR_WINDOW_WRONG_THREAD = 5
} sirWindowResult_t;

typedef enum sirWindowState_e : uint8_t
{
	SIR_WINDOW_FREE = 0,
	SIR_WINDOW_CREATING = 1,
	SIR_WINDOW_WAIT_CONFIGURE = 2,
	SIR_WINDOW_READY = 3,
	SIR_WINDOW_CLOSE_REQUESTED = 4,
	SIR_WINDOW_RETIRING = 5,
	SIR_WINDOW_DESTROYED = 6,
	SIR_WINDOW_LOST = 7
} sirWindowState_t;

typedef enum sirWindowPlatform_e : uint8_t
{
	SIR_WINDOW_PLATFORM_NULL = 0,
	SIR_WINDOW_PLATFORM_WIN32 = 1,
	SIR_WINDOW_PLATFORM_X11 = 2,
	SIR_WINDOW_PLATFORM_WAYLAND = 3
} sirWindowPlatform_t;

typedef enum sirWindowEventType_e : uint8_t
{
	SIR_WINDOW_EVENT_CREATED = 0,
	SIR_WINDOW_EVENT_RESIZED = 1,
	SIR_WINDOW_EVENT_FOCUS = 2,
	SIR_WINDOW_EVENT_CLOSE = 3,
	SIR_WINDOW_EVENT_DESTROYED = 4,
	SIR_WINDOW_EVENT_LOST = 5
} sirWindowEventType_t;

typedef struct sirWindowHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirWindowHandle_t;

typedef struct sirWindowDesc_s
{
	const char *pszTitle;
	uint32_t nWidth;
	uint32_t nHeight;
	bool bVisible;
} sirWindowDesc_t;

// SDK-neutral borrowed view. The renderer must retain its matching lease
// until all graphics surface/swapchain uses and completions have retired.
typedef struct sirWindowNativeView_s
{
	sirWindowPlatform_t ePlatform;
	void *pWindow;
	void *pDisplayOrInstance;
	uint64_t nWindowGeneration;
} sirWindowNativeView_t;

typedef struct sirWindowSnapshot_s
{
	sirWindowHandle_t hWindow;
	sirWindowState_t eState;
	uint32_t nWidth;
	uint32_t nHeight;
	uint64_t nRevision;
	bool bFocused;
	bool bVisible;
} sirWindowSnapshot_t;

typedef struct sirWindowEvent_s
{
	sirWindowHandle_t hWindow;
	sirWindowEventType_t eType;
	uint64_t nRevision;
} sirWindowEvent_t;

typedef struct sirWindowLease_s
{
	sirWindowHandle_t hWindow;
	uint64_t nToken;
} sirWindowLease_t;

class SIRInputService;
class SIRInputBackend;
class SIRWindowSystem;

class SIRWindowDriver
{
public:
	virtual ~SIRWindowDriver( void ) noexcept = default;
	virtual bool initialize( SIRWindowSystem& system ) noexcept = 0;
	virtual void shutdown( void ) noexcept = 0;
	virtual bool create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept = 0;
	virtual void destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept = 0;
	virtual bool pump( void ) noexcept = 0;
	virtual bool bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept = 0;
};

// A single owner-thread service manages all OS windows. All native operations,
// event pumping, and state notifications occur on that thread. Render workers
// may borrow/release generation-validated native views under the Core mutex.
class SIRWindowSystem
{
public:
	SIRWindowSystem( void ) noexcept;
	~SIRWindowSystem( void ) noexcept;
	SIRWindowSystem( const SIRWindowSystem& ) = delete;
	SIRWindowSystem& operator=( const SIRWindowSystem& ) = delete;

	sirWindowResult_t initialize( SIRWindowDriver& driver ) noexcept;
	sirWindowResult_t shutdown( void ) noexcept;
	sirWindowResult_t create( const sirWindowDesc_t& desc, sirWindowHandle_t *pWindow ) noexcept;
	sirWindowResult_t requestClose( sirWindowHandle_t hWindow ) noexcept;
	sirWindowResult_t pump( void ) noexcept;
	sirWindowResult_t bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept;
	bool snapshot( sirWindowHandle_t hWindow, sirWindowSnapshot_t *pSnapshot ) const noexcept;
	bool acquireNativeView( sirWindowHandle_t hWindow, sirWindowNativeView_t *pView, sirWindowLease_t *pLease ) noexcept;
	bool releaseNativeView( sirWindowLease_t hLease ) noexcept;
	bool popEvent( sirWindowEvent_t *pEvent ) noexcept;
	bool hasLiveWindows( void ) const noexcept;
	bool eventOverflowed( void ) const noexcept;
	uint32_t activeWindowCount( void ) const noexcept;

	// Called only by the backend's owner-thread native callback.
	bool notifyResize( sirWindowHandle_t hWindow, uint32_t nWidth, uint32_t nHeight ) noexcept;
	bool notifyFocus( sirWindowHandle_t hWindow, bool bFocused ) noexcept;
	bool notifyClose( sirWindowHandle_t hWindow ) noexcept;
	bool notifyLost( sirWindowHandle_t hWindow ) noexcept;

private:
	typedef struct sirWindowSlot_s
	{
		sirWindowNativeView_t view;
		uint32_t nGeneration;
		uint32_t nWidth;
		uint32_t nHeight;
		uint32_t nBorrowCount;
		uint64_t nRevision;
		uint64_t nBorrowSerial;
		uint64_t nBorrowTokens[32];
		sirWindowState_t eState;
		bool bFocused;
		bool bVisible;
	} sirWindowSlot_t;

	mutable SIRMutex m_Mutex;
	sirWindowSlot_t m_szSlots[SIR_WINDOW_MAX_WINDOWS];
	sirWindowEvent_t m_szEvents[SIR_WINDOW_EVENT_CAPACITY];
	uint32_t m_iEventRead;
	uint32_t m_nEventCount;
	uint32_t m_nLiveWindows;
	uint64_t m_nLeaseSerial;
	SIRWindowDriver *m_pDriver;
	bool m_bEventsOverflowed;
	bool m_bInitialized;
	bool m_bPumping;
#if defined( _WIN32 )
	DWORD m_nOwnerThread;
#else
	pthread_t m_hOwnerThread;
#endif

	bool onOwnerThread( void ) const noexcept;
	bool isCurrent( sirWindowHandle_t hWindow ) const noexcept;
	void pushEvent( sirWindowHandle_t hWindow, sirWindowEventType_t eType ) noexcept;
	sirWindowResult_t retirePending( void ) noexcept;
};

// Deterministic CPU-only driver for tests and explicit headless operation.
class SIRNullWindowDriver final : public SIRWindowDriver
{
public:
	SIRNullWindowDriver( void ) noexcept;
	bool initialize( SIRWindowSystem& system ) noexcept override;
	void shutdown( void ) noexcept override;
	bool create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept override;
	void destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept override;
	bool pump( void ) noexcept override;
	bool bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept override;

private:
	SIRWindowSystem *m_pSystem;
};

#endif // __SIRENGINE_SIRWINDOWSYSTEM_H__
