#include <Core/Platform/Window/SIRWindowSystem.h>

#include <string.h>

SIRWindowSystem::SIRWindowSystem( void ) noexcept
    : m_Mutex(), m_szSlots{}, m_szEvents{}, m_iEventRead( 0 ), m_nEventCount( 0 ), m_nLiveWindows( 0 ), m_nLeaseSerial( 0 ),
      m_pDriver( nullptr ), m_bEventsOverflowed( false ), m_bInitialized( false ), m_bPumping( false )
#if defined( _WIN32 )
      ,
      m_nOwnerThread( 0 )
#else
      ,
      m_hOwnerThread{}
#endif
{
}

SIRWindowSystem::~SIRWindowSystem( void ) noexcept
{
	// A caller must retire graphics leases before destructing the native owner.
	( void )shutdown();
}

bool SIRWindowSystem::onOwnerThread( void ) const noexcept
{
#if defined( _WIN32 )
	return m_nOwnerThread == GetCurrentThreadId();
#else
	return pthread_equal( m_hOwnerThread, pthread_self() ) != 0;
#endif
}

bool SIRWindowSystem::isCurrent( sirWindowHandle_t hWindow ) const noexcept
{
	return hWindow.nSlot < SIR_WINDOW_MAX_WINDOWS && hWindow.nGeneration != 0 &&
	       m_szSlots[hWindow.nSlot].nGeneration == hWindow.nGeneration && m_szSlots[hWindow.nSlot].eState != SIR_WINDOW_FREE &&
	       m_szSlots[hWindow.nSlot].eState != SIR_WINDOW_DESTROYED;
}

void SIRWindowSystem::pushEvent( sirWindowHandle_t hWindow, sirWindowEventType_t eType ) noexcept
{
	if ( m_nEventCount == SIR_WINDOW_EVENT_CAPACITY ) {
		m_bEventsOverflowed = true;
		return;
	}
	const uint32_t iWrite = ( m_iEventRead + m_nEventCount ) % SIR_WINDOW_EVENT_CAPACITY;
	m_szEvents[iWrite] = { hWindow, eType, m_szSlots[hWindow.nSlot].nRevision };
	++m_nEventCount;
}

