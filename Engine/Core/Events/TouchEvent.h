#ifndef __SIRENGINE_TOUCH_EVENT_H__
#define __SIRENGINE_TOUCH_EVENT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "EventBase.h"
#include <Engine/Core/Compiler.h>
#include <Engine/Core/MathLib/Types.h>

typedef enum {
	Finger_Down,
	Finger_Up,
	Finger_Motion
} FingerState_t;

class CTouchEvent : public IEventBase
{
public:
	CTouchEvent( const SDL_Event& eventData, float x, float y, FingerState_t nState )
		: m_Position( x, y ), m_nState( nState )
	{ }
	virtual ~CTouchEvent() override
	{ }

	virtual void Send( void ) override
	{ }

	SIRENGINE_FORCEINLINE bool IsDown( void ) const
	{ return m_nState == Finger_Down; }
	SIRENGINE_FORCEINLINE bool IsUp( void ) const
	{ return m_nState == Finger_Up; }
	SIRENGINE_FORCEINLINE bool IsMoving( void ) const
	{ return m_nState == Finger_Motion; }

	SIRENGINE_FORCEINLINE FingerState_t GetState( void ) const
	{ return m_nState; }

	SIRENGINE_FORCEINLINE virtual const char *GetName( void ) const override
    { return "TouchScreenEvent"; }
    SIRENGINE_FORCEINLINE virtual EventType_t GetType( void ) const override
    { return EventType_Touch; }
private:
	CVec2 m_Position;
	FingerState_t m_nState;
};

#endif