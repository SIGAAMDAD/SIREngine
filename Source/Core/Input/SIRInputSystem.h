/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_INPUT_SYSTEM_H__
#define __SIR_INPUT_SYSTEM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Pointers/SIRSharedPtr.h>
#include <Core/Platform/Shared/Window/SIRNativeInputDriver.h>
#include <Core/Systems/SIREngineSystem.h>
#include <Core/Events/SIREvent.h>

struct SIRButtonBindingEvent
{
	uint16_t iBindingID;
};

class SIRInputSystem final : public SIREngineSystem
{
public:
	explicit SIRInputSystem( const SIRSharedPtr<SIRNativeInputDriver>& inputDriver ) noexcept;
	~SIRInputSystem() noexcept override;

	virtual SIRSystemResult initialize( void ) noexcept override;
	virtual void update( float fDeltaSeconds ) noexcept override;
	virtual SIRSystemResult shutdown( void ) noexcept override;

private:
	SIREvent<SIRButtonBindingEvent> m_ButtonBindingEvent;

	SIRSharedPtr<SIRNativeInputDriver> m_InputDriver;
};

#endif // __SIR_INPUT_SYSTEM_H__
