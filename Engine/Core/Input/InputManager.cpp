#include <Engine/Core/Events/EventManager.h>
#include "InputManager.h"
#include <SDL2/SDL.h>
#include <Engine/Core/Events/ControllerStatusEvent.h>

using namespace SIREngine::Events;
using namespace SIREngine::Input;
using namespace SIREngine;

CInputManager CInputManager::g_InputManager;

int32_t g_nInputDeviceCount = 0;
CVarRef<int32_t> in_InputDeviceCount(
	"in.InputDeviceCount",
	g_nInputDeviceCount,
	Cvar_Default,
	"Sets the number of input devices the engine will handle.",
	CVG_SYSTEMINFO
);

int32_t g_nJoystickDeviceCount = 0;
CVarRef<int32_t> in_JoystickDeviceCount(
	"in.JoystickDeviceCount",
	g_nJoystickDeviceCount,
	Cvar_Developer,
	"Set to the number of found joystick devices. Queried from SDL2.",
	CVG_SYSTEMINFO
);

int32_t g_nHapticDeviceCount = 0;
CVarRef<int32_t> in_HapticDeviceCount(
	"in.HapticDeviceCount",
	g_nHapticDeviceCount,
	Cvar_Developer,
	"Set to the number of found haptic devices. Queried from SDL2.",
	CVG_SYSTEMINFO
);

bool g_bHapticEnabled = true;
CVarRef<bool> in_HapticFeedbackEnabled(
	"in.HapticFeedbackEnabled",
	g_bHapticEnabled,
	Cvar_Save,
	"Toggles controller haptic feedback effects.",
	CVG_SYSTEMINFO
);

void CInputDevice::InitTouchDevice( void )
{
	m_hTouchDevice = SDL_GetTouchDevice( m_nDeviceIndex );
}

void CInputDevice::InitKeyboard( void )
{

}

void CInputDevice::InitController( void )
{
	if ( !m_pController ) {
		m_pController = new JoystickData_t;
	}

	m_pController->m_pJoystick = SDL_JoystickOpen( m_nDeviceIndex );
	if ( !m_pController->m_pJoystick ) {
	#if defined(SIRENGINE_PLATFORM_PC)
		// on pc we can just default to the keyboard
		SIRENGINE_WARNING( "Error opening Joystick device at %i: %s", m_nDeviceIndex, SDL_GetError() );
		m_nType = EType::Keyboard;
		InitKeyboard();
	#elif defined(SIRENGINE_PLATFORM_CONSOLE)
		// controllers are required to play
		if ( nIndex > 0 ) {
			SIRENGINE_WARNING( "Error opening Joystick device at %i: %s", m_nDeviceIndex, SDL_GetError() );
		} else {
			SIRENGINE_ERROR( "Error opening Joystick device at %i: %s", m_nDeviceIndex, SDL_GetError() );
		}
	#elif defined(SIRENGINE_PLATFORM_MOBILE)
		// we can just default to touchscreen if on mobile
		m_nType = EType::TouchScreen;
		InitTouchDevice();
	#endif
		return;
	}
	if ( SDL_IsGameController( m_nDeviceIndex ) ) {
		m_pController->m_pGamepad = SDL_GameControllerOpen( m_nDeviceIndex );
		if ( !m_pController->m_pGamepad ) {
			SIRENGINE_LOG( "Unable to open GameController device at %i: %s", m_nDeviceIndex, SDL_GetError() );
		}
	}

	SDL_JoystickSetPlayerIndex( m_pController->m_pJoystick, m_nDeviceIndex );
	if ( m_pController->m_pGamepad ) {
		SDL_GameControllerSetPlayerIndex( m_pController->m_pGamepad, m_nDeviceIndex );
	}
	if ( SDL_JoystickSetLED( m_pController->m_pJoystick, 255, 0, 0 ) == -1 ) {
		SIRENGINE_NOTIFICATION( "Joystick Device %i doesn't have modifyable LEDs", m_nDeviceIndex );
	}
	if ( SDL_JoystickGetPlayerIndex( m_pController->m_pJoystick ) != m_nDeviceIndex ) {
		SIRENGINE_ERROR( "Joystick Device %i doesn't have the correct player index", m_nDeviceIndex );
	}
	m_pController->m_hJoystickID = SDL_JoystickGetDeviceInstanceID( m_nDeviceIndex );

	SIRENGINE_LOG( "Joystick Device %i opened.", m_nDeviceIndex );
	SIRENGINE_LOG( "  NAME: %s", SDL_JoystickName( m_pController->m_pJoystick ) );
	SIRENGINE_LOG( "  AXES: %i", SDL_JoystickNumAxes( m_pController->m_pJoystick ) );
	SIRENGINE_LOG( "  BALLS: %i", SDL_JoystickNumBalls( m_pController->m_pJoystick ) );
	SIRENGINE_LOG( "  BUTTONS: %i", SDL_JoystickNumButtons( m_pController->m_pJoystick ) );
	SIRENGINE_LOG( "  HATS: %i", SDL_JoystickNumHats( m_pController->m_pJoystick ) );
	SIRENGINE_LOG( "  INSTANCE ID: %i", SDL_JoystickGetDeviceInstanceID( m_nDeviceIndex ) );
	SIRENGINE_LOG( "  IS_GAMEPAD: %s", m_pController->m_pGamepad ? "Yes" : "No" );

	if ( g_bHapticEnabled ) {
		m_pController->m_pHaptic = SDL_HapticOpen( m_nDeviceIndex );
		if ( !m_pController->m_pHaptic ) {
			SIRENGINE_WARNING( "Error opening Haptic device at %i: %s", m_nDeviceIndex, SDL_GetError() );
			return;
		}
		if ( SDL_HapticRumbleSupported( m_pController->m_pHaptic ) ) {
			if ( SDL_HapticRumbleInit( m_pController->m_pHaptic ) < 0 ) {
				SIRENGINE_ERROR( "Error initializing Rumble effect for Haptic device at %i: %s", m_nDeviceIndex, SDL_GetError() );
			}
		} else {
			SIRENGINE_LOG( "Haptic Rumble effect isn't supported for Joystick Device at %i", m_nDeviceIndex );
		}
	}
}

