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
#include <Core/Platform/SIRAtomic.h>
#include <Core/Platform/SIRSync.h>
#include <Core/Platform/SIRScopedLock.h>
#include <Core/Platform/SIRPlatformThreadControl.h>

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
	SIRAtomic<uint64_t> nToken;
	SIRAtomic<uint64_t> nEpoch;
	SIRAtomic<uint32_t> nSlot;
	SIRAtomic<uint32_t> nGeneration;
	SIRAtomic<uint8_t> nState;
} sirWindowLeaseEntry_t;

// A caller-owned request is copied before the platform owner applies it.
// Completed tickets remain queryable until consumed; abandoned tickets
// explicitly exhaust the bounded table rather than overwriting an outcome.
constexpr uint32_t SIR_WINDOW_REQUEST_CAPACITY = 32;
constexpr uint32_t SIR_WINDOW_REQUEST_TITLE_CAPACITY = 128;
typedef struct sirWindowRequestEntry_s
{
	sirWindowHandle_t hWindow;
	sirWindowMutation_t mutation;
	uint64_t nTicket;
	sirWindowResult_t eResult;
	uint8_t nState;
	bool bClose;
	char szTitle[SIR_WINDOW_REQUEST_TITLE_CAPACITY];
} sirWindowRequestEntry_t;

// Core owns native window identities, event state and render-window leases.
// Mutating operations and event reads run on the window owner thread.
// querySnapshot and releaseNativeLease are safe for cross-thread consumers.
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
	sirWindowResult_t mutateWindow( sirWindowHandle_t hWindow, const sirWindowMutation_t& mutation ) noexcept;
	// Thread-safe bounded submission; native OS mutations execute only in pumpEvents.
	sirWindowResult_t queueMutation( sirWindowHandle_t hWindow, const sirWindowMutation_t& mutation, uint64_t *pTicket ) noexcept;
	sirWindowResult_t queueClose( sirWindowHandle_t hWindow, uint64_t *pTicket ) noexcept;
	sirWindowResult_t queryRequest( uint64_t nTicket, sirWindowResult_t *pResult, bool *pCompleted ) noexcept;
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
	void processRequests( void ) noexcept;
	sirWindowResult_t enqueueRequest( sirWindowHandle_t hWindow, const sirWindowMutation_t *pMutation, uint64_t *pTicket ) noexcept;
	void pushEvent( sirWindowHandle_t hWindow, sirWindowEventKind_t eKind, uint32_t nWidth, uint32_t nHeight ) noexcept;

	mutable SIRMutex m_SnapshotMutex;
	mutable SIRMutex m_RequestMutex;
	SIRWindowDriver *m_pDriver;
	uint64_t m_nOwnerThreadId;
	sirWindowSlot_t m_Slots[SIR_WINDOW_MAX_WINDOWS] = {};
	sirWindowLeaseEntry_t m_Leases[SIR_WINDOW_MAX_LEASES] = {};
	sirWindowEvent_t m_Events[SIR_WINDOW_EVENT_CAPACITY] = {};
	sirWindowRequestEntry_t m_Requests[SIR_WINDOW_REQUEST_CAPACITY] = {};
	uint64_t m_nRequestSerial = 0;
	uint64_t m_nEpoch;
	uint64_t m_nEventSequence;
	uint64_t m_nLeaseSerial;
	uint64_t m_nNativeGeneration;
	SIRAtomic<bool> m_bForeignThreadEvent;
	bool m_bInitialized;
};
#endif // __SIRENGINE_SIRWINDOWSYSTEM_H__
