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

#ifndef __SIRENGINE_SIRWINDOWSRAWDECODE_H__
#define __SIRENGINE_SIRWINDOWSRAWDECODE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

class SIRInputService;

// Internal Win32 collector processing: the raw OS record, then aligned batches.
bool sirInputDecodeWindowsRawRecord( SIRInputService &service, const RAWINPUT &input ) noexcept;
bool sirInputDecodeWindowsRawBatch( SIRInputService &service, const RAWINPUT *pRecords, uint32_t nCount, uint32_t nCapacityBytes ) noexcept;

#endif // __SIRENGINE_SIRWINDOWSRAWDECODE_H__
