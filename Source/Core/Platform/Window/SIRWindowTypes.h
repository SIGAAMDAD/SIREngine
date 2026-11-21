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
#ifndef __SIRENGINE_SIRWINDOWTYPES_H__
#define __SIRENGINE_SIRWINDOWTYPES_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirWindowPlatform_e : uint8_t
{
	SIR_WINDOW_PLATFORM_WIN32 = 1,
	SIR_WINDOW_PLATFORM_X11 = 2,
	SIR_WINDOW_PLATFORM_WAYLAND = 3,
	SIR_WINDOW_PLATFORM_HEADLESS = 4
} sirWindowPlatform_t;

typedef enum sirWindowResult_e : uint8_t
{
	SIR_WINDOW_OK = 0,
	SIR_WINDOW_INVALID_ARGUMENT,
	SIR_WINDOW_INVALID_HANDLE,
	SIR_WINDOW_WRONG_THREAD,
	SIR_WINDOW_OUT_OF_CAPACITY,
	SIR_WINDOW_EVENTS_LOST,
	SIR_WINDOW_NOT_READY,
	SIR_WINDOW_UNSUPPORTED,
	SIR_WINDOW_BUSY,
	SIR_WINDOW_NATIVE_FAILURE,
	SIR_WINDOW_STOPPED
} sirWindowResult_t;

typedef enum sirWindowEventKind_e : uint8_t
{
	SIR_WINDOW_EVENT_CREATED = 1,
	SIR_WINDOW_EVENT_READY,
	SIR_WINDOW_EVENT_CLOSE_REQUESTED,
	SIR_WINDOW_EVENT_DESTROYED,
	SIR_WINDOW_EVENT_RESIZED,
	SIR_WINDOW_EVENT_FOCUS_GAINED,
	SIR_WINDOW_EVENT_FOCUS_LOST,
	SIR_WINDOW_EVENT_DPI_CHANGED,
	SIR_WINDOW_EVENT_SHOWN,
	SIR_WINDOW_EVENT_HIDDEN,
	SIR_WINDOW_EVENT_MINIMIZED,
	SIR_WINDOW_EVENT_RESTORED,
	SIR_WINDOW_EVENT_LOST,
	SIR_WINDOW_EVENT_DISPLAY_CHANGED
} sirWindowEventKind_t;

typedef struct sirWindowHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nSystemEpoch;
} sirWindowHandle_t;

typedef struct sirWindowNativeView_s
{
	sirWindowPlatform_t ePlatform;
	uintptr_t nNativeWindow;
	uintptr_t nDisplayOrInstance;
	uint64_t nWindowGeneration;
} sirWindowNativeView_t;

typedef enum sirWindowMutationKind_e : uint8_t
{
	SIR_WINDOW_MUTATION_TITLE = 1,
	SIR_WINDOW_MUTATION_RESIZE,
	SIR_WINDOW_MUTATION_SHOW,
	SIR_WINDOW_MUTATION_HIDE,
	SIR_WINDOW_MUTATION_MINIMIZE,
	SIR_WINDOW_MUTATION_RESTORE,
	SIR_WINDOW_MUTATION_MAXIMIZE,
	SIR_WINDOW_MUTATION_FULLSCREEN_ENTER,
	SIR_WINDOW_MUTATION_FULLSCREEN_EXIT
} sirWindowMutationKind_t;

typedef struct sirWindowMutation_s
{
	sirWindowMutationKind_t eKind;
	const char *pszTitle;
	uint32_t nWidth;
	uint32_t nHeight;
} sirWindowMutation_t;

// No native window driver may inspect an unbounded caller-owned title.
// nTitleBytes = 0 accepts a bounded NUL-terminated UTF-8 title. An explicit
// byte count permits a nonterminated caller buffer; WindowSystem copies and
// terminates it before crossing the internal native driver boundary.
constexpr uint32_t SIR_WINDOW_TITLE_CAPACITY = 128;
typedef struct sirWindowCreateDesc_s
{
	const char *pszTitle;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nTitleBytes;
} sirWindowCreateDesc_t;

typedef struct sirWindowSnapshot_s
{
	sirWindowHandle_t hWindow;
	uint32_t nWidth;
	uint32_t nHeight;
	uint64_t nMetricsRevision;
	uint64_t nWindowGeneration;
	uint32_t nDpi;
	bool bReady;
	bool bVisible;
	bool bMinimized;
	bool bCloseRequested;
	bool bFocused;
} sirWindowSnapshot_t;

typedef struct sirWindowBackendEvent_s
{
	uint32_t nSlot;
	uintptr_t nNativeWindow;
	sirWindowEventKind_t eKind;
	uint32_t nWidth;
	uint32_t nHeight;
} sirWindowBackendEvent_t;

typedef struct sirWindowEvent_s
{
	sirWindowHandle_t hWindow;
	sirWindowEventKind_t eKind;
	uint32_t nWidth;
	uint32_t nHeight;
	uint64_t nSequence;
} sirWindowEvent_t;

typedef struct sirWindowEventRead_s
{
	uint64_t nAfterSequence;
	sirWindowEvent_t *pEvents;
	uint32_t nCapacity;
	uint32_t nCount;
	bool bLost;
} sirWindowEventRead_t;

typedef struct sirWindowNativeLease_s
{
	uint64_t nToken;
	uint64_t nSystemEpoch;
	uint32_t nSlot;
	uint32_t nGeneration;
} sirWindowNativeLease_t;

constexpr uint32_t SIR_WINDOW_MAX_WINDOWS = 8;
constexpr uint32_t SIR_WINDOW_MAX_LEASES = 32;
constexpr uint32_t SIR_WINDOW_EVENT_CAPACITY = 64;

#endif // __SIRENGINE_SIRWINDOWTYPES_H__
