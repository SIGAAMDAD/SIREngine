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

#ifndef __SIRENGINE_SIREDITORCATALOGBRIDGE_H__
#define __SIRENGINE_SIREDITORCATALOGBRIDGE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorResourceReferences.h>
#include <Resources/SIRResourceCatalog.h>

// The canonical importer supplies scene-scoped identities after resolving the
// source path and owning resource types. This bridge does not mint global IDs,
// intern strings, open files, or own resource bytes.
typedef struct sirEditorCatalogIdentity_s
{
	uint64_t nCanonicalId;
	uint64_t nPathAtom;
	uint32_t nTypeId;
} sirEditorCatalogIdentity_t;

typedef struct sirEditorCatalogBuildRequest_s
{
	const sirEditorTextResourceIndex_t *pSourceIndex;
	const sirEditorResourceReference_t *pReferences;
	const sirEditorCatalogIdentity_t *pIdentities;
	uint32_t nReferenceCount;
	uint32_t nIdentityCount;
	SIRResourceCatalog *pCatalog;
	sirResourceEntry_t *pEntries;
	sirResourceDependency_t *pDependencies;
	uint32_t *pScratch;
	uint8_t *pSeen;
	uint32_t nEntryCapacity;
	uint32_t nDependencyCapacity;
} sirEditorCatalogBuildRequest_t;

typedef enum sirEditorCatalogBuildResult_e : uint8_t
{
	SIR_EDITOR_CATALOG_OK = 0,
	SIR_EDITOR_CATALOG_INVALID_ARGUMENT = 1,
	SIR_EDITOR_CATALOG_INVALID_SOURCE = 2,
	SIR_EDITOR_CATALOG_INVALID_IDENTITY = 3,
	SIR_EDITOR_CATALOG_OUT_OF_CAPACITY = 4,
	SIR_EDITOR_CATALOG_CYCLIC = 5,
	SIR_EDITOR_CATALOG_FAILED = 6
} sirEditorCatalogBuildResult_t;

// Imports source-backed resource declarations and embedded-resource edges into
// the existing canonical catalog. Reference records owned by scene nodes are
// checked, but do not form catalog edges. Repeated dependency occurrences are
// coalesced. The catalog must be uninitialized on entry. Any failed import
// after initialization calls shutdown(), leaving it uninitialized; caller
// storage remains owned by the caller and is not guaranteed to be unchanged.
// The successful catalog is sealed and can produce a dependency-first order.
sirEditorCatalogBuildResult_t sirEditorBuildResourceCatalog( const sirEditorCatalogBuildRequest_t& request ) noexcept;

#endif // __SIRENGINE_SIREDITORCATALOGBRIDGE_H__
