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

#ifndef __SIR_NATIVE_INPUT_DRIVER_WIN32_H__
#define __SIR_NATIVE_INPUT_DRIVER_WIN32_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRNativeInputDriver.h>
#include <Core/Events/SIREvent.h>
#include <Core/Platform/Shared/Window/SIREventTypes.h>

class SIRNativeInputDriverWin32 final : public SIRNativeInputDriver
{
public:
	explicit SIRNativeInputDriverWin32( SIREvent<SIREventData>& eventPublisher ) noexcept;
	~SIRNativeInputDriverWin32() noexcept;

	SIRNativeInputBackend getBackend( void ) const noexcept override;
	void pumpEvents( void ) noexcept override;
	bool isKeyPressed( SIRKeycode eKey ) const noexcept override;
	bool isMouseButtonPressed( SIRMouseButton eButton ) const noexcept override;

private:
};

#endif // __SIR_NATIVE_INPUT_DRIVER_WIN32_H__
