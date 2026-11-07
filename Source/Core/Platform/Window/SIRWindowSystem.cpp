#include <Core/Platform/Window/SIRWindowSystem.h>

SIRWindowSystem::SIRWindowSystem( SIRWindowDriver& driver ) noexcept
    : m_pDriver( &driver ), m_nOwnerThreadId( 0 ), m_nEpoch( 0 ), m_nEventSequence( 0 ), m_nLeaseSerial( 0 ), m_nNativeGeneration( 0 ),
      m_bInitialized( false )
{
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_Slots[i].nGeneration = 1;
	}
}

sirSystemResult_t SIRWindowSystem::initialize( void ) noexcept
{
	if ( m_bInitialized || m_nEpoch == UINT64_MAX || m_pDriver->initialize() != SIR_WINDOW_OK ) {
		return SIR_SYSTEM_ERROR;
	}
	m_nOwnerThreadId = SIRPlatformThreadControl::getCurrentThreadId();
	{
		SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
		++m_nEpoch;
		m_nEventSequence = 0;
		m_bInitialized = true;
	}
	return SIR_SYSTEM_OK;
}

sirSystemResult_t SIRWindowSystem::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_SYSTEM_OK;
	}
	if ( requireOwner() != SIR_WINDOW_OK ) {
		return SIR_SYSTEM_ERROR;
	}
	processReleasedLeases();
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_Slots[i].bOccupied ) {
			return SIR_SYSTEM_BUSY;
		}
	}
	m_pDriver->shutdown();
	{
		SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
		m_bInitialized = false;
	}
	return SIR_SYSTEM_OK;
}

sirWindowResult_t SIRWindowSystem::requireOwner( void ) const noexcept
{
	if ( !m_bInitialized ) {
		return SIR_WINDOW_STOPPED;
	}
	return m_nOwnerThreadId == SIRPlatformThreadControl::getCurrentThreadId() ? SIR_WINDOW_OK : SIR_WINDOW_WRONG_THREAD;
}