void CInputDevice::Init( EType nInitialType, int32_t nIndex )
{
	m_nType = nInitialType;
	m_nDeviceIndex = nIndex;

	switch ( nInitialType ) {
	case EType::Keyboard:
		InitKeyboard();
		break;
	case EType::Controller:
		InitController();
		break;
	case EType::TouchScreen:
		InitTouchDevice();
		break;
	};
}

void CInputDevice::Shutdown( void )
{
	if ( m_pController ) {
		if ( m_pController->m_pJoystick ) {
			SDL_JoystickClose( m_pController->m_pJoystick );
			m_pController->m_pJoystick = NULL;
		}
		if ( m_pController->m_pGamepad ) {
			SDL_GameControllerClose( m_pController->m_pGamepad );
			m_pController->m_pGamepad = NULL;
		}
		if ( m_pController->m_pHaptic ) {
			SDL_HapticClose( m_pController->m_pHaptic );
			m_pController->m_pHaptic = NULL;
		}
		delete m_pController;
		m_pController = NULL;
	}
}

void CInputDevice::ApplyHaptic( int32_t nDuration, float nIntensity )
{
	if ( !m_pController ) {
		return;
	}
	if ( m_pController->m_pHaptic ) {
		SDL_HapticRumblePlay( m_pController->m_pHaptic, nIntensity, nDuration );
	}
}

CInputManager::CInputManager( void )
{
}

CInputManager::~CInputManager()
{
}

