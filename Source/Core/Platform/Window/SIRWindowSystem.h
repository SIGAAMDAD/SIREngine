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
#ifndef __SIRENGINE_SIRWINDOWSYSTEM_H__
#define __SIRENGINE_SIRWINDOWSYSTEM_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Window/SIRWindowDriver.h>
#include <Core/Systems/SIREngineSystem.h>
#include <atomic>
#include <thread>

typedef struct sirWindowSlot_s
{
	sirWindowSnapshot_t snapshot;
	sirWindowNativeView_t native;
	uint32_t nGeneration;
	uint32_t nLeaseCount;
	bool bOccupied;
} sirWindowSlot_t;

typedef struct sirWindowLeaseEntry_s
{
	std::atomic<uint64_t> nToken;
	std::atomic<uint64_t> nEpoch;
	std::atomic<uint32_t> nSlot;
	std::atomic<uint32_t> nGeneration;
	std::atomic<uint8_t> nState;
} sirWindowLeaseEntry_t;

// Core owns native window identities, event state and render-window leases.
// All operations except releaseNativeLease run on the initialize thread.
// A renderer must retire its GPU surface/fences BEFORE releasing its lease.
class SIRWindowSystem final : public SIREngineSystem
{
public:
	explicit SIRWindowSystem( SIRWindowDriver& driver ) noexcept;
	~SIRWindowSystem( void ) noexcept override = default;

	SIRWindowSystem( const SIRWindowSystem& ) = delete;
	SIRWindowSystem& operator=( const SIRWindowSystem& ) = delete;

	sirSystemResult_t initialize( void ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;

	sirWindowResult_t querySnapshot( sirWindowHandle_t hWindow, sirWindowSnapshot_t *pSnapshot ) const noexcept;

	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, sirWindowHandle_t *pHandle ) noexcept;
	sirWindowResult_t requestClose( sirWindowHandle_t hWindow ) noexcept;
	sirWindowResult_t destroyWindow( sirWindowHandle_t hWindow ) noexcept;
	sirWindowResult_t pumpEvents( uint32_t nBudget ) noexcept;
	sirWindowResult_t readEvents( sirWindowEventRead_t *pRead ) noexcept;
	sirWindowResult_t acquireNativeLease( sirWindowHandle_t hWindow, sirWindowNativeLease_t *pLease,
	                                      sirWindowNativeView_t *pView ) noexcept;
	sirWindowResult_t releaseNativeLease( sirWindowNativeLease_t hLease ) noexcept;

protected:
private:
	static void onDriverEvent( void *pContext, const sirWindowBackendEvent_t& event ) noexcept;
	sirWindowResult_t validate( sirWindowHandle_t hWindow ) const noexcept;
	sirWindowResult_t requireOwner( void ) const noexcept;
	void processReleasedLeases( void ) noexcept;
	void pushEvent( sirWindowHandle_t hWindow, sirWindowEventKind_t eKind, uint32_t nWidth, uint32_t nHeight ) noexcept;

	SIRWindowDriver *m_pDriver;
	std::thread::id m_OwnerThread;
	sirWindowSlot_t m_Slots[SIR_WINDOW_MAX_WINDOWS] = {};
	sirWindowLeaseEntry_t m_Leases[SIR_WINDOW_MAX_LEASES] = {};
	sirWindowEvent_t m_Events[SIR_WINDOW_EVENT_CAPACITY] = {};
	uint64_t m_nEpoch;
	uint64_t m_nEventSequence;
	uint64_t m_nLeaseSerial;
	uint64_t m_nNativeGeneration;
	bool m_bInitialized;
};
#endif // __SIRENGINE_SIRWINDOWSYSTEM_H__
