#include "EventManager.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <SDL2/SDL_events.h>
#include "KeyEvent.h"
#include "WindowEvent.h"

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

    m_nEventsHead = 0;
    m_nEventsTail = 0;
    m_EventList.resize( g_nMaxPushedEvents );
}

void CEventManager::Shutdown( void )
{
}

void CEventManager::Frame( int64_t msec )
{
    IEventBase *pEvent;
    SDL_Event event;

    // we will be using SDL until further notice

    while ( SDL_PollEvent( &event ) ) {
        switch ( event.type ) {
        case SDL_QUIT:
            PushEvent( &g_QuitEvent );
            break;
        
        case SDL_WINDOWEVENT: {
            switch ( event.window.type ) {
            case SDL_WINDOWEVENT_CLOSE:
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_KeyboardFocus_Gained" );
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_KeyboardFocus_Lost" );
                break;
            case SDL_WINDOWEVENT_LEAVE:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_MouseFocus_Lost" );
                break;
            case SDL_WINDOWEVENT_ENTER:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_MouseFocus_Gained" );
                break;
            case SDL_WINDOWEVENT_HIDDEN:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_Hiddent" );
                break;
            case SDL_WINDOWEVENT_SHOWN:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_Shown" );
                PushEvent( CreateWindowEvent( WindowEvent_Hidden, 0, 0 ) );
                break;
            case SDL_WINDOWEVENT_MAXIMIZED:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_Maximized" );
                PushEvent( CreateWindowEvent( WindowEvent_Maximized, 0, 0 ) );
                break;
            case SDL_WINDOWEVENT_MINIMIZED:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_Minimized" );
                PushEvent( CreateWindowEvent( WindowEvent_Minimized, 0, 0 ) );
                break;
            case SDL_WINDOWEVENT_RESIZED:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_Resized" );
                PushEvent( CreateWindowEvent( WindowEvent_Resized, event.window.data1, event.window.data2 ) );
                break;
            case SDL_WINDOWEVENT_MOVED:
                SIRENGINE_LOG( "EventTriggered: WindowEvent_Moved" );
                PushEvent( CreateWindowEvent( WindowEvent_Moved, event.window.data1, event.window.data2 ) );
                break;
            };
            break; }
        
        case SDL_CONTROLLERAXISMOTION:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERDEVICEREMAPPED:
        case SDL_CONTROLLERTOUCHPADDOWN:
        case SDL_CONTROLLERTOUCHPADMOTION:
        case SDL_CONTROLLERTOUCHPADUP:

        
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
            PushEvent( CreateKeyEvent( true, event.key.keysym.sym ) );
            break;
        case SDL_KEYUP:
            PushEvent( CreateKeyEvent( false, event.key.keysym.sym ) );
            break;
        
        case SDL_MOUSEMOTION:
            break;
        case SDL_MOUSEBUTTONDOWN:
            break;
        case SDL_MOUSEBUTTONUP:
            break;
        case SDL_MOUSEWHEEL:
            break;

#if !defined(SIRENGINE_PLATFORM_PC)
        case SDL_FINGERDOWN:
            event.tfinger.dx;
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
        case SDL_KEYMAPCHANGED:
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

void CEventManager::SaveGame( void )
{
}

void CEventManager::LoadGame( void )
{
}

void CEventManager::AddEventListener( const eastl::shared_ptr<IEventListener>& listener )
{
    if ( m_EventBindings[ listener->GetType() ].find( listener->GetName() ) != m_EventBindings[ listener->GetType() ].end() ) {
        SIRENGINE_WARNING( "EventListener Object \"%s\" already added.", listener->GetName() );
        return;
    }
    m_EventBindings[ listener->GetType() ].try_emplace( listener->GetName(), listener );
    SIRENGINE_LOG( "Registered IEventListener Object \"%s\" for event type %i", listener->GetName(), listener->GetType() );
}

void CEventManager::PushEvent( IEventBase *pEvent )
{
    m_EventList[ m_nEventsHead & ( g_nMaxPushedEvents - 1 ) ] = pEvent;

    if ( m_nEventsHead - m_nEventsTail >= g_nMaxPushedEvents ) {
        // overflow
        m_nEventsTail++;
    }
    m_nEventsHead++;

    for ( auto& it : m_EventBindings[ pEvent->GetType() ] ) {
        it.second->Dispatch( pEvent );
    }
}

IEventBase *CEventManager::CreateKeyEvent( bool bState, SDL_Keycode nKeyID )
{
    static IEventBase *pBase = SIREngine_CreateStackObject( CKeyEvent, bState, nKeyID );
    return pBase;
}

IEventBase *CEventManager::CreateWindowEvent( WindowEventType_t nEventType, int32_t nValue1, int32_t nValue2 )
{
    static IEventBase *pBase = SIREngine_CreateStackObject( CWindowEvent, nEventType, nValue1, nValue2 );
    return pBase;
}