sirWindowResult_t SIRWindowSystem::initialize( SIRWindowDriver& driver ) noexcept
{
	if ( m_bInitialized ) {
		return SIR_WINDOW_BUSY;
	}
#if defined( _WIN32 )
	m_nOwnerThread = GetCurrentThreadId();
#else
	m_hOwnerThread = pthread_self();
#endif
	m_iEventRead = 0;
	m_nEventCount = 0;
	m_bEventsOverflowed = false;
	m_pDriver = &driver;
	if ( !driver.initialize( *this ) ) {
		driver.shutdown();
		m_pDriver = nullptr;
		return SIR_WINDOW_PLATFORM_ERROR;
	}
	m_bInitialized = true;
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowSystem::create( const sirWindowDesc_t& desc, sirWindowHandle_t *pWindow ) noexcept
{
	if ( !m_bInitialized || pWindow == nullptr || desc.pszTitle == nullptr ) {
		return SIR_WINDOW_INVALID;
	}
	if ( desc.nWidth == 0 || desc.nHeight == 0 ) {
		return SIR_WINDOW_INVALID;
	}
	if ( !onOwnerThread() || m_bPumping ) {
		return SIR_WINDOW_WRONG_THREAD;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_WINDOW_MAX_WINDOWS; ++iSlot ) {
		m_Mutex.lock();
		sirWindowSlot_t& slot = m_szSlots[iSlot];
		if ( slot.eState != SIR_WINDOW_FREE && slot.eState != SIR_WINDOW_DESTROYED ) {
			m_Mutex.unlock();
			continue;
		}
		const uint32_t nNextGeneration = slot.nGeneration + 1 == 0 ? 1 : slot.nGeneration + 1;
		const sirWindowHandle_t hWindow = { iSlot, nNextGeneration };
		slot = {};
		slot.nGeneration = nNextGeneration;
		slot.eState = SIR_WINDOW_CREATING;
		slot.nWidth = desc.nWidth;
		slot.nHeight = desc.nHeight;
		slot.bVisible = desc.bVisible;
		m_Mutex.unlock();

		// Platform create may synchronously dispatch OS messages. Keep the
		// registry mutex unlocked while the native window is being created.
		sirWindowNativeView_t view = {};
		const bool bCreated = m_pDriver->create( hWindow, desc, &view );
		m_Mutex.lock();
		if ( !bCreated ) {
			slot.eState = SIR_WINDOW_DESTROYED;
			m_Mutex.unlock();
			return SIR_WINDOW_PLATFORM_ERROR;
		}
		view.nWindowGeneration = nNextGeneration;
		slot.view = view;
		slot.eState = SIR_WINDOW_READY;
		++slot.nRevision;
		++m_nLiveWindows;
		pushEvent( hWindow, SIR_WINDOW_EVENT_CREATED );
		*pWindow = hWindow;
		m_Mutex.unlock();
		return SIR_WINDOW_OK;
	}
	return SIR_WINDOW_NO_CAPACITY;
}

sirWindowResult_t SIRWindowSystem::requestClose( sirWindowHandle_t hWindow ) noexcept
{
	if ( !m_bInitialized || !onOwnerThread() ) {
		return SIR_WINDOW_WRONG_THREAD;
	}
	m_Mutex.lock();
	if ( !isCurrent( hWindow ) ) {
		m_Mutex.unlock();
		return SIR_WINDOW_INVALID;
	}
	sirWindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.eState != SIR_WINDOW_READY ) {
		m_Mutex.unlock();
		return SIR_WINDOW_BUSY;
	}
	slot.eState = SIR_WINDOW_CLOSE_REQUESTED;
	++slot.nRevision;
	pushEvent( hWindow, SIR_WINDOW_EVENT_CLOSE );
	m_Mutex.unlock();
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowSystem::retirePending( void ) noexcept
{
	bool bPendingLeases = false;
	for ( uint32_t iSlot = 0; iSlot < SIR_WINDOW_MAX_WINDOWS; ++iSlot ) {
		m_Mutex.lock();
		sirWindowSlot_t& slot = m_szSlots[iSlot];
		if ( slot.eState != SIR_WINDOW_CLOSE_REQUESTED && slot.eState != SIR_WINDOW_LOST ) {
			m_Mutex.unlock();
			continue;
		}
		if ( slot.nBorrowCount != 0 ) {
			bPendingLeases = true;
			m_Mutex.unlock();
			continue;
		}
		const sirWindowHandle_t hWindow = { iSlot, slot.nGeneration };
		const sirWindowNativeView_t view = slot.view;
		slot.eState = SIR_WINDOW_RETIRING;
		m_Mutex.unlock();

		// DestroyWindow may synchronously invoke the owner's native callback.
		// No mutex may be held across that call.
		if ( view.pWindow != nullptr ) {
			m_pDriver->destroy( hWindow, view );
		}
		m_Mutex.lock();
		slot.view = {};
		slot.eState = SIR_WINDOW_DESTROYED;
		++slot.nRevision;
		if ( m_nLiveWindows != 0 ) {
			--m_nLiveWindows;
		}
		pushEvent( hWindow, SIR_WINDOW_EVENT_DESTROYED );
		m_Mutex.unlock();
	}
	return bPendingLeases ? SIR_WINDOW_BUSY : SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowSystem::pump( void ) noexcept
{
	if ( !m_bInitialized || !onOwnerThread() ) {
		return SIR_WINDOW_WRONG_THREAD;
	}
	if ( m_bPumping ) {
		return SIR_WINDOW_BUSY;
	}
	m_bPumping = true;
	const bool bPumped = m_pDriver->pump();
	m_bPumping = false;
	const sirWindowResult_t eRetired = retirePending();
	if ( !bPumped ) {
		return SIR_WINDOW_PLATFORM_ERROR;
	}
	return eRetired;
}

sirWindowResult_t SIRWindowSystem::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_WINDOW_OK;
	}
	if ( !onOwnerThread() ) {
		return SIR_WINDOW_WRONG_THREAD;
	}
	if ( m_bPumping ) {
		return SIR_WINDOW_BUSY;
	}
	m_Mutex.lock();
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		sirWindowSlot_t& slot = m_szSlots[i];
		if ( slot.nBorrowCount != 0 ) {
			m_Mutex.unlock();
			return SIR_WINDOW_BUSY;
		}
		if ( slot.eState == SIR_WINDOW_READY ) {
			slot.eState = SIR_WINDOW_CLOSE_REQUESTED;
			++slot.nRevision;
			pushEvent( { i, slot.nGeneration }, SIR_WINDOW_EVENT_CLOSE );
		}
	}
	m_Mutex.unlock();
	const sirWindowResult_t eRetire = retirePending();
	if ( eRetire != SIR_WINDOW_OK ) {
		return eRetire;
	}
	m_pDriver->shutdown();
	m_pDriver = nullptr;
	m_bInitialized = false;
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowSystem::bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept
{
	if ( !m_bInitialized || !onOwnerThread() ) {
		return SIR_WINDOW_WRONG_THREAD;
	}
	m_Mutex.lock();
	const bool bReady = isCurrent( hWindow ) && m_szSlots[hWindow.nSlot].eState == SIR_WINDOW_READY;
	m_Mutex.unlock();
	if ( !bReady ) {
		return SIR_WINDOW_INVALID;
	}
	return m_pDriver->bindInput( hWindow, input, backend ) ? SIR_WINDOW_OK : SIR_WINDOW_PLATFORM_ERROR;
}

