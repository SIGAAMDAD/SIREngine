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

#ifndef __SIRENGINE_SIRINPUTFRAMESTATE_H__
#define __SIRENGINE_SIRINPUTFRAMESTATE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputTypes.h>

// Source Engine IInputSystem's polled current-state and transition semantics.
// Sole owner thread appends events; published frame lives until the next publish.
class SIRInputDeviceRegistry;
class SIRInputFrameState
{
public:
	SIRInputFrameState( void ) noexcept;
	bool begin( uint64_t nTick ) noexcept;
	bool postDevice( sirInputDeviceId_t id, sirInputDeviceClass_t eClass, sirInputSignal_t eSignal,
	                 const sirInputTimestamp_t &time ) noexcept;
	bool postChanges( sirInputDeviceId_t id, sirInputDeviceClass_t eClass, const sirInputControlChange_t *pChanges, uint32_t nCount,
	                  const sirInputTimestamp_t &time ) noexcept;
	bool postPointer( sirInputDeviceId_t id, sirInputDeviceClass_t eClass, sirInputSignal_t eSignal, uint16_t nControl, int32_t nX,
	                  int32_t nY, const sirInputTimestamp_t &time ) noexcept;
	void clearDevice( sirInputDeviceId_t id ) noexcept;
	bool publish( const SIRInputDeviceRegistry &registry ) noexcept;
	bool recover( const SIRInputDeviceRegistry &registry ) noexcept;
	bool canPost( uint32_t nCount ) const noexcept;
	bool overflowed( void ) const noexcept;
	bool collecting( void ) const noexcept;
	uint64_t lastPublishedTick( void ) const noexcept;
	const sirInputFrame_t *frame( void ) const noexcept;
	void stop( void ) noexcept;

private:
	bool append( const sirInputEvent_t &event ) noexcept;
	bool apply( sirInputDeviceState_t &state, const sirInputControlChange_t &event ) const noexcept;
	void markStateDirty( uint32_t nSlot ) noexcept;
	sirInputDeviceState_t m_Live[SIR_INPUT_DEVICE_CAPACITY];
	sirInputFrame_t m_Frames[2];
	uint64_t m_nTick;
	uint64_t m_nSequence;
	uint64_t m_nLastTick;
	uint32_t m_nCount;
	uint32_t m_nPublishedIndex;
	uint32_t m_nResync;
	uint32_t m_nStateDirty[2];
	uint32_t m_nTransientMask;
	bool m_bCollecting;
	bool m_bOverflowed;
	bool m_bPublished;
};

#endif // __SIRENGINE_SIRINPUTFRAMESTATE_H__
