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

#ifndef __SIRENGINE_SIRVIRTUALTEXTURERESOURCEIO_H__
#define __SIRENGINE_SIRVIRTUALTEXTURERESOURCEIO_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTextureManifest.h>
#include <Resources/SIRResourceGraph.h>

// SIRPack-compatible file interval: the container/catalog owns locating the
// interval, while this adapter validates only the immutable SVT1 chunk.
// There is no private staging heap. GRDG owns every published payload byte.
typedef struct sirVirtualTextureFileRequest_s
{
	const char *pszUtf8Path;
	sirResourceKey_t source;
	uint64_t nOffset;
	uint64_t nBytes;
	uint64_t nExpectedRevision;
	uint32_t nStagingDomain;
	uint32_t nAlignment;
} sirVirtualTextureFileRequest_t;

// Validates bytes BEFORE publishExpected. Rejected files never enter GRDG.
// On success, publication transfers the staging allocation to GRDG.
SIRResourceResult sirVTImportFile( SIRResourceGraph& graph, const sirVirtualTextureFileRequest_t& request,
                                   SIRResourceHandle *pHandle ) noexcept;

// Borrowed validated manifest view: the caller must keep scope alive for ALL
// uses of pView->pBytes and page pointers derived from it.
SIRResourceResult sirVTGetResourceManifest( const SIRResourceGraph& graph, const SIRResourceReadScope& scope, SIRResourceHandle handle,
                                            sirVirtualTextureManifestView_t *pView ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTURERESOURCEIO_H__
