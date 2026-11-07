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
#ifndef __SIRENGINE_SIRWINDOWDRIVERWAYLAND_H__
#define __SIRENGINE_SIRWINDOWDRIVERWAYLAND_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Window/SIRWindowDriver.h>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_seat;
struct zwp_pointer_constraints_v1;
struct zwp_relative_pointer_manager_v1;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct wl_array;

typedef void ( *sirWaylandSeatLostFn_t )( void *pContext ) noexcept;
typedef void ( *sirWaylandPointerProtocolLostFn_t )( void *pContext ) noexcept;

class SIRWindowDriverWayland;

typedef struct sirWaylandWindowSlot_s
{
	SIRWindowDriverWayland *pDriver;
	wl_surface *pSurface;
	xdg_surface *pXdgSurface;
	xdg_toplevel *pToplevel;
	uint32_t nIndex;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nPendingWidth;
	uint32_t nPendingHeight;
	bool bConfigured;
	bool bClosePending;
	bool bLostPending;
} sirWaylandWindowSlot_t;

// Native xdg-shell window owner. No graphics or input SDK types cross this ABI.
// A created surface remains NOT_READY until its first configure is acknowledged.
class SIRWindowDriverWayland final : public SIRWindowDriver
{
public:
	SIRWindowDriverWayland( void ) noexcept = default;
	~SIRWindowDriverWayland( void ) noexcept override;

	sirWindowResult_t initialize( void ) noexcept override;
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override;
	sirWindowResult_t destroyWindow( uint32_t nSlot ) noexcept override;
	sirWindowResult_t mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override;
	sirWindowResult_t pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override;
	void shutdown( void ) noexcept override;
	wl_seat *borrowSeat( void ) const noexcept;
	bool setSeatLostSink( sirWaylandSeatLostFn_t pSink, void *pContext ) noexcept;
	zwp_pointer_constraints_v1 *borrowPointerConstraints( void ) const noexcept;
	zwp_relative_pointer_manager_v1 *borrowRelativePointerManager( void ) const noexcept;
	bool setPointerProtocolLostSink( sirWaylandPointerProtocolLostFn_t pSink, void *pContext ) noexcept;

protected:
private:
	static void registryGlobal( void *pContext, wl_registry *pRegistry, uint32_t nName, const char *pszInterface,
	                            uint32_t nVersion ) noexcept;
	static void registryRemove( void *pContext, wl_registry *pRegistry, uint32_t nName ) noexcept;
	static void wmBasePing( void *pContext, xdg_wm_base *pBase, uint32_t nSerial ) noexcept;
	static void surfaceConfigure( void *pContext, xdg_surface *pSurface, uint32_t nSerial ) noexcept;
	static void topConfigure( void *pContext, xdg_toplevel *pTop, int32_t nWidth, int32_t nHeight, wl_array *pStates ) noexcept;
	static void topClose( void *pContext, xdg_toplevel *pTop ) noexcept;
	void queueEvent( uint32_t nSlot, sirWindowEventKind_t eKind, uint32_t nWidth, uint32_t nHeight ) noexcept;
	void loseDisplay( void ) noexcept;

	wl_display *m_pDisplay = nullptr;
	wl_registry *m_pRegistry = nullptr;
	wl_compositor *m_pCompositor = nullptr;
	wl_seat *m_pSeat = nullptr;
	zwp_pointer_constraints_v1 *m_pPointerConstraints = nullptr;
	zwp_relative_pointer_manager_v1 *m_pRelativePointerManager = nullptr;
	sirWaylandPointerProtocolLostFn_t m_pPointerProtocolLostSink = nullptr;
	void *m_pPointerProtocolLostContext = nullptr;
	sirWaylandSeatLostFn_t m_pSeatLostSink = nullptr;
	void *m_pSeatLostContext = nullptr;
	xdg_wm_base *m_pBase = nullptr;
	sirWaylandWindowSlot_t m_Slots[SIR_WINDOW_MAX_WINDOWS] = {};
	sirWindowBackendEvent_t m_Events[SIR_WINDOW_EVENT_CAPACITY] = {};
	uint32_t m_nEventRead = 0;
	uint32_t m_nEventWrite = 0;
	uint32_t m_nEventCount = 0;
	uint32_t m_nCompositorGlobal = 0;
	uint32_t m_nSeatGlobal = 0;
	uint32_t m_nConstraintsGlobal = 0;
	uint32_t m_nRelativePointerGlobal = 0;
	uint32_t m_nShellGlobal = 0;
	bool m_bLost = false;
	bool m_bOverflow = false;
};
#endif // __SIRENGINE_SIRWINDOWDRIVERWAYLAND_H__
