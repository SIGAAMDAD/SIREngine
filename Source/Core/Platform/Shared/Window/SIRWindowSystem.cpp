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

#include <Core/Platform/Shared/Window/SIRWindowSystem.h>
#include <string.h>

// Reject malformed UTF-8, including overlong sequences, lone continuation
// bytes, surrogate scalars and Unicode code points above U+10FFFF.
// This runs only on cold window creation/title mutations, not the event path.
static bool sirWindowCopyTitle( const char *pszSource, uint32_t nDeclaredBytes, char *pszTarget ) noexcept
{
	if ( pszSource == nullptr || pszTarget == nullptr || nDeclaredBytes >= SIR_WINDOW_TITLE_CAPACITY ) {
		return false;
	}
	uint32_t nBytes = nDeclaredBytes;
	if ( nBytes == 0 ) {
		while ( nBytes < SIR_WINDOW_TITLE_CAPACITY && pszSource[nBytes] != '\0' ) {
			++nBytes;
		}
		if ( nBytes == SIR_WINDOW_TITLE_CAPACITY ) {
			return false;
		}
	}
	uint32_t i = 0;
	while ( i < nBytes ) {
		const uint8_t nFirst = static_cast<uint8_t>(pszSource[i]);
		if ( nFirst == 0 ) {
			return false;
		}
		if ( nFirst < 0x80 ) {
			++i;
			continue;
		}
		uint32_t nWidth = 0;
		uint32_t nCodepoint = 0;
		uint32_t nMinimum = 0;
		if ( nFirst >= 0xC2 && nFirst <= 0xDF ) {
			nWidth = 2;
			nCodepoint = nFirst & 0x1F;
			nMinimum = 0x80;
		} else if ( nFirst >= 0xE0 && nFirst <= 0xEF ) {
			nWidth = 3;
			nCodepoint = nFirst & 0x0F;
			nMinimum = 0x800;
		} else if ( nFirst >= 0xF0 && nFirst <= 0xF4 ) {
			nWidth = 4;
			nCodepoint = nFirst & 0x07;
			nMinimum = 0x10000;
		} else {
			return false;
		}
		if ( nWidth > nBytes - i ) {
			return false;
		}
		for ( uint32_t k = 1; k < nWidth; ++k ) {
			const uint8_t nNext = static_cast<uint8_t>(pszSource[i + k]);
			if ( ( nNext & 0xC0 ) != 0x80 ) {
				return false;
			}
			nCodepoint = ( nCodepoint << 6 ) | ( nNext & 0x3F );
		}
		if ( nCodepoint < nMinimum || nCodepoint > 0x10FFFF || ( nCodepoint >= 0xD800 && nCodepoint <= 0xDFFF ) ) {
			return false;
		}
		i += nWidth;
	}
	memcpy( pszTarget, pszSource, nBytes );
	pszTarget[nBytes] = '\0';
	return true;
}

SIRWindowSystem::SIRWindowSystem( SIRWindowDriver& driver ) noexcept
    : m_pDriver( &driver ), m_nOwnerThreadId( 0 ), m_nEpoch( 0 ), m_nEventSequence( 0 ), m_nLeaseSerial( 0 ), m_nNativeGeneration( 0 ),
      m_bForeignThreadEvent( false ), m_bInitialized( false )
{
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_Slots[i].nGeneration = 1;
	}
}

SIRSystemResult SIRWindowSystem::initialize( void ) noexcept
{
	if ( m_bInitialized || m_nEpoch == UINT64_MAX ) {
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}
	if ( m_pDriver->initialize() != sirWindowResult::SIR_WINDOW_OK ) {
		// A native backend can acquire part of its process/display state
		// before reporting failure. Roll that state back before retrying.
		m_pDriver->shutdown();
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}
	m_nOwnerThreadId = SIRPlatformThreadControl::getCurrentThreadId();
	m_bForeignThreadEvent.store( false, std::memory_order_release );
	{
		SIRScopedLock<SIRMutex> requestGuard( m_RequestMutex );
		SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
		for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
			m_Requests[i] = {};
		}
		++m_nEpoch;
		m_nEventSequence = 0;
		m_bInitialized = true;
	}
	return SIRSystemResult::SIR_SYSTEM_OK;
}

