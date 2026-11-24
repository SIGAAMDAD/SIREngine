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
#ifndef __SIR_INPUTCURSORMODE_H__
#define __SIR_INPUTCURSORMODE_H__
#include <Core/SIRPCH.h>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class SIRInputCursorMode : uint8_t {
	SIR_INPUT_CURSOR_FREE = 0,
	SIR_INPUT_CURSOR_HIDDEN = 1,
	SIR_INPUT_CURSOR_CAPTURED = 2
};

#endif // __SIR_INPUTCURSORMODE_H__