sirWindowResult_t SIRWindowSystem::validate( sirWindowHandle_t hWindow ) const noexcept
{
	if ( hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS || hWindow.nGeneration == 0 || hWindow.nSystemEpoch != m_nEpoch ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	const sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	return slot.bOccupied && slot.nGeneration == hWindow.nGeneration ? SIR_WINDOW_OK : SIR_WINDOW_INVALID_HANDLE;
}

void SIRWindowSystem::pushEvent( sirWindowHandle_t hWindow, sirWindowEventKind_t eKind, uint32_t nWidth, uint32_t nHeight ) noexcept
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

sirWindowResult_t SIRWindowSystem::createWindow( const sirWindowCreateDesc_t& desc, sirWindowHandle_t *pHandle ) noexcept
{
	const sirWindowResult_t eOwner = requireOwner();
	if ( eOwner != SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( pHandle == nullptr || desc.pszTitle == nullptr || desc.nWidth == 0 || desc.nHeight == 0 ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		sirWindowSlot_t& slot = m_Slots[i];
		if ( slot.bOccupied ) {
			continue;
		}
		sirWindowNativeView_t view = {};
		const sirWindowResult_t eResult = m_pDriver->createWindow( desc, i, &view );
		if ( eResult != SIR_WINDOW_OK ) {
			return eResult;
		}
		if ( view.nNativeWindow == 0 || view.ePlatform == SIR_WINDOW_PLATFORM_HEADLESS ) {
			( void )m_pDriver->destroyWindow( i );
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		if ( m_nNativeGeneration == UINT64_MAX ) {
			( void )m_pDriver->destroyWindow( i );
			return SIR_WINDOW_OUT_OF_CAPACITY;
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
			slot.snapshot.bReady = view.ePlatform != SIR_WINDOW_PLATFORM_WAYLAND;
			slot.snapshot.bVisible = view.ePlatform != SIR_WINDOW_PLATFORM_WAYLAND;
			slot.snapshot.bMinimized = false;
			slot.bOccupied = true;
			*pHandle = slot.snapshot.hWindow;
		}
		pushEvent( *pHandle, SIR_WINDOW_EVENT_CREATED, desc.nWidth, desc.nHeight );
		if ( view.ePlatform != SIR_WINDOW_PLATFORM_WAYLAND ) {
			pushEvent( *pHandle, SIR_WINDOW_EVENT_READY, desc.nWidth, desc.nHeight );
		}
		return SIR_WINDOW_OK;
	}
	return SIR_WINDOW_OUT_OF_CAPACITY;
}

sirWindowResult_t SIRWindowSystem::requestClose( sirWindowHandle_t hWindow ) noexcept
{
	const sirWindowResult_t eOwner = requireOwner();
	if ( eOwner != SIR_WINDOW_OK ) {
		return eOwner;
	}
	const sirWindowResult_t eValid = validate( hWindow );
	if ( eValid != SIR_WINDOW_OK ) {
		return eValid;
	}
	SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
	sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	if ( !slot.snapshot.bCloseRequested ) {
		slot.snapshot.bCloseRequested = true;
		pushEvent( hWindow, SIR_WINDOW_EVENT_CLOSE_REQUESTED, slot.snapshot.nWidth, slot.snapshot.nHeight );
	}
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowSystem::mutateWindow( sirWindowHandle_t hWindow, const sirWindowMutation_t& mutation ) noexcept
{
	const sirWindowResult_t eOwner = requireOwner();
	if ( eOwner != SIR_WINDOW_OK ) {
		return eOwner;
	}
	const sirWindowResult_t eValid = validate( hWindow );
	if ( eValid != SIR_WINDOW_OK ) {
		return eValid;
	}
	if ( mutation.eKind < SIR_WINDOW_MUTATION_TITLE || mutation.eKind > SIR_WINDOW_MUTATION_MAXIMIZE ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( ( mutation.eKind == SIR_WINDOW_MUTATION_TITLE && mutation.pszTitle == nullptr ) ||
	     ( mutation.eKind == SIR_WINDOW_MUTATION_RESIZE && ( mutation.nWidth == 0 || mutation.nHeight == 0 ) ) ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	return m_pDriver->mutateWindow( hWindow.nSlot, mutation );
}

void SIRWindowSystem::processReleasedLeases( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_LEASES; ++i ) {
		sirWindowLeaseEntry_t& lease = m_Leases[i];
		if ( lease.nState.load( SIR_MEMORY_ORDER_ACQUIRE ) != 2 ) {
			continue;
		}
		const uint32_t nSlot = lease.nSlot.load( SIR_MEMORY_ORDER_RELAXED );
		const uint32_t nGeneration = lease.nGeneration.load( SIR_MEMORY_ORDER_RELAXED );
		const uint64_t nEpoch = lease.nEpoch.load( SIR_MEMORY_ORDER_RELAXED );
		if ( nSlot < SIR_WINDOW_MAX_WINDOWS && m_nEpoch == nEpoch && m_Slots[nSlot].nGeneration == nGeneration &&
		     m_Slots[nSlot].nLeaseCount != 0 ) {
			--m_Slots[nSlot].nLeaseCount;
		}
		lease.nState.store( 0, SIR_MEMORY_ORDER_RELEASE );
	}
}

sirWindowResult_t SIRWindowSystem::destroyWindow( sirWindowHandle_t hWindow ) noexcept
{
	const sirWindowResult_t eOwner = requireOwner();
	if ( eOwner != SIR_WINDOW_OK ) {
		return eOwner;
	}
	const sirWindowResult_t eValid = validate( hWindow );
	if ( eValid != SIR_WINDOW_OK ) {
		return eValid;
	}
	processReleasedLeases();
	sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	if ( slot.nLeaseCount != 0 ) {
		return SIR_WINDOW_BUSY;
	}
	if ( slot.nGeneration == UINT32_MAX ) {
		return SIR_WINDOW_OUT_OF_CAPACITY;
	}
	const sirWindowResult_t eResult = m_pDriver->destroyWindow( hWindow.nSlot );
	if ( eResult != SIR_WINDOW_OK ) {
		return eResult;
	}
	{
		SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
		pushEvent( hWindow, SIR_WINDOW_EVENT_DESTROYED, 0, 0 );
		slot.bOccupied = false;
		++slot.nGeneration;
		slot.snapshot = {};
		slot.native = {};
	}
	return SIR_WINDOW_OK;
}

void SIRWindowSystem::onDriverEvent( void *pContext, const sirWindowBackendEvent_t& event ) noexcept
{
	SIRWindowSystem *pSystem = ( SIRWindowSystem * )pContext;
	if ( event.nSlot >= SIR_WINDOW_MAX_WINDOWS || !pSystem->m_Slots[event.nSlot].bOccupied ||
	     event.nNativeWindow != pSystem->m_Slots[event.nSlot].native.nNativeWindow ) {
		return;
	}
	sirWindowSlot_t& slot = pSystem->m_Slots[event.nSlot];
	if ( event.eKind == SIR_WINDOW_EVENT_CLOSE_REQUESTED ) {
		( void )pSystem->requestClose( slot.snapshot.hWindow );
		return;
	}
	SIRScopedLock<SIRMutex> guard( pSystem->m_SnapshotMutex );
	if ( event.eKind == SIR_WINDOW_EVENT_READY ) {
		if ( event.nWidth == 0 || event.nHeight == 0 || slot.snapshot.bCloseRequested ) {
			return;
		}
		if ( slot.snapshot.nWidth != event.nWidth || slot.snapshot.nHeight != event.nHeight ) {
			++slot.snapshot.nMetricsRevision;
		}
		slot.snapshot.nWidth = event.nWidth;
		slot.snapshot.nHeight = event.nHeight;
		slot.snapshot.bVisible = true;
		slot.snapshot.bReady = true;
	} else if ( event.eKind == SIR_WINDOW_EVENT_RESIZED ) {
		if ( slot.snapshot.nWidth == event.nWidth && slot.snapshot.nHeight == event.nHeight ) {
			return;
		}
		slot.snapshot.nWidth = event.nWidth;
		slot.snapshot.nHeight = event.nHeight;
		slot.snapshot.bReady = event.nWidth != 0 && event.nHeight != 0 && slot.snapshot.bVisible && !slot.snapshot.bMinimized;
		++slot.snapshot.nMetricsRevision;
	} else if ( event.eKind == SIR_WINDOW_EVENT_FOCUS_GAINED ) {
		slot.snapshot.bFocused = true;
	} else if ( event.eKind == SIR_WINDOW_EVENT_FOCUS_LOST ) {
		slot.snapshot.bFocused = false;
	} else if ( event.eKind == SIR_WINDOW_EVENT_DPI_CHANGED ) {
		if ( event.nWidth == 0 || slot.snapshot.nDpi == event.nWidth ) {
			return;
		}
		slot.snapshot.nDpi = event.nWidth;
		++slot.snapshot.nMetricsRevision;
	} else if ( event.eKind == SIR_WINDOW_EVENT_SHOWN ) {
		slot.snapshot.bVisible = true;
		slot.snapshot.bReady = !slot.snapshot.bMinimized && slot.snapshot.nWidth != 0 && slot.snapshot.nHeight != 0;
	} else if ( event.eKind == SIR_WINDOW_EVENT_HIDDEN ) {
		slot.snapshot.bVisible = false;
		slot.snapshot.bReady = false;
	} else if ( event.eKind == SIR_WINDOW_EVENT_MINIMIZED ) {
		slot.snapshot.bMinimized = true;
		slot.snapshot.bReady = false;
	} else if ( event.eKind == SIR_WINDOW_EVENT_RESTORED ) {
		slot.snapshot.bMinimized = false;
		slot.snapshot.bReady = slot.snapshot.bVisible && slot.snapshot.nWidth != 0 && slot.snapshot.nHeight != 0;
	} else if ( event.eKind == SIR_WINDOW_EVENT_LOST ) {
		slot.snapshot.bReady = false;
		slot.snapshot.bCloseRequested = true;
	} else {
		return;
	}
	pSystem->pushEvent( slot.snapshot.hWindow, event.eKind, event.nWidth, event.nHeight );
}

sirWindowResult_t SIRWindowSystem::pumpEvents( uint32_t nBudget ) noexcept
{
	const sirWindowResult_t eOwner = requireOwner();
	if ( eOwner != SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( nBudget == 0 ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	processReleasedLeases();
	return m_pDriver->pumpEvents( nBudget, onDriverEvent, this );
}

sirWindowResult_t SIRWindowSystem::readEvents( sirWindowEventRead_t *pRead ) noexcept
{
	const sirWindowResult_t eOwner = requireOwner();
	if ( eOwner != SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( pRead == nullptr || ( pRead->nCapacity != 0 && pRead->pEvents == nullptr ) || pRead->nAfterSequence > m_nEventSequence ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	pRead->nCount = 0;
	const uint64_t nOldest = m_nEventSequence > SIR_WINDOW_EVENT_CAPACITY ? m_nEventSequence - SIR_WINDOW_EVENT_CAPACITY + 1 : 1;
	const uint64_t nFirst = pRead->nAfterSequence < nOldest - 1 ? nOldest : pRead->nAfterSequence + 1;
	pRead->bLost = pRead->nAfterSequence < nOldest - 1;
	for ( uint64_t nSequence = nFirst; nSequence <= m_nEventSequence && pRead->nCount < pRead->nCapacity; ++nSequence ) {
		pRead->pEvents[pRead->nCount++] = m_Events[( nSequence - 1 ) % SIR_WINDOW_EVENT_CAPACITY];
	}
	return pRead->bLost ? SIR_WINDOW_EVENTS_LOST : SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowSystem::querySnapshot( sirWindowHandle_t hWindow, sirWindowSnapshot_t *pSnapshot ) const noexcept
{
	SIRScopedLock<SIRMutex> guard( m_SnapshotMutex );
	if ( !m_bInitialized ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( pSnapshot == nullptr ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	const sirWindowResult_t eValid = validate( hWindow );
	if ( eValid != SIR_WINDOW_OK ) {
		return eValid;
	}
	*pSnapshot = m_Slots[hWindow.nSlot].snapshot;
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowSystem::acquireNativeLease( sirWindowHandle_t hWindow, sirWindowNativeLease_t *pLease,
                                                       sirWindowNativeView_t *pView ) noexcept
{
	const sirWindowResult_t eOwner = requireOwner();
	if ( eOwner != SIR_WINDOW_OK ) {
		return eOwner;
	}
	if ( pLease == nullptr || pView == nullptr ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	const sirWindowResult_t eValid = validate( hWindow );
	if ( eValid != SIR_WINDOW_OK ) {
		return eValid;
	}
	sirWindowSlot_t& slot = m_Slots[hWindow.nSlot];
	if ( !slot.snapshot.bReady || slot.snapshot.bCloseRequested ) {
		return SIR_WINDOW_NOT_READY;
	}
	processReleasedLeases();
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_LEASES; ++i ) {
		sirWindowLeaseEntry_t& lease = m_Leases[i];
		if ( lease.nState.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ) {
			continue;
		}
		if ( m_nLeaseSerial == UINT64_MAX ) {
			return SIR_WINDOW_OUT_OF_CAPACITY;
		}
		const uint64_t nToken = ++m_nLeaseSerial;
		lease.nToken.store( nToken, SIR_MEMORY_ORDER_RELAXED );
		lease.nEpoch.store( m_nEpoch, SIR_MEMORY_ORDER_RELAXED );
		lease.nSlot.store( hWindow.nSlot, SIR_MEMORY_ORDER_RELAXED );
		lease.nGeneration.store( hWindow.nGeneration, SIR_MEMORY_ORDER_RELAXED );
		lease.nState.store( 1, SIR_MEMORY_ORDER_RELEASE );
		++slot.nLeaseCount;
		*pLease = { nToken, m_nEpoch, hWindow.nSlot, hWindow.nGeneration };
		*pView = slot.native;
		return SIR_WINDOW_OK;
	}
	return SIR_WINDOW_OUT_OF_CAPACITY;
}

sirWindowResult_t SIRWindowSystem::releaseNativeLease( sirWindowNativeLease_t hLease ) noexcept
{
	if ( hLease.nToken == 0 || hLease.nGeneration == 0 || hLease.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_LEASES; ++i ) {
		sirWindowLeaseEntry_t& lease = m_Leases[i];
		if ( lease.nState.load( SIR_MEMORY_ORDER_ACQUIRE ) != 1 || lease.nToken.load( SIR_MEMORY_ORDER_RELAXED ) != hLease.nToken ||
		     lease.nEpoch.load( SIR_MEMORY_ORDER_RELAXED ) != hLease.nSystemEpoch ||
		     lease.nSlot.load( SIR_MEMORY_ORDER_RELAXED ) != hLease.nSlot ||
		     lease.nGeneration.load( SIR_MEMORY_ORDER_RELAXED ) != hLease.nGeneration ) {
			continue;
		}
		uint8_t nExpected = 1;
		return lease.nState.compareExchangeStrong( nExpected, 2, SIR_MEMORY_ORDER_RELEASE, SIR_MEMORY_ORDER_RELAXED )
		           ? SIR_WINDOW_OK
				   : SIR_WINDOW_INVALID_HANDLE;
	}
	return SIR_WINDOW_INVALID_HANDLE;
}
