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

#ifndef __SIR_ENGINESYSTEM_H__
#define __SIR_ENGINESYSTEM_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class SIRSystemResult : uint8_t {
	SIR_SYSTEM_OK = 0,
	SIR_SYSTEM_BUSY = 1,
	SIR_SYSTEM_ERROR = 2
};

// Implementations own their internal resources. The application owns the
// order of lifecycle calls, but borrows each registered system object.
// A failed initialize call must undo any partial initialization itself.
class SIREngineSystem
{
public:
	SIREngineSystem( void ) noexcept = default;
	SIREngineSystem( const SIREngineSystem& ) = delete;
	virtual ~SIREngineSystem( void ) noexcept;

	SIREngineSystem& operator=( const SIREngineSystem& ) = delete;

	virtual SIRSystemResult initialize( void ) noexcept = 0;
	virtual void update( float fDeltaSeconds ) noexcept;
	virtual void fixedUpdate( float fFixedDeltaSeconds ) noexcept;
	virtual SIRSystemResult shutdown( void ) noexcept = 0;

protected:
private:
};

#endif // __SIR_ENGINESYSTEM_H__
