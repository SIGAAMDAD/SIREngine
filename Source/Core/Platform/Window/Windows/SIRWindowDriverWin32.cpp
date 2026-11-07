#include <Core/Platform/Window/Windows/SIRWindowDriverWin32.h>

static constexpr wchar_t s_szWindowClass[] = L"SIREnginePlatformWindow";

bool SIRWindowDriverWin32::setMessageSink( sirWindowWin32MessageSink_t pSink, void *pContext ) noexcept
{
	if ( pSink != nullptr && pContext == nullptr ) {
		return false;
	}
	m_pSink = pSink;
	m_pSinkContext = pContext;
	return true;
}

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
	definition.hCursor = LoadCursorW( nullptr, MAKEINTRESOURCEW( 32512 ) );
	definition.lpszClassName = s_szWindowClass;
	// Preserve the original native input window's existing-class behavior.
	const ATOM nClass = RegisterClassW( &definition );
	if ( nClass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS ) {
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	m_bOwnsClass = nClass != 0;
	m_bRegistered = true;
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverWin32::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                      sirWindowNativeView_t *pView ) noexcept
{
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || pView == nullptr || desc.pszTitle == nullptr || desc.nWidth == 0 || desc.nHeight == 0 ||
	     desc.nWidth > 0x7fffffffU || desc.nHeight > 0x7fffffffU ) {
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
	RECT frame = { 0, 0, ( LONG )desc.nWidth, ( LONG )desc.nHeight };
	if ( !AdjustWindowRectEx( &frame, WS_OVERLAPPEDWINDOW, FALSE, 0 ) ) {
		slot = {};
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	const HWND hWindow =
	    CreateWindowExW( 0, s_szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left,
		                 frame.bottom - frame.top, nullptr, nullptr, GetModuleHandleW( nullptr ), &slot );
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

sirWindowResult_t SIRWindowDriverWin32::mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	if ( !m_bRegistered ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || m_Slots[nSlot].hWindow == nullptr ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	const HWND hWindow = m_Slots[nSlot].hWindow;
	switch ( mutation.eKind ) {
	case SIR_WINDOW_MUTATION_TITLE: {
		if ( mutation.pszTitle == nullptr ) {
			return SIR_WINDOW_INVALID_ARGUMENT;
		}
		wchar_t szTitle[128] = {};
		if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, mutation.pszTitle, -1, szTitle, 128 ) == 0 ) {
			return SIR_WINDOW_INVALID_ARGUMENT;
		}
		if ( !SetWindowTextW( hWindow, szTitle ) ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		break;
	}
	case SIR_WINDOW_MUTATION_RESIZE: {
		if ( mutation.nWidth == 0 || mutation.nHeight == 0 || mutation.nWidth > 0x7fffffffU || mutation.nHeight > 0x7fffffffU ) {
			return SIR_WINDOW_INVALID_ARGUMENT;
		}
		RECT frame = { 0, 0, ( LONG )mutation.nWidth, ( LONG )mutation.nHeight };
		const DWORD nStyle = ( DWORD )GetWindowLongPtrW( hWindow, GWL_STYLE );
		const DWORD nExStyle = ( DWORD )GetWindowLongPtrW( hWindow, GWL_EXSTYLE );
		if ( !AdjustWindowRectEx( &frame, nStyle, FALSE, nExStyle ) ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		if ( !SetWindowPos( hWindow, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top,
		                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE ) ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		break;
	}
	case SIR_WINDOW_MUTATION_SHOW:
		ShowWindow( hWindow, SW_SHOW );
		break;
	case SIR_WINDOW_MUTATION_HIDE:
		ShowWindow( hWindow, SW_HIDE );
		break;
	case SIR_WINDOW_MUTATION_MINIMIZE:
		ShowWindow( hWindow, SW_MINIMIZE );
		break;
	case SIR_WINDOW_MUTATION_RESTORE:
		ShowWindow( hWindow, SW_RESTORE );
		break;
	case SIR_WINDOW_MUTATION_MAXIMIZE:
		ShowWindow( hWindow, SW_MAXIMIZE );
		break;
	default:
		return SIR_WINDOW_UNSUPPORTED;
	}
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
		if ( pDriver->m_pSink != nullptr ) {
			const sirWindowWin32Message_t message = { ( uintptr_t )hWindow, nMessage, ( uintptr_t )nWParam, ( intptr_t )nLParam };
			const sirWindowWin32Dispatch_t result = pDriver->m_pSink( message, pDriver->m_pSinkContext );
			if ( result.bHandled ) {
				return ( LRESULT )result.nResult;
			}
		}
		if ( nMessage == WM_CLOSE ) {
			pDriver->m_bClosePending[pSlot->nIndex] = true;
			return 0;
		}
		if ( nMessage == WM_SHOWWINDOW ) {
			pDriver->queueEvent( pSlot->nIndex, nWParam ? SIR_WINDOW_EVENT_SHOWN : SIR_WINDOW_EVENT_HIDDEN, 0, 0 );
		} else if ( nMessage == WM_SIZE ) {
			if ( nWParam == SIZE_MINIMIZED ) {
				pDriver->queueEvent( pSlot->nIndex, SIR_WINDOW_EVENT_MINIMIZED, 0, 0 );
			} else {
				pDriver->queueEvent( pSlot->nIndex, SIR_WINDOW_EVENT_RESIZED, LOWORD( nLParam ), HIWORD( nLParam ) );
				pDriver->queueEvent( pSlot->nIndex, SIR_WINDOW_EVENT_RESTORED, 0, 0 );
			}
		} else if ( nMessage == WM_DPICHANGED ) {
			const RECT *pSuggested = ( const RECT * )nLParam;
			if ( pSuggested != nullptr ) {
				( void )SetWindowPos( hWindow, nullptr, pSuggested->left, pSuggested->top, pSuggested->right - pSuggested->left,
				                      pSuggested->bottom - pSuggested->top, SWP_NOZORDER | SWP_NOACTIVATE );
			}
			pDriver->queueEvent( pSlot->nIndex, SIR_WINDOW_EVENT_DPI_CHANGED, LOWORD( nWParam ), 0 );
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
	if ( m_bOwnsClass ) {
		( void )UnregisterClassW( s_szWindowClass, GetModuleHandleW( nullptr ) );
	}
	m_bOwnsClass = false;
	m_bRegistered = false;
	m_pSink = nullptr;
	m_pSinkContext = nullptr;
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
