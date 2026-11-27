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

#ifndef __SIR_EVENT_TYPES_H__
#define __SIR_EVENT_TYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class SIREventType : uint32_t
{
	SIR_EVENT_QUIT,
	SIR_EVENT_TERMINATING,
	SIR_EVENT_LOW_MEMORY,

	SIR_EVENT_WILL_ENTER_BACKGROUND,
	SIR_EVENT_DID_ENTER_BACKGROUND,

	SIR_EVENT_WILL_ENTER_FOREGROUND,
	SIR_EVENT_DID_ENTER_FOREGROUND,

	SIR_EVENT_LOCALE_CHANGED,
	SIR_EVENT_SYSTEM_THEME_CHANGED,
};

struct SIREvent
{
	const SIREventType eType;
};

#endif // __SIR_EVENT_TYPES_H__
