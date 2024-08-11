#ifndef __SIRENGINE_INPUT_MANAGER_H__
#define __SIRENGINE_INPUT_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/EngineApp.h>
#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_joystick.h>
#include <SDL2/SDL_haptic.h>
#include <SDL2/SDL_touch.h>

class CInputDevice
{
public:
	enum class EType : uint32_t {
		Keyboard = 0,
		Controller,
		TouchScreen
	};
public:
	CInputDevice( void )
		: m_nType( EType::Keyboard ), m_nDeviceIndex( 0 ), m_pController( NULL )
	{ }
	~CInputDevice()
	{ Shutdown(); }

	void Init( EType nInitialType, int32_t nIndex );
	void Shutdown( void );
	void ApplyHaptic( int32_t nDuration, float nIntensity );

	SIRENGINE_FORCEINLINE bool IsGamepad( void ) const
	{ return m_pController->m_pGamepad != NULL; }

	SIRENGINE_FORCEINLINE EType GetType( void ) const
	{ return m_nType; }
	SIRENGINE_FORCEINLINE int32_t GetIndex( void ) const
	{ return m_nDeviceIndex; }
private:
	void InitKeyboard( void );
	void InitController( void );
	void InitTouchDevice( void );

	typedef struct {
		SDL_Joystick *m_pJoystick;
		SDL_GameController *m_pGamepad;
		SDL_Haptic *m_pHaptic;
		SDL_JoystickID m_hJoystickID;
	} JoystickData_t;

	EType m_nType;
	int32_t m_nDeviceIndex;
	JoystickData_t *m_pController;
	
	SDL_TouchID m_hTouchDevice;
    SDL_FingerID m_hTouchFinger;
};

class CInputManager : public IEngineApp
{
public:
	CInputManager( void );
	virtual ~CInputManager() override;

	virtual const char *GetName( void ) const override
	{ return "Inputmanager"; }
	virtual bool IsActive( void ) const override
	{ return true; }
	virtual uint32_t GetState( void ) const override
	{ return 0; }

	virtual void Init( void ) override;
	virtual void Shutdown( void ) override;
	virtual void Frame( int64_t msec ) override;

	virtual void SaveGame( void ) override;
	virtual void LoadGame( void ) override;

	SIRENGINE_FORCEINLINE static CInputManager& Get( void )
	{ return g_InputManager; }
private:
	static void KeyboardEventListener( const IEventBase *pEventData );
	static void GamepadEventListener( const IEventBase *pEventData );
	static void ControllerStatusListener( const IEventBase *pEventData );

	void AddController( int32_t nDeviceID );
	void RemoveController( int32_t nDeviceID );

	CStaticArray<CInputDevice, SIRENGINE_MAX_COOP_PLAYERS> m_szInputDevices;

	static CInputManager g_InputManager;
};

#endif