bool SIRWindowSystem::snapshot( sirWindowHandle_t hWindow, sirWindowSnapshot_t *pSnapshot ) const noexcept
{
	if ( pSnapshot == nullptr ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrent( hWindow ) ) {
		m_Mutex.unlock();
		return false;
	}
	const sirWindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	*pSnapshot = { hWindow, slot.eState, slot.nWidth, slot.nHeight, slot.nRevision, slot.bFocused, slot.bVisible };
	m_Mutex.unlock();
	return true;
}

bool SIRWindowSystem::acquireNativeView( sirWindowHandle_t hWindow, sirWindowNativeView_t *pView, sirWindowLease_t *pLease ) noexcept
{
	if ( pView == nullptr || pLease == nullptr ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrent( hWindow ) || m_szSlots[hWindow.nSlot].eState != SIR_WINDOW_READY ) {
		m_Mutex.unlock();
		return false;
	}
	sirWindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.nBorrowCount >= 32 ) {
		m_Mutex.unlock();
		return false;
	}
	uint64_t nToken = ++m_nLeaseSerial;
	if ( nToken == 0 ) {
		nToken = ++m_nLeaseSerial;
	}
	slot.nBorrowTokens[slot.nBorrowCount++] = nToken;
	*pView = slot.view;
	*pLease = { hWindow, nToken };
	m_Mutex.unlock();
	return true;
}

bool SIRWindowSystem::releaseNativeView( sirWindowLease_t hLease ) noexcept
{
	m_Mutex.lock();
	if ( !isCurrent( hLease.hWindow ) || hLease.nToken == 0 ) {
		m_Mutex.unlock();
		return false;
	}
	sirWindowSlot_t& slot = m_szSlots[hLease.hWindow.nSlot];
	for ( uint32_t i = 0; i < slot.nBorrowCount; ++i ) {
		if ( slot.nBorrowTokens[i] == hLease.nToken ) {
			slot.nBorrowTokens[i] = slot.nBorrowTokens[slot.nBorrowCount - 1];
			--slot.nBorrowCount;
			m_Mutex.unlock();
			return true;
		}
	}
	m_Mutex.unlock();
	return false;
}

bool SIRWindowSystem::popEvent( sirWindowEvent_t *pEvent ) noexcept
{
	if ( pEvent == nullptr ) {
		return false;
	}
	m_Mutex.lock();
	if ( m_nEventCount == 0 ) {
		m_Mutex.unlock();
		return false;
	}
	*pEvent = m_szEvents[m_iEventRead];
	m_iEventRead = ( m_iEventRead + 1 ) % SIR_WINDOW_EVENT_CAPACITY;
	--m_nEventCount;
	m_Mutex.unlock();
	return true;
}

