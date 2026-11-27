#ifndef __SIR_NATIVE_INPUT_DRIVER_H__
#define __SIR_NATIVE_INPUT_DRIVER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIREventTypes.h"
#include <Core/Events/SIRSPSCEventRing.h>

enum class SIRNativeInputBackend : uint8_t
{
	X11,
	Evdev,
	Wayland,
	LibInput,

	Win32,
	XInput,
	DirectInput,

	SDL
};

class SIRNativeInputDriver
{
public:
	~SIRNativeInputDriver() noexcept = default;

	virtual SIRNativeInputBackend getBackend( void ) const noexcept = 0;

	virtual void pumpEvents( void ) noexcept = 0;

	virtual bool isKeyPressed( SIRKeycode key ) const noexcept = 0;
	virtual bool isMouseButtonPressed( SIRMouseButton mouseButton ) const noexcept = 0;

protected:
	explicit SIRNativeInputDriver( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept;

	// The platform host owns this queue. All native producers sharing it
	// must publish on the same producer thread.
	SIRSPSCEventRing<SIREventData>& m_EventQueue;
};

SIR_FORCEINLINE SIRNativeInputDriver::SIRNativeInputDriver( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept
	: m_EventQueue( eventQueue )
{
}

#endif // __SIR_NATIVE_INPUT_DRIVER_H__
