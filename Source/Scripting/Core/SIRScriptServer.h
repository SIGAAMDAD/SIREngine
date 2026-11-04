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

#ifndef __SIRENGINE_SIRSCRIPTSERVER_H__
#define __SIRENGINE_SIRSCRIPTSERVER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptRuntime.h>

// Owns shared schema publication and scripting lifecycle in that order.
// No VM or native compiler dependency is introduced into the Core layer.
class SIRScriptServer
{
public:
	SIRScriptServer( void ) noexcept;

	SIRScriptBindingRegistry &bindings( void ) noexcept;
	SIRScriptRuntime &runtime( void ) noexcept;

private:
	SIRScriptBindingRegistry m_Bindings;
	SIRScriptRuntime m_Runtime;
};

#endif // __SIRENGINE_SIRSCRIPTSERVER_H__
