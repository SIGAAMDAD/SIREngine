#ifndef __SIRENGINE_KEY_EVENT_H__
#define __SIRENGINE_KEY_EVENT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/Compiler.h>
#include "EventBase.h"
#include <SDL2/SDL_keyboard.h>
#include "KeyCodes.h"

extern const KeyNum_t SDLToKeynumTable[ NumKeyIds ];

class CKeyEvent : public IEventBase
{
public:
	CKeyEvent( bool bState, SDL_Keycode nKeyID )
		: m_bDown( bState ), m_nKeyValue( nKeyID )
	{ }
	virtual ~CKeyEvent() override
	{ }

	virtual void Send( void ) override
	{ }

	SIRENGINE_FORCEINLINE virtual const char *GetName( void ) const override
	{ return "KeyEvent"; }
	SIRENGINE_FORCEINLINE virtual EventType_t GetType( void ) const override
	{ return EventType_Key; }

	static KeyNum_t SDLKeyToEngineKey( SDL_Keysym *pKeysym );
private:
	bool32 m_bDown;
	uint32_t m_nKeyValue;
};

#endif