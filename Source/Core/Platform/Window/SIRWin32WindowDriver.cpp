#include <Core/Platform/Window/SIRNativeWindowDrivers.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>
#include <imm.h>

static constexpr wchar_t s_szWindowClass[] = L"SIREngineWindowSystem";

static LRESULT CALLBACK sirWindowNativeProc( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam )
{
	return ( LRESULT )SIRWin32WindowDriver::dispatchNative( hWindow, nMessage, nWParam, nLParam );
}

SIRWin32WindowDriver::SIRWin32WindowDriver( void ) noexcept
    : m_pSystem( nullptr ), m_hInstance( nullptr ), m_szSlots{}, m_pInput( nullptr ), m_pDesktop( nullptr ), m_hInputWindow{},
      m_bOwnClass( false ), m_bFocusLost( false )
{
}

bool SIRWin32WindowDriver::initialize( SIRWindowSystem& system ) noexcept
{
	if ( m_pSystem != nullptr ) {
		return false;
	}
	m_hInstance = GetModuleHandleW( nullptr );
	WNDCLASSW klass = {};
	klass.lpfnWndProc = sirWindowNativeProc;
	klass.hCursor = LoadCursorW( nullptr, IDC_ARROW );
	klass.hInstance = m_hInstance;
	klass.lpszClassName = s_szWindowClass;
	const ATOM nClass = RegisterClassW( &klass );
	if ( nClass == 0 ) {
		if ( GetLastError() != ERROR_CLASS_ALREADY_EXISTS ) {
			return false;
		}
		WNDCLASSW registered = {};
		if ( !GetClassInfoW( m_hInstance, s_szWindowClass, &registered ) || registered.lpfnWndProc != sirWindowNativeProc ) {
			return false;
		}
	}
	m_bOwnClass = nClass != 0;
	m_pSystem = &system;
	return true;
}

void SIRWin32WindowDriver::shutdown( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_szSlots[i].hNative != nullptr ) {
			DestroyWindow( m_szSlots[i].hNative );
		}
		m_szSlots[i] = {};
	}
	if ( m_bOwnClass ) {
		UnregisterClassW( s_szWindowClass, m_hInstance );
	}
	m_bOwnClass = false;
	m_hInstance = nullptr;
	m_pSystem = nullptr;
	m_pInput = nullptr;
	m_pDesktop = nullptr;
}

bool SIRWin32WindowDriver::create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept
{
	if ( m_pSystem == nullptr || pView == nullptr || hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return false;
	}
	sirWin32WindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.hNative != nullptr ) {
		return false;
	}
	wchar_t szTitle[128] = {};
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, desc.pszTitle, -1, szTitle, 128 ) == 0 ) {
		return false;
	}
	slot.pOwner = this;
	slot.hWindow = hWindow;
	const HWND hNative = CreateWindowExW( 0, s_szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	                                      ( int )desc.nWidth, ( int )desc.nHeight, nullptr, nullptr, m_hInstance, &slot );
	if ( hNative == nullptr ) {
		slot = {};
		return false;
	}
	slot.hNative = hNative;
	if ( desc.bVisible ) {
		ShowWindow( hNative, SW_SHOWNORMAL );
	}
	*pView = { SIR_WINDOW_PLATFORM_WIN32, hNative, m_hInstance, 0 };
	return true;
}

void SIRWin32WindowDriver::destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept
{
	( void )view;
	if ( hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return;
	}
	sirWin32WindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.hWindow.nGeneration != hWindow.nGeneration ) {
		return;
	}
	if ( slot.hNative != nullptr ) {
		DestroyWindow( slot.hNative );
	}
	slot = {};
}

intptr_t SIRWin32WindowDriver::dispatchNative( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam ) noexcept
{
	if ( nMessage == WM_NCCREATE ) {
		const CREATESTRUCTW *pCreate = ( const CREATESTRUCTW * )nLParam;
		sirWin32WindowSlot_t *pSlot = ( sirWin32WindowSlot_t * )pCreate->lpCreateParams;
		pSlot->hNative = hWindow;
		SetWindowLongPtrW( hWindow, GWLP_USERDATA, ( LONG_PTR )pSlot );
	}
	sirWin32WindowSlot_t *pSlot = ( sirWin32WindowSlot_t * )GetWindowLongPtrW( hWindow, GWLP_USERDATA );
	if ( pSlot == nullptr || pSlot->pOwner == nullptr ) {
		return ( intptr_t )DefWindowProcW( hWindow, nMessage, nWParam, nLParam );
	}
	return pSlot->pOwner->dispatchMessage( pSlot->hWindow.nSlot, hWindow, nMessage, nWParam, nLParam );
}

