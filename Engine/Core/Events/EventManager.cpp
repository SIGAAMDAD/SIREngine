#include "EventManager.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <SDL2/SDL_events.h>
#include "KeyEvent.h"
#include "WindowEvent.h"
#include "TouchEvent.h"
#include "ControllerStatusEvent.h"
#include "MouseEvent.h"
#include "GamepadEvent.h"

CEventManager CEventManager::g_EventManager;
CQuitEvent CEventManager::g_QuitEvent;

extern CVarRef<int32_t> e_WindowPosX;
extern CVarRef<int32_t> e_WindowPosY;

uint64_t g_nMaxPushedEvents = 2048;
CVarRef<uint64_t> e_MaxPushedEvents(
	"e.MaxPushedEvents",
	g_nMaxPushedEvents,
	Cvar_Developer,
	"Sets the maximum amount of events that can be buffered at a time.",
	CVG_SYSTEMINFO
);

CEventManager::CEventManager( void )
{
}

CEventManager::~CEventManager()
{
}

void CEventManager::Init( void )
{
	SIRENGINE_LOG( "Initializing SIREngine EventSystem..." );

	e_MaxPushedEvents.Register();

	m_nBufferedEventsHead = 0;
	m_nBufferedEventsTail = 0;
	m_EventBuffer.resize( g_nMaxPushedEvents );
}

void CEventManager::Shutdown( void )
{
}

void CEventManager::Frame( int64_t msec )
{
	// we will be using SDL until further notice

	PumpEvents();

	while ( 1 ) {
		const CEventData& event = GetEvent();

		// nothing left
		if ( event.GetType() == EventType_None ) {
			break;
		}

		for ( auto& it : m_EventBindings[ event.GetType() ] ) {
			it.second->Dispatch( event.GetData() );
		}
	}
}

void CEventManager::SaveGame( void )
{
}

void CEventManager::LoadGame( void )
{
}

const CEventData& CEventManager::GetEvent( void )
{
	uint64_t nEventTime;

	// do we have data?
	if ( m_nBufferedEventsHead - m_nBufferedEventsTail > 0 ) {
		return m_EventBuffer[ ( m_nBufferedEventsTail++ ) & ( g_nMaxPushedEvents - 1 ) ];
	}

	PumpEvents();

	nEventTime = time( NULL );

	// do we have data?
	if ( m_nBufferedEventsHead - m_nBufferedEventsTail > 0 ) {
		return m_EventBuffer[ ( m_nBufferedEventsTail++ ) & ( g_nMaxPushedEvents - 1 ) ];
	}

	// create an empty event
	static CEventData emptyEvent = CreateEmptyEvent();
	return emptyEvent;
}

void CEventManager::AddEventListener( const eastl::shared_ptr<IEventListener>& listener )
{
	if ( listener->GetType() > m_EventBindings.size() ) {
		SIRENGINE_ERROR( "EventListener Object has unregistered EventType_t %i", listener->GetType() );
	}
	if ( m_EventBindings[ listener->GetType() ].find( listener->GetName() ) != m_EventBindings[ listener->GetType() ].end() ) {
		SIRENGINE_WARNING( "EventListener Object \"%s\" already added.", listener->GetName() );
		return;
	}
	m_EventBindings[ listener->GetType() ].try_emplace( listener->GetName(), listener );
	SIRENGINE_LOG( "Registered IEventListener Object \"%s\" for event type %i", listener->GetName().c_str(), listener->GetType() );
}

void CEventManager::PushEvent( uint64_t nTime, const CEventData& pData )
{
	m_EventBuffer[ m_nBufferedEventsHead & ( g_nMaxPushedEvents - 1 ) ] = pData;

	if ( m_nBufferedEventsHead - m_nBufferedEventsTail >= g_nMaxPushedEvents ) {
		// overflow
		m_nBufferedEventsTail++;
	}
	m_nBufferedEventsHead++;
}

CEventData CEventManager::CreateKeyEvent( const SDL_Event& eventData, bool bState, KeyNum_t nKeyID )
{
	return CKeyEvent( eventData, bState, nKeyID );
}

CEventData CEventManager::CreateWindowEvent( const SDL_Event& eventData, WindowEventType_t nEventType, int32_t nValue1, int32_t nValue2 )
{
	return CWindowEvent( eventData, nEventType, nValue1, nValue2 );
}

CEventData CEventManager::CreateTouchEvent( const SDL_Event& eventData, float x, float y, uint32_t nFingerState )
{
	FingerState_t nState;
	switch ( nFingerState ) {
	case SDL_FINGERDOWN:
		nState = Finger_Down;
	case SDL_FINGERUP:
		nState = Finger_Up;
		break;
	case SDL_FINGERMOTION:
		nState = Finger_Motion;
		break;
	default:
		SIRENGINE_ERROR( "CEventManager::CreateTouchEvent: invalid touch finger event type %u", nFingerState );
	};
	return CTouchEvent( eventData, x, y, nState );
}