void CInputManager::Init( void )
{
	uint32_t i, numControllers;

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) ) {
		SIRENGINE_LOG( "SDL_InitSubSystem( SDL_INIT_JOYSTICK )" );
		if ( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) < 0 ) {
			if ( IsPlatformConsole() ) {
				SIRENGINE_ERROR( "SDL_INIT_JOYSTICK Failed: %s", SDL_GetError() );
			} else {
				SIRENGINE_WARNING( "SDL_INIT_JOYSTICK Failed: %s", SDL_GetError() );
			}
		} else {
			SIRENGINE_LOG( "SDL_Joystick SubSystem Initialized." );
		}
	}
	if ( !SDL_WasInit( SDL_INIT_GAMECONTROLLER ) ) {
		SIRENGINE_LOG( "SDL_InitSubSystem( SDL_INIT_GAMECONTROLLER )" );
		if ( SDL_InitSubSystem( SDL_INIT_GAMECONTROLLER ) < 0 ) {
			SIRENGINE_WARNING( "SDL_INIT_GAMECONTROLLER Failed: %s", SDL_GetError() );
		} else {
			SIRENGINE_LOG( "SDL_GameController SubSystem Initialized." );
		}
	}
	if ( !SDL_WasInit( SDL_INIT_HAPTIC ) ) {
		SIRENGINE_LOG( "SDL_InitSubSystem( SDL_INIT_HAPTIC )" );
		if ( SDL_InitSubSystem( SDL_INIT_HAPTIC ) < 0 ) {
			SIRENGINE_WARNING( "SDL_INIT_HAPTIC Failed: %s", SDL_GetError() );
		} else {
			SIRENGINE_LOG( "SDL_Haptic SubSystem Initialized." );
		}
	}

	in_InputDeviceCount.Register();
	in_JoystickDeviceCount.Register();
	in_HapticDeviceCount.Register();
	in_HapticFeedbackEnabled.Register();

	in_JoystickDeviceCount.SetValue( SDL_NumJoysticks() );
	in_HapticDeviceCount.SetValue( SDL_NumHaptics() );

	g_nInputDeviceCount = 0;

	// initialize all input devices as controllers (the first one is always a keyboard on PC however)
	// corrections to this will be adjustable in settings
	memset( m_szInputDevices.data(), 0, sizeof( CInputDevice ) * m_szInputDevices.size() );

#if defined(SIRENGINE_PLATFORM_PC)
	m_szInputDevices[i].Init( CInputDevice::EType::Keyboard, 0 );
	g_nInputDeviceCount++;
	for ( i = 1; i < in_JoystickDeviceCount.GetValue(); i++ ) {
		if ( i - 1 > g_nJoystickDeviceCount ) {
			break; // no more devices available
		}
		m_szInputDevices[i].Init( CInputDevice::EType::Controller, i );
		g_nInputDeviceCount++;
	}
#elif defined(SIRENGINE_PLATFORM_CONSOLE)
	for ( i = 0; i < m_szInputDevices.size(); i++ ) {
		if ( i > g_nJoystickDeviceCount ) {
			break; // no more devices available
		}
		m_szInputDevices[i].Init( CInputDevice::EType::Controller, i );
		g_nInputDeviceCount++;
	}
#elif defined(SIRENGINE_PLATFORM_MOBILE)
	// only one input device possible
	m_szInputDevices[0].Init( CInputDevice::EType::TouchScreen, 0 );
	g_nInputDeviceCount++;
#endif

	// should this be just a warning?
	if ( SDL_JoystickEventState( SDL_ENABLE ) < 0 ) {
		SIRENGINE_ERROR( "Error enabling SDL Joystick Event State: %s", SDL_GetError() );
	}
	if ( SDL_GameControllerEventState( SDL_ENABLE ) < 0 ) {
		SIRENGINE_WARNING( "Error enabling SDL GameController Event State: %s", SDL_GetError() );
	}

	CEventManager::Get().AddEventListener( eastl::make_shared<CEventListener>(
		"KeyboardEventListener", EventType_Key, CInputManager::KeyboardEventListener
	) );
	CEventManager::Get().AddEventListener( eastl::make_shared<CEventListener>(
		"GamepadEventListener", EventType_Gamepad, CInputManager::GamepadEventListener
	) );
	CEventManager::Get().AddEventListener( eastl::make_shared<CEventListener>(
		"ControllerStatusListener", EventType_ControllerStatus, CInputManager::ControllerStatusListener
	) );
}

void CInputManager::Shutdown( void )
{
	SIRENGINE_LOG( "Deallocating Input Devices..." );
	for ( auto& it : m_szInputDevices ) {
		it.Shutdown();
	}

	if ( SDL_WasInit( SDL_INIT_JOYSTICK ) ) {
		SIRENGINE_LOG( "Shutting down SDL_Joystick SubSystem..." );
		SDL_QuitSubSystem( SDL_INIT_JOYSTICK );
	}
	if ( SDL_WasInit( SDL_INIT_GAMECONTROLLER ) ) {
		SIRENGINE_LOG( "Shutting down SDL_GameController SubSystem..." );
		SDL_QuitSubSystem( SDL_INIT_GAMECONTROLLER );
	}
	if ( SDL_WasInit( SDL_INIT_HAPTIC ) ) {
		SIRENGINE_LOG( "Shutting down SDL_Haptic SubSystem..." );
		SDL_QuitSubSystem( SDL_INIT_HAPTIC );
	}
}

