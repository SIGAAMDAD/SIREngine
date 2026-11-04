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

#ifndef __SIRENGINE_SIRGLCAPABILITIES_H__
#define __SIRENGINE_SIRGLCAPABILITIES_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Renderer/Drivers/OpenGL/SIRGLContext.h>

sirGraphicsResult_t sirGLValidateCore46( const sirGLProbe_t *pProbe ) noexcept;
sirGraphicsResult_t sirGLBuildAdapter( const sirGLProbe_t *pProbe, sirGraphicsABIAdapter_t *pAdapter ) noexcept;
sirGraphicsResult_t sirGLBuildCapabilities( const sirGLProbe_t *pProbe, uint32_t nGeneration,
                                            sirGraphicsABICapabilities_t *pCapabilities ) noexcept;
sirGraphicsResult_t sirGLQuerySupportedFormat( uint32_t nFormat, sirGraphicsABIFormat_t *pFormat ) noexcept;

#endif // __SIRENGINE_SIRGLCAPABILITIES_H__
