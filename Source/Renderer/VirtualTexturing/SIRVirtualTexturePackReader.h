/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREPACKREADER_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREPACKREADER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

// Terrain3D Sparse V1 record semantics. This is the only accepted pack ABI.
// The view borrows caller-owned bytes; no allocations or payload copies occur.
// A metadata-only view validates all record tables without reading payloads.
bool sirVTPackOpenValidated( const sirVirtualTexturePackInput_t& input, sirVirtualTexturePackView_t *pView ) noexcept;

// Safe unaligned access to the frozen native V1 record layout. Requires an
// already validated view, whose byte lifetime remains owned by the caller.
bool sirVTPackGetPage( const sirVirtualTexturePackView_t& view, uint32_t nIndex, sirVirtualTexturePackPageRecord_t *pPage ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREPACKREADER_H__