void CInputManager::Frame( int64_t msec )
{
	uint32_t i;

	SDL_LockJoysticks();
	for ( i = 0; i < g_nInputDeviceCount; i++ ) {

	}
	SDL_UnlockJoysticks();

}

void CInputManager::SaveGame( void )
{
}

void CInputManager::LoadGame( void )
{
}

void CInputManager::KeyboardEventListener( const IEventBase *pEventData )
{
	const CKeyEvent *pKeyEvent = dynamic_cast<const CKeyEvent *>( pEventData );

	if ( pKeyEvent->IsPressed() ) {

	} else {

	}
}

void CInputManager::GamepadEventListener( const IEventBase *pEventData )
{
	const CGamepadEvent *pGamePadEvent = dynamic_cast<const CGamepadEvent *>( pEventData );


}

void CInputManager::ControllerStatusListener( const IEventBase *pEventData )
{
	const CControllerStatusEvent *pStatusEvent = dynamic_cast<const CControllerStatusEvent *>( pEventData );

	SDL_LockJoysticks();
	if ( pStatusEvent->IsDeviceAdded() ) {
		CInputManager::Get().OnAddController( pStatusEvent->GetDeviceID() );
	} else {
		CInputManager::Get().OnRemoveController( pStatusEvent->GetDeviceID() );
	}
	SDL_UnlockJoysticks();
}

CInputDevice *CInputManager::GetKeyboard( void )
{
	int32_t i;

	for ( i = 0; i < g_nInputDeviceCount; i++ ) {
		if ( m_szInputDevices[ i ].GetType() == CInputDevice::EType::Keyboard ) {
			return &m_szInputDevices[ i ];
		}
	}

	SIRENGINE_ERROR( "CInputManager::GetKeyboard: no keyboard found" );
}

void CInputManager::OnKeyDown( KeyNum_t nKeyID )
{
	CInputDevice *pKeyboard = GetKeyboard();
	eastl::unique_ptr<CBindSet>& pBindings = pKeyboard->GetBindings();

	if ( !pBindings ) {
		SIRENGINE_ERROR( "CInputManager::OnKeyDown: No bindset mapping for keyboard!" );
	}

	KeyBind_t *pBind = pBindings->GetBind( nKeyID );
	if ( !pBind ) {
		return; // do nothing
	}

	pBind->pDownCommand->Execute();
}

void CInputManager::OnKeyUp( KeyNum_t nKeyID )
{
	CInputDevice *pKeyboard = GetKeyboard();
	eastl::unique_ptr<CBindSet>& pBindings = pKeyboard->GetBindings();

	if ( !pBindings ) {
		SIRENGINE_ERROR( "CInputManager::OnKeyUp: No bindset mapping for keyboard!" );
	}

	KeyBind_t *pBind = pBindings->GetBind( nKeyID );
	if ( !pBind ) {
		return; // do nothing
	}

	pBind->pUpCommand->Execute();
}

void CInputManager::OnAddController( int32_t nDeviceID )
{
	if ( g_nInputDeviceCount == SIRENGINE_MAX_COOP_PLAYERS ) {
		SIRENGINE_WARNING( "Not enough player slots for device %i", nDeviceID );
		return;
	}

	SIRENGINE_LOG( "Connecting controller at device slot %i...", nDeviceID );
	if ( m_szInputDevices[ nDeviceID ].ValidController() ) {
		// this is caused by the event callback being triggered during the first couple frames,
		// we'll simply ignore the call
		return;
	}
	m_szInputDevices[ nDeviceID ].Init( CInputDevice::EType::Controller, nDeviceID );
	g_nInputDeviceCount++;
}

void CInputManager::OnRemoveController( int32_t nDeviceID )
{
	int32_t i;

	if ( g_nInputDeviceCount == 0 ) {
		SIRENGINE_ERROR( "CInputManager::OnRemoveController: InputDevice count is 0" );
	}

	for ( i = 0; i < g_nInputDeviceCount; i++ ) {
		if ( m_szInputDevices[ i ].GetInstanceID() == nDeviceID ) {
			SIRENGINE_LOG( "Removing controlller at device slot %i...", m_szInputDevices[ i ].GetIndex() );
			m_szInputDevices[ i ].Shutdown();
			g_nInputDeviceCount--;
			return;
		}
	}
	SIRENGINE_ERROR( "CInputManager::OnRemoveController: invalid Joystick InstanceID %i", nDeviceID );
}