SIRSystemResult SIRWindowSystem::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIRSystemResult::SIR_SYSTEM_OK;
	}
	if ( requireOwner() != sirWindowResult::SIR_WINDOW_OK ) {
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}
	processReleasedLeases();
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_Slots[i].bOccupied ) {
			return SIRSystemResult::SIR_SYSTEM_BUSY;
		}
	}
	m_pDriver->shutdown();
	{
		SIRScopedLock<SIRMutex> requestGuard( m_RequestMutex );
		SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
		m_bInitialized = false;
		for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
			if ( m_Requests[i].nState == 1 ) {
				m_Requests[i].eResult = sirWindowResult::SIR_WINDOW_STOPPED;
				m_Requests[i].nState = 3;
			}
		}
	}
	return SIRSystemResult::SIR_SYSTEM_OK;
}

sirWindowResult SIRWindowSystem::requireOwner( void ) const noexcept
{
	if ( !m_bInitialized ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	return m_nOwnerThreadId == SIRPlatformThreadControl::getCurrentThreadId() ? sirWindowResult::SIR_WINDOW_OK : sirWindowResult::SIR_WINDOW_WRONG_THREAD;
}

sirWindowResult SIRWindowSystem::validate( sirWindowHandle_t hWindow ) const noexcept
{
	if ( hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS || hWindow.nGeneration == 0 || hWindow.nSystemEpoch != m_nEpoch ) {
		return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
	}
	const sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	return slot.bOccupied && slot.nGeneration == hWindow.nGeneration ? sirWindowResult::SIR_WINDOW_OK : sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
}

void SIRWindowSystem::pushEvent( sirWindowHandle_t hWindow, sirWindowEventKind eKind, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( m_nEventSequence == UINT64_MAX ) {
		return;
	}
	++m_nEventSequence;
	sirWindowEvent_t& event = m_Events[( m_nEventSequence - 1 ) % SIR_WINDOW_EVENT_CAPACITY];
	event.hWindow = hWindow;
	event.eKind = eKind;
	event.nWidth = nWidth;
	event.nHeight = nHeight;
	event.nSequence = m_nEventSequence;
}

sirWindowResult SIRWindowSystem::createWindow( const sirWindowCreateDesc_t& desc, sirWindowHandle_t *pHandle ) noexcept
{
	const sirWindowResult eOwner = requireOwner();
	if ( eOwner != sirWindowResult::SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( pHandle == nullptr || desc.nWidth == 0 || desc.nHeight == 0 ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	char szTitle[SIR_WINDOW_TITLE_CAPACITY];
	memset( szTitle, 0, sizeof( szTitle ) );
	if ( !sirWindowCopyTitle( desc.pszTitle, desc.nTitleBytes, szTitle ) ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	sirWindowCreateDesc_t nativeDesc = desc;
	nativeDesc.pszTitle = szTitle;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		sirWindowSlot_t& slot = m_Slots[i];
		if ( slot.bOccupied ) {
			continue;
		}
		sirWindowNativeView_t view {};
		const sirWindowResult eResult = m_pDriver->createWindow( nativeDesc, i, &view );
		if ( eResult != sirWindowResult::SIR_WINDOW_OK ) {
			return eResult;
		}
		if ( view.nNativeWindow == 0 || view.nDisplayOrInstance == 0 || view.ePlatform == sirWindowPlatform::SIR_WINDOW_PLATFORM_HEADLESS ) {
			static_cast<void>(m_pDriver->destroyWindow( i ));
			return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
		}
		if ( m_nNativeGeneration == UINT64_MAX ) {
			static_cast<void>(m_pDriver->destroyWindow( i ));
			return sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY;
		}
		{
			SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
			view.nWindowGeneration = ++m_nNativeGeneration;
			slot.native = view;
			slot.snapshot = {};
			slot.snapshot.hWindow = { i, slot.nGeneration, m_nEpoch };
			slot.snapshot.nWindowGeneration = view.nWindowGeneration;
			slot.snapshot.nMetricsRevision = 1;
			slot.snapshot.nDpi = 96;
			slot.snapshot.nWidth = desc.nWidth;
			slot.snapshot.nHeight = desc.nHeight;
			slot.snapshot.bReady = view.ePlatform != sirWindowPlatform::SIR_WINDOW_PLATFORM_WAYLAND;
			slot.snapshot.bVisible = view.ePlatform != sirWindowPlatform::SIR_WINDOW_PLATFORM_WAYLAND;
			slot.snapshot.bMinimized = false;
			slot.bOccupied = true;
			*pHandle = slot.snapshot.hWindow;
		}
		pushEvent( *pHandle, sirWindowEventKind::SIR_WINDOW_EVENT_CREATED, desc.nWidth, desc.nHeight );
		if ( view.ePlatform != sirWindowPlatform::SIR_WINDOW_PLATFORM_WAYLAND ) {
			pushEvent( *pHandle, sirWindowEventKind::SIR_WINDOW_EVENT_READY, desc.nWidth, desc.nHeight );
		}
		return sirWindowResult::SIR_WINDOW_OK;
	}
	return sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY;
}

sirWindowResult SIRWindowSystem::requestClose( sirWindowHandle_t hWindow ) noexcept
{
	const sirWindowResult eOwner = requireOwner();
	if ( eOwner != sirWindowResult::SIR_WINDOW_OK ) {
		return eOwner;
	}
	const sirWindowResult eValid = validate( hWindow );
	if ( eValid != sirWindowResult::SIR_WINDOW_OK ) {
		return eValid;
	}
	SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
	sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	if ( !slot.snapshot.bCloseRequested ) {
		slot.snapshot.bCloseRequested = true;
		slot.snapshot.bReady = false;
		pushEvent( hWindow, sirWindowEventKind::SIR_WINDOW_EVENT_CLOSE_REQUESTED, slot.snapshot.nWidth, slot.snapshot.nHeight );
	}
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowSystem::mutateWindow( sirWindowHandle_t hWindow, const sirWindowMutation_t& mutation ) noexcept
{
	const sirWindowResult eOwner = requireOwner();
	if ( eOwner != sirWindowResult::SIR_WINDOW_OK ) {
		return eOwner;
	}
	const sirWindowResult eValid = validate( hWindow );
	if ( eValid != sirWindowResult::SIR_WINDOW_OK ) {
		return eValid;
	}
	if ( m_Slots[hWindow.nSlot].snapshot.bCloseRequested ) {
		return sirWindowResult::SIR_WINDOW_BUSY;
	}
	if ( mutation.eKind < sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE || mutation.eKind > sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_EXIT ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( ( mutation.eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE && mutation.pszTitle == nullptr ) ||
	     ( mutation.eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_RESIZE && ( mutation.nWidth == 0 || mutation.nHeight == 0 ) ) ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( mutation.eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE ) {
		char szTitle[SIR_WINDOW_TITLE_CAPACITY];
		memset( szTitle, 0, sizeof( szTitle ) );
		if ( !sirWindowCopyTitle( mutation.pszTitle, 0, szTitle ) ) {
			return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
		}
		sirWindowMutation_t nativeMutation = mutation;
		nativeMutation.pszTitle = szTitle;
		return m_pDriver->mutateWindow( hWindow.nSlot, nativeMutation );
	}
	return m_pDriver->mutateWindow( hWindow.nSlot, mutation );
}

sirWindowResult SIRWindowSystem::enqueueRequest( sirWindowHandle_t hWindow, const sirWindowMutation_t *pMutation,
                                                   uint64_t *pTicket ) noexcept
{
	if ( pTicket == nullptr ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( pMutation != nullptr ) {
		if ( pMutation->eKind < sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE || pMutation->eKind > sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_EXIT ||
		     ( pMutation->eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE && pMutation->pszTitle == nullptr ) ||
		     ( pMutation->eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_RESIZE && ( pMutation->nWidth == 0 || pMutation->nHeight == 0 ) ) ) {
			return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
		}
	}
	SIRScopedLock<SIRMutex> requestGuard( m_RequestMutex );
	SIRScopedLock<SIRMutex> snapshotGuard( m_SnapshotMutex );
	if ( !m_bInitialized ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	const sirWindowResult eValid = validate( hWindow );
	if ( eValid != sirWindowResult::SIR_WINDOW_OK ) {
		return eValid;
	}
	if ( m_Slots[hWindow.nSlot].snapshot.bCloseRequested ) {
		return sirWindowResult::SIR_WINDOW_BUSY;
	}
	if ( m_nRequestSerial == UINT64_MAX ) {
		return sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY;
	}
	uint32_t nSlot = SIR_WINDOW_REQUEST_CAPACITY;
	for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
		if ( m_Requests[i].nState == 0 ) {
			nSlot = i;
			break;
		}
	}
	if ( nSlot == SIR_WINDOW_REQUEST_CAPACITY ) {
		return sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY;
	}
	sirWindowRequestEntry_t& request = m_Requests[nSlot];
	request = {};
	request.hWindow = hWindow;
	request.bClose = pMutation == nullptr;
	if ( pMutation != nullptr ) {
		request.mutation = *pMutation;
		if ( pMutation->eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE ) {
			if ( !sirWindowCopyTitle( pMutation->pszTitle, 0, request.szTitle ) ) {
				return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
			}
			request.mutation.pszTitle = request.szTitle;
		}
	}
	request.nTicket = ++m_nRequestSerial;
	request.nState = 1;
	*pTicket = request.nTicket;
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowSystem::queueMutation( sirWindowHandle_t hWindow, const sirWindowMutation_t& mutation,
                                                  uint64_t *pTicket ) noexcept
{
	return enqueueRequest( hWindow, &mutation, pTicket );
}

sirWindowResult SIRWindowSystem::queueClose( sirWindowHandle_t hWindow, uint64_t *pTicket ) noexcept
{
	return enqueueRequest( hWindow, nullptr, pTicket );
}

sirWindowResult SIRWindowSystem::queryRequest( uint64_t nTicket, sirWindowResult *pResult, bool *pCompleted ) noexcept
{
	if ( nTicket == 0 || pResult == nullptr || pCompleted == nullptr ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	SIRScopedLock<SIRMutex> guard( m_RequestMutex );
	for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
		sirWindowRequestEntry_t& request = m_Requests[i];
		if ( request.nState == 0 || request.nTicket != nTicket ) {
			continue;
		}
		*pCompleted = request.nState == 3;
		if ( request.nState == 3 ) {
			*pResult = request.eResult;
			request = {};
		}
		return sirWindowResult::SIR_WINDOW_OK;
	}
	return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
}

void SIRWindowSystem::processRequests( void ) noexcept
{
	for ( uint32_t nProcessed = 0; nProcessed < SIR_WINDOW_REQUEST_CAPACITY; ++nProcessed ) {
		uint64_t nTicket = UINT64_MAX;
		sirWindowHandle_t hWindow {};
		sirWindowMutation_t mutation {};
		char szTitle[SIR_WINDOW_REQUEST_TITLE_CAPACITY];
		memset( szTitle, 0, sizeof( szTitle ) );
		bool bClose = false;
		{
			SIRScopedLock<SIRMutex> guard( m_RequestMutex );
			uint32_t nCandidate = SIR_WINDOW_REQUEST_CAPACITY;
			for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
				const sirWindowRequestEntry_t& request = m_Requests[i];
				if ( request.nState == 1 && request.nTicket < nTicket ) {
					nTicket = request.nTicket;
					nCandidate = i;
				}
			}
			if ( nCandidate == SIR_WINDOW_REQUEST_CAPACITY ) {
				break;
			}
			sirWindowRequestEntry_t& request = m_Requests[nCandidate];
			request.nState = 2;
			hWindow = request.hWindow;
			mutation = request.mutation;
			bClose = request.bClose;
			if ( !bClose && mutation.eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE ) {
				memcpy( szTitle, request.szTitle, SIR_WINDOW_REQUEST_TITLE_CAPACITY );
				mutation.pszTitle = szTitle;
			}
		}
		const sirWindowResult eResult = bClose ? requestClose( hWindow ) : mutateWindow( hWindow, mutation );
		{
			SIRScopedLock<SIRMutex> guard( m_RequestMutex );
			for ( uint32_t i = 0; i < SIR_WINDOW_REQUEST_CAPACITY; ++i ) {
				sirWindowRequestEntry_t& request = m_Requests[i];
				if ( request.nTicket == nTicket && request.nState == 2 ) {
					request.eResult = eResult;
					request.nState = 3;
					break;
				}
			}
		}
	}
}

void SIRWindowSystem::processReleasedLeases( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_LEASES; ++i ) {
		sirWindowLeaseEntry_t& lease = m_Leases[i];
		if ( lease.nState.load( std::memory_order_acquire ) != 2 ) {
			continue;
		}
		const uint32_t nSlot = lease.nSlot.load( std::memory_order_relaxed );
		const uint32_t nGeneration = lease.nGeneration.load( std::memory_order_relaxed );
		const uint64_t nEpoch = lease.nEpoch.load( std::memory_order_relaxed );
		if ( nSlot < SIR_WINDOW_MAX_WINDOWS && m_nEpoch == nEpoch && m_Slots[nSlot].nGeneration == nGeneration &&
		     m_Slots[nSlot].nLeaseCount != 0 ) {
			--m_Slots[nSlot].nLeaseCount;
		}
		lease.nState.store( 0, std::memory_order_release );
	}
}

sirWindowResult SIRWindowSystem::destroyWindow( sirWindowHandle_t hWindow ) noexcept
{
	const sirWindowResult eOwner = requireOwner();
	if ( eOwner != sirWindowResult::SIR_WINDOW_OK ) {
		return eOwner;
	}
	const sirWindowResult eValid = validate( hWindow );
	if ( eValid != sirWindowResult::SIR_WINDOW_OK ) {
		return eValid;
	}
	processReleasedLeases();
	sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	if ( slot.nLeaseCount != 0 ) {
		return sirWindowResult::SIR_WINDOW_BUSY;
	}
	if ( slot.nGeneration == UINT32_MAX ) {
		return sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY;
	}
	const sirWindowResult eResult = m_pDriver->destroyWindow( hWindow.nSlot );
	if ( eResult != sirWindowResult::SIR_WINDOW_OK ) {
		return eResult;
	}
	{
		SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
		pushEvent( hWindow, sirWindowEventKind::SIR_WINDOW_EVENT_DESTROYED, 0, 0 );
		slot.bOccupied = false;
		++slot.nGeneration;
		slot.snapshot = {};
		slot.native = {};
	}
	return sirWindowResult::SIR_WINDOW_OK;
}

void SIRWindowSystem::onDriverEvent( void *pContext, const sirWindowBackendEvent_t& event ) noexcept
{
	SIRWindowSystem *pSystem = static_cast<SIRWindowSystem *>(pContext);
	if ( pSystem->requireOwner() != sirWindowResult::SIR_WINDOW_OK ) {
		// Native display callbacks cannot publish state from a foreign lane.
		// Report the broken backend contract to the owner pump without racing
		// against its snapshot and event-sequence fields.
		pSystem->m_bForeignThreadEvent.store( true, std::memory_order_release );
		return;
	}
	if ( event.nSlot >= SIR_WINDOW_MAX_WINDOWS || !pSystem->m_Slots[event.nSlot].bOccupied ||
	     event.nNativeWindow != pSystem->m_Slots[event.nSlot].native.nNativeWindow ) {
		return;
	}
	sirWindowSlot_t& slot = pSystem->m_Slots[event.nSlot];
	if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_CLOSE_REQUESTED ) {
		static_cast<void>(pSystem->requestClose( slot.snapshot.hWindow ));
		return;
	}
	SIRScopedLock<SIRMutex> guard( pSystem->m_SnapshotMutex );
	if ( slot.snapshot.bCloseRequested &&
	     ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_READY || event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_SHOWN || event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_RESTORED ) ) {
		return;
	}
	if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_READY ) {
		if ( event.nWidth == 0 || event.nHeight == 0 ) {
			return;
		}
		if ( slot.snapshot.nWidth != event.nWidth || slot.snapshot.nHeight != event.nHeight ) {
			++slot.snapshot.nMetricsRevision;
		}
		slot.snapshot.nWidth = event.nWidth;
		slot.snapshot.nHeight = event.nHeight;
		slot.snapshot.bVisible = true;
		slot.snapshot.bReady = true;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_RESIZED ) {
		if ( slot.snapshot.nWidth == event.nWidth && slot.snapshot.nHeight == event.nHeight ) {
			return;
		}
		slot.snapshot.nWidth = event.nWidth;
		slot.snapshot.nHeight = event.nHeight;
		slot.snapshot.bReady = event.nWidth != 0 && event.nHeight != 0 && slot.snapshot.bVisible && !slot.snapshot.bMinimized &&
		                       !slot.snapshot.bCloseRequested;
		++slot.snapshot.nMetricsRevision;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_FOCUS_GAINED ) {
		slot.snapshot.bFocused = true;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_FOCUS_LOST ) {
		slot.snapshot.bFocused = false;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_DPI_CHANGED ) {
		if ( event.nWidth == 0 || slot.snapshot.nDpi == event.nWidth ) {
			return;
		}
		slot.snapshot.nDpi = event.nWidth;
		++slot.snapshot.nMetricsRevision;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_SHOWN ) {
		slot.snapshot.bVisible = true;
		slot.snapshot.bReady =
		    !slot.snapshot.bMinimized && !slot.snapshot.bCloseRequested && slot.snapshot.nWidth != 0 && slot.snapshot.nHeight != 0;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_HIDDEN ) {
		slot.snapshot.bVisible = false;
		slot.snapshot.bReady = false;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_MINIMIZED ) {
		slot.snapshot.bMinimized = true;
		slot.snapshot.bReady = false;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_RESTORED ) {
		slot.snapshot.bMinimized = false;
		slot.snapshot.bReady =
		    slot.snapshot.bVisible && !slot.snapshot.bCloseRequested && slot.snapshot.nWidth != 0 && slot.snapshot.nHeight != 0;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_DISPLAY_CHANGED ) {
		// Native display topology changes invalidate presentation constraints,
		// including present modes and color formats with unchanged dimensions.
		if ( event.nWidth != 0 && event.nHeight != 0 ) {
			slot.snapshot.nWidth = event.nWidth;
			slot.snapshot.nHeight = event.nHeight;
		}
		++slot.snapshot.nMetricsRevision;
	} else if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_LOST ) {
		// The OS handle is no longer borrowable even while a GPU lease
		// continues to pin its retirement bookkeeping. Invalidate the
		// published native identity and presentation revision immediately.
		slot.native = {};
		slot.snapshot.nWindowGeneration = 0;
		++slot.snapshot.nMetricsRevision;
		slot.snapshot.bReady = false;
		slot.snapshot.bVisible = false;
		slot.snapshot.bFocused = false;
		slot.snapshot.bCloseRequested = true;
	} else {
		return;
	}
	pSystem->pushEvent( slot.snapshot.hWindow, event.eKind, event.nWidth, event.nHeight );
}

sirWindowResult SIRWindowSystem::pumpEvents( uint32_t nBudget ) noexcept
{
	const sirWindowResult eOwner = requireOwner();
	if ( eOwner != sirWindowResult::SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( nBudget == 0 ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	processReleasedLeases();
	processRequests();
	const sirWindowResult eResult = m_pDriver->pumpEvents( nBudget, onDriverEvent, this );
	if ( m_bForeignThreadEvent.exchange( false, std::memory_order_acq_rel ) ) {
		return sirWindowResult::SIR_WINDOW_WRONG_THREAD;
	}
	return eResult;
}

sirWindowResult SIRWindowSystem::readEvents( sirWindowEventRead_t *pRead ) noexcept
{
	const sirWindowResult eOwner = requireOwner();
	if ( eOwner != sirWindowResult::SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( pRead == nullptr || ( pRead->nCapacity != 0 && pRead->pEvents == nullptr ) || pRead->nAfterSequence > m_nEventSequence ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	pRead->nCount = 0;
	const uint64_t nOldest = m_nEventSequence > SIR_WINDOW_EVENT_CAPACITY ? m_nEventSequence - SIR_WINDOW_EVENT_CAPACITY + 1 : 1;
	const uint64_t nFirst = pRead->nAfterSequence < nOldest - 1 ? nOldest : pRead->nAfterSequence + 1;
	pRead->bLost = pRead->nAfterSequence < nOldest - 1;
	for ( uint64_t nSequence = nFirst; nSequence <= m_nEventSequence && pRead->nCount < pRead->nCapacity; ++nSequence ) {
		pRead->pEvents[pRead->nCount++] = m_Events[( nSequence - 1 ) % SIR_WINDOW_EVENT_CAPACITY];
	}
	if ( pRead->nCount != 0 ) {
		pRead->nAfterSequence = pRead->pEvents[pRead->nCount - 1].nSequence;
	}
	return pRead->bLost ? sirWindowResult::SIR_WINDOW_EVENTS_LOST : sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowSystem::querySnapshot( sirWindowHandle_t hWindow, sirWindowSnapshot_t *pSnapshot ) const noexcept
{
	SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
	if ( !m_bInitialized ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	if ( pSnapshot == nullptr ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	const sirWindowResult eValid = validate( hWindow );
	if ( eValid != sirWindowResult::SIR_WINDOW_OK ) {
		return eValid;
	}
	*pSnapshot = m_Slots[hWindow.nSlot].snapshot;
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowSystem::acquireNativeLease( sirWindowHandle_t hWindow, sirWindowNativeLease_t *pLease,
                                                       sirWindowNativeView_t *pView ) noexcept
{
	const sirWindowResult eOwner = requireOwner();
	if ( eOwner != sirWindowResult::SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( pLease == nullptr || pView == nullptr ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	const sirWindowResult eValid = validate( hWindow );
	if ( eValid != sirWindowResult::SIR_WINDOW_OK ) {
		return eValid;
	}
	sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	if ( !slot.snapshot.bReady || slot.snapshot.bCloseRequested ) {
		return sirWindowResult::SIR_WINDOW_NOT_READY;
	}
	processReleasedLeases();
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_LEASES; ++i ) {
		sirWindowLeaseEntry_t& lease = m_Leases[i];
		if ( lease.nState.load( std::memory_order_acquire ) != 0 ) {
			continue;
		}
		if ( m_nLeaseSerial == UINT64_MAX ) {
			return sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY;
		}
		const uint64_t nToken = ++m_nLeaseSerial;
		lease.nToken.store( nToken, std::memory_order_relaxed );
		lease.nEpoch.store( m_nEpoch, std::memory_order_relaxed );
		lease.nSlot.store( hWindow.nSlot, std::memory_order_relaxed );
		lease.nGeneration.store( hWindow.nGeneration, std::memory_order_relaxed );
		lease.nState.store( 1, std::memory_order_release );
		++slot.nLeaseCount;
		*pLease = { nToken, m_nEpoch, hWindow.nSlot, hWindow.nGeneration };
		*pView = slot.native;
		return sirWindowResult::SIR_WINDOW_OK;
	}
	return sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY;
}

sirWindowResult SIRWindowSystem::releaseNativeLease( sirWindowNativeLease_t hLease ) noexcept
{
	if ( hLease.nToken == 0 || hLease.nGeneration == 0 || hLease.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_LEASES; ++i ) {
		sirWindowLeaseEntry_t& lease = m_Leases[i];
		if ( lease.nState.load( std::memory_order_acquire ) != 1 || lease.nToken.load( std::memory_order_relaxed ) != hLease.nToken ||
		     lease.nEpoch.load( std::memory_order_relaxed ) != hLease.nSystemEpoch ||
		     lease.nSlot.load( std::memory_order_relaxed ) != hLease.nSlot ||
		     lease.nGeneration.load( std::memory_order_relaxed ) != hLease.nGeneration ) {
			continue;
		}
		uint8_t nExpected = 1;
		return lease.nState.compare_exchange_strong( nExpected, 2, std::memory_order_release, std::memory_order_relaxed )
		           ? sirWindowResult::SIR_WINDOW_OK
				   : sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
	}
	return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
}

sirWindowResult SIRWindowSystem::acquireNativeLease( sirWindowHandle_t hWindow, SIRNativeWindowLease& lease ) noexcept
{
 if ( lease.isValid() ) { return sirWindowResult::SIR_WINDOW_BUSY; }
 sirWindowNativeLease_t token{};
 sirWindowNativeView_t view{};
 const sirWindowResult eResult = acquireNativeLease( hWindow, &token, &view );
 if ( eResult == sirWindowResult::SIR_WINDOW_OK ) { lease.m_pOwner = this; lease.m_Lease = token; lease.m_View = view; }
 return eResult;
}
