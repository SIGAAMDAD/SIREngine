#ifndef __SIRENGINE_CONTROLLER_EVENT_H__
#define __SIRENGINE_CONTROLLER_EVENT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "EventBase.h"
#include "KeyCodes.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_gamecontroller.h>

class CGamepadEvent : public IEventBase
{
public:
	CGamepadEvent( const SDL_Event *pEventData );
	virtual ~CGamepadEvent() override
	{ }

	virtual void Send( void ) override
	{ }

	SIRENGINE_FORCEINLINE virtual const char *GetName( void ) const override
	{ return "GamepadEvent"; }
	SIRENGINE_FORCEINLINE virtual EventType_t GetType( void ) const override
	{ return EventType_Gamepad; }
private:
	KeyNum_t m_nButton;
};

#endif