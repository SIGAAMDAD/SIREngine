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
#ifndef __SIRENGINE_SIRD3D9FORMATS_H__
#define __SIRENGINE_SIRD3D9FORMATS_H__
#include <SIRPCH.hpp>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#ifdef _WIN32
#include <Renderer/Drivers/D3D9/SIRD3D9Native.h>

sirGraphicsResult_t sirD3D9NativeMapFormat( sirGraphicsFormat_t eFormat, D3DFORMAT *pNativeFormat ) noexcept;
sirGraphicsResult_t sirD3D9NativeQueryFormat( sirD3D9Native_t *pNative, sirGraphicsABIFormat_t *pFormat ) noexcept;
#endif
#endif // __SIRENGINE_SIRD3D9FORMATS_H__
