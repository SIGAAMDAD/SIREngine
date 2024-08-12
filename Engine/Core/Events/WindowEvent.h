#ifndef __SIRENGINE_WINDOWEVENT_H__
#define __SIRENGINE_WINDOWEVENT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/Compiler.h>
#include "EventBase.h"

namespace SIREngine::Events {
    typedef enum {
        WindowEvent_Minimized,
        WindowEvent_Maximized,
        WindowEvent_Closed,
        WindowEvent_Resized,
        WindowEvent_Moved,
        WindowEvent_Shown,
        WindowEvent_MouseFocus,
        WindowEvent_KeyboardFocus,
    } WindowEventType_t;

    class CWindowEvent : public IEventBase
    {
    public:
        CWindowEvent( const SDL_Event& eventData, WindowEventType_t nType, int32_t value1, int32_t value2 )
            : m_nEventType( nType ), m_nValue1( value1 ), m_nValue2( value2 )
        { }
        virtual ~CWindowEvent() override
        { }

        virtual void Send( void ) override
        { }

        SIRENGINE_FORCEINLINE WindowEventType_t GetEventType( void ) const
        { return m_nEventType; }
        SIRENGINE_FORCEINLINE int32_t GetValue1( void ) const
        { return m_nValue1; }
        SIRENGINE_FORCEINLINE int32_t GetValue2( void ) const
        { return m_nValue2; }
        SIRENGINE_FORCEINLINE int32_t GetWidth( void ) const
        { return m_nValue1; }
        SIRENGINE_FORCEINLINE int32_t GetHeight( void ) const
        { return m_nValue2; }
        SIRENGINE_FORCEINLINE int32_t GetXCoord( void ) const
        { return m_nValue1; }
        SIRENGINE_FORCEINLINE int32_t GetYCoord( void ) const
        { return m_nValue2; }
        SIRENGINE_FORCEINLINE bool IsFocused( void ) const
        { return m_nValue1 == true; }

        SIRENGINE_FORCEINLINE virtual const char *GetName( void ) const override
        { return "WindowEvent"; }
        SIRENGINE_FORCEINLINE virtual EventType_t GetType( void ) const override
        { return EventType_Window; }
    private:
        WindowEventType_t m_nEventType;
        int32_t m_nValue1;
        int32_t m_nValue2;
    };
};

#endif