CEventData CEventManager::CreateControllerStatusEvent( const SDL_Event& eventData, bool bStatus )
{
	return CControllerStatusEvent( eventData, bStatus );
}

CEventData CEventManager::CreateMouseEvent( const SDL_Event& eventData, int x, int y )
{
	return CMouseEvent( eventData, x, y );
}

CEventData CEventManager::CreateEmptyEvent( void )
{
	return CEventData( IEventBase() );
}

void CEventManager::PumpEvents( void )
{
	SDL_Event event;

	while ( SDL_PollEvent( &event ) ) {
		switch ( event.type ) {
		case SDL_QUIT:
			PushEvent( 0, g_QuitEvent );
			break;
		
		case SDL_WINDOWEVENT: {
			switch ( event.window.event ) {
			case SDL_WINDOWEVENT_CLOSE:
				break; // is this just a fancy SDL_QUIT?
			
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_KeyboardFocus, true, 0 ) );
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_KeyboardFocus, false, 0 ) );
				break;
			
			case SDL_WINDOWEVENT_LEAVE:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_MouseFocus, false, 0 ) );
				break;
			case SDL_WINDOWEVENT_ENTER:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_MouseFocus, true, 0 ) );
				break;

			// these are triggered by the same action
			case SDL_WINDOWEVENT_HIDDEN:
			case SDL_WINDOWEVENT_MINIMIZED:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_Minimized, 0, 0 ) );
				break;
			
			case SDL_WINDOWEVENT_SHOWN:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_Shown, 0, 0 ) );
				break;
			case SDL_WINDOWEVENT_MAXIMIZED:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_Maximized, 0, 0 ) );
				break;
			case SDL_WINDOWEVENT_RESIZED:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_Resized, event.window.data1, event.window.data2 ) );
				break;
			case SDL_WINDOWEVENT_MOVED:
				PushEvent( 0, CreateWindowEvent( event, WindowEvent_Moved, event.window.data1, event.window.data2 ) );
				break;
			};
			break; }
		
		case SDL_CONTROLLERAXISMOTION:
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
		case SDL_CONTROLLERTOUCHPADDOWN:
		case SDL_CONTROLLERTOUCHPADMOTION:
		case SDL_CONTROLLERTOUCHPADUP:
			break;
		
		case SDL_JOYAXISMOTION:
			break;
		case SDL_JOYBALLMOTION:
			break;
		case SDL_JOYBUTTONDOWN:
			break;
		case SDL_JOYBUTTONUP:
			break;
		case SDL_JOYHATMOTION:
			break;

		case SDL_KEYDOWN:
			PushEvent( 0, CreateKeyEvent( event, true, event.key.keysym.sym ) );
			break;
		case SDL_KEYUP:
			PushEvent( 0, CreateKeyEvent( event, false, event.key.keysym.sym ) );
			break;
		case SDL_TEXTEDITING:
		case SDL_TEXTINPUT:
			break;
		
		case SDL_MOUSEMOTION:
			PushEvent( 0, CreateMouseEvent( event, event.motion.x, event.motion.y ) );
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: {
			KeyNum_t b;

			switch ( event.button.button ) {
			case SDL_BUTTON_LEFT:	b = Key_MouseLeft; break;
			case SDL_BUTTON_MIDDLE:	b = Key_MouseMiddle; break;
			case SDL_BUTTON_RIGHT:	b = Key_MouseRight; break;
			case SDL_BUTTON_X1:		b = Key_MouseButton4; break;
			case SDL_BUTTON_X2:		b = Key_MouseButton5; break;
			};
			PushEvent( 0, CreateKeyEvent( event, event.type == SDL_MOUSEBUTTONDOWN, b ) );
			break; }
		case SDL_MOUSEWHEEL:
			PushEvent( 0, CreateKeyEvent( event, true, CKeyEvent::SDLKeyToEngineKey( event ) ) );
			PushEvent( 0, CreateKeyEvent( event, false, CKeyEvent::SDLKeyToEngineKey( event ) ) );
			break;

#if !defined(SIRENGINE_PLATFORM_PC)
		case SDL_FINGERDOWN:
			break;
		case SDL_FINGERUP:
			break;
		case SDL_FINGERMOTION:
			break;
#endif

		// uncommon events
		case SDL_JOYDEVICEADDED:
			break;
		case SDL_JOYDEVICEREMOVED:
			break;
		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
			PushEvent( 0, CreateControllerStatusEvent( event, event.cdevice.type == SDL_CONTROLLERDEVICEADDED ) );
			break;
		case SDL_CONTROLLERDEVICEREMAPPED:
		case SDL_KEYMAPCHANGED:
			break;
		
		case SDL_CLIPBOARDUPDATE:
			break;

#if defined(SIRENGINE_PLATFORM_IOS)
		case SDL_APP_TERMINATING:
		case SDL_APP_LOWMEMORY:
		case SDL_APP_WILLENTERBACKGROUND:
		case SDL_APP_WILLENTERFOREGROUND:
#endif
		default:
			SIRENGINE_WARNING( "Unknown SDL2 event type %i", event.type );
			break;
		};
	}
}