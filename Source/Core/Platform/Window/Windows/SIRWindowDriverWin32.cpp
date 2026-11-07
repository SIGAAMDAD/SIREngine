#include <Core/Platform/Window/Windows/SIRWindowDriverWin32.h>

static constexpr wchar_t s_szWindowClass[] = L"SIREnginePlatformWindow";

SIRWindowDriverWin32::~SIRWindowDriverWin32( void ) noexcept
{
	shutdown();
}

sirWindowResult_t SIRWindowDriverWin32::initialize( void ) noexcept
{
	if ( m_bRegistered ) {
		return SIR_WINDOW_BUSY;
	}
	WNDCLASSW definition = {};
	definition.lpfnWndProc = windowProc;
	definition.hInstance = GetModuleHandleW( nullptr );
	definition.hCursor = LoadCursorW( nullptr, IDC_ARROW );
	definition.lpszClassName = s_szWindowClass;
	if ( RegisterClassW( &definition ) == 0 ) {
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	m_bRegistered = true;
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverWin32::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                      sirWindowNativeView_t *pView ) noexcept
{
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || pView == nullptr || desc.pszTitle == nullptr || desc.nWidth == 0 || desc.nHeight == 0 ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( !m_bRegistered ) {
		return SIR_WINDOW_STOPPED;
	}
	sirWin32Slot_t& slot = m_Slots[nSlot];
	if ( slot.hWindow != nullptr ) {
		return SIR_WINDOW_BUSY;
	}
	wchar_t szTitle[128] = {};
	const int nCount = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, desc.pszTitle, -1, szTitle, 128 );
	if ( nCount == 0 ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	slot.pDriver = this;
	slot.nIndex = nSlot;
	slot.bDestroying = false;
	slot.bLost = false;
	m_bClosePending[nSlot] = false;
	m_bLostPending[nSlot] = false;
	m_nLostNativeWindow[nSlot] = 0;
	const HWND hWindow = CreateWindowExW( 0, s_szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	                                      ( int )desc.nWidth, ( int )desc.nHeight, nullptr, nullptr, GetModuleHandleW( nullptr ), &slot );
	if ( hWindow == nullptr ) {
		slot = {};
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	slot.hWindow = hWindow;
	*pView = { SIR_WINDOW_PLATFORM_WIN32, ( uintptr_t )hWindow, ( uintptr_t )GetModuleHandleW( nullptr ), 0 };
	ShowWindow( hWindow, SW_SHOWNORMAL );
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverWin32::destroyWindow( uint32_t nSlot ) noexcept
{
	if ( !m_bRegistered ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	sirWin32Slot_t& slot = m_Slots[nSlot];
	if ( slot.hWindow == nullptr && !slot.bLost ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	if ( slot.hWindow != nullptr ) {
		slot.bDestroying = true;
		if ( !DestroyWindow( slot.hWindow ) ) {
			slot.bDestroying = false;
			return SIR_WINDOW_NATIVE_FAILURE;
		}
	}
	slot = {};
	m_bClosePending[nSlot] = false;
	m_bLostPending[nSlot] = false;
	m_nLostNativeWindow[nSlot] = 0;
	return SIR_WINDOW_OK;
}

void SIRWindowDriverWin32::queueEvent( uint32_t nSlot, sirWindowEventKind_t eKind, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( m_nEventCount == SIR_WINDOW_EVENT_CAPACITY ) {
		m_bOverflow = true;
		return;
	}
	m_Events[m_nEventWrite] = { nSlot, ( uintptr_t )m_Slots[nSlot].hWindow, eKind, nWidth, nHeight };
	m_nEventWrite = ( m_nEventWrite + 1 ) % SIR_WINDOW_EVENT_CAPACITY;
	++m_nEventCount;
}

LRESULT CALLBACK SIRWindowDriverWin32::windowProc( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam )
{
	if ( nMessage == WM_NCCREATE ) {
		const CREATESTRUCTW *pCreate = ( const CREATESTRUCTW * )nLParam;
		sirWin32Slot_t *pSlot = ( sirWin32Slot_t * )pCreate->lpCreateParams;
		pSlot->hWindow = hWindow;
		SetWindowLongPtrW( hWindow, GWLP_USERDATA, ( LONG_PTR )pSlot );
	}
	sirWin32Slot_t *pSlot = ( sirWin32Slot_t * )GetWindowLongPtrW( hWindow, GWLP_USERDATA );
	if ( pSlot != nullptr && pSlot->pDriver != nullptr ) {
		SIRWindowDriverWin32 *pDriver = pSlot->pDriver;
		if ( nMessage == WM_CLOSE ) {
			pDriver->m_bClosePending[pSlot->nIndex] = true;
			return 0;
		}
		if ( nMessage == WM_SIZE ) {
			pDriver->queueEvent( pSlot->nIndex, SIR_WINDOW_EVENT_RESIZED, LOWORD( nLParam ), HIWORD( nLParam ) );
		} else if ( nMessage == WM_SETFOCUS ) {
			pDriver->queueEvent( pSlot->nIndex, SIR_WINDOW_EVENT_FOCUS_GAINED, 0, 0 );
		} else if ( nMessage == WM_KILLFOCUS ) {
			pDriver->queueEvent( pSlot->nIndex, SIR_WINDOW_EVENT_FOCUS_LOST, 0, 0 );
		} else if ( nMessage == WM_DESTROY && !pSlot->bDestroying ) {
			pDriver->m_nLostNativeWindow[pSlot->nIndex] = ( uintptr_t )hWindow;
			pDriver->m_bLostPending[pSlot->nIndex] = true;
			pSlot->bLost = true;
			pSlot->hWindow = nullptr;
		} else if ( nMessage == WM_NCDESTROY ) {
			SetWindowLongPtrW( hWindow, GWLP_USERDATA, 0 );
		}
	}
	return DefWindowProcW( hWindow, nMessage, nWParam, nLParam );
}

sirWindowResult_t SIRWindowDriverWin32::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	if ( !m_bRegistered ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nBudget == 0 || pCallback == nullptr ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	MSG message = {};
	uint32_t nProcessed = 0;
	while ( nProcessed < nBudget && PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) ) {
		if ( message.message == WM_QUIT ) {
			for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
				if ( m_Slots[i].hWindow != nullptr ) {
					m_bClosePending[i] = true;
				}
			}
		} else {
			TranslateMessage( &message );
			DispatchMessageW( &message );
		}
		++nProcessed;
	}
	while ( m_nEventCount != 0 ) {
		pCallback( pContext, m_Events[m_nEventRead] );
		m_nEventRead = ( m_nEventRead + 1 ) % SIR_WINDOW_EVENT_CAPACITY;
		--m_nEventCount;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_bLostPending[i] ) {
			m_bLostPending[i] = false;
			const sirWindowBackendEvent_t event = { i, m_nLostNativeWindow[i], SIR_WINDOW_EVENT_LOST, 0, 0 };
			pCallback( pContext, event );
		}
		if ( m_bClosePending[i] ) {
			m_bClosePending[i] = false;
			const sirWindowBackendEvent_t event = { i, ( uintptr_t )m_Slots[i].hWindow, SIR_WINDOW_EVENT_CLOSE_REQUESTED, 0, 0 };
			pCallback( pContext, event );
		}
	}
	const bool bLost = m_bOverflow;
	m_bOverflow = false;
	return bLost ? SIR_WINDOW_EVENTS_LOST : SIR_WINDOW_OK;
}

void SIRWindowDriverWin32::shutdown( void ) noexcept
{
	if ( !m_bRegistered ) {
		return;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_Slots[i].hWindow != nullptr ) {
			m_Slots[i].bDestroying = true;
			DestroyWindow( m_Slots[i].hWindow );
		}
		m_Slots[i] = {};
	}
	UnregisterClassW( s_szWindowClass, GetModuleHandleW( nullptr ) );
	m_bRegistered = false;
	m_nEventRead = 0;
	m_nEventWrite = 0;
	m_nEventCount = 0;
	m_bOverflow = false;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_bClosePending[i] = false;
		m_bLostPending[i] = false;
		m_nLostNativeWindow[i] = 0;
	}
}
