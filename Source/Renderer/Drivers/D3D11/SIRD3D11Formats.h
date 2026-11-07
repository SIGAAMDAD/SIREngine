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

#ifndef __SIRENGINE_SIRD3D11FORMATS_H__
#define __SIRENGINE_SIRD3D11FORMATS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

// nDimensionMask uses bit (dimension - 1). nSampleCountMask uses bit
// log2(sample-count): bit zero is 1x, bit one is 2x, etc. Native DXGI
// flags and Windows SDK objects do not escape the backend implementation.
typedef struct sirD3D11FormatSupport_s
{
	uint32_t nSize;
	sirGraphicsFormat_t eFormat;
	uint32_t nDimensionMask;
	uint32_t nSupportedUsages;
	uint32_t nSampleCountMask;
} sirD3D11FormatSupport_t;

// The portable admission check is also used by the native format probe test.
sirGraphicsResult_t sirD3D11ValidateTextureSupport( const sirGraphicsTextureDesc_t *pDesc,
                                                    const sirD3D11FormatSupport_t *pSupport ) noexcept;

// The pointer is a borrowed native ID3D11Device; never retained. Returned
// capability bits are built from CheckFormatSupport and multisample queries.
sirGraphicsResult_t sirD3D11QueryFormatSupport( void *pNativeDevice, sirGraphicsFormat_t eFormat,
                                                sirD3D11FormatSupport_t *pSupport ) noexcept;
uint32_t sirD3D11GetMaxSampleCount( const sirD3D11FormatSupport_t *pSupport ) noexcept;
// Native DXGI_FORMAT numeric value; the signature remains platform-neutral.
uint32_t sirD3D11GetNativeFormatCode( sirGraphicsFormat_t eFormat ) noexcept;

#endif // __SIRENGINE_SIRD3D11FORMATS_H__