intptr_t SIRWin32WindowDriver::dispatchMessage( uint32_t iSlot, HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam ) noexcept
{
	sirWin32WindowSlot_t& slot = m_szSlots[iSlot];
	if ( nMessage == WM_CLOSE ) {
		( void )m_pSystem->notifyClose( slot.hWindow );
		return 0;
	}
	if ( nMessage == WM_SIZE ) {
		( void )m_pSystem->notifyResize( slot.hWindow, LOWORD( nLParam ), HIWORD( nLParam ) );
	}
	if ( nMessage == WM_SETFOCUS || nMessage == WM_KILLFOCUS ) {
		const bool bFocused = nMessage == WM_SETFOCUS;
		( void )m_pSystem->notifyFocus( slot.hWindow, bFocused );
		if ( m_pInput != nullptr && iSlot == m_hInputWindow.nSlot ) {
			m_pInput->text().setFocused( bFocused );
			m_pInput->setHapticsPaused( !bFocused );
			if ( !bFocused ) {
				m_bFocusLost = true;
			}
		}
	}
	if ( nMessage == WM_NCDESTROY ) {
		( void )m_pSystem->notifyLost( slot.hWindow );
		slot.hNative = nullptr;
		SetWindowLongPtrW( hWindow, GWLP_USERDATA, 0 );
		return ( intptr_t )DefWindowProcW( hWindow, nMessage, nWParam, nLParam );
	}
	if ( m_pInput != nullptr && iSlot == m_hInputWindow.nSlot ) {
		if ( nMessage == WM_IME_STARTCOMPOSITION || nMessage == WM_IME_ENDCOMPOSITION ) {
			m_pInput->text().clearPreedit();
		}
		if ( nMessage == WM_IME_COMPOSITION && ( nLParam & GCS_COMPSTR ) != 0 ) {
			HIMC hContext = ImmGetContext( hWindow );
			if ( hContext != nullptr ) {
				uint16_t szUnits[SIR_INPUT_PREEDIT_CAPACITY] = {};
				const LONG nBytes = ImmGetCompositionStringW( hContext, GCS_COMPSTR, szUnits, sizeof( szUnits ) );
				if ( nBytes >= 0 && nBytes <= ( LONG )sizeof( szUnits ) && ( nBytes % ( LONG )sizeof( uint16_t ) ) == 0 ) {
					const LONG nCaretUnits = ImmGetCompositionStringW( hContext, GCS_CURSORPOS, nullptr, 0 );
					const uint32_t nUnits = ( uint32_t )nBytes / sizeof( uint16_t );
					const uint32_t nLimit = nCaretUnits < 0 ? 0 : ( uint32_t )nCaretUnits > nUnits ? nUnits : ( uint32_t )nCaretUnits;
					uint32_t nCaret = 0;
					for ( uint32_t iUnit = 0; iUnit < nLimit; ++iUnit ) {
						if ( szUnits[iUnit] < 0xdc00 || szUnits[iUnit] > 0xdfff ) {
							++nCaret;
						}
					}
					( void )m_pInput->text().setPreeditUtf16( szUnits, nUnits, nCaret, 0 );
				}
				ImmReleaseContext( hWindow, hContext );
			}
		}
		if ( nMessage == WM_CHAR ) {
			const uint16_t nUnit = ( uint16_t )nWParam;
			if ( nUnit >= 32 || nUnit == 9 || nUnit == 10 || nUnit == 13 ) {
				( void )m_pInput->text().appendUtf16( nUnit );
			}
			return 0;
		}
		if ( nMessage == WM_UNICHAR ) {
			if ( nWParam == UNICODE_NOCHAR ) {
				return TRUE;
			}
			( void )m_pInput->text().appendCodepoint( ( uint32_t )nWParam );
			return 0;
		}
		if ( nMessage == WM_INPUT || nMessage == WM_INPUT_DEVICE_CHANGE ) {
			( ( SIRWindowsRawBackend * )m_pDesktop )->processMessage( *m_pInput, nMessage, nWParam, nLParam );
			if ( nMessage == WM_INPUT_DEVICE_CHANGE ) {
				return 0;
			}
		}
	}
	return ( intptr_t )DefWindowProcW( hWindow, nMessage, nWParam, nLParam );
}

bool SIRWin32WindowDriver::pump( void ) noexcept
{
	if ( m_pSystem == nullptr ) {
		return false;
	}
	MSG message = {};
	while ( PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) ) {
		if ( message.message == WM_QUIT ) {
			return false;
		}
		TranslateMessage( &message );
		DispatchMessageW( &message );
	}
	if ( m_bFocusLost && m_pInput != nullptr ) {
		m_bFocusLost = false;
		( void )m_pInput->loseBackendState( SIR_INPUT_WIN_RAW );
	}
	return true;
}

bool SIRWin32WindowDriver::bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept
{
	if ( m_pInput != nullptr || backend.id() != SIR_INPUT_WIN_RAW || hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return false;
	}
	const sirWin32WindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.hNative == nullptr || slot.hWindow.nGeneration != hWindow.nGeneration ) {
		return false;
	}
	SIRWindowsRawBackend& desktop = ( SIRWindowsRawBackend& )backend;
	if ( !desktop.attachWindow( slot.hNative ) ) {
		return false;
	}
	if ( !input.attach( backend ) ) {
		desktop.stop( input );
		return false;
	}
	m_pInput = &input;
	m_pDesktop = &backend;
	m_hInputWindow = hWindow;
	return true;
}
