/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRINPUTACTIONS_H__
#define __SIRENGINE_SIRINPUTACTIONS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputTypes.h>

constexpr uint32_t SIR_INPUT_ACTION_CAPACITY = 128;
constexpr uint32_t SIR_INPUT_CONTEXT_CAPACITY = 16;
constexpr uint32_t SIR_INPUT_BINDING_CAPACITY = 256;
constexpr uint16_t SIR_INPUT_NO_CHORD = UINT16_MAX;
constexpr uint32_t SIR_INPUT_PROFILE_HEADER_BYTES = 24;
constexpr uint32_t SIR_INPUT_PROFILE_CONTEXT_BYTES = 16;
constexpr uint32_t SIR_INPUT_PROFILE_ACTION_BYTES = 8;
constexpr uint32_t SIR_INPUT_PROFILE_BINDING_BYTES = 28;

typedef enum sirInputBindingFlags_e : uint8_t
{
	SIR_INPUT_BIND_NONE = 0,
	SIR_INPUT_BIND_CONSUME = 1,
	SIR_INPUT_BIND_TOGGLE = 2
} sirInputBindingFlags_t;

typedef enum sirInputCaptureStatus_e : uint8_t
{
	SIR_INPUT_CAPTURE_IDLE = 0,
	SIR_INPUT_CAPTURE_PENDING = 1,
	SIR_INPUT_CAPTURE_ACCEPTED = 2,
	SIR_INPUT_CAPTURE_CONFLICT = 3
} sirInputCaptureStatus_t;

typedef struct sirInputActionContext_s
{
	uint32_t nContextId;
	uint32_t nUser;
	int32_t nPriority;
	bool bActive;
} sirInputActionContext_t;

typedef struct sirInputActionBinding_s
{
	uint32_t nActionId;
	uint32_t nContextId;
	uint16_t nControl;
	uint16_t nChord;
	uint32_t nUser;
	int32_t nDeadzone;
	int32_t nScale;
	sirInputDeviceClass_t eClass;
	sirInputSignal_t eSignal;
	uint8_t nFlags;
} sirInputActionBinding_t;

typedef struct sirInputActionState_s
{
	uint32_t nActionId;
	uint32_t nUser;
	int32_t nValue;
	uint32_t nTransitions;
	bool bHeld;
	bool bPressed;
	bool bReleased;
} sirInputActionState_t;

// Bounded action/context evaluation: no per-tick allocations or native SDKs.
class SIRInputActions
{
public:
	SIRInputActions( void ) noexcept;
	bool addAction( uint32_t nActionId, uint32_t nUser ) noexcept;
	bool addContext( uint32_t nContextId, uint32_t nUser, int32_t nPriority, bool bActive ) noexcept;
	bool setContextActive( uint32_t nContextId, uint32_t nUser, bool bActive ) noexcept;
	bool addBinding( const sirInputActionBinding_t& binding ) noexcept;
	bool rebind( uint32_t iBinding, const sirInputActionBinding_t& binding ) noexcept;
	bool removeBinding( uint32_t iBinding ) noexcept;
	bool beginCapture( uint32_t iBinding ) noexcept;
	void cancelCapture( void ) noexcept;
	sirInputCaptureStatus_t captureStatus( void ) const noexcept;
	bool describeBinding( uint32_t iBinding, sirInputActionBinding_t *pOut ) const noexcept;
	bool query( uint32_t nActionId, uint32_t nUser, sirInputActionState_t *pOut ) const noexcept;
	void evaluate( const sirInputFrame_t& frame ) noexcept;
	bool saveProfile( uint8_t *pData, uint32_t nCapacity, uint32_t *pWritten ) const noexcept;
	bool loadProfile( const uint8_t *pData, uint32_t nBytes ) noexcept;
	void clear( void ) noexcept;
	uint32_t bindingCount( void ) const noexcept;
	uint32_t contextCount( void ) const noexcept;

private:
	int32_t actionIndex( uint32_t nActionId, uint32_t nUser ) const noexcept;
	int32_t contextIndex( uint32_t nContextId, uint32_t nUser ) const noexcept;
	bool bindingValid( const sirInputActionBinding_t& binding, int32_t iExclude ) const noexcept;
	bool activeBinding( const sirInputActionBinding_t& binding, int32_t *pPriority ) const noexcept;
	bool shadowed( const sirInputActionBinding_t& binding, const sirInputFrame_t& frame ) const noexcept;
	bool bindingHeld( const sirInputActionBinding_t& binding, const sirInputFrame_t& frame ) const noexcept;
	int32_t bindingValue( const sirInputActionBinding_t& binding, const sirInputFrame_t& frame ) const noexcept;
	void refreshAction( uint32_t iAction, const sirInputFrame_t& frame, int32_t nMinimumPriority, bool bEdge ) noexcept;
	void processEvent( const sirInputEvent_t& event, const sirInputFrame_t& frame ) noexcept;

	sirInputActionContext_t m_Contexts[SIR_INPUT_CONTEXT_CAPACITY];
	sirInputActionBinding_t m_Bindings[SIR_INPUT_BINDING_CAPACITY];
	sirInputActionState_t m_Actions[SIR_INPUT_ACTION_CAPACITY];
	uint32_t m_Held[SIR_INPUT_DEVICE_CAPACITY][SIR_INPUT_BUTTON_WORDS];
	int32_t m_Axes[SIR_INPUT_DEVICE_CAPACITY][SIR_INPUT_AXIS_CAPACITY];
	sirInputDeviceId_t m_DeviceIds[SIR_INPUT_DEVICE_CAPACITY];
	sirInputDeviceClass_t m_Classes[SIR_INPUT_DEVICE_CAPACITY];
	bool m_Toggled[SIR_INPUT_ACTION_CAPACITY];
	uint32_t m_nContexts;
	uint32_t m_nBindings;
	uint32_t m_nActions;
	uint64_t m_nLastTick;
	bool m_bContextDirty;
	sirInputCaptureStatus_t m_eCaptureStatus;
	uint32_t m_iCaptureBinding;
};

#endif // __SIRENGINE_SIRINPUTACTIONS_H__
