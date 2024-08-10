#ifndef __SIRENGINE_EVENT_MANAGER_H__
#define __SIRENGINE_EVENT_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/EngineApp.h>
#include "EventBase.h"
#include "WindowEvent.h"
#include <SDL2/SDL_keycode.h>
#include <EASTL/shared_ptr.h>

class IEventListener
{
public:
	IEventListener( const CString& name, EventType_t nType )
		: m_Name( name ), m_nCaptureType( nType )
	{ }
	virtual ~IEventListener()
	{ }

	virtual void Dispatch( const IEventBase *pEventData ) = 0;

	SIRENGINE_FORCEINLINE const CString& GetName( void ) const
	{ return m_Name; }
	SIRENGINE_FORCEINLINE EventType_t GetType( void ) const
	{ return m_nCaptureType; }
protected:
	CString m_Name;
	EventType_t m_nCaptureType;
};

class CEventListener : public IEventListener
{
public:
	template<typename Fn, typename... Args>
	CEventListener( const CString& name, EventType_t nType, Fn&& pCallback, Args&&... args )
		: IEventListener( name, nType ),
		m_Function( [&]( const IEventBase *pEventData ){ pCallback( eastl::forward<Args>( args )..., pEventData ); } )
	{ }
	template<typename Fn>
	CEventListener( const CString& name, EventType_t nType, Fn&& pCallback )
		: IEventListener( name, nType ),
		m_Function( [&]( const IEventBase *pEventData ){ pCallback( pEventData ); } )
	{ }
	virtual ~CEventListener() override
	{ }

	inline virtual void Dispatch( const IEventBase *pBase ) override
	{ m_Function( pBase ); }
private:
	eastl::function<void( const IEventBase * )> m_Function;
};

class CEventManager : public IEngineApp
{
	using CEventListenerList = eastl::unordered_map<CString, eastl::shared_ptr<IEventListener>>;
public:
	CEventManager( void );
	virtual ~CEventManager() override;

	virtual const char *GetName( void ) const override
	{ return "EventManager"; }
	virtual bool IsActive( void ) const override
	{ return true; }
	virtual uint32_t GetState( void ) const override
	{ return 0; }

	virtual void Init( void ) override;
	virtual void Shutdown( void ) override;
	virtual void Frame( int64_t msec ) override;

	virtual void SaveGame( void ) override;
	virtual void LoadGame( void ) override;

	void AddEventListener( const eastl::shared_ptr<IEventListener>& listener );
	void PushEvent( IEventBase *pEvent );

	static IEventBase *CreateKeyEvent( bool bState, SDL_Keycode nKeyID );
	static IEventBase *CreateWindowEvent( WindowEventType_t nEventType, int32_t nValue1, int32_t nValue2 );
	static IEventBase *CreateGamepadEvent( );
	static IEventBase *CreateJoystickEvent( );
	static IEventBase *CreateTouchEvent( float x, float y, uint32_t nFingerState );
	static IEventBase *CreateMouseEvent();
	static IEventBase *CreateActionEvent();
	static IEventBase *CreateControllerStatusEvent( bool bStatus );

	SIRENGINE_FORCEINLINE static CEventManager& Get( void )
	{ return g_EventManager; }
private:
	CStaticArray<CEventListenerList, NumEventTypes> m_EventBindings;

	CVector<IEventBase *> m_EventList;
	uint64_t m_nEventsHead;
	uint64_t m_nEventsTail;

	static CEventManager g_EventManager;
	static CQuitEvent g_QuitEvent;
};

#endif