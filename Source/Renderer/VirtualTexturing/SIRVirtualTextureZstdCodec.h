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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREZSTDCODEC_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREZSTDCODEC_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTextureClusterDecode.h>

// Native Terrain3D Sparse V1 outer-cluster Zstd codec. The caller retains
// ownership of encoded and decoded buffers. This codec never retains them.
// Compiled only with SIR_WITH_VT_ZSTD and its pinned ThirdParty/Zstd source.
const sirVirtualTextureClusterCodec_t *sirVTGetZstdClusterCodec( void ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREZSTDCODEC_H__
