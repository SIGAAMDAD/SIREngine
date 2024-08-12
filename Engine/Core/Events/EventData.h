#ifndef __SIRENGINE_EVENT_DATA_H__
#define __SIRENGINE_EVENT_DATA_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "ControllerStatusEvent.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "GamepadEvent.h"
#include "TouchEvent.h"
#include "WindowEvent.h"
#include <EASTL/optional.h>

#define MAX_EVENT_SIZE 24

namespace SIREngine::Events {

class CEventData
{
public:
	CEventData( void )
	{ memset( this, 0, sizeof( *this ) ); }
	CEventData( const CEventData& other )
	{ memcpy( this, eastl::addressof( other ), sizeof( *this ) ); }

	CEventData( const IEventBase& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }
	CEventData( const CWindowEvent& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }
	CEventData( const CTouchEvent& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }
	CEventData( const CKeyEvent& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }
	CEventData( const CControllerStatusEvent& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }
	CEventData( const CGamepadEvent& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }
	CEventData( const CMouseEvent& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }
	CEventData( const CQuitEvent& eventData )
	{ Copy( eastl::addressof( eventData ), sizeof( eventData ) ); }

	~CEventData()
	{ }

	const CEventData& operator=( const CEventData& other )
	{ return *(const CEventData *)memcpy( this, eastl::addressof( other ), sizeof( *this ) ); }

	SIRENGINE_FORCEINLINE const IEventBase *GetData( void ) const
	{ return (const IEventBase *)m_szData; }
	SIRENGINE_FORCEINLINE EventType_t GetType( void ) const
	{ return ( (const IEventBase *)m_szData )->GetType(); }

	SIRENGINE_FORCEINLINE const CWindowEvent& GetWindowData( void ) const
	{ return *(const CWindowEvent *)m_szData; }
	SIRENGINE_FORCEINLINE const CTouchEvent& GetTouchData( void ) const
	{ return *(const CTouchEvent *)m_szData; }
	SIRENGINE_FORCEINLINE const CKeyEvent& GetKeyData( void ) const
	{ return *(const CKeyEvent *)m_szData; }
	SIRENGINE_FORCEINLINE const CControllerStatusEvent& GetControllerStatusData( void ) const
	{ return *(const CControllerStatusEvent *)m_szData; }
	SIRENGINE_FORCEINLINE const CGamepadEvent& GetGamepadData( void ) const
	{ return *(const CGamepadEvent *)m_szData; }
	SIRENGINE_FORCEINLINE const CMouseEvent& GetMouseData( void ) const
	{ return *(const CMouseEvent *)m_szData; }
	SIRENGINE_FORCEINLINE const CQuitEvent& GetQuitData( void ) const
	{ return *(const CQuitEvent *)m_szData; }
private:
	inline void Copy( const void *pData, size_t nBytes )
	{ memset( m_szData, 0, MAX_EVENT_SIZE ); memcpy( m_szData, pData, nBytes ); }
	byte m_szData[ MAX_EVENT_SIZE ];
};

};

#endif