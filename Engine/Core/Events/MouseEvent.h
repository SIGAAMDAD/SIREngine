#ifndef __SIRENGINE_MOUSE_EVENT_H__
#define __SIRENGINE_MOUSE_EVENT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/Compiler.h>
#include "EventBase.h"
#include <Engine/Core/MathLib/Types.h>

namespace SIREngine::Events {
	class CMouseEvent : public IEventBase
	{
	public:
		CMouseEvent( int x, int y )
			: m_Delta( x, y )
		{ }
		virtual ~CMouseEvent() override
		{ }

		virtual void Send( void ) override
		{ }

		SIRENGINE_FORCEINLINE const CIVec2& GetDelta( void ) const
		{ return m_Delta; }

		SIRENGINE_FORCEINLINE virtual const char *GetName( void ) const override
		{ return "MouseEvent"; }
		SIRENGINE_FORCEINLINE virtual EventType_t GetType( void ) const override
		{ return EventType_Mouse; }
	private:
		CIVec2 m_Delta;
	};
};

#endif