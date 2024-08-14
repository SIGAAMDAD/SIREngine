#ifndef __SIRENGINE_KEY_EVENT_H__
#define __SIRENGINE_KEY_EVENT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/Compiler.h>
#include <Engine/Core/Types.h>
#include "EventBase.h"
#include <SDL2/SDL_keyboard.h>
#include "KeyCodes.h"

namespace SIREngine::Events {
	class CKeyEvent : public IEventBase
	{
	public:
		CKeyEvent( bool bState, KeyNum_t nKeyID )
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

		SIRENGINE_FORCEINLINE bool IsPressed( void ) const
		{ return m_bDown; }
		SIRENGINE_FORCEINLINE KeyNum_t GetKey( void ) const
		{ return m_nKeyValue; }

#if !defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
		static KeyNum_t SDLKeyToEngineKey( const SDL_Event& eventData );
#else
		static KeyNum_t GLFW3KeyToEngineKey( int key, int mods );
#endif
	private:
		bool32 m_bDown;
		KeyNum_t m_nKeyValue;
	};
};

#endif