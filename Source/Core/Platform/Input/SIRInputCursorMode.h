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
#ifndef __SIRENGINE_SIRINPUTCURSORMODE_H__
#define __SIRENGINE_SIRINPUTCURSORMODE_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirInputCursorMode_e : uint8_t
{
	SIR_INPUT_CURSOR_FREE = 0,
	SIR_INPUT_CURSOR_HIDDEN = 1,
	SIR_INPUT_CURSOR_CAPTURED = 2
} sirInputCursorMode_t;

#endif // __SIRENGINE_SIRINPUTCURSORMODE_H__
