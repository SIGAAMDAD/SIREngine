#ifndef __SIRENGINE_EVENT_BASE_H__
#define __SIRENGINE_EVENT_BASE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

typedef enum : uint32_t {
	EventType_Key,
	EventType_Mouse,
	EventType_Gamepad,
	EventType_Joystick,
	EventType_Action,
	EventType_Window,
	EventType_Touch,

	// add/remove controller device
	EventType_ControllerStatus,

	// only for internal engine use
	EventType_Quit,

	NumEventTypes
} EventType_t;

class IEventBase
{
public:
	IEventBase( void )
	{ }
	virtual ~IEventBase()
	{ }

	virtual void Send( void ) = 0;

	virtual const char *GetName( void ) const = 0;
	virtual EventType_t GetType( void ) const = 0;
};

class CQuitEvent : public IEventBase
{
public:
	CQuitEvent( void )
	{ }
	virtual ~CQuitEvent() override
	{ }

	virtual void Send( void ) override
	{ }

	virtual const char *GetName( void ) const override
	{ return "QuitEvent"; }
	virtual EventType_t GetType( void ) const override
	{ return EventType_Quit; }
};

#endif