bool SIRWindowSystem::hasLiveWindows( void ) const noexcept
{
	m_Mutex.lock();
	const bool bLive = m_nLiveWindows != 0;
	m_Mutex.unlock();
	return bLive;
}

uint32_t SIRWindowSystem::activeWindowCount( void ) const noexcept
{
	m_Mutex.lock();
	const uint32_t nCount = m_nLiveWindows;
	m_Mutex.unlock();
	return nCount;
}

bool SIRWindowSystem::eventOverflowed( void ) const noexcept
{
	m_Mutex.lock();
	const bool bOverflowed = m_bEventsOverflowed;
	m_Mutex.unlock();
	return bOverflowed;
}

bool SIRWindowSystem::notifyResize( sirWindowHandle_t hWindow, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( !m_bInitialized || !onOwnerThread() ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrent( hWindow ) || m_szSlots[hWindow.nSlot].eState != SIR_WINDOW_READY ) {
		m_Mutex.unlock();
		return false;
	}
	sirWindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.nWidth != nWidth || slot.nHeight != nHeight ) {
		slot.nWidth = nWidth;
		slot.nHeight = nHeight;
		++slot.nRevision;
		pushEvent( hWindow, SIR_WINDOW_EVENT_RESIZED );
	}
	m_Mutex.unlock();
	return true;
}

bool SIRWindowSystem::notifyFocus( sirWindowHandle_t hWindow, bool bFocused ) noexcept
{
	if ( !m_bInitialized || !onOwnerThread() ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrent( hWindow ) || m_szSlots[hWindow.nSlot].eState != SIR_WINDOW_READY ) {
		m_Mutex.unlock();
		return false;
	}
	sirWindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.bFocused != bFocused ) {
		slot.bFocused = bFocused;
		++slot.nRevision;
		pushEvent( hWindow, SIR_WINDOW_EVENT_FOCUS );
	}
	m_Mutex.unlock();
	return true;
}

bool SIRWindowSystem::notifyClose( sirWindowHandle_t hWindow ) noexcept
{
	return requestClose( hWindow ) == SIR_WINDOW_OK;
}

bool SIRWindowSystem::notifyLost( sirWindowHandle_t hWindow ) noexcept
{
	if ( !m_bInitialized || !onOwnerThread() ) {
		return false;
	}
	m_Mutex.lock();
	if ( !isCurrent( hWindow ) ) {
		m_Mutex.unlock();
		return false;
	}
	sirWindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.eState != SIR_WINDOW_RETIRING && slot.eState != SIR_WINDOW_LOST ) {
		slot.eState = SIR_WINDOW_LOST;
		++slot.nRevision;
		pushEvent( hWindow, SIR_WINDOW_EVENT_LOST );
	}
	slot.view.pWindow = nullptr;
	m_Mutex.unlock();
	return true;
}

SIRNullWindowDriver::SIRNullWindowDriver( void ) noexcept : m_pSystem( nullptr )
{
}

bool SIRNullWindowDriver::initialize( SIRWindowSystem& system ) noexcept
{
	if ( m_pSystem != nullptr ) {
		return false;
	}
	m_pSystem = &system;
	return true;
}

void SIRNullWindowDriver::shutdown( void ) noexcept
{
	m_pSystem = nullptr;
}

bool SIRNullWindowDriver::create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept
{
	( void )hWindow;
	( void )desc;
	if ( m_pSystem == nullptr || pView == nullptr ) {
		return false;
	}
	*pView = { SIR_WINDOW_PLATFORM_NULL, nullptr, nullptr, 0 };
	return true;
}

void SIRNullWindowDriver::destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept
{
	( void )hWindow;
	( void )view;
}

bool SIRNullWindowDriver::pump( void ) noexcept
{
	return m_pSystem != nullptr;
}

bool SIRNullWindowDriver::bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept
{
	( void )hWindow;
	( void )input;
	( void )backend;
	return false;
}
