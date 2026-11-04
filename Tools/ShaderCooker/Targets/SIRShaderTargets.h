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

#ifndef __SIRENGINE_SIRSHADERTARGETS_H__
#define __SIRENGINE_SIRSHADERTARGETS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Tools/ShaderCooker/Frontend/SIRShaderFrontend.h>

// GL and legacy D3D have explicit recipe versions but require separate
// offline cross-compilation and cannot silently receive DXIL/SPIR-V bytes.
sirShaderCookResult_t sirShaderCookResolveTarget( sirGraphicsBackend_t eBackend, uint32_t nStage, std::string *pProfile, bool *pSpirv );

#endif // __SIRENGINE_SIRSHADERTARGETS_H__
