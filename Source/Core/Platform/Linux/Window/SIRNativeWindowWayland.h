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

#ifndef __SIR_NATIVE_WINDOW_WAYLAND_H__
#define __SIR_NATIVE_WINDOW_WAYLAND_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/SIRNativeWindow.h>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
#include <fractional-scale-v1-client-protocol.h>
#include <viewporter-client-protocol.h>
#endif
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
#include <pointer-constraints-unstable-v1-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>
#endif
#include <errno.h>
#include <poll.h>

// TODO:
class SIRNativeWindowWayland final : public SIRNativeWindow
{
public:
private:
};

#endif // __SIR_NATIVE_WINDOW_WAYLAND_H__
