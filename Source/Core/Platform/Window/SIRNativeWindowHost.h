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
#ifndef __SIRENGINE_SIRNATIVEWINDOWHOST_H__
#define __SIRENGINE_SIRNATIVEWINDOWHOST_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Window/SIRWindowDriver.h>

// Ported from the original editor native host. The driver is borrowed for
// one visible window lifecycle and is destroyed by its caller after return.
int sirRunWindowHostsWithDriver( SIRWindowDriver& driver, const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept;
int sirRunNativeWindowHosts( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept;
int sirRunWindowHostWithDriver( SIRWindowDriver& driver, const sirWindowCreateDesc_t& desc ) noexcept;
int sirRunNativeWindowHost( const sirWindowCreateDesc_t& desc ) noexcept;

#endif // __SIRENGINE_SIRNATIVEWINDOWHOST_H__
